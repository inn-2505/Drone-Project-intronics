#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include <string.h>
#include <stdio.h>
#include "esp_event.h"
#include "driver/gpio.h"
#include "esp_rom_gpio.h"
#include "config.h"
#include <stdint.h>
#include "cJSON.h"
#include "drone_protocol.h"

static const char *TAG = "UART_CONNECTING";

uint8_t calc_checksum(uint8_t len, const uint8_t *data)
{
    uint8_t chk = len;
    for (int i = 0; i < len; i++) chk ^= data[i];
    return chk;
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

void uart_send(const uint8_t *data, size_t length)
{
    //  Header (2 Byte) + Data (24 Byte) = 25 Byte
    size_t packet_len = 2 + length;
    uint8_t packet[packet_len];

    packet[0] = HEADER1;
    packet[1] = HEADER2;
    memcpy(&packet[2], data, length);
    uint8_t checksum = calc_checksum(packet_len, packet);

    uart_write_bytes(UART_PORT_NUM, (const char *)packet, packet_len); // ยิง Header + Data
    uart_write_bytes(UART_PORT_NUM, (const char *)&checksum, 1);       // ยิง Checksum ปิดท้าย
}
// Send a UART message

void tx_task(void *pvParameters)
{   
    uint8_t payload[DATA_LEN_COMMAND]; // ขนาด 24 Byte (Command Packet)
    ESP_LOGI(TAG, "uart_tx_queue Task is running and waiting for Queue...");
    
    while (1) {
        // รอรับข้อความในคิว
        if (xQueueReceive(uart_tx_queue, payload, portMAX_DELAY) == pdPASS) {
            uint8_t command;
            int32_t lat1, lon1, lat2, lon2;
            uint16_t alt;
            uint8_t speed, throttle, yaw, pitch, roll;
            command = payload[0]; 
            memcpy(&lat1,  &payload[1],  4);
            memcpy(&lon1,  &payload[5],  4);
            memcpy(&lat2,  &payload[9],  4);
            memcpy(&lon2,  &payload[13], 4);
            memcpy(&alt,   &payload[17], 2);
            speed = payload[19]; 
            throttle = payload[20];
            yaw = payload[21];
            pitch = payload[22];
            roll = payload[23];


            // หาร 1,000,000.0 เพื่อให้กลับเป็นทศนิยม
            ESP_LOGI("UART_TX", "Sending Data -> P1: (%.6f, %.6f) | P2: (%.6f, %.6f) | Alt: %u m | Spd: %u km/h | Th: %u, Y: %u, P: %u, R: %u",
         lat1 / 1000000.0, lon1 / 1000000.0, 
         lat2 / 1000000.0, lon2 / 1000000.0, 
         alt, speed, throttle, yaw, pitch, roll);

            
            uart_send(payload, DATA_LEN_COMMAND);       
        }
    }
    
    vTaskDelete(NULL);
}

void convert2json(const uint8_t *data, char *json_buffer, size_t buffer_size)
{
    // แปลง byte array ดิบ ให้เป็น struct
    monitor_packet_t parsed;
    memcpy(&parsed, data, sizeof(monitor_packet_t));

    // ประกอบเป็น JSON string
    snprintf(json_buffer, buffer_size,
             "{"
             "\"flight_mode\":\"%s\","
             "\"latitude\":%.6f,"
             "\"longitude\":%.6f,"
             "\"altitude\":%.2f,"
             "\"speed\":%.2f,"
             "\"battery_voltage\":%.2f,"
             "\"battery_percentage\":%u"
             "}",
             get_flight_mode_str(parsed.flight_mode),
             parsed.latitude / 1000000.0,
             parsed.longitude / 1000000.0,
             parsed.altitude,
             parsed.speed,
             parsed.batt_voltage,
             parsed.batt_percentage);
 
}
 

/* เรียกเมื่อ parse packet สำเร็จ (checksum ตรง) */
void on_packet(const uint8_t *data,size_t length)
{
    ESP_LOGI(TAG, "Got packet:");
    for (int i = 0 ; i < length; i++) { 
        printf("%02X ", data[i]);  
    }
    printf("\n");
    char json_payload[256];
    convert2json(data, json_payload, sizeof(json_payload));

        //  ส่งเข้า queue เดิม ให้ http_sender_task ไปยิง HTTP POST ต่อ
    if (xQueueSend(http_queue, json_payload, pdMS_TO_TICKS(10)) != pdPASS) {
        ESP_LOGW(TAG, "http_queue full, telemetry dropped!");
    }
}


void rx_task(void *pvParameters)
{   
    ESP_LOGI(TAG, "rx_task started successfully!");
    state_t state = WAIT_HEADER1;
    
    size_t length = sizeof(monitor_packet_t) + 3; // 22 bytes (data) + 2 byte (header)  + 1 byte (checksum)
    static uint8_t buf[256];
    uint8_t idx = 0;
    uint8_t byte_in;


     while (1) {
        if (uart_read_bytes(UART_PORT_NUM, &byte_in, 1, pdMS_TO_TICKS(20)) > 0) {
            switch (state) {
 
                case WAIT_HEADER1:
                    if (byte_in == HEADER1) {
                        idx = 0;
                        buf[idx] = byte_in; 
                        idx++;
                        state = WAIT_HEADER2;
                    }
                    
                    break;
                case WAIT_HEADER2:
                    if (byte_in == HEADER2) {
                        buf[idx] = byte_in; 
                        idx++;
                        state = READ_LENGTH;
                    } // ถ้าไม่ตรง header ก็แค่ทิ้ง byte นี้ วนอ่านตัวถัดไปเรื่อยๆ
                    break;

                case READ_LENGTH:
                    if (byte_in == DATA_LEN_MONITOR) { 
                        buf[idx] = byte_in;
                        idx++;
                        state = READ_DATA;
                    }
                    break;
 
                case READ_DATA:
                    buf[idx] = byte_in;
                    idx++;
                    if (idx >= length-1) {      // อ่านถึงdata
                        state = READ_CHECKSUM;
                    }
                    break;
 
                case READ_CHECKSUM:
                    buf[idx] = byte_in;
                    int calculated_checksum = calc_checksum(length-1, buf);
                    if (byte_in == calculated_checksum) {
                        monitor_packet_t data_packet;

                        // ข้าม Header 
                        memcpy(&data_packet, &buf[3], sizeof(monitor_packet_t));
                        on_packet((const uint8_t *)&data_packet, sizeof(monitor_packet_t));   //มีแค่ data (22 bytes) ไม่รวม Header LENGTH และ Checksum
                    } else {
                        ESP_LOGW(TAG, "checksum mismatch, dropped");
                        ESP_LOGW("RX_DEBUG", "Calculated: 0x%02X | Packet Expected: 0x%02X", 
                                calculated_checksum, 
                                byte_in); // เช็กว่ามันหยิบถูกช่องไหม
                    }
                    state = WAIT_HEADER1;          // กลับไปรอ packet ใหม่เสมอ
                    break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

