#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include <stdio.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

/**
 * @brief GPIO number for the network LED
 * 
 */
#define NW_LED GPIO_NUM_13

/**
 * @brief GPIO number for button
 * 
 */
#define BTN GPIO_NUM_34

/**
 * @brief Timer number use for buzzer
 * 
 */
#define LEDC_TIMER LEDC_TIMER_0

/**
 * @brief Ledc speed mode use for buzzer
 * 
 */
#define LEDC_MODE LEDC_LOW_SPEED_MODE

/**
 * @brief GPIO number for buzzer
 * 
 */
#define LEDC_OUTPUT_IO (19) // Define the output GPIO

/**
 * @brief The desired channel use for buzzer
 * 
 */
#define LEDC_CHANNEL LEDC_CHANNEL_0

/**
 * @brief Duty resolution use for buzzer
 * 
 */
#define LEDC_DUTY_RES LEDC_TIMER_13_BIT 

/**
 * @brief Buzzer duty
 * 
 */
#define LEDC_DUTY (819)

/**
 * @brief Buzzer frequency in Hertz
 * 
 */
#define LEDC_FREQUENCY (294)           

/**
 * @brief E note frequency, defaulting at 2637
 * 
 */
#define E 2637 // 329


/**
 * @brief Initialize the network LED
 * 
 */
void init_nw_led(void);

/**
 * @brief Turn on the network LED
 * 
 */
void nw_led_on(void);

/**
 * @brief Turn off the network LED
 * 
 */
void nw_led_off(void);

/**
 * @brief Initialize the button
 * 
 */
void init_btn(void);

/**
 * @brief Sound the buzzer at a freq for a duration
 * 
 * @param gpio_num The GPIO number of the buzzer
 * @param freq Frequency in KHz
 * @param duration Duration in ms
 */
void buz_sound(int gpio_num, uint32_t freq, uint32_t duration);

/**
 * @brief Turn on the alarm
 * 
 */
void alarm_on(void);

/**
 * @brief Turn off the alarm
 * 
 */
void alarm_off(void);

/**
 * @brief Blink network LED in AP mode 
 * 
 */
void ap_led_control(void);

/**
 * @brief Blink network LED when MQTT is not connected
 * 
 */
void mqtt_led_control(void);

/**    
 * @brief  Controlling network LED based on device state
 * 
 * @param q The queue
 * @param state The state of the nw led
 */
void control_nw_led(QueueHandle_t* q, enum DEVICE_STATE* state);