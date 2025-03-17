idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_wifi esp_driver_gpio nvs_flash esp_http_server lwip esp_netif json
                       ) 
