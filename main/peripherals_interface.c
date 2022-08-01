#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include <stdio.h>
#include "common.h"
#include "peripherals_interface.h"

static const char* TAG = "peripherals";
void init_nw_led(void)
{
        gpio_reset_pin(NW_LED);
        gpio_set_direction(NW_LED, GPIO_MODE_OUTPUT);
}

void init_btn(void)
{
        gpio_set_direction(BTN, GPIO_MODE_INPUT);
        gpio_set_pull_mode(BTN, GPIO_PULLUP_ONLY);
}

void nw_led_on(void)
{
        // ESP_LOGI(TAG, "NW NW_LED Turned ON");
        gpio_set_level(NW_LED, 1);
}

void nw_led_off(void)
{
        // ESP_LOGI(TAG, "NW NW_LED Turned OFF");
        gpio_set_level(NW_LED, 0);
}

void buz_sound(int gpio_num, uint32_t freq, uint32_t duration)
{
        ledc_timer_config_t ledc_timer = {
            .speed_mode = LEDC_MODE,
            .timer_num = LEDC_TIMER,
            .duty_resolution = LEDC_DUTY_RES,
            .freq_hz = freq, 
            .clk_cfg = LEDC_AUTO_CLK};
        ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

        // Prepare and then apply the LEDC PWM channel configuration
        ledc_channel_config_t ledc_channel = {
            .speed_mode = LEDC_MODE,
            .channel = LEDC_CHANNEL,
            .timer_sel = LEDC_TIMER,
            .intr_type = LEDC_INTR_DISABLE,
            .gpio_num = LEDC_OUTPUT_IO,
            .duty = 0, // Set duty to 0%
            .hpoint = 0};
        ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

        // start
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY); // 12% duty - play here for your speaker or buzzer
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
        vTaskDelay(duration / portTICK_PERIOD_MS);
        alarm_off();
}

void alarm_on(void)
{
        while (1) {
                buz_sound(LEDC_OUTPUT_IO, E, 2000);
                vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
}

void alarm_off(void)
{
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

void ap_led_control(void)
{
        nw_led_on();
        vTaskDelay(300 / portTICK_PERIOD_MS);
        nw_led_off();
        vTaskDelay(2700 / portTICK_PERIOD_MS);
}

void mqtt_led_control(void)
{
        nw_led_on();
        vTaskDelay(300 / portTICK_PERIOD_MS);
        nw_led_off();
        vTaskDelay(300 / portTICK_PERIOD_MS);
}

void control_nw_led(QueueHandle_t* q, enum DEVICE_STATE* state)
{
	xQueueSend(*q, state, (TickType_t)1);
}