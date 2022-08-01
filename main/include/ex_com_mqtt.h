/**
 * @brief Prefix to MQTT topics
 * 
 */
#define MQTT_PREFIX_TOPIC "/devices"

/**
 * @brief Topic for upstream events
 * 
 */
#define TOPIC_UPSTREAM_EVENTS "/events"

/**
 * @brief Topic for upstream state
 * 
 */
#define TOPIC_UPSTREAM_STATE "/devices/%s/state", id

/**
 * @brief Topic for upstream telemetry
 * 
 */
#define TOPIC_UPSTREAM_TELEMETRY(id) "/devices/%s/events/telemetry", id

/**
 * @brief Topic for upstream analytics
 * 
 */
#define TOPIC_UPSTREAM_ANALYTICS(id) "/devices/%s/events/analytics", id

/**
 * @brief Topic for downstream commands
 * 
 */
#define TOPIC_DOWNSTREAM_COMMANDS "/commands/#"

/**
 * @brief Topic for downstream config
 * 
 */
#define TOPIC_DOWNSTREAM_CONFIG "/config"

/**
 * @brief Time interval between states in seconds
 * 
 */
#define EVENT_STATE_INTERVAL 60

typedef struct presence_mqtt_params{
    esp_mqtt_client_handle_t client;
    bool is_present;
}presence_mqtt_params;

/**
 * @brief Init MQTT, fetch config from storage
 * 
 * @return esp_mqtt_client_handle_t The MQTT client handle
 */
esp_mqtt_client_handle_t init_mqtt_client();


/**
 * @brief Send presence message via MQTT
 * 
 * @param presence_msg The message
 */
void send_presence(presence_mqtt_params* presence_msg);


/**
 *  @brief Communicate to mqtt: send fall state in fall processing
 *
 *  @param  client      MQTT client from esp idf
 *  @param  status      State of fall
 *  @param   ts         Timestamp of fall process
 *  @param   update_ts  Timestamp of fall process update
 *  @param   end_ts     Timestamp of fall process end
*/
void send_fall(esp_mqtt_client_handle_t client, const char* status, uint32_t ts, uint32_t update_ts, uint32_t end_ts);


/**
 * @brief Log MQTT error
 * 
 * @param message The messase
 * @param error_code The error code
 */
void log_error_if_nonzero(const char *message, int error_code);

/**
 * @brief Handle messages from MQTT topic
 * 
 * @param event the MQTT event
 */
void s_handle_mqtt_topic(esp_mqtt_event_handle_t event);

/**
 * @brief Get MQTT topic based on device uuid
 * 
 * @param mqtt_topic the topic from #DEFINE
 * @return char* The finalised opic
 */
char *s_get_mqtt_topic(char *mqtt_topic);