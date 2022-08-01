#include <esp_http_server.h>
#include "lwip/err.h"
#include <stdbool.h>
#include "esp_event.h"

/**
 * @brief Initialize device to start AP mode
 * 
 */
void wifi_init_softap(void);

/**
 * @brief Initialize device to start STA mode
 * 
 * @return esp_err_t the error code, if it's ESP_OK then all is good
 */
esp_err_t wifi_init_sta(void);
