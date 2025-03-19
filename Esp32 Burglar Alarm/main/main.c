#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <inttypes.h>
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_now.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h" 
#include "esp_http_server.h"
#include "esp_sntp.h"
#include "esp_netif.h"
#include "lwip/ip4.h"
#include "cJSON.h"
#include "esp_event.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/ip4_addr.h"



#define LED_GPIO_PIN 2
#define BUZZER_PIN 33
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC2STR(mac)  (mac)[0], (mac)[1], (mac)[2], (mac)[3], (mac)[4], (mac)[5]
#define MAX_LOG_ENTRIES 10
#define WIFI_SSID      "Insert Wifi SSID"
#define WIFI_PASSWORD  "Insert Wifi Password"

uint8_t pmk[16] = {0x76, 0xDF, 0x7F, 0x59, 0x77, 0xAF, 0x72, 0xF7,
    0x7D, 0xE7, 0x5B, 0x6D, 0x72, 0xF5, 0xC6, 0x23};

uint8_t lmk[16] = {0x1A, 0x2B, 0x3C, 0x4D, 0x5E, 0x6F, 0x70, 0x81,
    0x92, 0xA3, 0xB4, 0xC5, 0xD6, 0xE7, 0xF8, 0x09};

uint8_t sensor_mac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

uint8_t wifi_channel = 12;

char detection_log[MAX_LOG_ENTRIES][50];

int log_index = 0;

bool alarm_active = false;
bool alarm_tripped = false;

struct timeval tv;
struct timezone tz;

static const char *TAG = "MAIN";
static SemaphoreHandle_t log_mutex;

TaskHandle_t web_server_task_handle;
TaskHandle_t espnow_receive_task_handle;
TaskHandle_t espnow_send_task_handle; 
TaskHandle_t alarm_task_handle;
TaskHandle_t wifi_task_handle;

typedef uint8_t espnow_message_t;

void add_log_entry(const char *message);
void web_server_startup();
void espnow_receive_task(void *pvParameters);
void espnow_send_task(void *pvParameters);
void alarm_task(void *pvParameters);
void wifi_task(void *pvParameters);

void config_io()//configure buzzer and built-in LED
{
    gpio_reset_pin(LED_GPIO_PIN);
    gpio_set_direction(LED_GPIO_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO_PIN, 0);
    ESP_LOGI(TAG, "LED configured.");
    gpio_reset_pin(BUZZER_PIN);
    gpio_set_direction(BUZZER_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(BUZZER_PIN, 0);
    ESP_LOGI(TAG,"Buzzer configured");
}

void setup_nvs()//setups non volatile storage - needed for wifi and ESP-NOW
{
    esp_log_level_set("*", ESP_LOG_DEBUG); //Ensures ESP logs appear 
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) 
    { // erase NVS corruption
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG, "NVS initialization failed: %s", esp_err_to_name(err));
        return;
    }
}

void init_time_sync()
{
    ESP_LOGI(TAG, "Initializing NTP...");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "216.239.35.12");// time.google.com IP
    esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    sntp_set_sync_interval(10000);
    esp_sntp_init();
    setenv("TZ", "GMT0BST,M3.5.0/01,M10.5.0/02", 1);
    tzset();
}

void wait_for_time_sync()//waits for time to sync
{
    esp_log_level_set("sntp", ESP_LOG_DEBUG);
    ESP_LOGI(TAG, "Waiting for NTP time sync");
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 60;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && retry < retry_count)
    {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry + 1, retry_count);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        retry++;
    }
    if (retry == retry_count)
    {
        ESP_LOGW(TAG, "Time sync failed! Check your Wi-Fi connection.");
        return;
    }
    time(&now);
    localtime_r(&now, &timeinfo);
    ESP_LOGI(TAG, "Time synced: %s", asctime(&timeinfo));
}

void on_data_send(const uint8_t *mac_addr, esp_now_send_status_t status)//ESP-NOW send data  
{
    if (status == ESP_NOW_SEND_SUCCESS) 
    {
        ESP_LOGI(TAG, "ESP-NOW packet sent successfully");
    } 
    else 
    {
        ESP_LOGE(TAG, "ESP-NOW packet failed to send");
    }
}

void add_peer_device(uint8_t *macAddress) // Adds sensor to ESP-NOW
{
    uint8_t lmk[16] = {0x1A, 0x2B, 0x3C, 0x4D, 0x5E, 0x6F, 0x70, 0x81,
        0x92, 0xA3, 0xB4, 0xC5, 0xD6, 0xE7, 0xF8, 0x09};
    esp_wifi_get_channel(&wifi_channel, NULL);
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, macAddress, ESP_NOW_ETH_ALEN);
    peerInfo.channel = wifi_channel;
    peerInfo.ifidx = ESP_IF_WIFI_STA;
    peerInfo.encrypt = true;
    memcpy(peerInfo.lmk, lmk, ESP_NOW_KEY_LEN); // Set the LMK for encryption
    ESP_LOGI(TAG, "Assigned LMK: %02X%02X%02X%02X...%02X", lmk[0], lmk[1], lmk[2], lmk[3], lmk[15]);
    esp_err_t err = esp_now_add_peer(&peerInfo);
    if (!esp_now_is_peer_exist(macAddress))
    {
        ESP_LOGI(TAG, "Peer not found. Attempting to add");
        
        int retry_count = 20;
        while (retry_count > 0)
        {
            ESP_LOGI(TAG, "Attempting to add peer on channel: %d", peerInfo.channel);
            err = esp_now_add_peer(&peerInfo);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to add peer: %s. Retrying", esp_err_to_name(err));
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                retry_count--;
            }
            else
            {
                ESP_LOGI(TAG, "Peer added successfully on channel %d", wifi_channel);
                break;
            }
        }
        if (retry_count == 0)
        {
            ESP_LOGE(TAG, "Failed to add peer after multiple attempts.");
        }
    }
    else
    {
        esp_now_peer_num_t peer_count;
        esp_now_get_peer_num(&peer_count);
        ESP_LOGI(TAG, "Total peers: %d", peer_count.total_num);
        ESP_LOGI(TAG, "Peer added successfully");
    }
    if (esp_now_is_peer_exist(macAddress))
    {
        ESP_LOGI(TAG, "Peer exists and is ready for communication.");
        ESP_LOGI(TAG, "Encryption enabled: %s", peerInfo.encrypt ? "YES" : "NO");
    }
    else
    {
        ESP_LOGE(TAG, "Peer failed to add correctly. Check encryption settings.");
    }
}

esp_err_t setup_espnow(uint8_t *macAddress) // Setup for ESP-NOW
{
    esp_err_t err = esp_now_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "ESP-NOW initialization failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "ESP-NOW initialized successfully");

    // Set the PMK for encryption
    err = esp_now_set_pmk(pmk);
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG, "Failed to set PMK: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "PMK set successfully");

    gpio_set_level(LED_GPIO_PIN, 1);
    ESP_LOGI(TAG, "Current Wifi Channel: %d", wifi_channel);
    add_peer_device(macAddress);
    esp_now_peer_num_t peer_count;
    esp_now_get_peer_num(&peer_count);
    ESP_LOGI(TAG, "Total peers: %d", peer_count.total_num);
    if (peer_count.total_num > 0)
    {
        ESP_LOGI(TAG, "Peer added. Proceeding with communication.");
    }
    else
    {
        ESP_LOGE(TAG, "No peers added. Check peer settings and encryption.");
    }

    return ESP_OK;
}

void add_log_entry(const char *message)//function for adding intruder logs 
{
    if (xSemaphoreTake(log_mutex, portMAX_DELAY)) 
    {
        snprintf(detection_log[log_index], sizeof(detection_log[log_index]), "%s", message);
        log_index = (log_index + 1) % MAX_LOG_ENTRIES;
        xSemaphoreGive(log_mutex);
    }
}

void on_data_received(const esp_now_recv_info_t *info, const uint8_t *data, int len)//ESP-NOW receive information
{
    if (info == NULL || data == NULL) 
    {
        return;
    }
    ESP_LOGI(TAG, "Received Data from: " MACSTR, MAC2STR(info->src_addr));
    ESP_LOGI(TAG, "Data length: %d", len);
    printf("Data: ");
    for (int i = 0; i < len; i++)
    {
        printf("%02X ", data[i]);  //hex format
    }
    printf("\n");
    uint8_t sensor_notification = data[0];
    switch (sensor_notification)
    {
        case 1: 
            ESP_LOGI(TAG, "Motion Detected!");
            if (!(alarm_tripped)) {
                alarm_tripped = true;
                xTaskCreatePinnedToCore(alarm_task, "Alarm Task", 2048, NULL, 3, &alarm_task_handle, tskNO_AFFINITY);
            }
            break;      
        case 2:
            ESP_LOGI(TAG, "Sensor turned On");      
            break;
        case 3:
            ESP_LOGI(TAG,"Sensor Turned Off");
            break;
        default:
            ESP_LOGI(TAG, "Unknown Message Received");
            break;
    }
}

void alarm_on_setting()//function for alarm activation
{
    alarm_active = true;
    xTaskCreatePinnedToCore(espnow_receive_task, "ESP-NOW Receive Task", 4096, NULL, 2, &espnow_receive_task_handle, 1);
    esp_now_register_recv_cb(on_data_received);
    xTaskCreatePinnedToCore(espnow_send_task, "ESP-NOW Send Task", 4096, NULL, 1, &espnow_send_task_handle, 1);
    ESP_LOGI(TAG,"Alarm on");
}

void alarm_off_setting()//function for alarm de-activation
{ 
    alarm_active = false;
    alarm_tripped = false;
    xTaskCreatePinnedToCore(espnow_send_task, "ESP-NOW Send Task", 4096, NULL, 1, &espnow_send_task_handle, 1);   
    esp_err_t err = esp_now_unregister_recv_cb();
    if(err != ESP_OK)
    {
        ESP_LOGW(TAG, "ESP-NOW Callbacks already unregistered");
    }
    else
    {
    ESP_LOGI(TAG, "ESP-NOW Callbacks now unregistered");
    }
    if (espnow_receive_task_handle != NULL) 
    {
        vTaskDelete(espnow_receive_task_handle);
        espnow_receive_task_handle = NULL;
    }
    gpio_set_level(LED_GPIO_PIN, 0);
    ESP_LOGI(TAG,"Alarm off");
}

void reset_logs() {
    if (xSemaphoreTake(log_mutex, portMAX_DELAY)) //ensures logs do not get corrupted when changing logs and user accessing them
    {
        // Resets logs to empty strings
        memset(detection_log, 0, sizeof(detection_log));
        log_index = 0;
        xSemaphoreGive(log_mutex);
        ESP_LOGI(TAG, "Logs have been reset.");
    }
}



////HTTP HANDLERS



esp_err_t get_handler(httpd_req_t *req)//allows web browser access
{
    char response[1024];
    snprintf(response, sizeof(response), "<html><body><h1>Intruder Logs</h1><ul>");
    for (int i = 0; i < MAX_LOG_ENTRIES; i++) 
    {
        if (strlen(detection_log[i]) > 0) 
        {
            snprintf(response + strlen(response), sizeof(response) - strlen(response),
                     "<li>%s</li>", detection_log[i]);
        }
    }
    snprintf(response + strlen(response), sizeof(response) - strlen(response), "</ul></body></html>");
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t deactivate_handler(httpd_req_t *req) {//deactivate alarm
    alarm_off_setting();
    httpd_resp_send(req, "Alarm Deactivated", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t activate_handler(httpd_req_t *req) {//activate alarm
    alarm_on_setting();
    httpd_resp_send(req, "Alarm Activated", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t logs_handler(httpd_req_t *req)//sends logs as JSON
{
    cJSON *root = cJSON_CreateArray();

    if (xSemaphoreTake(log_mutex, portMAX_DELAY)) 
    {
        for (int i = 0; i < MAX_LOG_ENTRIES; i++) 
        {
            if (strlen(detection_log[i]) > 0) 
            {
                cJSON_AddItemToArray(root, cJSON_CreateString(detection_log[i]));
            }
        }
        xSemaphoreGive(log_mutex);
    }
    char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
    free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t reset_logs_handler(httpd_req_t *req) 
{
    reset_logs();
    httpd_resp_send(req, "Logs Reset", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t get_status_handler(httpd_req_t *req)
{
    if(alarm_active == true)
    {
        httpd_resp_send(req, "Alarm Active", HTTPD_RESP_USE_STRLEN);
    }
    else
    {
        httpd_resp_send(req, "Alarm Inactive", HTTPD_RESP_USE_STRLEN);
    }
    return ESP_OK;
}

static int get_sta_netif_exec(void *arg)//network interface
{
    *(esp_netif_t **)arg = esp_netif_next_unsafe(NULL);
    return ESP_OK; 
}

static esp_netif_t *get_sta_netif(void)//network interface 
{
    esp_netif_t *netif = NULL;
    esp_netif_tcpip_exec(get_sta_netif_exec, &netif);
    if (netif == NULL) 
    {
        ESP_LOGE(TAG, "Failed to get network interface!");
    } 
    else 
    {
        ESP_LOGI(TAG, "Network interface retrieved successfully.");
    }
    return netif;
}

void wait_for_ip() {
    while (true) 
    {
        esp_netif_ip_info_t ip_info;
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        ESP_LOGW("MAIN", "Awaiting IP address");
        vTaskDelay(pdMS_TO_TICKS(100));
        if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0) 
        {
        vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

void web_server_startup() 
{
    ESP_LOGI(TAG, "Starting web server");
    esp_netif_t *netif = get_sta_netif();
    if (netif == NULL) 
    {
        ESP_LOGE(TAG, "Failed to retrieve network interface, aborting web server startup");
        return;
    }
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    httpd_handle_t server = NULL;
    // Starts HTTP server
    esp_err_t err = httpd_start(&server, &config);
    if (err == ESP_OK) 
    {
        ESP_LOGI(TAG, "HTTP server started successfully.");
        // Register URI handlers
        httpd_uri_t activate_uri = {
            .uri = "/activate",
            .method = HTTP_GET,
            .handler = activate_handler
        };
        httpd_uri_t deactivate_uri = {
            .uri = "/deactivate",
            .method = HTTP_GET,
            .handler = deactivate_handler
        };
        httpd_uri_t logs_uri = {
            .uri = "/logs",
            .method = HTTP_GET,
            .handler = logs_handler
        };
        httpd_uri_t reset_logs_uri = {
            .uri = "/reset_logs",
            .method = HTTP_GET,
            .handler = reset_logs_handler
        };
        httpd_uri_t get_status_uri = {
            .uri = "/get_status",
            .method = HTTP_GET,
            .handler = get_status_handler
        };
        httpd_register_uri_handler(server, &activate_uri);
        httpd_register_uri_handler(server, &deactivate_uri);
        httpd_register_uri_handler(server, &logs_uri);
        httpd_register_uri_handler(server, &reset_logs_uri);
        httpd_register_uri_handler(server, &get_status_uri);
        vTaskDelay(pdMS_TO_TICKS(1000));
        // Retrieve and log the IP address of ESP32
        esp_netif_ip_info_t ip_info;
        esp_netif_get_ip_info(netif, &ip_info);
        ESP_LOGI(TAG, "Web Server Started! Access at http://%s", ip4addr_ntoa((const ip4_addr_t *)&(ip_info.ip)));
    } 
    else 
    {
        ESP_LOGE(TAG, "Failed to start web server.");
    }

    ESP_LOGI(TAG, "End of web server startup");
    setup_espnow(sensor_mac);
    alarm_off_setting();
    esp_now_register_send_cb(on_data_send);
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    init_time_sync();
    wait_for_time_sync();
    while (1)//ensures web server task is running 
    {
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}




//Free RTOS tasks //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////





void web_server_task(void *pvParameters)
{
    web_server_startup();
}

void wifi_task(void *pvParameters)//init wifi
{
    esp_err_t err;
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.ampdu_tx_enable = false;
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG, "Wi-Fi initialization failed: %s", esp_err_to_name(err));
        return;
    }
    wifi_config_t wifi_config = 
    {
        .sta = 
        {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .bssid_set = false,
        },
    };
    err = esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG, "Failed to set Wi-Fi config: %s", esp_err_to_name(err));
        return;
    }
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG, "Failed to set Wi-Fi mode: %s", esp_err_to_name(err));
        return;
    }
    err = esp_wifi_start();
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG, "Failed to start Wi-Fi: %s", esp_err_to_name(err));
        return;
    }
    err = esp_wifi_set_channel(wifi_channel, WIFI_SECOND_CHAN_NONE);
    ESP_LOGI(TAG, "Connecting to Wi-Fi");
    err = esp_wifi_connect();
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG, "Failed to connect to Wi-Fi: %s", esp_err_to_name(err));
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
    //Verify WIFI connection
    wifi_ap_record_t ap_info;
    err = esp_wifi_sta_get_ap_info(&ap_info);
    if (err == ESP_OK) 
    {
        ESP_LOGI(TAG, "Connected to %s, RSSI: %d", ap_info.ssid, ap_info.rssi);
    } 
    else 
    {
        ESP_LOGE(TAG, "Failed to get AP info: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "Wi-Fi Initialized and Connected to %s", WIFI_SSID);
    // Set Static IP
    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 192, 168, 1, 200);  //Static IP address
    IP4_ADDR(&ip_info.gw, 192, 168, 1, 1);   // Default gateway
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);//subnet mask
    // Stop DHCP
    if (esp_netif_dhcpc_stop(netif) != ESP_OK) 
    {
        ESP_LOGW(TAG, "DHCP client was not started or already stopped");
    }
    // Assigns static IP
    if (esp_netif_set_ip_info(netif, &ip_info) != ESP_OK) 
    {
        ESP_LOGE(TAG, "Failed to set static IP");
    } 
    else 
    {
        ESP_LOGI(TAG, "Static IP set successfully: " IPSTR, IP2STR(&ip_info.ip));
        xTaskCreatePinnedToCore(web_server_task, "Web Server Task", 8192, NULL, 2, &web_server_task_handle, 1);
    }
    while (1)//monitor Wifi connection
    {
        err = esp_wifi_sta_get_ap_info(&ap_info);
        if (err == ESP_OK) 
        {
            ESP_LOGI(TAG, "WiFi Status: Connected to AP: %s, Signal Strength: %d", ap_info.ssid, ap_info.rssi);
        } 
        else 
        {
            ESP_LOGW(TAG, "WiFi Status: Not connected to any AP.");
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void alarm_task(void *pvParameters)//runs if alarm has been triggered
{
    ESP_LOGI(TAG, "Intruder Detected! Alarm Activated!");
    gettimeofday(&tv, NULL);
    struct tm timeinfo;
    localtime_r(&tv.tv_sec, &timeinfo);
    char timestamp[100];
    // Formatted as "YYYY-MM-DD HH:MM:SS"
    snprintf(timestamp, sizeof(timestamp), 
             "Intruder detected at %04d-%02d-%02d %02d:%02d:%02d", 
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, 
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    add_log_entry(timestamp);
    ESP_LOGI(TAG, "Added entry to logs");
    if (espnow_receive_task_handle != NULL) 
    {
        vTaskDelete(espnow_receive_task_handle);
        espnow_receive_task_handle = NULL;
    }
    esp_now_unregister_recv_cb();
    while (alarm_active == true)//auto deletes itself if alarm is turned off
    {
        gpio_set_level(BUZZER_PIN, 1); 
        vTaskDelay(pdMS_TO_TICKS(1000));
        gpio_set_level(BUZZER_PIN, 0); 
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    gpio_set_level(LED_GPIO_PIN, 0);
    gpio_set_level(BUZZER_PIN, 0);
    ESP_LOGI(TAG, "Alarm Deactivated.");
    vTaskDelete(NULL); 
}

void espnow_receive_task(void *pvParameters)
{
    esp_now_register_recv_cb(on_data_received);
    while (1)
    {
       vTaskDelay(10 / portTICK_PERIOD_MS);
       taskYIELD();//let other tasks run
    }
}

void espnow_send_task(void *pvParameters)
{
    espnow_message_t message;
    if(!(alarm_active))
    {//deactivates the sensor
        message = 1;
        esp_err_t result = esp_now_send(NULL, (uint8_t *)&message, sizeof(message));
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "ESP-NOW Send failed: %s", esp_err_to_name(result));
        }
    }
    else//activates the sensor
    {
        message = 2;
        esp_err_t result = esp_now_send(NULL, (uint8_t *)&message, sizeof(message));
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "ESP-NOW Send failed: %s", esp_err_to_name(result));
        }
    }
    vTaskDelete(NULL);
}

void app_main(void)//setup for main burglar alarm
{
    esp_log_level_set("*", ESP_LOG_DEBUG);
    esp_event_loop_args_t loop_args = {
        .queue_size = 64,//default 32
        .task_name = "event_task",
        .task_priority = 2,
        .task_stack_size = 4096,
        .task_core_id = tskNO_AFFINITY
    };
    esp_event_loop_handle_t event_loop;
    esp_event_loop_create(&loop_args, &event_loop);
    config_io(); 
    setup_nvs();
    vTaskDelay(1000);
    xTaskCreatePinnedToCore(wifi_task,"WIFI Task",4092,NULL,2,&wifi_task_handle,0);
    log_mutex = xSemaphoreCreateMutex();
}
