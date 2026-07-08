#include <stdio.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/queue.h"
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
    
    http_queue = xQueueCreate(10, 512);
    if (http_queue == NULL) 
    {
        ESP_LOGE(TAG, "FAILED TO CREATE HTTP QUEUE!");
        return;
    }

     // Create queue to hold up to 5 items of type drone_command_t
    uart_tx_queue = xQueueCreate(5, sizeof(drone_command_t));
    if (uart_tx_queue == NULL) {
        ESP_LOGE(TAG, "FAILED TO CREATE UART TX QUEUE!");
        return;
    }

    // BEGIN NVS Flash
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Wi-Fi & UART
    wifi_init_sta();
    init_uart();

    // Task UART , HTTP POST  
   
    xTaskCreate(http_sender_task, "http_sender_task", 8192, NULL, 5, NULL); // เพิ่ม Stack Size เป็น 8KB เนื่องจาก HTTP Client ใช้ RAM ค่อนข้างเยอะ
    xTaskCreate(udp_receiver_task, "udp_receiver_task", 4096, NULL, ถ, NULL);
    xTaskCreate(tx_task, "uart_tx_task", 1024 * 2, NULL,9, NULL);
    xTaskCreate(rx_task, "uart_rx_task",1024 * 2, NULL,10,NULL);
}
