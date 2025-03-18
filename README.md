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
2. In Burglar Alarm insert the WIFI SSID for "#define WIFI_SSID      "Insert Wifi SSID" and insert password for "#define WIFI_PASSWORD  "Insert Wifi Password" . Both of these can be found at the top of the file.
3. In Burglar Alarm and Burglar Alarm Sensors insert the mac address(in hexadecimal format) of the relevant ESP32 for "uint8_t sensor_mac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};" and "burglar_alarm_mac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};"
4. Set the "wifi_channel" for both Burglar Alarm and Burglar Alarm Sensors. Ensure these are on the same channel otherwise ESP-NOW will not work.
5. In Burglar Alarm change static IP settings "IP4_ADDR(&ip_info.ip, 192, 168, 1, 200);//Static IP address" "IP4_ADDR(&ip_info.gw, 192, 168, 1, 1);//Default gateway"   "IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);//subnet mask"
DHCP was disabled due to problems obtaining an IP address
6.Change the IP address in the Unity Companion App to match the IP address of Burglar Alarm


