idf_component_register(SRCS "uart.c" "wifi.c" "HTTP.c" "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES driver esp_wifi esp_event nvs_flash esp_http_client esp_timer json)
