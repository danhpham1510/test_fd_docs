/**
 * @brief Initialize SPIFFS storage 
 * 
 */
void init_spiffs();

/**
 *  @brief Write device uuid into storage
 *  
 *  @param uuid The uuid of the device
 */
void write_uuid(char* uuid);

/**
 * @brief Read UUID from storage
 * 
 * @return char*  The UUID 
 */
char* read_uuid(void);

/**
 * @brief Write MQTT config into storage
 * 
 * @param hostname MQTT hostname
 * @param port MQTT port
 * @param username MQTT username
 * @param password MQTT password
 */
void write_mqtt_cfg(char* hostname, char* port, char* username, char* password);

/**
 * @brief Read MQTT config from storage
 * 
 * @return char* The MQTT config
 */
char* read_mqtt_cfg(void);

/**
 * @brief Write wifi config into storage
 * 
 * @param ssid Wifi ssid
 * @param password Wifi password
 */
void write_wifi_cfg(char* ssid, char* password);

/**
 * @brief Read wifi config from storage
 * 
 * @return char* The wifi config
 */
char* read_wifi_cfg(void);

/**
 * @brief Write radar config to storage
 * 
 * @param json_buffer Radar config from MQTT message
 */
void write_radar_cfg(char* json_buffer);

/**
 * @brief Read radar config from storage
 * 
 * @return char* The radar config
 */
char* read_radar_cfg(void);

/**
 * @brief Write current network state to storage
 * 
 * @param state The network state
 */
void write_nw_state(char* state);

/**
 * @brief Read network state form storage
 * 
 * @return char* The network state
 */
char* read_nw_state(void);

/**
 * @brief Unmount SPIFFS
 * 
 */
void unregister_spiffs(void);

/**
 *  @brief Parse static boundary box config into string
 *  
 *  @param content The json string when read from spiffs
 * 
 *  @return Static boundary box config as a string
 */
char* parse_static_boundary_box(char* content);

/**
 *  @brief  Parse presence boundary box config into string
 *  
 *  @param  content The json string when read from spiffs
 *  
 *  @return Presence boundary box config as a string
 */
char* parse_presence_boundary_box(char* content);

/**
 *  @brief  Parse boundary box config into string
 *  
 *  @param  content  The json string when read from spiffs
 *  
 *  @return Boundary box config as a string
 */
char* parse_boundary_box(char* content);

/**
 *  @brief    Parse sensor position config into string
 *
 *  @param    content  The json string when read from spiffs
 * 
 *  @return   Sensor position config as a string
 */
char* parse_sensor_position(char*  content);

/**
 *  @brief    Parse gating param config into string
 *  
 *  @param    content  The json string when read from spiffs
 *  
 *  @return   Gating param config as a string
 */
char* parse_gating_param(char*  content);

/**
 *  @brief   Parse state param config into string
 * 
 *  @param   content The json string when read from spiffs
 *  
 *  @return  State param config as a string
 */
char* parse_state_param(char*  content);

/**
 *  @brief  Parse allocation param config into string
 * 
 *  @param  content The json string when read from spiffs
 * 
 *  @return Allocation param config as a string
 */
char* parse_allocation_param(char*  content);

/**
 *  @brief  Parse max acceleration config into string
 *  
 *  @param  content The json string when read from spiffs
 *  
 *  @return Max acceleration config as a string
 */
char* parse_max_acceleration(char*  content);

/**
 *  @brief  Parse tracking config into string
 *  
 *  @param  content The json string when read from spiffs
 *  
 *  @return Tracking config as a string
 */
char* parse_tracking_cfg(char*  content);

/**
 *  @brief  Parse wifi ssid into string
 *  
 *  @param  content  The json string when read from spiffs
 *  
 *  @return Wifi ssid as a string
 */
char* parse_wifi_ssid(char* content);

/**
 *  @brief  Parse wifi password into string
 *  
 *  @param  Content The json string when read from spiffs
 *  
 *  @return Wifi password as a string
 */
char* parse_wifi_pwd(char* content);

/**
 *  @brief  Parse mqtt hostname into string
 *  
 *  @param  content The json string when read from spiffs
 *  
 *  @return Mqtt hostname as a string
 */
char* parse_mqtt_hostname(char* content);

/**
 *  @brief  Parse mqtt port into string
 *  
 *  @param  content The json string when read from spiffs
 *  
 *  @return Mqtt port as a string
 */
char* parse_mqtt_port(char* content);

/**
 *  @brief  Parse mqtt username into string
 *  
 *  @param  content  The json string when read from spiffs
 *  
 *  @return Mqtt username as a string
 */
char* parse_mqtt_username(char* content);

/**
 *  @brief   Parse mqtt password into string
 *  
 *  @param   content The json string when read from spiffs
 *  
 *  @return  Mqtt password as a string
 */
char* parse_mqtt_password(char* content);