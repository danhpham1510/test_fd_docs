#include "mqtt_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include "esp_heap_caps.h"
#include "time.h"
#include "ex_com_mqtt.h"
#include "utils.h"
#include "handle_spiffs.h"

const char* MQTT = "mqtt";


#define FALL_EVENT 1
#define PRESENCE_EVENT 0

void log_error_if_nonzero(const char* message, int error_code)
{
        if (error_code != 0) {
                ESP_LOGE(MQTT, "Last error %s: 0x%x", message, error_code);
        }
}

/**
 * @brief Create presence buffer to send to MQTT
 * 
 * @param is_present True or False
 * @param ts Timestamp
 * @return char* The buffer
 */
static char* s_contruct_presence_data_json(bool is_present, uint32_t ts)
{
        // cJson Components
        char* string = NULL;
        cJSON *type = NULL;
        cJSON *payload = NULL;
        cJSON *is_present_json = NULL;
        cJSON *ts_json = NULL;
        cJSON *presence_data = cJSON_CreateObject();
        if (presence_data == NULL) {
                goto end;
        }
        /*  Type */
        type = cJSON_CreateNumber(PRESENCE_EVENT);
        if (type == NULL) {
                goto end;
        }
        cJSON_AddItemToObject(presence_data, "type", type);
        /*  Payload */
        payload = cJSON_CreateObject();
        if (payload == NULL) {
                goto end;
        }
        is_present_json = cJSON_CreateBool(is_present);
        if (is_present_json == NULL) {
                goto end;
        }
        cJSON_AddItemToObject(payload, "presenceDetected", is_present_json);
        ts_json = cJSON_CreateNumber(ts);
        if (ts_json == NULL) {
                goto end;
        }
        cJSON_AddItemToObject(payload, "timestamp", ts_json);
        cJSON_AddItemToObject(presence_data, "payload", payload);
        // Result
        string = cJSON_Print(presence_data);
        if (string == NULL) {
                printf("Failed to print fall_data.\n");
        }
end:
        cJSON_Delete(presence_data);
        return string;
}

/**
 * @brief Create fall buffer to send to MQTT
 * 
 * @param status The fall status
 * @param ts The timestamp      
 * @param update_ts The timestamp at update moment
 * @param end_ts The timestamp where event ends
 * @return char* The buffer
 */
static char* s_contruct_fall_data_json(const char* status, uint32_t ts, uint32_t update_ts, uint32_t end_ts)
{
        // cJson Components
        char* string = NULL;
        cJSON *type = NULL;
        cJSON *payload = NULL;
        cJSON *status_json = NULL;
        cJSON *ts_json = NULL;
        cJSON *update_ts_json = NULL;
        cJSON *end_ts_json = NULL;
        cJSON *fall_data = cJSON_CreateObject();
        if (fall_data == NULL) {
                goto end;
        }
        /*  Type */
        type = cJSON_CreateNumber(FALL_EVENT);
        if (type == NULL) {
                goto end;
        }
        cJSON_AddItemToObject(fall_data, "type", type);
        /*  Payload */
        payload = cJSON_CreateObject();
        if (payload == NULL) {
                goto end;
        }
        status_json = cJSON_CreateString(status);
        if (status_json == NULL) {
                goto end;
        }
        cJSON_AddItemToObject(payload, "status", status_json);
        ts_json = cJSON_CreateNumber(ts);
        if (ts_json == NULL) {
                goto end;
        }
        cJSON_AddItemToObject(payload, "timestamp", ts_json);
        update_ts_json = cJSON_CreateNumber(update_ts);
        if (update_ts_json == NULL) {
                goto end;
        }
        cJSON_AddItemToObject(payload, "statusUpdateTimestamp", update_ts_json);
        end_ts_json = cJSON_CreateNumber(end_ts);
        if (end_ts_json == NULL) {
                goto end;
        }
        cJSON_AddItemToObject(payload, "endTimestamp", end_ts_json);
        cJSON_AddItemToObject(fall_data, "payload", payload);
        // Result
        string = cJSON_Print(fall_data);
        if (string == NULL) {
                printf("Failed to print fall_data.\n");
        }
end:
        cJSON_Delete(fall_data);
        return string;
}

esp_mqtt_client_handle_t init_mqtt_client(void *callback)
{
        // TODO: get flash memory
        char* mqtt_json = read_mqtt_cfg();
        char* mqtt_hostname = parse_mqtt_hostname(mqtt_json);
        char* mqtt_port = parse_mqtt_port(mqtt_json);
        char* mqtt_username = parse_mqtt_username(mqtt_json);
        char* mqtt_password = parse_mqtt_password(mqtt_json);

        const esp_mqtt_client_config_t mqtt_cfg = {
            .uri = mqtt_hostname,
            .port = atoi(mqtt_port),
            .client_id = "danh-esp",
            // .username = mqtt_username,
            // .password = mqtt_password,
            .keepalive = 15,
            .reconnect_timeout_ms = 15,
            .message_retransmit_timeout = 60};
        esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
        esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, callback, client);
        if (esp_mqtt_client_start(client) != ESP_OK) {
                return NULL;
        }
        return (client);
}

char* s_get_mqtt_topic(char* mqtt_topic)
{
        int next_ptr_pos = 0;
        char device_id[100];
        get_device_id(device_id);
        // printf("mydev %s\n", device_id);
        int total_len = strlen(MQTT_PREFIX_TOPIC) + 1 + strlen(device_id) + strlen(mqtt_topic) + 1;
        char* topic = (char* )malloc(sizeof(char) * total_len);
        strcpy(topic, MQTT_PREFIX_TOPIC);
        next_ptr_pos += strlen(MQTT_PREFIX_TOPIC);
        strcpy(topic + next_ptr_pos, "/");
        next_ptr_pos += 1;
        // TODO: get device id
        strcpy(topic + next_ptr_pos, device_id);
        next_ptr_pos += strlen(device_id);
        strcpy(topic + next_ptr_pos, mqtt_topic);
        // free(device_id);
        return topic;
}

void send_fall(esp_mqtt_client_handle_t client, const char* status, uint32_t ts, uint32_t update_ts, uint32_t end_ts)
{
        char* topic = s_get_mqtt_topic(TOPIC_UPSTREAM_EVENTS);
        char* msg = s_contruct_fall_data_json(status, ts, update_ts, end_ts);
        int msg_id = esp_mqtt_client_publish(client, topic, msg, 0, 1, 0);
        ESP_LOGI(MQTT, "sent publish successful, msg_id=%d", msg_id);
        free(topic);
        free(msg);
}

void send_presence(presence_mqtt_params *presence_msg)
{
        char* topic = s_get_mqtt_topic(TOPIC_UPSTREAM_EVENTS);
        char* msg = s_contruct_presence_data_json(presence_msg->is_present, (intmax_t)time(NULL));
        int msg_id = esp_mqtt_client_publish(presence_msg->client, topic, msg, 0, 1, 0);
        ESP_LOGI(MQTT, "sent publish successful, msg_id=%d", msg_id);
        free(topic);
        free(msg);
}
/**
 * @brief Handler for MQTT_CONNECTED event
 * 
 * @param client MQTT client
 */
static void s_mqtt_connect(esp_mqtt_client_handle_t client)
{
        ESP_LOGI(MQTT, "MQTT_EVENT_CONNECTED");
}

/**
 * @brief Handler for MQTT_DISCONNECTED event
 * 
 */
static void s_mqtt_disconnect()
{
        ESP_LOGE(MQTT,"Disconnected from MQTT broker");
}

/**
 * @brief Handler for MQTT_EVENT_SUBSCRIBED
 * 
 * @param client MQTT client
 * @param event MQTT event
 */
static void s_mqtt_subscribe(esp_mqtt_client_handle_t client, esp_mqtt_event_handle_t event)
{
        ESP_LOGI(MQTT, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
}

void s_handle_mqtt_topic(esp_mqtt_event_handle_t event)
{
        char mq_topic[50];
        char mq_data[500];
        char* check_cmd;
        char* check_cfg;
        sprintf(mq_topic, "%.*s", event->topic_len, event->topic);
        sprintf(mq_data, "%.*s", event->data_len, event->data);
        check_cmd = strstr(mq_topic, "commands");
        check_cfg = strstr(mq_topic, "config");
        if (check_cmd != NULL) {
                //topic is from command
        }
        if (check_cfg != NULL) {
                //topic is from config
                write_radar_cfg(mq_data);
        }
}

