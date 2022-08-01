#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "freertos/ringbuf.h"
#include "driver/gpio.h"
#include <string.h>
#include <inttypes.h>
#include "esp_timer.h"
#include "mqtt_client.h"
#include "nvs_flash.h"
#include "esp_sntp.h"
#include "esp_task_wdt.h"
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"

#include "svm.h"
#include "common.h"
#include "utils.h"
#include "fall_logic.h"
#include "ex_com_mqtt.h"
#include "handle_spiffs.h"
#include "sensor_command.h"
#include "radar_interface.h"
#include "network_interface.h"
#include "peripherals_interface.h"


#define DEBUG_VELOCITY 0
#define DEBUG_TIME 0
#define TEST_FALL 1
#define CLOSE 1
#define VERBOSE 0
#define MAX_BUFF_SIZE 4666

#define MAX_TARGETS 13

static const char* TAG = "main";
static const char* MQTT = "mqtt";

static QueueHandle_t q_radar2fall;
static QueueHandle_t q_fall2mqtt;
static QueueHandle_t isr_uart;
static QueueHandle_t ppr_queue;

static RingbufHandle_t rb_data_cube;

static SemaphoreHandle_t mutex_q_radar2fall;
static SemaphoreHandle_t mutex_rb_data_cube;

static enum DEVICE_STATE curr_state = IDLE;

/**
 * @brief Get current network state
 * 
 * @param json the json State from storage
 * @return enum DEVICE_STATE curr network state
 */
static enum DEVICE_STATE get_nw_state(char* json)
{
        char* check_ap;
        char* check_sta;

        check_ap = strstr(json, "AP");
        check_sta = strstr(json, "STA");
        if (check_ap != NULL) {
                return AP;
        } else if (check_sta != NULL) {
                return STA;
        } else {
                return IDLE;
        }
}

/**
 * @brief Initialize network state
 * 
 */
static void init_network(void)
{
        char* nw_state = read_nw_state();
        if (nw_state == NULL) {
                write_nw_state("AP");
                wifi_init_softap();
                curr_state = AP;
		control_nw_led(&ppr_queue, &curr_state);
                return;
        }
        int state = get_nw_state(nw_state);
        if (state == AP) {
                curr_state = AP;
		control_nw_led(&ppr_queue, &curr_state);
                wifi_init_softap();
        } else {
                curr_state = STA;
                if (wifi_init_sta() == ESP_OK) {
                        ESP_LOGI("webserver", "Init STA");
                }
        }
}

/**
 * @brief Handler for button interactions
 * 
 */
static void button_logic(void)
{
        int count = 0;
        while (1) {
                if (gpio_get_level(BTN) == 0) {
                        count += 1;
                        vTaskDelay(500 / portTICK_PERIOD_MS);
                        if (10 == count) {
                                switch (curr_state) {
                                case STA:
                                        count = 0;
                                        write_nw_state("AP");
                                        // TODO: start buzzer for 2 seconds
                                        buz_sound(LEDC_OUTPUT_IO, E, 2000);
					
                                        break;
                                case AP:
                                        count = 0;
                                        write_nw_state("STA");
                                        // TODO: start buzzer for 2 seconds
					buz_sound(LEDC_OUTPUT_IO, E, 2000);
					
                                        break;
                                default:
					ESP_LOGE("BUTTON", "Failed to specify state, setting state to AP and restart device");
					write_nw_state("AP");
					vTaskDelay(300 / portTICK_PERIOD_MS);
				}
                                esp_restart();
                        }
                } else {
                        count = 0;
                        vTaskDelay(500 / portTICK_PERIOD_MS);
                }
        }
}

static void read_data_task(){
	printf("============ Starting extract radar data ============\n");
	while (true){
		extract_radar_data(&rb_data_cube, &q_radar2fall, 
				   &mutex_rb_data_cube, &mutex_q_radar2fall);
		vTaskDelay(10/portTICK_PERIOD_MS);
	}
}


static void fall_logic_processing_task()
{
	const uint8_t num_feat 			= 10;
	const uint8_t qlen 			= 40;
	const uint8_t num_computation_frame 	= 40;   // Computation of fall logic based mostly on 10 frame
	static int fall_timer 			= 0;
	static bool send2mqtt 			= false;              // Control msg has been sent to mqtt
	static uint32_t abscent_count 		= 0;
	static uint32_t presence_count 		= 0;
	static enum DEVICE_STATE fall_state 	= IDLE;

	static bool height_condition_data[MAX_TARGETS], 
		    velocity_condition_data[MAX_TARGETS], 
		    classification_action_need_data[MAX_TARGETS];

	static float absH_data[40*MAX_TARGETS], 
		     q_velocity_data[40*MAX_TARGETS], 
		     average_height_data[40*MAX_TARGETS];

	bool* p_hcond 	= 	height_condition_data;
	bool* p_vcond 	= 	velocity_condition_data;
	bool* classify 	= 	classification_action_need_data;
	float* p_absH 	= 	absH_data;
	float* p_vel 	= 	q_velocity_data;
	float* p_avgH 	= 	average_height_data;
 
	// Init computation queue
	for (int i = 0; i < MAX_TARGETS; i++) {
		*(p_absH + i) = 0;
		*(p_vel + i) = 0;
		*(p_avgH + i) = 0;
		*(p_hcond + i) = 0;
		*(p_vcond + i) = 0;
		*(classify + i) = 0;
	}

	printf("============ Starting fall logic task ============\n");
	vTaskDelay(100/portTICK_PERIOD_MS);
	for (;;) {
		struct fall_features* feat;

		if(uxQueueMessagesWaiting(q_radar2fall) == 0 ) {
			vTaskDelay(49/portTICK_PERIOD_MS);
			continue;
		}
		if(xSemaphoreTake(mutex_q_radar2fall, (TickType_t)1000) == pdTRUE) {
			if(xQueueReceive(q_radar2fall, &feat, (TickType_t)1000) != pdTRUE) {
				ESP_LOGE(TAG, "Cannot receive features from queue");
			}
			xSemaphoreGive(mutex_q_radar2fall);
		}
		/* Presence */
		if (feat->num_targets > 0) {
			if (fall_state == IDLE) {
				abscent_count = 0;
				fall_state = OCCUPIED;
				control_fall_led(&ppr_queue, &fall_state);
				ESP_LOGI(TAG, "[TRACKING] People in room");
			}
			presence_count += 1;
			if (presence_count == 1) 
				pub_to_mqtt(&q_fall2mqtt, 0, "true");
		} else {
			if (fall_state == OCCUPIED) {
				abscent_count += 1;
				if (abscent_count == 20 * 3) {
					abscent_count = 0;
					presence_count = 0;
					fall_state = VACANT;
					control_fall_led(&ppr_queue, &fall_state);
					pub_to_mqtt(&q_fall2mqtt, 0, "false");
					fall_state = IDLE;
					// TODO: Is fill queue action with value 0 need?
					ESP_LOGI(TAG, "[TRACKING] Empty room");
				}
			}
		}
		/* Fall process */
		for (int tid = 0; tid < feat->num_targets; tid++) {
			printf("====\nFrame %u = Taget %.2f - (%f), tid: %d\n", 
				feat->frame_number, 
				feat->target[tid*num_feat], 
				feat->target[tid*num_feat + 6], 
				tid);
			uint8_t target_index = (uint8_t)feat->target[tid*num_feat];
			
			if (target_index - feat->target[tid*num_feat] !=0) {
				ESP_LOGE(TAG, "Wrong tid from features");
			}
	
			float absH = feat->abs_height[target_index];

			if (absH <= -10 || absH > 4) 
				absH = *(p_avgH + (target_index + 1)*qlen - 1);
			fill_computation_queue( &p_absH, &p_avgH, &p_vel, target_index, qlen, absH,
						feat->target[tid*num_feat + 6]);
			#if CLOSE == 0
			/* Model classification */
			if (classify[tid]) {
				if (fall_timer > 10) {
					// Get model label here
					if (predict(frame_queue, num_computation_frame) == 1) {
						ESP_LOGI(TAG, "[FALL] Fall detected after model");
					} else {
						ESP_LOGI(TAG, "[FALL] Fall exit by model");
						fall_state = FALL_EXITED;
					}
					classify[tid] = false;
				}
			}
			#endif
			// State machine for fall process
			if (fall_state != IDLE && p_hcond[target_index] && p_vcond[target_index]) {
				fall_timer += 1;
				if (fall_timer == FALL_CONFIRMED_TIME*20) {
					fall_state = FALL_CONFIRMED;
					send2mqtt = true;
					pub_to_mqtt(&q_fall2mqtt, 1, "fall confirmed");
					ESP_LOGI(TAG, "[FALL] Fall confirmed");
				}

				if (fall_timer == CALLING_TIME*20) {
					//TODO: turn on buzzer here
					fall_state = CALLING;
					pub_to_mqtt(&q_fall2mqtt, 1, "calling");
					ESP_LOGI(TAG, "[FALL] Calling");
				}

				if (fall_timer == FINISHED_TIME*20) {
					fall_state = FINISHED;
					pub_to_mqtt(&q_fall2mqtt, 1, "finished");
					ESP_LOGI(TAG, "[FALL] Finished");
					fall_state = FALL_EXITED;
				}

				if (fall_timer >= 20) {
					if ((check_fall_exit_height(p_absH + target_index*qlen, qlen) == true) && (fall_state!=FALL_EXITED)) {
						fall_state = FALL_EXITED;
						ESP_LOGI(TAG, "[FALL] [Target %u] Target stand up", target_index);
					}
					
					if (fall_state == FALL_EXITED) {
						if (send2mqtt) 
							pub_to_mqtt(&q_fall2mqtt, 1, "fall exited");
						fall_state = IDLE;
						fall_timer = 0;
						send2mqtt = false;
						p_hcond[target_index] = false;
						p_vcond[target_index] = false;
						control_fall_led(&ppr_queue, &fall_state);
						ESP_LOGI(TAG, "[FALL] [Target %u] Fall Exited", target_index);
						//TODO: turn off buzzer here
					}
				}
			}
			/* Prescreening check */
			if (fall_state == OCCUPIED && (presence_count > 40) && 
			   (!p_hcond[target_index] || !p_vcond[target_index])) {
				if (presence_count > 40 && p_hcond[target_index]==false) 
					p_hcond[target_index] = check_prescreening(p_avgH, target_index, qlen);
				if (p_hcond[target_index] && !p_vcond[target_index]) {
					fall_timer += 1;
					p_vcond[target_index] = check_velocity_condition(p_vel, target_index, qlen);
				}
				if (fall_timer > 40) {
					p_hcond[target_index] = false;
					fall_timer = 0;
				}
				if (p_hcond[target_index] == true && p_vcond[target_index] == true) {
					fall_timer = 0;
					fall_state = FALL_DETECTED;
					classify[target_index] = true;
					pub_to_mqtt(&q_fall2mqtt, 1, "fall detected");
					control_fall_led(&ppr_queue, &fall_state);
					ESP_LOGI(TAG, "[FALL] [Target %u] Fall detected", target_index);
				}
			}
		}			
		free(feat);
		feat = NULL;
		// printf("%d\n", uxTaskGetStackHighWaterMark(NULL));

	}
}


void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
        ESP_LOGD(MQTT, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
	static enum DEVICE_STATE nw_state;
        // printf("%s\n", TOPIC_UPSTREAM_EVENTS("device123"));
        esp_mqtt_event_handle_t event = event_data;
        esp_mqtt_client_handle_t client = event->client;
        switch ((esp_mqtt_event_id_t)event_id)
        {
        case MQTT_EVENT_CONNECTED:
		nw_state = MQTT_CONNECTED;
		control_nw_led(&ppr_queue, &nw_state);
                ESP_LOGI(MQTT, "MQTT_EVENT_CONNECTED");
                char* dscfg = s_get_mqtt_topic(TOPIC_DOWNSTREAM_CONFIG);
                char* dscmd = s_get_mqtt_topic(TOPIC_DOWNSTREAM_COMMANDS);
                ESP_LOGI(MQTT, "Subscribing to %s", dscfg);
                esp_mqtt_client_subscribe(client, dscfg, 1);
                ESP_LOGI(MQTT, "Subscribing to %s", dscmd);
                esp_mqtt_client_subscribe(client, dscmd, 1);
                free(dscfg);
                free(dscmd);
                break;
        case MQTT_EVENT_DISCONNECTED:
                ESP_LOGI(MQTT,"Disconnected from MQTT broker");
		nw_state = MQTT_DISCONNECTED;
		control_nw_led(&ppr_queue, &nw_state);
                break;
        case MQTT_EVENT_SUBSCRIBED:
                ESP_LOGI(MQTT, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
                break;
        case MQTT_EVENT_PUBLISHED:
                ESP_LOGI(MQTT, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
                break;
        case MQTT_EVENT_DATA:
                s_handle_mqtt_topic(event);
                break;
        case MQTT_EVENT_ERROR:
                ESP_LOGI(MQTT, "MQTT_EVENT_ERROR");
                if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
                {
                        log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
                        log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
                        log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
                        ESP_LOGI(MQTT, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
			vTaskDelay(2000/portTICK_PERIOD_MS);
                }
                break;
        default:
                ESP_LOGI(MQTT, "Other event id:%d", event->event_id);
                break;
        }
}

static void mqtt_station_task(){
	
	static enum DEVICE_STATE nw_state;
	// sntp_setoperatingmode(SNTP_OPMODE_POLL);
	// sntp_setservername(0, "pool.ntp.org");
	// sntp_init();


	esp_mqtt_client_handle_t mqtt_client = init_mqtt_client(&mqtt_event_handler);
	if (mqtt_client == NULL) {
		nw_state = MQTT_DISCONNECTED;
		control_nw_led(&nw_state, &curr_state);		
	};
	// esp_mqtt_client_handle_t mqtt_client = NULL;

	presence_mqtt_params presence_p = {
		.client = mqtt_client,
		.is_present = false
	};
	intmax_t ts = 0, update_ts = 0, end_ts = 0;

	for(;;) {
		static struct mqtt_fall_event* fall_event;
		if( q_fall2mqtt == 0 ) 
			continue;
		if( xQueueReceive(q_fall2mqtt, &fall_event, (TickType_t) 10)){
			/*Presence event*/
			if (fall_event->event_id == 0){
				if (strcmp(fall_event->msg, "true")==0){
					presence_p.is_present = true;
				}else{
					presence_p.is_present = false;
				}
			}

			if (fall_event->event_id == 1){
				if (strcmp(fall_event->msg, "fall detected")==0){
					ts = (intmax_t)time(NULL);
					update_ts = (intmax_t)time(NULL);
					end_ts = 0;
				}else if (strcmp(fall_event->msg, "fall confirmed")==0){
					ts = (intmax_t)time(NULL);
					update_ts = (intmax_t)time(NULL);
					end_ts = 0;
				}else if (strcmp(fall_event->msg, "calling")==0){
					update_ts = (intmax_t)time(NULL);
					end_ts = 0;
				}else if (strcmp(fall_event->msg, "finished")==0){
					update_ts = (intmax_t)time(NULL);
					end_ts = (intmax_t)time(NULL);
				}else if (strcmp(fall_event->msg, "fall exited")==0){
					update_ts = (intmax_t)time(NULL);
					end_ts = (intmax_t)time(NULL);
				}
				// send_fall(mqtt_client, fall_event->msg, ts, update_ts, end_ts);
			}
		}
		// send_presence((void*)&presence_p);
		// ESP_LOGI(TAG, "Send to mqtt");
		vTaskDelay(8000/portTICK_PERIOD_MS);
	}
}

static void uart_event_task(void *pvParameters)
{
	uart_event_t event;

	printf("============ Starting get data task ============\n");
	for(;;) {
		if(xQueueReceive(isr_uart, (void * )&event, 1000)) {
			switch(event.type) {
			case UART_DATA:
				// printf("[Radar] get data\n");
				read_and_send_to_ring_buffer(&rb_data_cube, &mutex_rb_data_cube);
				break;
			case UART_FIFO_OVF:
				ESP_LOGI(TAG, "hw fifo overflow");
				uart_flush_input(UART_NUM_1);
				xQueueReset(isr_uart);
				break;
			case UART_BUFFER_FULL:
				ESP_LOGI(TAG, "ring buffer full");
				uart_flush_input(UART_NUM_1);
				xQueueReset(isr_uart);
				break;
			case UART_BREAK:
				ESP_LOGI(TAG, "uart rx break");
				break;
			case UART_PARITY_ERR:
				ESP_LOGI(TAG, "uart parity error");
				break;
			case UART_FRAME_ERR:
				ESP_LOGI(TAG, "uart frame error");
				break;
			default:
				ESP_LOGI(TAG, "uart event type: %d", event.type);
				break;
			}
		}
	}
	vTaskDelete(NULL);
}

static void peripherals_control_task()
{
	init_nw_led();
	init_fall_led();
	const esp_timer_create_args_t presence_led_timer_args = {
		.callback = &presence_led_control,
		.name = "presence_led_control"
	};
	esp_timer_handle_t presence_led_timer;
	
	ESP_ERROR_CHECK(esp_timer_create(&presence_led_timer_args, &presence_led_timer));
	// ap led timer
	const esp_timer_create_args_t ap_led_timer_args = {
		.callback = &ap_led_control,
		.name = "ap_led_control"
	};
	esp_timer_handle_t ap_led_timer;
	
	ESP_ERROR_CHECK(esp_timer_create(&ap_led_timer_args, &ap_led_timer));
	// mqtt led timer
	const esp_timer_create_args_t mqtt_led_timer_args = {
		.callback = &mqtt_led_control,
		.name = "mqtt_led_control"
	};
	esp_timer_handle_t mqtt_led_timer;
	
	ESP_ERROR_CHECK(esp_timer_create(&mqtt_led_timer_args, &mqtt_led_timer));
	
	
	esp_task_wdt_init(5000, true);
	static enum DEVICE_STATE state;
	static char msg[20];
	// Need to refactor
	int nw_tick = 0;
	int on_time = 0;
	int off_time = 0;

	int fall_tick = 0;
	int fall_on_time = 0;
	int fall_off_time = 0;

	for (;;) {
		if (nw_tick < on_time/10){
			nw_led_on();
		} else if (nw_tick < (off_time + on_time)/10){
			nw_led_off();
		}
		if ((on_time + off_time)/10<nw_tick){
			nw_tick = 0;
		}
		nw_tick++;
		
		if (fall_tick < fall_on_time/10){
			gpio_set_level(17, 1);
		} else if (fall_tick < (fall_off_time + fall_on_time)/10) {
			gpio_set_level(17, 0);
		}
		if ((fall_on_time + fall_off_time)/10 < fall_tick){
			fall_tick = 0;
		}
		fall_tick++;
		if(xQueueReceive(ppr_queue, (void *)&state, 10/portTICK_PERIOD_MS)) {
			switch (state)
			{
			case AP:
				on_time = 330;
				off_time = 2700;
				break;	
			case MQTT_DISCONNECTED:
				on_time = 330;
				off_time = 330;
				break;			
			case MQTT_CONNECTED:
				on_time = 100;
				off_time = 0;
				break;
			case OCCUPIED:
				fall_on_time = 330;
				fall_off_time = 330;
				memcpy(msg, "OCCUPIED\0", 10);
				break;
			case VACANT:
				fall_on_time = 0;
				fall_off_time = 100;
				memcpy(msg, "VACANT\0", 8);
				break;
			case FALL_DETECTED:
				// Turn led to solid
				fall_on_time = 100;
				fall_off_time = 0;
				memcpy(msg, "FALL DETECTED\0", 15);
				break;
			case FALL_CONFIRMED:
				// ignore
				memcpy(msg, "FALL CONFIRMED\0", 16);
				break;
			case CALLING:
				memcpy(msg, "CALLING\0", 9);
				// Turn on buzzer
				break;
			case FALL_EXITED:
				memcpy(msg, "FALL EXITED\0", 13);		
				// Turn of everything
				fall_on_time = 0;
				fall_off_time = 100;
				break;
			case FINISHED:
				memcpy(msg, "FINISHED\0", 10);
				// Ignore
				break;
			case IDLE:
				memcpy(msg, "IDLE\0", 6);
				// Ignore
				break;
			default:
				memcpy(msg, "Another\0", 9);
				break;
			}
			ESP_LOGI(TAG, "Receive %s", msg);
		}
		esp_task_wdt_reset();
	}
}


void app_main()
{   
	init_spiffs();
	char* uuid = "aura_GHAJSDGSA27625";
	esp_log_level_set("mqtt", ESP_LOG_VERBOSE);
	esp_log_level_set("main", ESP_LOG_DEBUG);
	// /* Wifi */
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
	  ESP_ERROR_CHECK(nvs_flash_erase());
	  ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
	
		/* Init setting */
	mutex_rb_data_cube = xSemaphoreCreateMutex();
	if( mutex_rb_data_cube == NULL ) {
		ESP_LOGE(TAG, "Failed to create mutex ring buffer\n");
	}
	mutex_q_radar2fall = xSemaphoreCreateMutex();
	if( mutex_q_radar2fall == NULL ) {
		ESP_LOGE(TAG, "Failed to create mutex features queue\n");
	}
	rb_data_cube  = xRingbufferCreate(1024*10, RINGBUF_TYPE_BYTEBUF);
	if (rb_data_cube == NULL) {
		ESP_LOGE(TAG, "Failed to create ring buffer\n");
	}
	/* Create a pipe for another tasks control peripherals */
	ppr_queue = xQueueCreate(40, sizeof(enum DEVICE_STATE*));
	if (ppr_queue == NULL) {
		ESP_LOGE(TAG, "Cannot create peripherals queue");
		//TODO: need to reset
	}
	q_radar2fall = xQueueCreate( 40*2, sizeof(struct fall_features *));
	if( q_radar2fall == 0 ){
		ESP_LOGE(TAG, "Cannot create features queue");
		// TODO: need to reset
	}
	q_fall2mqtt = xQueueCreate( 10*2, sizeof( struct mqtt_fall_event * ) );
	if( q_fall2mqtt == 0 ){
		ESP_LOGE(TAG, "Cannot create mqtt event queue");
		// TODO: Some thing wrong need to reset?
	}
	init_uart_port(&isr_uart); 
	vTaskDelay(10/portTICK_PERIOD_MS);
	/* Assign task */
	xTaskCreatePinnedToCore(fall_logic_processing_task, "fall_logic_processing_task", 
				1024*5, NULL, 10, NULL, 1);
	vTaskDelay(10/portTICK_PERIOD_MS);
	xTaskCreatePinnedToCore(read_data_task, "radar_interface", 
				1024*30, NULL, 10, NULL, 1);
	vTaskDelay(10/portTICK_PERIOD_MS);
	xTaskCreatePinnedToCore(peripherals_control_task, "peripherals_control_task", 
				1024*2, NULL, 10, NULL, 1);
	// vTaskDelay(10/portTICK_PERIOD_MS);
	/* Start radar */
	change_radar_running_mode();
	reset_radar();
	vTaskDelay(10/portTICK_PERIOD_MS);
	send_sensor_config(cfg);

	write_uuid(uuid);

        init_btn();
        nw_led_off();
        init_network();
        vTaskDelay(500 / portTICK_PERIOD_MS);
        xTaskCreatePinnedToCore(button_logic, "button_logic", 1024 * 10, NULL, 10, NULL, 0);
	if (STA == curr_state ) {
		vTaskDelay(500 / portTICK_PERIOD_MS);
		xTaskCreatePinnedToCore(mqtt_station_task, "mqtt_station_task", 1024*2, NULL, 10, NULL, 0);
	}
}
