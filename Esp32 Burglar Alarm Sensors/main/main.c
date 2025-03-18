#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_now.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h" 

#define LED_GPIO_PIN 2
#define IR_SENSOR 26 //Uses HC-SR501 PIR Infrared Motion Detection Sensor

bool activate_sensor = false;
bool movement_dectected = false;
bool sensor_warm = false;

uint8_t wifi_channel = 12;

uint8_t burglar_alarm_mac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const char *TAG = "MAIN";

void espnow_receive_task(void *pvParameters);
void espnow_send_task(void *pvParameters);
void wifi_task(void *pvParameters);
void sensor_deactivate_task(void *pvParameters);
void sensor_active_task(void *pvParameters);

TaskHandle_t espnow_send_task_handle;
TaskHandle_t espnow_receive_task_handle;
TaskHandle_t sensor_active_task_handle;
TaskHandle_t sensor_deactivate_task_handle;

void config_io()//configure sensor and built-in LED
{
    gpio_reset_pin(LED_GPIO_PIN);
    gpio_set_direction(LED_GPIO_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO_PIN, 0);
    gpio_set_direction(IR_SENSOR, GPIO_MODE_INPUT);
    ESP_LOGI(TAG, "ESP32 configured.");
}

void setup_nvs()//setups non volatile storage - needed for wifi and ESP-NOW
{
    esp_log_level_set("*", ESP_LOG_DEBUG);
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) 
    {   // erase NVS corruption
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG, "NVS initialization failed: %s", esp_err_to_name(err));
        return;
    }
}

void add_peer_device(uint8_t *macAddress) 
{
    esp_wifi_get_channel(&wifi_channel, NULL);
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, macAddress, 6);
    peerInfo.channel = wifi_channel;
    peerInfo.ifidx = ESP_IF_WIFI_STA;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
    if (!esp_now_is_peer_exist(macAddress))
    {
        ESP_LOGI(TAG, "Peer not found. Attempting to add");
        int retry_count = 20;
        while (retry_count > 0)
        {
            ESP_LOGI(TAG, "Attempting to add peer on channel: %d", peerInfo.channel);
            esp_err_t err = esp_now_add_peer(&peerInfo);
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
        ESP_LOGI(TAG, "Peer added sucessfully");
    }
}

esp_err_t setup_espnow(uint8_t *macAddress)//setup for ESP-NOW
{
    esp_err_t err = esp_now_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "ESP-NOW initialization failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "ESP-NOW initialized successfully");
    gpio_set_level(LED_GPIO_PIN, 1);
    ESP_LOGI(TAG, "Current Wi-Fi Channel: %d", wifi_channel);
    add_peer_device(macAddress);
    return ESP_OK;
}

void setup_wifi() // Needed for ESP-NOW
{
    esp_err_t err;
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.ampdu_tx_enable = false;  // Disable AMPDU - not needed for ESP-NOW
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG, "Wi-Fi initialization failed: %s", esp_err_to_name(err));
        return;
    }
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG, "Failed to set Wi-Fi mode: %s", esp_err_to_name(err));
        return;
    }
    // Start Wi-Fi - without connecting to network
    err = esp_wifi_start();
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG, "Failed to start Wi-Fi: %s", esp_err_to_name(err));
        return;
    }
    err = esp_wifi_set_channel(wifi_channel, WIFI_SECOND_CHAN_NONE);
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG, "Failed to set Wi-Fi channel: %s", esp_err_to_name(err));
        return;
    }
    setup_espnow(burglar_alarm_mac);
}

void on_data_received(const esp_now_recv_info_t *info, const uint8_t *data, int len)//ESP-NOW receive information
{
    uint8_t command = data[0];
    switch (command)
    {
        case 1:
            xTaskCreatePinnedToCore(sensor_deactivate_task, "Sensor Deactivate Task",4096, NULL,1, &sensor_deactivate_task_handle, tskNO_AFFINITY);
            ESP_LOGI(TAG, "Sensor deactivated");
            break;    
        case 2:
            xTaskCreatePinnedToCore(sensor_active_task, "Sensor Activate Task",4096, NULL,1, &sensor_active_task_handle, tskNO_AFFINITY);
            ESP_LOGI(TAG, "Sensor activated");
            break;
        default:
            ESP_LOGI(TAG, "Unrecognised command");
            break;

    }
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




//FreeRTOS tasks




void sensor_active_task(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_now_register_send_cb(on_data_send);
    int sensor_value = 0;
    if(sensor_warm == true)
    {
        while (1) {
            sensor_value = gpio_get_level(IR_SENSOR);
            ESP_LOGI(TAG, "Sensor Value: %d", sensor_value);
            if(sensor_value == 1)
            {
                ESP_LOGI(TAG, "Movement Detected!");
                gpio_set_level(LED_GPIO_PIN, 1); 
                movement_dectected = true;
                xTaskCreatePinnedToCore(espnow_send_task, "ESP-NOW Send Task", 4096, NULL, 4, &espnow_send_task_handle, tskNO_AFFINITY);
                vTaskDelay(pdMS_TO_TICKS(60000));  // Wait 60 seconds to reset
                if(gpio_get_level(IR_SENSOR) == 0)
                {
                    ESP_LOGI(TAG, "Sensor reset - No movement detected.");
                    gpio_set_level(LED_GPIO_PIN, 0);
                    movement_dectected = false;
                }
            }
            else
            {
                ESP_LOGI(TAG, "No movement");
                gpio_set_level(LED_GPIO_PIN, 0); 
                movement_dectected = false;
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    sensor_active_task_handle = NULL;
    vTaskDelete(NULL);
}

void wifi_task(void *pvParameter)////init wifi and ensures connection
{
    setup_wifi();
    while(1)
    {
        vTaskDelay(1000);
    }
}

void sensor_deactivate_task(void *pvParameters)
{
    esp_now_unregister_send_cb();
    xTaskCreatePinnedToCore(espnow_send_task, "ESP-NOW Send Task", 4096, NULL, 4, &espnow_send_task_handle, tskNO_AFFINITY);
    if((espnow_send_task_handle != NULL))
    {
        vTaskDelete(sensor_active_task_handle);
        sensor_active_task_handle = NULL;
    }
    vTaskDelete(NULL);
}

void espnow_receive_task(void *pvParameters)
{
    esp_now_register_recv_cb(on_data_received);
    while (1)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
        taskYIELD();
    }
}

void espnow_send_task(void *pvParameters)
{
    uint8_t cmd;
    if(movement_dectected == true)
    {
        cmd = 1;
        esp_now_send(NULL,&cmd,1);
        movement_dectected = false;
    }
    else if(activate_sensor == true)
    {
        cmd = 2;
        esp_now_send(NULL,&cmd,1);
    }
    else
    {
        cmd = 3;
        esp_now_send(NULL,&cmd,1);
    }
    ESP_LOGI(TAG, "Sent CMD %d", cmd);
    vTaskDelete(NULL);
}

void app_main(void)
{
    config_io();
    setup_nvs();
    xTaskCreate(wifi_task, "wifi_task", 4096, NULL, 5, 0);
    vTaskDelay(1000);
    xTaskCreatePinnedToCore(espnow_receive_task, "ESP-NOW Receive Task", 4096, NULL, 4, &espnow_receive_task_handle, 0);
    ESP_LOGI(TAG,"Warming Up Sensor");
    vTaskDelay(pdMS_TO_TICKS(60000));//sensor needs 1 minute to activate
    sensor_warm = true;
}