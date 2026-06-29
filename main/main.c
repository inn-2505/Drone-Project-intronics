#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "driver/uart.h"
#include "esp_log.h"

#define UART_PORT      UART_NUM_1
#define UART_TX_PIN    17   // ขา TX ของ ESP32 -> RX ของ STM32
#define UART_RX_PIN    16   // ขา RX ของ ESP32 <- TX ของ STM32
#define UART_BUF_SIZE  256

void uart_init(void) {
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    uart_param_config(UART_PORT, &uart_config);
    uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_PORT, UART_BUF_SIZE * 2, 0, 0, NULL, 0);
}

void uart_receive_task(void *arg) {
    uint8_t data[UART_BUF_SIZE];

    while (1) {
        int len = uart_read_bytes(UART_PORT, data, UART_BUF_SIZE - 1, pdMS_TO_TICKS(1000));
        if (len > 0) {
            data[len] = '\0';  // ปิด string ให้ถูกต้อง
            ESP_LOGI("UART", "Received: %s", data);

            float temperature, humidity;
            // แยกค่าจาก string "25.30,60.50"
            if (sscanf((char*)data, "%f,%f", &temperature, &humidity) == 2) {
                send_to_db(temperature, humidity);  // ฟังก์ชันที่เคยเขียนไว้ก่อนหน้า
            }
        }
    }
}
void wifi_init_sta(void) {
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "YOUR_SSID",
            .password = "YOUR_PASSWORD",
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();
}
void app_main(void)
{
    wifi_init_sta();
    uart_init();
    xTaskCreate(uart_receive_task, "uart_rx", 4096, NULL, 5, NULL);
}
