#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_spiffs.h"
#include "cJSON.h"
#include <sys/unistd.h>
#include <sys/stat.h>
#include "handle_spiffs.h"
#include "cJSON.h"
#define portTICK_PERIOD_MS ((TickType_t)(1000 / configTICK_RATE_HZ))

static char* TAG = "spiffs storage";
static esp_vfs_spiffs_conf_t conf = {
    .base_path = "/spiffs",
    .partition_label = NULL,
    .max_files = 5,
    .format_if_mount_failed = true};

// Initialize spiffs storage
void init_spiffs(void)
{
        ESP_LOGI(TAG, "Initializing SPIFFS");
        // Use settings defined above to initialize and mount SPIFFS filesystem.
        // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
        esp_err_t ret = esp_vfs_spiffs_register(&conf);

        if (ret != ESP_OK) {
                switch (ret){
                        case ESP_FAIL:
                                ESP_LOGE(TAG, "Failed to mount or format filesystem");
                                break;
                        case ESP_ERR_NOT_FOUND :
                                ESP_LOGE(TAG, "Failed to find SPIFFS partition");
                                break;
                        default:
                                ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
                }
                return;
        }
        // Get Partition info
        size_t total = 0, used = 0;
        ret = esp_spiffs_info(conf.partition_label, &total, &used);
        if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
        } else {
                ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
        }
}


void write_uuid(char* uuid)
{
        
        ESP_LOGI(TAG, "Opening device info");
        FILE *f = fopen("/spiffs/device.json", "w");
        if (f == NULL) {
                ESP_LOGE(TAG, "Failed to open file for writing");
        } else {
                cJSON *root;
                root = cJSON_CreateObject();
                cJSON_AddStringToObject(root, "uuid", uuid);
                char* mqtt = cJSON_Print(root);
                fprintf(f, mqtt);
                fclose(f);
                ESP_LOGI(TAG, "File written");
                cJSON_Delete(root);
                cJSON_free(mqtt);
        }
}


char* read_uuid(void)
{
        ESP_LOGI(TAG, "Reading device info");
        FILE *fp = fopen("/spiffs/device.json", "r");
        if (fp == NULL) {
                ESP_LOGE(TAG, "Failed to open file for reading");
                ESP_LOGE(TAG, "Initializing uuid as \"Not Init\"");
                write_uuid("NOT_INIT");
                vTaskDelay(300 / portTICK_PERIOD_MS);
                return read_uuid();
        }
        char* config_file = (char* )malloc(sizeof(char) * 1000);
        if (config_file == NULL) {
                ESP_LOGE(TAG,"ACCESSING NULL POINTER");
                return NULL;
        } else {
                char curr_line[100];
                assert(fp != NULL);
                int next_pos = 0;
                while (fgets(curr_line, sizeof(curr_line), fp) != NULL) {
                        memcpy(config_file + next_pos, curr_line, strlen(curr_line));
                        next_pos += strlen(curr_line);
                }
        *(config_file + next_pos) = '\0';
        }

        fclose(fp);
        return config_file;
}

void write_mqtt_cfg(char* hostname, char* port, char* username, char* password)
{
        ESP_LOGI(TAG, "Opening mqtt config");
        FILE *f = fopen("/spiffs/mqtt.json", "w");
        if (f == NULL) {
                ESP_LOGE(TAG, "Failed to open file for writing");
        } else {
                cJSON *root;
                root = cJSON_CreateObject();
                cJSON_AddStringToObject(root, "mqtt_hostname", hostname);
                cJSON_AddStringToObject(root, "mqtt_port", port);
                cJSON_AddStringToObject(root, "mqtt_username", username);
                cJSON_AddStringToObject(root, "mqtt_password", password);
                char* mqtt = cJSON_Print(root);
                fprintf(f, mqtt);
                fclose(f);
                ESP_LOGI(TAG, "File written");
                cJSON_Delete(root);
                cJSON_free(mqtt);
        }
}


char* read_mqtt_cfg(void)
{
        ESP_LOGI(TAG, "Reading mqtt config file");
        FILE *fp = fopen("/spiffs/mqtt.json", "r");
        if (fp == NULL) {
                ESP_LOGE(TAG, "Failed to open file for reading");
                ESP_LOGE(TAG, "Initializing MQTT creds as \"Not Init\"");
                write_mqtt_cfg("NOT_INIT", "NOT_INIT", "NOT_INIT", "NOT_INIT");
                vTaskDelay(500 / portTICK_PERIOD_MS);
                return read_mqtt_cfg();
        }
        char* config_file = (char* )malloc(sizeof(char) * 1000);
        char curr_line[100];
        assert(fp != NULL);
        int next_pos = 0;
        while (fgets(curr_line, sizeof(curr_line), fp) != NULL) {
                memcpy(config_file + next_pos, curr_line, strlen(curr_line));
                next_pos += strlen(curr_line);
        }
        *(config_file + next_pos) = '\0';
        fclose(fp);
        return config_file;
}


void write_wifi_cfg(char* ssid, char* password)
{
        ESP_LOGI(TAG, "Opening wifi config");
        FILE *f = fopen("/spiffs/wifi.json", "w");
        if (f == NULL) {
                ESP_LOGE(TAG, "Failed to open file for writing");
        } else {
                cJSON *root;
                root = cJSON_CreateObject();
                cJSON_AddStringToObject(root, "ssid", ssid);
                cJSON_AddStringToObject(root, "password", password);
                char* wifi = cJSON_Print(root);
                fprintf(f, wifi);
                fclose(f);
                ESP_LOGI(TAG, "File written");
                cJSON_Delete(root);
                cJSON_free(wifi);
        }
}

char* read_wifi_cfg(void)
{
        ESP_LOGI(TAG, "Reading wifi file");
        FILE *fp = fopen("/spiffs/wifi.json", "r");
        if (fp == NULL) {
                ESP_LOGE(TAG, "Failed to open file for reading, file not found");
                ESP_LOGE(TAG, "Initializing WiFi creds as \"Not Init\"");
                write_wifi_cfg("NOT_INIT", "NOT_INIT");
                vTaskDelay(300 / portTICK_PERIOD_MS);
                return read_wifi_cfg();
        }
        char* config_file = (char* )malloc(sizeof(char) * 1000);
        if (config_file == NULL) {
                ESP_LOGE(TAG,"ACCESSING NULL POINTER");
                return NULL;
        } else {
                char curr_line[100];
                assert(fp != NULL);
                int next_pos = 0;
                while (fgets(curr_line, sizeof(curr_line), fp) != NULL) {
                        memcpy(config_file + next_pos, curr_line, strlen(curr_line));
                        next_pos += strlen(curr_line);
                }
        *(config_file + next_pos) = '\0';
        }
        fclose(fp);
        return config_file;
}

char* read_nw_state(void)
{
        ESP_LOGI(TAG, "Reading network status");
        FILE *fp = fopen("/spiffs/nw.json", "r");
        if (fp == NULL) {
                ESP_LOGE(TAG, "Failed to open file for reading");
                char* file = NULL;
                return file;
        }
        char* file = (char* )malloc(sizeof(char) * 1000);
        char curr_line[100];
        assert(fp != NULL);
        int next_pos = 0;
        while (fgets(curr_line, sizeof(curr_line), fp) != NULL) {
                memcpy(file + next_pos, curr_line, strlen(curr_line));
                next_pos += strlen(curr_line);
        }
        *(file + next_pos) = '\0';
        fclose(fp);
        return file;
}

void write_nw_state(char* state)
{
        ESP_LOGI(TAG, "Opening network status");
        FILE *f = fopen("/spiffs/nw.json", "w");
        if (f == NULL) {
                ESP_LOGE(TAG, "Failed to open file for writing");
        } else {
                cJSON *root;
                root = cJSON_CreateObject();
                cJSON_AddStringToObject(root, "nw_state", state);
                char* nw_state = cJSON_Print(root);
                fprintf(f, nw_state);
                fclose(f);
                ESP_LOGI(TAG, "File written");
                cJSON_Delete(root);
                cJSON_free(nw_state);
        }
}

// Write radar cfg to storage
void write_radar_cfg(char* json_buffer)
{
        ESP_LOGI(TAG, "Opening radar config");
        FILE *f = fopen("/spiffs/radar.json", "w");
        if (f == NULL) {
                ESP_LOGE(TAG, "Failed to open file for writing");
        } else {
                fprintf(f, json_buffer);
                fclose(f);
                ESP_LOGI(TAG, "File written");
        }
}

// Read radar config from storage
char* read_radar_cfg(void)
{
        ESP_LOGI(TAG, "Reading radar config file");
        FILE *fp = fopen("/spiffs/radar.json", "r");
        if (fp == NULL) {
                char* default_cfg = "{\"staticBoundaryBox\": \"-2.0 2.0 0 4.0 -0.5 2.9\",\"boundaryBox\": \"-2.5 2.5 0 4.5 -0.5 2.9\",\"sensorPosition\": \"2.2 0 25\",\"gatingParam\": \"3 2 2 3 4\",\"stateParam\": \"3 3 8 500 5 6000\",\"allocationParam\": \"100 150 0.05 20 0.5 20\",\"maxAcceleration\": \"0.3 0.3 0.5\",\"trackingCfg\": \"1 2 700 1 46 96 55\",\"presenceBoundaryBox\": \"-2.5 2.5 0 5 -0.5 2.9\"}";
                ESP_LOGE(TAG, "Failed to open file for reading");
                ESP_LOGE(TAG, "Initializing radar cfg as default");
                write_radar_cfg(default_cfg);
                vTaskDelay(300 / portTICK_PERIOD_MS);
                return read_radar_cfg();
        }
        char* config_file = (char* )malloc(sizeof(char) * 1000);
        if (config_file == NULL) {
                ESP_LOGE(TAG,"ACCESSING NULL POINTER");
                return NULL;
        } else {
                char curr_line[100];
                assert(fp != NULL);
                int next_pos = 0;
                while (fgets(curr_line, sizeof(curr_line), fp) != NULL) {
                        memcpy(config_file + next_pos, curr_line, strlen(curr_line));
                        next_pos += strlen(curr_line);
                }
        *(config_file + next_pos) = '\0';
        }
        fclose(fp);
        return config_file;
}

void unregister_spiffs(void)
{
        esp_vfs_spiffs_unregister(conf.partition_label);
        ESP_LOGI(TAG, "SPIFFS unmounted");
}

char* parse_static_boundary_box(char* content)
{
        cJSON *recv_json = cJSON_Parse(content);
        cJSON *cj_sbb = cJSON_GetObjectItemCaseSensitive(recv_json, "staticBoundaryBox");
        return cj_sbb->valuestring;
}

char* parse_boundary_box(char* content)
{
        cJSON *recv_json = cJSON_Parse(content);
        cJSON *cj_bb = cJSON_GetObjectItemCaseSensitive(recv_json, "boundaryBox");
        return cj_bb->valuestring;
}

char* parse_sensor_position(char* content)
{
        cJSON *recv_json = cJSON_Parse(content);
        cJSON *cj_sp = cJSON_GetObjectItemCaseSensitive(recv_json, "sensorPosition");
        return cj_sp->valuestring;
}

char* parse_gating_param(char* content)
{
        cJSON *recv_json = cJSON_Parse(content);
        cJSON *cj_gp = cJSON_GetObjectItemCaseSensitive(recv_json, "gatingParam");
        return cj_gp->valuestring;
}

char* parse_state_param(char* content)
{
        cJSON *recv_json = cJSON_Parse(content);
        cJSON *cj_sp = cJSON_GetObjectItemCaseSensitive(recv_json, "stateParam");
        return cj_sp->valuestring;
}

char* parse_allocation_param(char* content)
{
        cJSON *recv_json = cJSON_Parse(content);
        cJSON *cj_ap = cJSON_GetObjectItemCaseSensitive(recv_json, "allocationParam");
        return cj_ap->valuestring;
}

char* parse_max_acceleration(char* content)
{
        cJSON *recv_json = cJSON_Parse(content);
        cJSON *cj_ma = cJSON_GetObjectItemCaseSensitive(recv_json, "maxAcceleration");
        return cj_ma->valuestring;
}

char* parse_tracking_cfg(char* content)
{
        cJSON *recv_json = cJSON_Parse(content);
        cJSON *cj_tc = cJSON_GetObjectItemCaseSensitive(recv_json, "trackingCfg");
        return cj_tc->valuestring;
}

char* parse_presence_boundary_box(char* content)
{
        cJSON *recv_json = cJSON_Parse(content);
        cJSON *cj_pb = cJSON_GetObjectItemCaseSensitive(recv_json, "presenceBoundaryBox");
        return cj_pb->valuestring;
}



char* parse_wifi_ssid(char* content)
{
        cJSON *recv_json = cJSON_Parse(content);
        cJSON *cj_ssid = cJSON_GetObjectItemCaseSensitive(recv_json, "ssid");
        return cj_ssid->valuestring;
}

char* parse_wifi_pwd(char* content)
{
        cJSON *recv_json = cJSON_Parse(content);
        cJSON *cj_pwd = cJSON_GetObjectItemCaseSensitive(recv_json, "password");
        return cj_pwd->valuestring;
}

char* parse_mqtt_hostname(char* content)
{
        cJSON *recv_json = cJSON_Parse(content);
        cJSON *cj_hostname = cJSON_GetObjectItemCaseSensitive(recv_json, "mqtt_hostname");
        return cj_hostname->valuestring;
}

char* parse_mqtt_port(char* content)
{
        cJSON *recv_json = cJSON_Parse(content);
        cJSON *cj_port = cJSON_GetObjectItemCaseSensitive(recv_json, "mqtt_port");
        return cj_port->valuestring;
}

char* parse_mqtt_username(char* content)
{
        cJSON *recv_json = cJSON_Parse(content);
        cJSON *cj_username = cJSON_GetObjectItemCaseSensitive(recv_json, "mqtt_username");
        return cj_username->valuestring;
}

char* parse_mqtt_password(char* content)
{
        cJSON *recv_json = cJSON_Parse(content);
        cJSON *cj_password = cJSON_GetObjectItemCaseSensitive(recv_json, "mqtt_password");
        return cj_password->valuestring;
}