
/**
 * @brief Set up communication between sensor and mcu
 * @param q_uart_event queue to cacth event from uart isr
*/
void init_uart_port(QueueHandle_t* q_uart_event);


/**
 * @brief   Read data from sensor through uart 1
 * @param   cfg lines of config
*/   
bool send_sensor_config(char* cfg[]);


/**
 * @brief Press reset button of radar through NSRESET of radar
*/
void reset_radar();


/**
 *  @brief Read data from sensor then forward it to ring buffer
 *  @param buffer Ring buffer to send
 *  @param buffer_mutex mutex to avoid access to buffer
 *  @return length of buffer had been send to ring buffer
*/
int read_and_send_to_ring_buffer(RingbufHandle_t* buffer, SemaphoreHandle_t* buffer_mutex);


/**
 * @brief Extract radar data
 * @details
 *  Find magicword in binary array then start to extract frame header (52 bytes)
 *  with struct pattern "Q10I2H".
 * 
 *   * magic
 *   * version
 *   * packetLength
 *   * platform
 *   * frameNum 
 *   * subFrameNum
 *   * chirpMargin 
 *   * frameMargin 
 *   * uartSentTime 
 *   * trackProcessTime
 *   * numTLVs
 *   * checksum
 * 
 *  @param data buffer array from sensor
 *  @retval 1 sucess
 *  @retval 0 fail
*/
bool extract_radar_data(RingbufHandle_t* buffer, QueueHandle_t* data_queue, SemaphoreHandle_t* buffer_sph, SemaphoreHandle_t* data_key);

/**
 * @brief Set radar mode to running mode
 * 
 */
void change_radar_running_mode(void);

/**
 * @brief Set radar mode to flash mode
 * 
 */
void change_flashing_mode(void);