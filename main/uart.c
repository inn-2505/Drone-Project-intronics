#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include "string.h"
#include <stdio.h>
#include "esp_event.h"
#include "driver/gpio.h"
#include "esp_rom_gpio.h"
#include "config.h"

static const char *TAG = "UART_CONNECTING";

void init_uart(void) {
    const uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    //  Driver & PIN CONFIGURATION
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_LOGI(TAG, "CONFIGURATION UART ALREADY (Baud rate: %d)", UART_BAUD_RATE);
}

// Send a UART message
static void tx_task()
{
    int tx_bytes = 0;
    char *tx_data = "Hello from ESP32 ...\r\n"; // String to be send
    int tx_length = strlen(tx_data);

    while (1)
    {
        // Transmit string with number of transmited bytes (-1 is an error)
        tx_bytes = uart_write_bytes(UART_PORT_NUM, tx_data, tx_length); 
        printf("Transmit (%d bytes): %s", tx_bytes, tx_data);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

