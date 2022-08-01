#include <math.h>
#include <inttypes.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "string.h"
#include "esp_sntp.h"

#include "common.h"
#include "matrix_calc.h"
#include "utils.h"
#include "fall_logic.h"
#include "radar_interface.h"


/* Port to receive data*/
#define UART1_TXD  GPIO_NUM_19				/**< Pin ignore*/
#define UART1_RXD  GPIO_NUM_25				/**< Pin to get data from radar*/
#define UART1_BAUDRATE 921600				/**< Baudrate need to high because maximum packet of a frame is 8k*/
/* Port to send config*/
#define UART2_TXD  GPIO_NUM_4				/**< Pin to send config to radar*/
#define UART2_RXD  GPIO_NUM_5				/**< Pin to receive status of a cmd when send config to radar*/
#define UART2_BAUDRATE 115200
#define CONFIG_RTS  UART_PIN_NO_CHANGE			/**< Pin ignore*/	
#define CONFIG_CTS  UART_PIN_NO_CHANGE			/**< Pin ignore*/

#define BUF_SIZE 4096					/**< Maximum size read from radar each loop*/

static const char *TAG = "radar_interface";

/**
 * @brief Use for extract magicword
 * 
 */
union magic_word_t {
        uint64_t u64_val;
	uint8_t  u8_arr[sizeof(uint64_t)];
};

/**
 * @brief Information about all data in one frame
 * 
 */
struct frame_header {
        uint64_t sync; 				/**< magic number to identify a frame*/
        uint32_t version;			/**< version of radar firmware*/
        uint32_t totalPacketLen;		/**< length of package*/
        uint32_t platform;			
        uint32_t frameNumber;			/**< frame number*/
        uint32_t subFrameNumber;		/**< if one not enough then radar will split frame and send (Ignore in this version)*/
        uint32_t chirpProcessingMargin;
        uint32_t frameProcessingMargin;
        uint32_t uartSentTime;
        uint32_t trackProcessTime;
        uint16_t numTLVs;			/**< each frame has subpacket idencate for pcs, targets, index*/
        uint16_t checksum;			/**< goal to verify frame header*/
};


//TODO: Need to modify calc.c because of removing malloc
void dot_two_matrixes_radar(double* src, double* des, int msrc, int nsrc, int mdes, int ndes, double* result)
{
	for (int src_row = 0; src_row < msrc; src_row++) {
		for (int des_col = 0; des_col < ndes; des_col++) {
			*(result + src_row * ndes + des_col) = 0;
			for (int idx = 0; idx < nsrc; idx++) {
				*(result + src_row * ndes + des_col) += *(src + src_row * nsrc + idx) * *(des + idx * ndes + des_col);
			}
		}
	}
}

/**
 * @brief Rotate point with a specific angle follow by x-axis
 * 
 * @param x x axis
 * @param y y axis
 * @param z z axis
 * @param theta angle in degree
 * @warning angle need to be in degree
 */
static void rotX(float* x, float* y, float* z, float theta) {
	float theta_rad = theta * PI / 180;
	double Rx[9] = {1, 0, 0, 0, cos(theta_rad), sin(theta_rad), 0, -sin(theta_rad), cos(theta_rad)};
	double target[3];
	target[0] = (double)*x;
	target[1] = (double)*y;
	target[2] = (double)*z;
	double res[3];
	dot_two_matrixes_radar(Rx, target, 3, 3, 3, 1, res);
	*x = (float)*(res);
	*y = (float)*(res+1);
	*z = (float)*(res+2) + SENSOR_HEIGHT;
}

/**
 * @brief Check if ring buffer enough for data
 * 
 * @param rb  ring buffer
 * @param mutex mutex for buffer
 * @param require_len requirement length of data
 * @param verbose print size of ring buffer
 * @retval 1 success
 * @retval 0 fail
 */
static bool check_rb_data_enough(RingbufHandle_t* rb, SemaphoreHandle_t* mutex, int require_len, bool verbose)
{
	UBaseType_t len = 0;

	if (xSemaphoreTake(*mutex, 200/portTICK_PERIOD_MS) == pdTRUE) {
		vRingbufferGetInfo(*rb, NULL, NULL, NULL, NULL, &len);
		xSemaphoreGive(*mutex);
	} else {
		return 0;
	}
	if (verbose)
		printf("rb size: %d - %d\n", len, require_len);
	if (len < require_len)
		return 0;
	return 1;
}

/**
 * @brief Unpack value in different type from uint8_t type array
 * 
 * @param data pointer to pointer of uint8_t array
 * @param data_length length of that buffer
 * @param res pointer to result variable
 * @param pattern need to have specific type get from <a href="https://docs.python.org/3/library/struct.html#format-characters"> python struct </a>
 */
static void struct_unpack_one_value(uint8_t **data, int* data_length, void* res, const char* pattern)
{
	if (strcmp(pattern, "I")==0) {
		*((uint32_t*)res) = *(*data + 0) << 0 | *(*data + 1) << 8 | *(*data + 2) << 16 | *(*data + 3) << 24; 
	} else if (strcmp(pattern, "H")==0) {
		*((unsigned short*)res) = *(*data + 0) << 0 | *(*data + 1) << 8;
	} else if (strcmp(pattern, "f")==0) {
        	struct_unpack(*data, pattern, (float*)res);
	} else if (strcmp(pattern, "b")==0) {
		*((char *)res) = *(*data + 0) << 0; 
	} else if (strcmp(pattern, "B")==0) {
		*((unsigned char *)res) = *(*data + 0) << 0;
	} else if (strcmp(pattern, "h")==0) {
		*((short*)res) = *(*data + 0) << 0 | *(*data + 1) << 8; 
	}
	*data += struct_calcsize(pattern);
	*data_length -= struct_calcsize(pattern);
}


/**
 *  @brief Extract point clouds data from sensor 
 *  @details
 *  Get information about (range, azimuth, elevation, doppler, snr) of each point then
 *      store it in param pc_data which is float matrix M(num of detected point, 5)
 * 
 *  @param    data        buffer contain pcs
 *  @param    data_len    length of that buffer
 *  @param    pc_data     pointer to matrix of pcs data (=NULL)
 *  @return   number of points had been detected
*/
static int parseCapon3DPolar(uint8_t* data, int data_len, float** pc_data)
{
	const char* pUnitStruct = "f";
	const int numUnit = 5;
	float pUnit[5];
	for (int i =0; i < numUnit; i ++) {
		struct_unpack_one_value(&data, &data_len, &pUnit[i], pUnitStruct);
	}
	const char*objStruct = "2bh2H";
	int objSize = struct_calcsize(objStruct);
	int numDetectedObj = (int)(data_len/objSize);

	for (int i = 0 ; i < numDetectedObj; i++) {
		char elev;
		char az;
		short doppler;
		unsigned short ran;
		unsigned short snr;
		struct_unpack_one_value(&data, &data_len, &elev, "b");
		struct_unpack_one_value(&data, &data_len, &az, "b");
		struct_unpack_one_value(&data, &data_len, &doppler, "h");
		struct_unpack_one_value(&data, &data_len, &ran, "H");
		struct_unpack_one_value(&data, &data_len, &snr, "H");
		int azE = az;
		int elevE = elev;
		int dopplerE = doppler;
		if ((int)az >= 128) {
			ESP_LOGD(TAG, "Az greater than 127");
			azE -= 256;
		}
		if ((int)elev >= 128) {
			ESP_LOGD(TAG, "Elev greater than 127");
			elevE -= 256;
		}
		if (doppler >= 128) {
			ESP_LOGD(TAG, "Doppler greater than 32768");
			doppler -= 65536;
		}
		float ranU = ran*pUnit[3];
		float azU = azE * pUnit[1];
		float elevU = elevE * pUnit[0];
		float dopplerU = dopplerE * pUnit[2];
		float snrU = snr * pUnit[4];
		// Change coordinate space (x, y, z, doppler, snr)
		*(*(pc_data) + i*5 + 0) = ranU * cos(elevU) * sin(azU); 
		*(*(pc_data) + i*5 + 1) = ranU * cos(elevU) * cos(azU);
		*(*(pc_data) + i*5 + 2) = sin(elevU) * ranU;		
		*(*(pc_data) + i*5 + 3) = dopplerU;
		*(*(pc_data) + i*5 + 4) = snrU;
		// TODO: Rotate dimension
		rotX(&(*(*(pc_data) + i*5 + 0)), &(*(*(pc_data) + i*5 + 1)), &(*(*(pc_data) + i*5 + 2)), TILT_ANGLE);
	}
	return numDetectedObj;
}


/**
 *  @brief Extract target data from sensor 
 *  @details
 *  If there is a person in radar area, this information will be send to uart
 * 
 *  (tid, x, y, z, vx, vy, vz, accx, accy, accz) 
 * 
 *  of each target then the function will extract and store it in param target_data which is matrix M(13, 10)
 *  Number of targets limited due to radar specs
 *  Note: Target information may be delay because the algorithm of sensor
 * 
 *  @param    data            buffer contain pcs
 *  @param    data_len        length of that buffer
 *  @param    target_data     pointer to matrix of targets (=NULL)
 *  @return   number of targets had been detected
*/
static int parseDetectedTracks3D(uint8_t* data, int data_len, float** target_data)
{
	//TODO: Need to test
	const char* targetStruct = "I27f";
	int numTargetInfo = 28;
	int targetSize = struct_calcsize(targetStruct);
	int numDetectedTarget = (int)(data_len/targetSize);

	for (int idx = 0; idx < numDetectedTarget; idx++) {
		unsigned int tid = data[0] | data[1] << 1 | data[2] << 2 | data[3] << 3;
		*(*target_data + idx*numTargetInfo) = 0.0 + tid;
		data += 4;
		data_len -= 4;
		for (int i = 1; i < numTargetInfo; i ++) {
			float value;
			*((uint8_t*)(&value) + 0) = *(data + 0);
			*((uint8_t*)(&value) + 1) = *(data + 1);
			*((uint8_t*)(&value) + 2) = *(data + 2);
			*((uint8_t*)(&value) + 3) = *(data + 3);
			data += 4;
			data_len -= 4;
			*(*target_data + idx*numTargetInfo + i) = value;
		}
		// TODO: Rotate dimension
	}
	return numDetectedTarget;
}


/**
 *  @brief	Extract index data from sensor 
 *  @details
 *  This data will be send after sensor catched a target and will be extracted and stored in index_data
 *      
 *  @note This is a list of index of each point clouds (classify to be target or not) in last frame (not current frame)  
 * 
 *  @param  	data            buffer contain pcs
 *  @param    	data_len        length of that buffer
 *  @param    	index_data      pointer to array of index (=NULL)
 *  @return     number id of points had been detected
*/
static int parseTargetAssociations(uint8_t* data, int data_len, uint8_t** indexes)
{
	const char* targetStruct = "B";
	int targetSize = struct_calcsize(targetStruct);
	int numIndexes = (int)(data_len/targetSize);

	for (int i = 0; i < numIndexes; i++) {
		struct_unpack_one_value(&data, &data_len, &(*indexes)[i], targetStruct);
	}
	return numIndexes;
}

/**
 * @brief Process frame data before sending to computation task 
 * 
 * @param curr_f pointer to current frame to log and use index for processing previous frame
 * @return pointer to features 
 */
static struct fall_features* feature_processing(struct frame_struct* curr_f)
{
	static float prev_pc[750*5]; 
	static float prev_tar[28*13];
	static uint8_t prev_idx[750];
	static struct frame_struct prev_frame;
	struct frame_struct* prev_f = &prev_frame;
	struct timeval tv_start;
	struct timeval tv_stop;

	struct fall_features* res = (struct fall_features*)malloc(sizeof(struct fall_features));
	if (res == NULL) {
		ESP_LOGE(TAG, "Cannot malloc fall features!");
		//TODO: reset
	}

	prev_f->point_clouds 	= prev_pc;
	prev_f->targets 	= prev_tar;
	prev_f->indexes 	= prev_idx;

	gettimeofday(&tv_start, NULL);
	// printf("Frame prev: %u vs %u\n", curr_f->frame_number, prev_f->frame_number);
	if (curr_f->frame_number - prev_f->frame_number > 1) {
		ESP_LOGW(TAG, "Missing Frame");
		prev_f->frame_number = curr_f->frame_number;
		return NULL;
	}
	res->frame_number 	= 	prev_f->frame_number;
	res->num_targets 	=	prev_f->num_targets;
	for (uint8_t tid = 0; tid < res->num_targets; tid ++) {
		// Truncate features to send
		for (uint8_t i = 0; i < 10; i ++) {
			res->target[tid*10 + i] = *(prev_f->targets + tid*28 + i);
		}
	}
	for (uint8_t tid = 0; tid < prev_f->num_targets; tid++) {
		uint8_t target_index = (uint8_t)*(prev_f->targets + 28*tid);
		res->abs_height[target_index] = calc_absolute_height( 	prev_f->point_clouds, 
									curr_f->indexes, 
									prev_f->num_point_clouds, 
									curr_f->num_indexes,
									target_index);
	}
	if (curr_f->frame_number == 1) 
		goto end;
	if (curr_f->num_indexes == 0) 
		goto end;
	if (curr_f->num_indexes != prev_f->num_point_clouds) {
		ESP_LOGW(TAG, "Different previous point clouds and indexes");
		goto end;
	}
	
	//TODO: Process features here
	gettimeofday(&tv_stop, NULL);
	float time_sec = tv_stop.tv_sec - tv_start.tv_sec + 1e-6f * (tv_stop.tv_usec - tv_start.tv_usec);
	vTaskDelay((30 - time_sec)/portTICK_PERIOD_MS); // Maximum time to have a normal run
end:
	prev_f->frame_number 		= curr_f->frame_number;
	prev_f->num_point_clouds 	= curr_f->num_point_clouds;
	prev_f->num_targets 		= curr_f->num_targets;
	prev_f->num_indexes 		= curr_f->num_indexes;
	memcpy(prev_f->point_clouds, 	curr_f->point_clouds, 	sizeof(float)*5*curr_f->num_point_clouds);
	memcpy(prev_f->targets, 	curr_f->targets, 	sizeof(float)*28*curr_f->num_targets);
	memcpy(prev_f->indexes, 	curr_f->indexes, 	sizeof(float)*curr_f->num_indexes);
	return res;
}


/**
 *  @brief Extract frame info
 *  @details
 *  After identify data of frame, we need to find 
 *      which data is point clouds/targets/indexes through tlv header (type, length)
 * 
 *  @param buf      		buffer contain frame
 *  @param data_queue		queue communicate with another task
 *  @param buf_len   		length of that buffer
 *  @param num_tlv	   	number of tlv has been sent in that buffer
 *  @param fn 			frame number
 *  @retval 1	success
 *  @retval 0	fail
*/
static bool extract_frame_info(uint8_t** buf,  QueueHandle_t* data_queue, int buf_len, uint16_t num_tlv, uint32_t fn)
{
	bool err = 0;
	uint8_t move = 0;
	static uint32_t tlv_type;
	static uint32_t tlv_length;
	static struct frame_struct frame;
	struct frame_struct* f_ptr = &frame;

	float point_clouds[750*5];  // Each point cloud has 5 values (range, azimuth, elevation, doppler, snr)
	float targets[28*13];
	uint8_t indexes_data[750];

	f_ptr->num_point_clouds = 0;
	f_ptr->num_targets = 0;
	f_ptr->num_indexes = 0;
	f_ptr->point_clouds = point_clouds;
	f_ptr->targets = targets;
	f_ptr->indexes = indexes_data;
	f_ptr->frame_number = fn;

	for (uint8_t i = 0; i < num_tlv; i++) {
		const uint8_t tlv_struct_length = struct_calcsize("2I");
		struct_unpack_one_value(buf, &buf_len, &tlv_type, "I");
		struct_unpack_one_value(buf, &buf_len, &tlv_length, "I");
		if ((tlv_type > 20) | (tlv_length > 10000)) {
			ESP_LOGE(TAG, "Wrong bytes data when extract frame info %u %u", tlv_type, tlv_length);
			err = 1;
			move += 8;
			return -1;
		}
		if (tlv_type == 6) 
			f_ptr->num_point_clouds = parseCapon3DPolar( *buf, 
								     tlv_length - tlv_struct_length,
								     &f_ptr->point_clouds);
		if (tlv_type == 7) 
			f_ptr->num_targets = parseDetectedTracks3D( *buf, 
								    tlv_length - tlv_struct_length, 
								    &f_ptr->targets);
		if (tlv_type == 8) 
			f_ptr->num_indexes = parseTargetAssociations( *buf, 
								      tlv_length - tlv_struct_length, 
								      &f_ptr->indexes);
		*buf += (tlv_length - tlv_struct_length);
		move += tlv_length;
	}
	struct fall_features* feat = feature_processing(&frame);
	if (feat != NULL)
		xQueueSend(*data_queue, &feat, ( TickType_t ) 1000 );
	*buf -= move;
	if (err == 1) return -1;
	return 0;
}


/**
 *  @brief Fetch data from radar with given size
 *  @param data  memory to allocate new data
 *  @return len of data read from uart
*/
static int read_sensor_data(uint8_t *data)
{
	size_t tmp_size;
	esp_err_t is_ok = uart_get_buffered_data_len(UART_NUM_1, &tmp_size);

	if (is_ok != ESP_OK || tmp_size < 2048) {
		return 0;
	}
	int len = uart_read_bytes(UART_NUM_1, data, tmp_size, 1000 / portTICK_PERIOD_MS);
	return len;
}

/**
 * @brief Log frame header value for debug
 * 
 * @param fh pointer to frame header
 */
static void log_frame_header(struct frame_header* fh)
{
	ESP_LOGD(TAG, "Magic %" PRIu64 "\n", fh->sync);
	ESP_LOGD(TAG, "Version %" PRIu32 "\n", fh->version);
	ESP_LOGD(TAG, "totalPacketLen %" PRIu32 "\n", fh->totalPacketLen);
	ESP_LOGD(TAG, "Platform %" PRIu32 "\n", fh->platform);
	ESP_LOGD(TAG, "Frame Number: %" PRId32"\n", fh->frameNumber);
	ESP_LOGD(TAG, "subFrameNumber: %" PRIu32 "\n", fh->subFrameNumber);
	ESP_LOGD(TAG, "chirpProcessingMargin: %" PRIu32 "\n", fh->chirpProcessingMargin);
	ESP_LOGD(TAG, "frameProcessingMargin: %" PRIu32 "\n", fh->frameProcessingMargin);
	ESP_LOGD(TAG, "uartSentTime: %" PRIu32 "\n", fh->uartSentTime);
	ESP_LOGD(TAG, "trackProcessTime: %" PRIu32 "\n", fh->trackProcessTime);
	ESP_LOGD(TAG, "numTLVs: %" PRIu16 "\n", fh->numTLVs);
	ESP_LOGD(TAG, "checksum: %" PRIu16 "\n", fh->checksum);
}

/**
 * @brief Get data with specify length from ring buffer
 * 
 * @param rb ring buffer
 * @param mutex mutex buffer
 * @param len length to get
 * @param des pointer to 
 * @param tick_wait 
 * @retval 1 sucess
 * @retval 0 fail 
 */
static bool fetch_rb_data(RingbufHandle_t* rb, SemaphoreHandle_t* mutex, uint8_t len, uint8_t* des, uint8_t tick_wait)
{
	uint8_t cp_bytes = 0;
	static size_t item_len;

	while (cp_bytes < len) { 
		if (xSemaphoreTake(*mutex, 200/portTICK_PERIOD_MS) == pdTRUE) {
			uint8_t* item = (uint8_t *)xRingbufferReceiveUpTo( *rb, 
									   &item_len, 
									   pdMS_TO_TICKS(tick_wait), 
									   len - cp_bytes);
			if (item == NULL) {
				xSemaphoreGive(*mutex);
				return false;
			}
			xSemaphoreGive(*mutex);
			memcpy(des + cp_bytes, item, item_len);
			vRingbufferReturnItem(*rb, (void *)item);
			cp_bytes += item_len;
		}
	}
	return true;
}

/**
 * @brief Using standard from ti source code checksum to verify frame header
 * 
 * @param packet frame header buffer
 * @param checksum goal to check
 * @retval 1 right checksum  
 * @retval 0 wrong
 */
static bool verify_checksum(void* packet, uint16_t checksum)
{
	uint32_t sum = 0;
	uint16_t calc_checksum = 0;
	uint16_t* headerPtr = (uint16_t*)packet;

	for (int n = 0; n < sizeof(struct frame_header) / sizeof(uint16_t); n++) {
		sum += *headerPtr++;
	}
	calc_checksum = ~((sum >> 16) + (sum & 0xFFFF));
	if (calc_checksum != checksum) {
		ESP_LOGE(TAG, "Wrong check sum! %u %u", checksum, calc_checksum);
		return false;
	}
	return true;
}


void init_uart_port(QueueHandle_t* q_uart_event)
{
	esp_log_level_set(TAG, ESP_LOG_INFO);
	/*Set up config port*/
	uart_config_t uart_config = {
		.baud_rate = UART2_BAUDRATE,
		.data_bits = UART_DATA_8_BITS,
		.parity    = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_APB,
	};
	ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, BUF_SIZE, 0, 0, NULL, 0));
	ESP_ERROR_CHECK(uart_param_config(UART_NUM_2, &uart_config));
	ESP_ERROR_CHECK(uart_set_pin(UART_NUM_2, UART2_TXD, UART2_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
	/*Set up data port*/
	uart_config_t uart_data_config = {
		.baud_rate = UART1_BAUDRATE,
		.data_bits = UART_DATA_8_BITS,
		.parity    = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
	};
	ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, 1024*32, 0, 1024, q_uart_event, 0));
	ESP_ERROR_CHECK(uart_set_mode(UART_NUM_1, UART_MODE_UART));
	ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_data_config));
	ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, UART1_TXD, UART1_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
	ESP_ERROR_CHECK(uart_set_word_length(UART_NUM_1, UART_DATA_8_BITS));
}


bool extract_radar_data(RingbufHandle_t* rb, QueueHandle_t* data_queue,  SemaphoreHandle_t* buffer_mutex, SemaphoreHandle_t* data_key)
{
	int fh_len = 48;
	int num_bytes_read = 0;
	static struct frame_header fh;
	static union magic_word_t magic_word;
	uint8_t fh_buf[48];
	uint8_t* data = fh_buf;

	memset((void*)&fh, 0, sizeof(struct frame_header));
	if (!check_rb_data_enough(rb, buffer_mutex, fh_len, false))
		return 0;

	fetch_rb_data(rb, buffer_mutex, fh_len, data, 100);
	// Find in data until indentify magicword (8 bytes)
	while (true) {
		for (uint8_t i = 0; i < 8; i++)
			magic_word.u8_arr[i] = data[i];
		if (magic_word.u64_val == (uint64_t)506660481457717506)
			break;
		if (check_rb_data_enough(rb, buffer_mutex, 1, false)) {
			memcpy(data, data + 1, fh_len - 1);
			if (!fetch_rb_data(rb, buffer_mutex, 1, data + fh_len - 1, 5)) {
				ESP_LOGE(TAG, "Cant receive buf from Ring");
				continue;
			}
		}
	}
	// Got frame extract information bellow
	fh.sync = 0x0708050603040102;
	fh.version                  =   data[8]  << 0 | data[9] << 8  | data[10] << 16 | data[11] << 24;
	fh.totalPacketLen           =   data[12] << 0 | data[13] << 8 | data[14] << 16 | data[15] << 24;
	fh.platform                 =   data[16] << 0 | data[17] << 8 | data[18] << 16 | data[19] << 24;
	fh.frameNumber              =   data[20] << 0 | data[21] << 8 | data[22] << 16 | data[23] << 24;
	fh.subFrameNumber           =   data[24] << 0 | data[25] << 8 | data[26] << 16 | data[27] << 24;
	fh.chirpProcessingMargin    =   data[28] << 0 | data[29] << 8 | data[30] << 16 | data[31] << 24;
	fh.frameProcessingMargin    =   data[32] << 0 | data[33] << 8 | data[34] << 16 | data[35] << 24;
	fh.uartSentTime             =   data[36] << 0 | data[37] << 8 | data[38] << 16 | data[39] << 24;
	fh.trackProcessTime         =   data[40] << 0 | data[41] << 8 | data[42] << 16 | data[43] << 24;
	fh.numTLVs                  =   data[44] << 0 | data[45] << 8;
	// Check the checksum to make sure right packet
	fh.checksum		    =   0;
	if (!verify_checksum((void*)&fh, data[46] << 0 | data[47] << 8))
		return 0;
	/* Check whether missing frame */
	static uint32_t lastframe = 0;
	if (fh.frameNumber - lastframe > 1) {
		ESP_LOGE(TAG, "Missing frame: %u, %u", lastframe, fh.frameNumber);
	}
	lastframe = fh.frameNumber;

	log_frame_header(&fh);

	// Frame header info
	int tlv_data_len = fh.totalPacketLen - 48;
	uint8_t frame_data[8910]; // 8*750 + 108*20 + 750
	uint8_t* p_frame_data = frame_data;
	
	num_bytes_read = 0;
	while (tlv_data_len - num_bytes_read > 0) { 
		if (!check_rb_data_enough(rb, buffer_mutex, tlv_data_len - num_bytes_read, false)) {
			// vTaskDelay(100/portTICK_PERIOD_MS);
			continue;
		}
		if (xSemaphoreTake( *buffer_mutex, 110/portTICK_PERIOD_MS ) == pdTRUE ) {
			size_t item_len;
			uint8_t* item = (uint8_t *)xRingbufferReceiveUpTo(  *rb, 
									    &item_len, 
									    pdMS_TO_TICKS(1000), 
									    tlv_data_len - num_bytes_read );
			if (item == NULL) {
				xSemaphoreGive(*buffer_mutex);
				ESP_LOGE(TAG, "(Frame data) No data in Ring buffer!");
				continue;
			}
			xSemaphoreGive(*buffer_mutex);
			vTaskDelay(1/portTICK_PERIOD_MS);
			memcpy(p_frame_data + num_bytes_read, item, item_len);
			vRingbufferReturnItem(*rb, (void *)item);
			num_bytes_read += item_len;
		} else {
			printf("Cannot get mutex!!!\n");
		}
	}
	for(;;) {
		if(xSemaphoreTake(*data_key, (TickType_t)100) == pdTRUE) {
			if (extract_frame_info(&p_frame_data, data_queue, tlv_data_len, 
						fh.numTLVs, fh.frameNumber) == 0) {
				ESP_LOGE(TAG, "Error when extract frame information");
				printf("Frame Num: %u\n", fh.frameNumber);
			}
			xSemaphoreGive(*data_key);
			break;
		} else {
			ESP_LOGE(TAG, "Busy in accessing Data Queue");
		}
	}
	// check_rb_data_enough(rb, buffer_mutex, 1, true);
	return true;
}


int read_and_send_to_ring_buffer(RingbufHandle_t* buffer, SemaphoreHandle_t* buffer_mutex)
{
	uint8_t tx_item[4096];
	
	if (xSemaphoreTake(*buffer_mutex, 10/portTICK_PERIOD_MS) != pdTRUE) {
		return 0;
	}
	int data_len = read_sensor_data(tx_item);
	if (0 == data_len) {
		xSemaphoreGive( *buffer_mutex );
		return 0;
	}
	UBaseType_t res =  xRingbufferSend(*buffer, tx_item, data_len, pdMS_TO_TICKS(1000));
	if (res != pdTRUE) {
		ESP_LOGE(TAG, "Cannot send data to ring buffer");
		data_len = 0;
	}
	xSemaphoreGive( *buffer_mutex );
	return data_len;
}


bool send_sensor_config(char*cfg[])
{
	const char* endline = "\n";
	uint8_t data[4666];
	uint8_t* p_data = data;

	for (int i = 0; i < 32; i++) {
		ESP_LOGI(TAG, "%s", cfg[i]);
		uart_write_bytes(UART_NUM_2, (const char *)cfg[i] , strlen(cfg[i]));
		uart_write_bytes(UART_NUM_2, endline , strlen(endline));
		vTaskDelay(10);
		//TODO: check return done from sensor
		int len = uart_read_bytes(UART_NUM_2, p_data, BUF_SIZE, 20 / portTICK_PERIOD_MS);
		ESP_LOGI(TAG, "%d", len);
	}
	return true;
}


void reset_radar()
{
	gpio_reset_pin(RADAR_RESET_GPIO);
	gpio_set_direction(RADAR_RESET_GPIO, GPIO_MODE_OUTPUT);
	gpio_set_level(RADAR_RESET_GPIO, 0);
	vTaskDelay(10);
	gpio_set_level(RADAR_RESET_GPIO, 1);
	vTaskDelay(100);
}


void change_flashing_mode(void)
{
	//SOP2
	gpio_reset_pin(SOP2);
	gpio_set_direction(SOP2, GPIO_MODE_OUTPUT);
	gpio_set_level(SOP2, 1);

	//UART_MUX
	gpio_reset_pin(UART_MUX_CTRL);
	gpio_set_direction(UART_MUX_CTRL, GPIO_MODE_OUTPUT);
	gpio_set_level(UART_MUX_CTRL, 0);
}


void change_radar_running_mode(void)
{
	//SOP2
	gpio_reset_pin(SOP2);
	gpio_set_direction(SOP2, GPIO_MODE_OUTPUT);
	gpio_set_level(SOP2, 0);

	//UART_MUX
	gpio_reset_pin(UART_MUX_CTRL);
	gpio_set_direction(UART_MUX_CTRL, GPIO_MODE_OUTPUT);
	gpio_set_level(UART_MUX_CTRL, 0);
}