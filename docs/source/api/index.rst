HTTP API
=======================================

HTTP access
#########################################
Authentication is handled using Basic Authentication, and is performed on all requests.

| The username and password for Authentication is:
| **devmaster**
| **12345678**

Requests
#########################################


Get device UUID
*****************************************

.. code-block:: rst

   GET 192.168.4.1/conn 

Get current wifi information
*****************************************
.. code-block:: rst

   GET 192.168.4.1/wifi/info 

Restart and switch to STA mode
*****************************************
.. code-block:: rst

   GET 192.168.4.1/switchmode


Get current MQTT information
*****************************************
.. code-block:: rst

   GET 192.168.4.1/mqtt/info

Post MQTT credentials
*****************************************
.. code-block:: rst

   POST 192.168.4.1/mqtt/creds

*Example request:*

.. code-block:: rst

    {
      "mqtt_hostname":"<hostname>",
      "mqtt_port":"<port>",
      "mqtt_username":"<username>",
      "mqtt_password":"<password>"
    }

Post Wifi credentials
****************************************
.. code-block:: rst

   POST 192.168.4.1/wifi/creds

*Example request:*

.. code-block:: rst

    {
      "ssid":"<ssid>",
      "password":"<password>"
    }

