#include <stdio.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "HTTP.h"
#include "wifi.h"
#include "config.h"
#include "uart.h"

static const char *TAG = "TIME_DEBUG";

void init_state(){
    //CHECK FOR INITIAL STATE
    gpio_reset_pin(RED);
    gpio_set_direction(RED, GPIO_MODE_OUTPUT);
    gpio_set_level(RED,true);

    vTaskDelay(pdMS_TO_TICKS(1500));
    gpio_set_level(RED, false);

    ESP_LOGI(TAG, "System initialized. Starting Timer...");
}

void app_main(void)
{
    
    init_state();
    // BEGIN NVS Flash
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Wi-Fi & UART
    wifi_init_sta();
    //init_uart();

    // Task UART , HTTP POST
    xTaskCreate(http_sender_task, "http_sender_task", 8192, NULL, 5, NULL); // เพิ่ม Stack Size เป็น 8KB เนื่องจาก HTTP Client ใช้ RAM ค่อนข้างเยอะ
    xTaskCreate(udp_receiver_task, "udp_receiver_task", 4096, NULL, 5, NULL);
}
