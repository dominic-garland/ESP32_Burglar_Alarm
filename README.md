# ESP32_Burglar_Alarm
Burglar Alarm using ESP32. Contains burglar alarm, sensor and unity app.
Unity Companion App link:
https://drive.google.com/file/d/14aeFLBAw0lD5uuB7Utj5Otp15ORYoUO6/view?usp=sharing



THIS FILE IS A WIP




This ESP32 project consists of folders:
1.Burglar Alarm - contains code for the burglar alarm itself
2.Burglar Alarm Sensors - contains the code for sensor communicating with the burglar alarm
3.Security Alarm App/Unity Companion App - contains a unity project with build for easy alarm control

Setup:
1. Download all files including the Unity Companion App
2. Ensure your ESP-IDF has the correct menu configurations. The changes can be found in the MenuConfig section and can be seen in z/z/z  ...... ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
3. ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
4. ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
5. In Burglar Alarm insert the WIFI SSID for "#define WIFI_SSID      "Insert Wifi SSID" and insert password for "#define WIFI_PASSWORD  "Insert Wifi Password" . Both of these can be found at the top of the file.
6. In Burglar Alarm and Burglar Alarm Sensors insert the mac address(in hexadecimal format) of the relevant ESP32 for "uint8_t sensor_mac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};" and "burglar_alarm_mac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};"
7. Set the "wifi_channel" for both Burglar Alarm and Burglar Alarm Sensors. Ensure these are on the same channel otherwise ESP-NOW will not work.
8. In Burglar Alarm change static IP settings "IP4_ADDR(&ip_info.ip, 192, 168, 1, 200);//Static IP address" "IP4_ADDR(&ip_info.gw, 192, 168, 1, 1);//Default gateway"   "IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);//subnet mask"
DHCP was disabled due to problems obtaining an IP address
6.Change the IP address in the Unity Companion App to match the IP address of Burglar Alarm
7.If you are not in the UK then the NTP timezone will need to be changed. The timezone needs to be changed for the Burglar Alarm file and on this line "setenv("TZ", "GMT0BST,M3.5.0/01,M10.5.0/02", 1);"
8.If you are not in the UK then the regulatory rules that the ESP32s follow may need to be changed. For example the WIFI channels allowed in the UK are 1-13 however, specific country restrictions should be researched to ensure functionality.
9.The IR sensor used is a HC-SR501 PIR Infrared Motion Detection Sensor. This motion sensor has 2 potentiometers that can be adjusted. These should be adjusted based on your requirements and your model. Please refer to documentation provided with your sensor.

Testing:
To test the system is working it is easiest to use the Unity Companion App with the ESPs however, you can also go onto the web server.
Assuming the devices have been setup correctly the Burglar Alarm code can be uploaded onto an ESP32. 
Once turned on it could take up to a minute for it to start functioning. After a minute you can use the Unity Companion App to test if the device is working. 

1.Ensure that the device running the app is on the same WIFI as the ESP32. 
2.Change the IP address in the companion app to match the ESP32. This can be done in settings on the app.
3.Once returned to the main menu the app should start working. The textbox in the centre should say either "Alarm active" or "Alarm inactive". If it says "Cannot connect to device" try restarting the app. If this does not work and the device is on the same WIFI try pressing the reset button on the ESP32. Sometimes the ESP32 fails at connecting to WIFI and therefore a simple restart can amend this.
4.Now the Burglar Alarm Sensor can be turned on. This will take up to 2 minutes to be ready due to the sensor needing to warm-up. After 2 minutes it should automatically connect to the other ESP32 assuming it is in range. To test if they are connected set the alarm to active and have movement in front of the sensor. If the ESP32 are working correctly the alarm will be triggered causing the buzzer to go off and a new log to be created in the logs menu on the Unity Companion App. Once confirmed it is working you can press the deactivate alarm button to switch off the alarm. When the alarm is triggered the sensor needs 1 and a half minutes to reset.

About:
  Burglar Alarm:
    This should take ~60 seconds to start up. To confirm that it has loaded correctly the web server should be tested. This can be tested by using the Unity Companion App which will show on the landing page if it connected to the device or using the web server URIs can      be used. A guide to the functions available can be seen in the web server section. If these methods do not work ensure that the ESP32 is on the same WIFI as the device that is trying to access it. Also ensure that the static IP address set is available. If these         checks do not result in the device working try restarting the device as sometimes the ESP32 struggles with connecting to the WIFI.  
    Logs:
      Logs are stored in the device NVS(non-volatile storage) meaning that alerts will be stored even when rebooted. When the alarm is triggered the device automatically records the time of the event. Upon starting the device will get the time from 216.239.35.12 which         is the IP address for time.google.com . It will then sync this time every 10 seconds to ensure that the correct time is recorded. The maximum logs stored is 10 however, this can be increased if necessary by changing "MAX_LOG_ENTRIES". If the logs are reset the         old logs cannot be restored so caution should be taken when using this feature. 
  Burglar Alarm Sensor:
    This should take ~2 minutes to start up because the IR-sensor needs 60 seconds to warm-up upon first loading.
    IR-Sensor:
      Upon first loading the IR-Sensor needs 60 seconds to warm up to ensure correct functionality. The IR sensor used will detect if there is movement through Infrared. If movement is detected the program stops detecting movement for 60 seconds or until reset to             ensure detections aren't duplicated. The sensor should be adjusted using the 2 potentiometers on the device to control sensitivity and output delay for the best functionality. Also refer to the sensor manual to ensure the sensor is being used correctly and there         is no interference such as light sources near to the module.
  Unity Companion App:
      The Unity Companion App communicates with the Burglar Alarm by sending web requests and therefore is not needed as direct connection to the web server can be used. Information about this process can be found in the web server section. This means the app requires         the device to be on the same WIFI as the ESP32 Burglar Alarm in order to function. It also means that the IP address needs to be correctly set in the settings page of the app. To reduce issues the ESP32 should have a reserved IP address. The Companion App allows         users to see the alarm status(active/deactivated), set the alarm status(active/deactivated), view the intruder logs, and reset the logs.
  ESP-NOW:
      ESP-NOW is used to connect the Burglar Alarm to the sensors and the sensors to the Burglar Alarm. ESP-NOW is a "wireless communication protocol defined by Espressif(creators of the ESP32)". This implementation of ESP-NOW uses WIFI instead of BLE. ESP-NOW is used 
      for the Burglar Alarm to turn on/off the sensing for the sensors and it also allows the sensors to communicate wirelessly to the Burglar Alarm when movement is detected. For the Burglar Alarm ESP-NOW needs to be on the same channel as WIFI as the ESP32 only has 
      1 antenna that is shared by both devices. The Burglar Alarm Sensor does not have this problem as it is not connected to WIFI. Both the Burglar Alarm and the Burglar Alarm Sensor need to be on the same channel to communicate. 
  WIFI:
      The Burglar Alarm ESP32 uses WIFI for web server purposes allowing the alarm to be controlled through the Companion App or through the URIs. The Burglar Alarm Sensor setups WIFI up however, does not connect to the WIFI. This is because WIFI needs to be initialised 
      for ESP-NOW to work in this implementation. The code could be edited to use BLE instead of WIFI for ESP-NOW however, if WIFI is removed then the Companion App and the web URIs would no longer be able to be used.
  FreeRTOS:
      FreeRTOS is used for both the Burglar Alarm and the Burglar Alarm Sensor. The job of FreeRTOS is to prioritise tasks and run higher priority tasks before lower priority tasks. The prioritisation chosen allows for easy future expansion. The task priorities are as          follows:
        Burglar Alarm:
          Priority Level 8:
            alarm_task
          Priority Level 4:
            espnow_receive_task
            espnow_send_task
            web_server_task
            wifi_task
            
        Burglar Alarm Sensor:
          Priority Level 4:
            espnow_send_task
            espnow_receive_task
          Priority Level 1:
            sensor_deactivate_task
            sensor_active_task
            
WebServer:
The need for the Unity Companion App can be bypassed by going to the URIs for the web server. To access this type the local IP address of the ESP32 followed by the command. For example to access the logs you would go to (IP address)/logs . So for an IP address of 192.168.1.5 you would type 192.168.1.5/logs
There are 5 commands available:
  Activate - activates the alarm. (/activate)
  Deactivate - deactivates the alarm. (/deactivate)
  Logs - shows the 10 most recent intruder alarm alerts. (logs)
  Reset Logs - resets the logs. (/reset_logs)
  Get Status - retrieves alarm status. (/get_status)
The web pages are extremely basic and do not confirm if the command has worked. Therefore if using the web pages ensure each command has worked correctly. For example if you wanted to activate the alarm you would visit the get status page after to ensure the alarm has been set correctly.


MenuConfig:
