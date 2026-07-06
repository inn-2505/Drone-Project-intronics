#ifndef CONFIG_H
#define CONFIG_H

#include "driver/gpio.h"

#define WIFI_SSID          "INTRONICS-WIFI-3RD3"      
#define WIFI_PASS          "intronics029391222" 
#define WIFI_CONNECTED_BIT BIT0
// Django Server
#define DJANGO_API_URL     "http://192.168.1.182:8000/api/data/"          

#define UART_PORT_NUM       UART_NUM_1      
#define UART_BAUD_RATE      115200                
#define TXD_PIN             (GPIO_NUM_4)         
#define RXD_PIN             (GPIO_NUM_5)         
#define BUF_SIZE            (1024)        

#define UDP_PORT 12345

#define RED GPIO_NUM_21

#endif