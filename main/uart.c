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

static drone_telemetry_t drone_data = {0};
void parse_msp_packet(uint8_t cmd, uint8_t *payload, uint8_t size) {
    if (cmd == MSP_RAW_GPS) {
        // MSP_RAW_GPS payload structure:
        // Byte 0: Fix status (1 byte)
        // Byte 1: Num Satellites (1 byte)
        // Byte 2-5: Latitude (4 bytes int32, scaled by 10,000,000)
        // Byte 6-9: Longitude (4 bytes int32, scaled by 10,000,000)
        // Byte 10-11: Altitude (2 bytes uint16, meters)
        // Byte 12-13: Speed (2 bytes uint16, cm/s)
        
        if (size >= 14) {
            int32_t raw_lat = (int32_t)(payload[2] | (payload[3] << 8) | (payload[4] << 16) | (payload[5] << 24));
            int32_t raw_lon = (int32_t)(payload[6] | (payload[7] << 8) | (payload[8] << 16) | (payload[9] << 24));
            uint16_t raw_alt = (uint16_t)(payload[10] | (payload[11] << 8));
            uint16_t raw_speed = (uint16_t)(payload[12] | (payload[13] << 8));
            
            drone_data.latitude = raw_lat / 10000000.0;
            drone_data.longitude = raw_lon / 10000000.0;
            drone_data.altitude = (float)raw_alt; // Altitude in meters
            drone_data.speed = raw_speed / 100.0f; // Convert cm/s to m/s
        }
    } 
    
}



// Helper function to send MSP command to Flight Controller
void send_msp_command(uint8_t cmd, uint8_t *payload, uint8_t size) {
    uint8_t header[5];
    uint8_t checksum = 0;
    
    // MSP V1 Header format: '$', 'M', '<' (indicating command sent to FC)
    header[0] = '$';
    header[1] = 'M';
    header[2] = '<';
    header[3] = size;
    header[4] = cmd;
    
    // Calculate XOR checksum starting with size and command ID
    checksum = size ^ cmd;
    
    // XOR payload data bytes into checksum
    for (int i = 0; i < size; i++) {
        checksum ^= payload[i];
    }
    
    // Write header, payload, and checksum to UART TX
    uart_write_bytes(UART_PORT_NUM, (const char *)header, 5);
    if (size > 0 && payload != NULL) {
        uart_write_bytes(UART_PORT_NUM, (const char *)payload, size);
    }
    uart_write_bytes(UART_PORT_NUM, (const char *)&checksum, 1);
}

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
void tx_task(void *pvParameters)
{   
    drone_command_t received_cmd;
    uint8_t msp_payload[13]; // Size: 3 floats (4*3 = 12 bytes) + 1 byte arm state = 13 bytes
    ESP_LOGI(TAG, "UART Transmitter Task is waiting for command queue...");
    while (1)
    {
        // Blocks and waits for commands in the queue
        if (xQueueReceive(uart_tx_queue, &received_cmd, portMAX_DELAY) == pdPASS) {
            ESP_LOGI(TAG, "Received command from queue! Forwarding to Drone...");
            // Serialize target coordinates and arm state into binary payload (Little-Endian)
            // Latitude (Bytes 0-3)
            memcpy(&msp_payload[0], &received_cmd.latitude, 4);
            // Longitude (Bytes 4-7)
            memcpy(&msp_payload[4], &received_cmd.longitude, 4);
            // Altitude (Bytes 8-11)
            memcpy(&msp_payload[8], &received_cmd.altitude, 4);
            // Arm State (Byte 12)
            msp_payload[12] = received_cmd.arm_state;

            // Transmit the command via MSP packet over UART
            send_msp_command(MSP_SET_DESTINATION, msp_payload, 13);
            ESP_LOGI(TAG, "MSP Command Sent (ID: %d, Payload Size: 13 bytes)", MSP_SET_DESTINATION);
        }     
    }
}

void rx_task(void* pvParameters)
{
    esp_log_level_set(TAG ,ESP_LOG_INFO);
    uint8_t byte_in;

    // Local state variables for parsing MSP packets
    msp_parser_state_t state = MSP_IDLE;
    uint8_t msp_size = 0;
    uint8_t msp_cmd = 0;
    uint8_t msp_payload[256];
    uint8_t msp_payload_idx = 0;
    uint8_t calculated_checksum = 0;

    char json_payload[512];
    
    ESP_LOGI("UART_RX", "Betaflight MSP Parser started...");

    
    while(1){
        // Read incoming bytes one by one from UART buffer (with 10ms timeout)
        int len = uart_read_bytes(UART_PORT_NUM, &byte_in, 1, pdMS_TO_TICKS(10));
        if (len > 0) {
            // State machine to parse MSP packet structure
            switch(state) {
                case MSP_IDLE:
                    if (byte_in == '$') {
                        state = MSP_HEADER_START;
                    }
                    break;
                    
                case MSP_HEADER_START:
                    if (byte_in == 'M') {
                        state = MSP_HEADER_M;
                    } else {
                        state = MSP_IDLE; // Invalid header start, reset
                    }
                    break;
                    
                case MSP_HEADER_M:
                    if (byte_in == '>') { // Confirm incoming response from FC to ESP32
                        state = MSP_HEADER_ARROW;
                    } else {
                        state = MSP_IDLE;
                    }
                    break;
                    
                case MSP_HEADER_ARROW:
                    msp_size = byte_in;
                    calculated_checksum = byte_in; // Start XOR checksum calculation
                    msp_payload_idx = 0;
                    state = MSP_HEADER_SIZE;
                    break;
                    
                case MSP_HEADER_SIZE:
                    msp_cmd = byte_in;
                    calculated_checksum ^= byte_in; // XOR command ID into checksum
                    if (msp_size == 0) {
                        state = MSP_CHECKSUM; // Jump directly to checksum verification if payload is empty
                    } else {
                        state = MSP_PAYLOAD;
                    }
                    break;
                    
                case MSP_PAYLOAD:
                    msp_payload[msp_payload_idx] = byte_in;
                    calculated_checksum ^= byte_in; // XOR payload bytes into checksum
                    msp_payload_idx++;
                    
                    if (msp_payload_idx >= msp_size) {
                        state = MSP_CHECKSUM;
                    }
                    break;
                    
                case MSP_CHECKSUM:
                    if (byte_in == calculated_checksum) {
                        // Checksum matches. Decode payload binary content.
                        parse_msp_packet(msp_cmd, msp_payload, msp_size);
                        
                        // Once new GPS coordinate is received, assemble telemetry into a JSON payload and queue it
                        if (msp_cmd == MSP_RAW_GPS) {
                            snprintf(json_payload, sizeof(json_payload),
                                     "{"
                                     "\"flight_mode\":\"AUTO\","
                                     "\"latitude\":%.7f,"
                                     "\"longitude\":%.7f,"
                                     "\"altitude\":%.1f,"
                                     "\"speed\":%.2f"
                                     "}",
                                     drone_data.latitude,
                                     drone_data.longitude,
                                     drone_data.altitude,
                                     drone_data.speed
                                    );
                            
                            // Send JSON string to HTTP Queue (non-blocking, drops packet if queue is full after 10ms)
                            if (xQueueSend(http_queue, json_payload, pdMS_TO_TICKS(10)) != pdPASS) {
                                ESP_LOGW("UART_RX", "HTTP queue full, drop telemetry!");
                            }
                        }
                    } else {
                        ESP_LOGW("UART_RX", "MSP Checksum Error!");
                    }
                    state = MSP_IDLE; // Reset parser state for next packet
                    break;
            }
        }
        
        // Yield CPU time if there is no data on UART
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

