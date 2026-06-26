idf_component_register(SRCS "bili_http.c"
                    INCLUDE_DIRS "." 
                    REQUIRES json nvs_flash spiffs esp_http_client esp_wifi espressif__esp_new_jpeg lvgl__lvgl)
