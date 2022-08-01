MQTT
===========================

All uncommon topic have a prefix of **/devices/{deviceId}/**

Upstream uncommon topics
****************************************

Topic that the device publishes to.

+-----------------------+----------------------------+-------------------------------------+
| Topic                 | Event                      | Payload                             |
+=======================+============================+=====================================+
|  **events**           | Presence detected          ||  {                                 |
||                      |                            ||    "type":0,                       |
||                      |                            ||    "payload": {                    |
||                      |                            ||      "presenceDetected": true,     |
||                      |                            ||      "timestamp": int              |
||                      |                            ||      },                            |
||                      |                            ||  }                                 |
||                      +----------------------------+-------------------------------------+
||                      | Fall confirmed             ||  {                                 |
||                      |                            ||    "type":1,                       |
||                      |                            ||    "payload": {                    |
||                      |                            ||      "status": "fall confirmed",   |
||                      |                            ||      "timestamp": int,             |
||                      |                            ||      "statusUpdateTimestamp": int, |
||                      |                            ||      "endTimestamp": int           |
||                      |                            ||      },                            |
||                      |                            ||  }                                 |
||                      +----------------------------+-------------------------------------+
||                      | Fall exit                  ||  {                                 |
||                      |                            ||    "type":1,                       |
||                      |                            ||    "payload": {                    |
||                      |                            ||      "status": "fall exit",        |
||                      |                            ||      "timestamp": int,             |
||                      |                            ||      "statusUpdateTimestamp": int, |
||                      |                            ||      "endTimestamp": int           |
||                      |                            ||      },                            |
||                      |                            ||  }                                 |
||                      +----------------------------+-------------------------------------+
||                      | Calling                    ||  {                                 |
||                      |                            ||    "type":1,                       |
||                      |                            ||    "payload": {                    |
||                      |                            ||      "status": "calling",          |
||                      |                            ||      "timestamp": int,             |
||                      |                            ||      "statusUpdateTimestamp": int, |
||                      |                            ||      "endTimestamp": int           |
||                      |                            ||      },                            |
||                      |                            ||  }                                 |
||                      +----------------------------+-------------------------------------+
||                      | Finished process           ||  {                                 |
||                      |                            ||    "type":1,                       |
||                      |                            ||    "payload": {                    |
||                      |                            ||      "status": "finished",         |
||                      |                            ||      "timestamp": int,             |
||                      |                            ||      "statusUpdateTimestamp": int, |
||                      |                            ||      "endTimestamp": int           |
||                      |                            ||      },                            |
||                      |                            ||  }                                 |
||                      +----------------------------+-------------------------------------+
||                      | response from Commands     ||  {                                 |
||                      |                            ||    "type":2,                       |
||                      |                            ||    "payload": {                    |
||                      |                            ||      "id": id of last command,     |
||                      |                            ||      "code": int,                  |
||                      |                            ||      "msg": "Success",             |
||                      |                            ||      "timestamp": int              |
||                      |                            ||      },                            |
||                      |                            ||  }                                 |
+-----------------------+----------------------------+-------------------------------------+
| **state**             | Sent every 2 minutes       ||  {                                 |
||                      |                            ||    "status":"monitoring",          |
||                      |                            ||    "uptime": int,                  |
||                      |                            ||    "radar_uptime": int,            |
||                      |                            ||    "memory_usage": int,            |
||                      |                            ||    "fall_led": boolean,            |
||                      |                            ||    "nw_led": boolean,              |
||                      |                            ||    "buzzer": boolean,              |
||                      |                            ||    "wifi": {                       |
||                      |                            ||       "ssid": string,              |
||                      |                            ||       "rssi": int                  |
||                      |                            ||       },                           |
||                      |                            ||  }                                 |
+-----------------------+----------------------------+-------------------------------------+

.. note::
	The code from *response from Commands* stands for: 
	**0: success**
	**1: invalid**
	**2: network timeout**
	**3: hardware issue**

Downstream uncommon topics 
************************************

Topics that the device subscribes to.

+-----------------------+----------------------------+-------------------------------------------------------+
| Topic                 | Event                      | Payload                                               |
+=======================+============================+=======================================================+
| **commands**          | Upload device logs         ||  {                                                   |
||                      | (not yet supported)        ||    "id": string,                                     | 
||                      |                            ||    "type": 1,                                        |
||                      |                            ||    "timestamp": int,                                 |
||                      |                            ||    "day": int                                        |
||                      |                            ||  }                                                   |
||                      +----------------------------+-------------------------------------------------------+ 
||                      | Radar on                   ||  {                                                   |
||                      |                            ||    "id": string,                                     | 
||                      |                            ||    "type": 2,                                        |
||                      |                            ||    "timestamp": int                                  |
||                      |                            ||  }                                                   |
||                      +----------------------------+-------------------------------------------------------+
||                      | Restart MCU                ||  {                                                   |
||                      |                            ||    "id": string,                                     | 
||                      |                            ||    "type": 3,                                        |
||                      |                            ||    "timestamp": int                                  |
||                      |                            ||  }                                                   |
||                      +----------------------------+-------------------------------------------------------+
||                      | Cancel Alarm               ||  {                                                   |
||                      |                            ||    "id": string,                                     | 
||                      |                            ||    "type": 4,                                        |
||                      |                            ||    "timestamp": int                                  |
||                      |                            ||  }                                                   |
||                      +----------------------------+-------------------------------------------------------+
||                      | Radar off                  ||  {                                                   |
||                      |                            ||    "id": string,                                     | 
||                      |                            ||    "type": 5,                                        |
||                      |                            ||    "timestamp": int                                  |
||                      |                            ||  }                                                   |
+-----------------------+----------------------------+-------------------------------------------------------+
| **config**            | Update radar config        ||  {                                                   |
||                      |                            ||    "radar_config": {                                 |
||                      |                            ||      "staticBoundaryBox":"-2.0 2.0 0 4.0 -1.5 2.9",  |
||                      |                            ||      "boundaryBox":"-2.5 2.5 0 4.5 -1.5 2.9",        | 
||                      |                            ||      "sensorPosition":"2.2 0 25",                    |
||                      |                            ||      "gatingParam": "3 2 2 3 4",                     |
||                      |                            ||      "stateParam":"3 3 8 500 5 6000",                |
||                      |                            ||      "allocationParam":"100 150 0.05 20 0.5 20",     |
||                      |                            ||      "maxAcceleration":"0.3 0.3 0.5",                |
||                      |                            ||      "trackingCfg":"1 2 700 1 46 96 55",             |
||                      |                            ||      "presenceBoundaryBox":"-2.0 2.0 0 4.0 -1.5 2.9",|
||                      |                            ||    },                                                |
||                      |                            ||     "sub_region": [                                  |
||                      |                            ||       {"x_min": int,                                 |
||                      |                            ||        "x_max": int,                                 |
||                      |                            ||        "y_min": int,                                 |
||                      |                            ||        "y_max": int},                                |
||                      |                            ||       {"x_min": int,                                 |
||                      |                            ||        "x_max": int,                                 |
||                      |                            ||        "y_min": int,                                 |
||                      |                            ||        "y_max": int},                                |
||                      |                            ||     ],                                               |      
||                      |                            ||     "id": string,                                    |
||                      |                            ||     "timestamp": int                                 |                             
||                      |                            ||   }                                                  |
||                      |                            ||                                                      |
||                      +----------------------------+-------------------------------------------------------+
||                      | Update app config          ||  {                                                   |
||                      |                            ||     "app_config": {                                  |
||                      |                            ||       "max_log_file": int,                           |
||                      |                            ||       "presenceReportMinRateMills": int,             |
||                      |                            ||       "reportPresenceToMqtt": boolean,               |
||                      |                            ||       "reportFallsToMqtt": boolean,                  |
||                      |                            ||       "nw_led": boolean,                             |
||                      |                            ||       "fall_led": boolean,                           |
||                      |                            ||       "buzzer": boolean                              |
||                      |                            ||  },                                                  |
||                      |                            ||     "id": string,                                    |
||                      |                            ||     "timestamp": int                                 |
||                      |                            ||  }                                                   |
+-----------------------+----------------------------+-------------------------------------------------------+

.. note::
	"radar_config”: This configuration will replace the tracking config of radar.
	“sub_region”: The list of positions where fall events are ignored.

	**AFTER SEDNING CONFIG, DEVICE WILL RESET**

Common topics
*******************************************
These topics does not have {device id}, and are common between all devices.
Common topics share the same prefix **/devices/common/**

Common downstream topic
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
+-----------------------+----------------------------+-----------------------------------+
| Topic                 | Event                      | Payload                           |
+=======================+============================+===================================+
|  **request**          | broadcast request          ||  {                               |
||                      |                            ||    "type":"request",             |
||                      |                            ||    "requests":"status",          |
||                      |                            ||    "id": string,                 |
||                      |                            ||    "timestamp": int              |
||                      |                            ||  }                               |
+-----------------------+----------------------------+-----------------------------------+

Common upstream topic
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
+-----------------------+----------------------------+-----------------------------------+
| Topic                 | Event                      | Payload                           |
+=======================+============================+===================================+
|  **response**         |  broadcast response        ||  {                               |
||                      |                            ||    "type":"status",              |
||                      |                            ||    "id": string,                 |
||                      |                            ||    "code": 0                     |
||                      |                            ||    "time": int                   |
||                      |                            ||     "payload": {                 |
||                      |                            ||       "ssid": string,            |
||                      |                            ||       "rssi": int,               |
||                      |                            ||       "fw_title": string,        |
||                      |                            ||       "fw_ver": string,          |
||                      |                            ||       "prod_ver": string,        |
||                      |                            ||       "prod_name": string,       |
||                      |                            ||       "mqtt_host": string,       |
||                      |                            ||       "mqtt_port": string,       |
||                      |                            ||       "mqtt_user": string,       |
||                      |                            ||       "mqtt_pass": string,       |
||                      |                            ||       "device_id": string,       |
||                      |                            ||       "mac": "string",           |
||                      |                            ||       "ipaddress": string,       |
||                      |                            ||       "devtype": string          |
||                      |                            ||       "data": {                  |
||                      |                            ||         "nw_led": true,          |
||                      |                            ||         "fall_led": false,       |
||                      |                            ||         "buzzer": false          |
||                      |                            ||        }                         |
||                      |                            ||      }                           |
||                      |                            ||    }                             |
+-----------------------+----------------------------+-----------------------------------+

MQTT API
*******************************************

.. doxygenfile:: ex_com_mqtt.h 
	:project: Fall
