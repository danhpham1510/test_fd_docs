

/**
 * @brief Find absolute height of target base on pcs
 * 
 * @param pcs pointer to point cloud data
 * @param indexes pointer to indexes data
 * @param num_pcs number of pcs
 * @param num_index number of indexes
 * @param tid target id to find height
 * @return height of target
 * 
 * @warning num_pcs must be the same to number of indexes, indexes identicate to be index of each pcs
 * in last frame
 */
float calc_absolute_height(float* pcs, uint8_t* indexes, int num_pcs, int num_index, uint8_t tid);


/**
 * @brief Cumulative sum of height of target through each frame
 * 
 * @details
 * In other to stablize the height of target because the limitation of radar when detect pcs from each frame
 * 
 * @param abs_height height of target
 * @param last_avg_height last average height of target
 * @return float 
 */
float calc_avg_height(float abs_height, float last_avg_height);

/**
 * @brief [Check presceening] Monitor height fluctuation in height of last 10 frame (0.5s)
 * 
 * @param q_height queue average height of target
 * @param id target index
 * @param len length of queue
 * @retval 1 pass height condition
 * @retval 0 not pass
 */
bool check_prescreening(float* q_height, uint8_t id, uint8_t len);

/**
 * @brief [Check presceening] Monitor mean velocity last 30 frame (1.5s)
 * 
 * @param q_velo velocity queue
 * @param id target index
 * @param len length of queue
 * @return If there is a downgrade in velocity of target
 * @warning When use this function need to aware that there is a delay can get from target 
 * due to algorithm of radar firmware
 * 
 */
bool check_velocity_condition(float* q_velo,  uint8_t id, uint8_t len);

/**
 * @brief Change state of fall led
 * 
 * @param status (1 for on, 0 for off)
 */
void turn_fall_led(bool status);

/**
 * @brief Ignore from now
 * 
 */
void presence_led_control();
/**
 * @brief Init fall led
 * 
 */
void init_fall_led();

/**
 * @brief Monitor height after fall event to indentify if person had stand up
 * 
 * @param abs_height_queue absolute height queue
 * @param len length of queue
 * @retval 1 stand up
 * @retval 0 still lay on floor
 */
bool check_fall_exit_height(float* abs_height_queue, int len);

/**
 * @brief Add a new value to each computation queue
 * 
 * @param q_absH absolute height queue
 * @param q_avgH average height queue
 * @param q_velocity velocity queue
 * @param idx target index
 * @param max_len length of queue
 * @param absH absolute height
 * @param vel velocity of target
 */
void fill_computation_queue( float** q_absH, float** q_avgH,  float** q_velocity, 
			     uint8_t idx, uint8_t max_len, float absH, float vel);

/**
 * @brief Publish event to mqtt task
 * 
 * @param q_mqtt bridge to communicate between mqtt and fall task
 * @param id index of msg (fall 1, presence 0)
 * @param msg message to send (fall event, presence event)
 */
void pub_to_mqtt(QueueHandle_t* q_mqtt, uint8_t id, char* msg);

/**
 * @brief Send state to control center
 * 
 * @param q bridge to comunicate between fall to mqtt task
 * @param state device state to delivery
 */
void control_fall_led(QueueHandle_t* q, enum DEVICE_STATE* state);
