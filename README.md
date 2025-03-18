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
8.If you are not in the UK then the reglatory rules that the ESP32s follow may need to be changed. For example the WIFI channels allowed in the UK are 1-13 however, specific country restrictions should be researched to ensure functionality.
9.The IR sensor used is a HC-SR501 PIR Infrared Motion Detection Sensor. This motion sensor has 2 potentiometers that can be adjusted. These should be adjusted based on your requirements and your model. Please refer to documentation porvided with your sensor.

Testing:
To test the system is working it is easiest to use the Unity Companion App with the ESPs however, you can also go onto the web server.
Assuming the devices have been setup correctly the Burglar Alarm code can be uploaded onto an ESP32. 
Once turned on it could take up to a minute for it to start functioning. After a minute you can use the Unity Companion App to test if the device is working. 
1.Ensure that the device running the app is on the same WIFI as the ESP32. 
2.Change the IP address in the companion app to match the ESP32. This can be done in settings on the app.
3.Once returned to the main menu the app should start working. The textbox in the center should say either "Alarm active" or "Alarm inactive". If it says "Cannot connect to device" try restarting the app. If this does not work and the device is on the same WIFI try pressing the reset button on the ESP32. Sometimes the ESP32 fails at connecting to WIFI and therefore a simple restart can amend this.
4.Now the Burglar Alarm Sensor can be turned on. This will take up to 2 minutes to be ready due to the sensor needing to warm-up. After 2 minutes it should automatically connect to the other ESP32 assuming it is in range. To test if they are connected set the alarm to active and have movement in front of the sensor. If the ESP32 are working correctly the alarm will be triggered causing the buzzer to go off and a new log to be created in the logs menu on the Unity Companion App. Once confirmed it is working you can press the deactivate alarm button to switch off the alarm. When the alarm is triggered the sensor needs 1 and a half minutes to reset.

WebServer:
The need for the Unity Companion App can be bypassed by going to the URIs for the web server. To access this type the local IP address of the ESP32 followed by the command. For example to access the logs you would go to (IP address)/logs . So for an IP address of 192.168.1.5 you would type 192.168.1.5/logs
There are 5 commands avaliable:
  Activate - activates the alarm. (/activate)
  Deactivate - deactivates the alarm. (/deactivate)
  Logs - shows the 10 most recent intruder alarm alerts. (logs)
  Reset Logs - resets the logs. (/reset_logs)
  Get Status - retrieves alarm status. (/get_status)
The web pages are extremely basic and do not confirm if the command has worked. Therefore if using the web pages ensure each command has worked correctly. For example if you wanted to activate the alarm you would visit the get status page after to ensure the alarm has been set correctly.


MenuConfig:
