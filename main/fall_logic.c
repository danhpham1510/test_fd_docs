#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>
#include <stdbool.h>
#include "esp_log.h"
#include "utils.h"

#include "driver/gpio.h"

#include "common.h"
#include "fall_logic.h"

// Fall condition setting
#define DELTA_HEIGHT_CONSTRAINT -0.08                   /**< Delta height conditon*/	
#define VELOCITY_CONSTRAINT -0.05                       /**< Velocity condition*/	

const char * TAG = "FALL_LOGIC";


float calc_absolute_height(float* pcs, uint8_t* indexes, int num_pcs, int num_index, uint8_t tid)
{
	float res = - 10;

	for (int i = 0; i < num_pcs; i++) {
		if (*(indexes + i) == tid && res < *(pcs + i*5 + 2)) 
			res = *(pcs + i*5 + 2);
	}
	return res;
}


float calc_avg_height(float abs_height, float last_avg_height){
	if (abs_height == -10) 
		return last_avg_height;
	if (last_avg_height == 0)
		last_avg_height = abs_height;
	return (abs_height/20 + last_avg_height*19/20);
}


void fill_computation_queue( float** q_absH, float** q_avgH, float** q_velocity, 
			     uint8_t idx, uint8_t max_len, float absH, float vel) 
{
	*q_absH += idx*max_len;
	push_to_queue(q_absH, max_len, absH);
	*q_absH -= idx*max_len;

	*q_avgH += idx*max_len;
	push_to_queue(q_avgH, max_len, calc_avg_height(absH, *(*q_avgH + max_len - 1)));
	*q_avgH -= idx*max_len;

	*q_velocity += idx*max_len;
	push_to_queue(q_velocity, max_len, vel);
	*q_velocity -= idx*max_len;
}


bool check_fall_exit_height(float* abs_height_queue, int len)
{
	double mean_height = 0;
	const int num_frames_to_check = 20;

	for (int i = len; i > len - num_frames_to_check; i--)
		mean_height += *(abs_height_queue + i -1);
	mean_height = mean_height / num_frames_to_check;
	if (mean_height > 1.3)
		return true;
	return false;
}


bool check_prescreening(float* q_height, uint8_t id, uint8_t len)
{
	q_height += id*len;
	float deltaH = *(q_height + len - 1) - *(q_height + len - 10);
	q_height -= id*len;
	if ((deltaH > -0.45) && (deltaH < DELTA_HEIGHT_CONSTRAINT)) {
		ESP_LOGE(TAG, "Pass height");
		return true;
	}
	return false;
}


bool check_velocity_condition(float* q_velo, uint8_t id, uint8_t len)
{
	float mean_vz = 0;

	q_velo += id*len;
	for (int i = len - 30; i < len - 1; i++) {
		mean_vz += *(q_velo + i)/30;
	}
	q_velo -= id*len;
	if (mean_vz <= VELOCITY_CONSTRAINT) {
		ESP_LOGE(TAG, "Velo pass");
		return true;
	}
	return false;
}


void init_fall_led()
{
	gpio_reset_pin(FALL_LED_GPIO);
	gpio_set_direction(FALL_LED_GPIO, GPIO_MODE_OUTPUT);
}


void turn_fall_led(bool status)
{
	gpio_set_level(FALL_LED_GPIO, status);
}


void presence_led_control()
{
	turn_fall_led(1);
	vTaskDelay(330/portTICK_PERIOD_MS);
	turn_fall_led(0);
	vTaskDelay(330/portTICK_PERIOD_MS);
}


void pub_to_mqtt(QueueHandle_t* q_mqtt, uint8_t id, char* msg)
{
	struct mqtt_fall_event event;
	struct mqtt_fall_event* p_event = &event;
	p_event->event_id = 0;
	p_event->msg = msg;
	xQueueSend(*q_mqtt, &p_event, (TickType_t)1);
}


void control_fall_led(QueueHandle_t* q, enum DEVICE_STATE* state)
{
	xQueueSend(*q, state, (TickType_t)1);
}