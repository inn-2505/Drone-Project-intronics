#ifndef CONFIG_H
#define CONFIG_H

#include "driver/gpio.h"
#include "freertos/queue.h"

#define WIFI_SSID          "INTRONICS-WIFI-3RD3"      
#define WIFI_PASS          "intronics029391222" 
#define WIFI_CONNECTED_BIT BIT0

// Django Server
#define DJANGO_API_URL     "http://192.168.1.196:8000/api/data/"          
#define UDP_PORT           1234

#define UART_PORT_NUM      UART_NUM_0      
#define UART_BAUD_RATE     115200                
#define TXD_PIN            (GPIO_NUM_16)         
#define RXD_PIN            (GPIO_NUM_17)         
#define BUF_SIZE           (1024)        

#define RED                GPIO_NUM_21

#define MSP_RAW_GPS         106  
#define MSP_SET_DESTINATION 250  

// ตัวแปรระบบคิว (Queue) สำหรับแชร์ข้ามไฟล์
extern QueueHandle_t http_queue;
extern QueueHandle_t uart_tx_queue;

// Shared telemetry structure for storing current drone data
typedef struct {
    double latitude;
    double longitude;
    float altitude;
    float speed; 
} drone_telemetry_t;

// Struct for holding commands sent to the drone
typedef struct {
    float latitude;      // Target Latitude
    float longitude;     // Target Longitude
    float altitude;      // Target Altitude (in meters)
    uint8_t arm_state;   // 1 to ARM, 0 to DISARM
} drone_command_t;

// Parser state machine definitions for MSP V1 protocol
typedef enum {
    MSP_IDLE,
    MSP_HEADER_START,  // Found '$'
    MSP_HEADER_M,      // Found 'M'
    MSP_HEADER_ARROW,  // Found '>'
    MSP_HEADER_SIZE,   // Payload size
    MSP_HEADER_CMD,    // Command ID
    MSP_PAYLOAD,       // Extracting binary payload data
    MSP_CHECKSUM       // Verifying XOR checksum
} msp_parser_state_t;

#endif // CONFIG_H