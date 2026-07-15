#ifndef CONFIG_H
#define CONFIG_H

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

extern QueueHandle_t http_queue; 
extern QueueHandle_t uart_tx_queue;

#define WIFI_SSID          "INTRONICS-WIFI-3RD3"      
#define WIFI_PASS          "intronics029391222" 
#define WIFI_CONNECTED_BIT BIT0
// Django Server
#define DJANGO_API_URL     "http://192.168.1.166:8000/api/data" 

#define UART_PORT_NUM       UART_NUM_0      
#define UART_BAUD_RATE      115200                
#define TXD_PIN             (GPIO_NUM_16)         
#define RXD_PIN             (GPIO_NUM_17)         
#define BUF_SIZE            (1024)        

#define HEADER1      0xAA
#define HEADER2      0x55

#define DATA_LEN_MONITOR       22       // *** ขนาด data คงที่ ปรับตรงนี้ถ้า format เปลี่ยน ***
#define DATA_LEN_COMMAND       24       // *** ขนาด data คงที่ ปรับตรงนี้ถ้า format เปลี่ยน ***

#define UDP_PORT 1234

#define RED GPIO_NUM_21

#endif