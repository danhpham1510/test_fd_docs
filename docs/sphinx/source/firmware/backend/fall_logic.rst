Fall logic
=================================
     
This section will cover these things:

*   Fall computation logic
*   Process after fall
*   Process presence status

General pipeline
---------------------------

.. seqdiag::
    :caption: Fall logic diagram
    :align: center

    seqdiag sample-scenarios-station-mode {
        activation = none;
        node_width = 100;
        node_height = 60;
        edge_length = 140;
        span_height = 5;
        default_shape = roundedbox;
        default_fontsize = 11;

        EXTR_TASK  [label = "Extract \n radar data\ntask"];
        LOGIC_TASK   [label = "Fall logic\ntask"];
        MQTT_TASK [label = "MQTT\ntask"];
        CONTR_TASK  [label = "Center\n Control task"];

        === 1. Send data ===
        EXTR_TASK  ->  EXTR_TASK    [label="Mutex lock"];
        EXTR_TASK  ->  LOGIC_TASK   [label="Fall feature sent"];
        EXTR_TASK  ->  EXTR_TASK    [label="Mutex release"];
        === 2. Receive data ===
        LOGIC_TASK  ->  LOGIC_TASK    [label="Mutex lock"];
        LOGIC_TASK  ->  LOGIC_TASK    [label="Fall feature receive"];
        LOGIC_TASK  ->  LOGIC_TASK    [label="Mutex release"];
        === 3. Fall logic ===
        LOGIC_TASK  ->  MQTT_TASK    [label="3.1> Send fall status if fall happen"];
        LOGIC_TASK  ->  CONTR_TASK   [label="3.2> Control fall led"];
        LOGIC_TASK  ->  CONTR_TASK   [label="3.3> Control buzzer"];
        LOGIC_TASK  ->  MQTT_TASK    [label="3.4> Send presence status"];
    }
    
Fall logic process
---------------------------

After receive :cpp:struct:`fall_features` from extract radar data task in state 2, the fall logic task will do these task:

*   Check if there is people in room and set presence status to OCCUPIED to send to MQTT else 
this task will wait until there is no person in room for 3s and change presence status to VACANT then notify for MQTT.

*   There are 3 queues to store processed :cpp:struct:`fall_features`:

    All queues will be :cpp:func:`fill_computation_queue` every time presence detected.

    *   Absolute height queue: get value from struct and used to :cpp:func:`check_fall_exit_height` after a fall.

    *   Average height queue: get value from current absolute height and used to :cpp:func:`check_prescreening` of height. 
    
        The formula of this value: avgH[n] = 19/20*avgH[n-1] + 1/20*current_absH

    *   Velocity queue: get value from struct and used as a second condition for prescreening in :cpp:func:`check_velocity_condition`

State machine
---------------------------
This task will follow all state in FD_flowchart.pdf document. At every state need to notify, :cpp:func:`pub_to_mqtt` will take care the msg to MQTT task. 
Controling peripherals in this task by :cpp:func:`control_fall_led` and you can customize led and buzzer in MQTT handler task.

Fall logic API
-----------------------------------------

.. doxygenfile:: fall_logic.h 
	:project: Fall

