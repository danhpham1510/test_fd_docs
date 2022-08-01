#include "struct.h"
#include <stdio.h>


#define FALL_LED_GPIO GPIO_NUM_17                       /**< Pin of fall led*/	
#define RADAR_RESET_GPIO GPIO_NUM_22			/**< Pin to control NRESET pin on radar*/			
#define SOP2 GPIO_NUM_21				/**< Pin to control mode of radar*/
//TODO: need to change after wrap up schematic
#define UART_MUX_CTRL GPIO_NUM_27			/**< Pin to control data flow(pin header or uart) of radar*/


//TODO: need to read from file config
#define TILT_ANGLE 25					/**< theta in rotx()*/
#define SENSOR_HEIGHT 2.2				/**< elevate coordinate*/

#define PI 3.14159265358979323846


struct frame_struct{
        uint32_t frame_number;
        int num_targets;
        int num_point_clouds;
        int num_indexes;
        float* point_clouds;
        float* targets;
        uint8_t* indexes;
};

struct mqtt_fall_event{
        uint32_t event_id;
        char * msg;
};

/**
 * @brief State machine of device life
 * 
 */
enum DEVICE_STATE {
        FALL_DETECTED,          /*!< Someone fall in room */
        FALL_CONFIRMED,         /*!< Confirm a fall */
        CALLING,                /*!< Notification for users */
        FINISHED,               /*!< After fall event */
        FALL_EXITED,            /*!< Exit fall event */
        IDLE,                   /*!< Idle state */
        OCCUPIED,               /*!< Some on in room  */
        VACANT,                 /*!< Room empty */
        AP,                     /*!< Access point mode */
        STA,                    /*!< Station (normal mode) */
        MQTT_CONNECTED,         /*!< MQTT state */
        MQTT_DISCONNECTED       /*!< MQTT state */
};

/**
 * @brief Timer to change state in fall event
 * 
 */
enum FALL_STATE_TIME {
        FALL_DETECTED_TIME = 3,                 /*!< Time(s) wait to change to fall detected state */
        FALL_CONFIRMED_TIME = 3 + 40,           /*!< Time(s) wait to change to fall confirmed state */
        CALLING_TIME = 3 + 40 + 40,             /*!< Time(s) wait to change to fall calling state */
        FINISHED_TIME = 3 + 40 + 40 + 60,       /*!< Time(s) wait to change to fall confirmed state */
};

/**
 * @brief Optimize data type before send to computation task
 * 
 */
struct fall_features {
        uint32_t frame_number;
        int num_targets;
        float abs_height[13];
        float target[10*13]; 
};