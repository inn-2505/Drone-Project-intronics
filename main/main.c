#include <stdio.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/queue.h"
#include <string.h>
#include "HTTP.h"
#include "wifi.h"
#include "config.h"
#include "uart.h"
#include "drone_protocol.h"
#include "sim.h"

#include <assert.h>

static const char *TAG = "TIME_DEBUG";

QueueHandle_t http_queue = NULL;
QueueHandle_t uart_tx_queue = NULL;

void run_server_simulation_test(void) {
    ESP_LOGI("TEST", "========================================");
    ESP_LOGI("TEST", "🧪 RUNNING SERVER COMMAND TRANSMIT TEST...");
    ESP_LOGI("TEST", "========================================");

    // 1. 📦 สร้างและหยอดข้อมูลใส่ Struct คำสั่ง (19 Byte) ตรงๆ ตามที่คุณบอก
    
    const char *mock_json = "{"
                            "\"command\": \"ARM\","
                            "\"lat_1\": 13.756300,"
                            "\"long_1\": 100.501800,"
                            "\"lat_2\": 14.123456,"
                            "\"long_2\": 101.987654,"
                            "\"altitude\": 100,"
                            "\"speed\": 25"
                            ",\"throttle\": 50,"
                            "\"yaw\": 10,"
                            "\"pitch\": 5,"
                            "\"roll\": 0"
                            "}";
    ESP_LOGI("TEST", "Sending prepared JSON string directly to your transmitter...");

    // 2. 🚀 ยื่น Struct คำสั่งนี้ให้ฟังก์ชันท่อส่งหลักของคุณจัดการส่งออก
    // (เปลี่ยนชื่อฟังก์ชันตรงนี้ให้ตรงกับฟังก์ชันที่ระบบของคุณใช้ยิงคำสั่งออกไปนะครับ)
    convert2uart(mock_json); 
    ESP_LOGI("TEST", "✅ convert2uart executed. Check UART TX Task logs to see the output bytes!");
    ESP_LOGI("TEST", "========================================");
}
void init_state()
{
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
    
    uart_tx_queue = xQueueCreate(10, sizeof(uint8_t) * DATA_LEN_COMMAND);
    http_queue = xQueueCreate(10, 256);

    // สั่งจองขนาดคิวให้เก็บสตริง JSON 512 ไบต์ ได้สูงสุด 10 คิว
    if (http_queue == NULL) {
        ESP_LOGE(TAG, "FAILED TO CREATE HTTP QUEUE!");
        return;
    }
   
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
    ESP_LOGI("MAIN", "Initialization finished.");
    
    // Task UART , HTTP POST  
   
    xTaskCreate(http_sender_task, "http_sender_task", 8192, NULL, 5, NULL); // เพิ่ม Stack Size เป็น 8KB เนื่องจาก HTTP Client ใช้ RAM ค่อนข้างเยอะ
    xTaskCreate(udp_receiver_task, "udp_receiver_task", 4096, NULL, 5, NULL);
    xTaskCreate(tx_task, "uart_tx_task", 8192, NULL,5, NULL);
    xTaskCreate(rx_task, "uart_rx_task",8192, NULL,6,NULL);
    xTaskCreate(drone_sim_task, "drone_simulator", 4096, NULL, 2, NULL);
    // run_server_simulation_test() ;
}
