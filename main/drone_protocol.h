#ifndef drone_protocol_H
#define drone_protocol_H

// Define the structure of the drone monitoring packet
typedef enum {
    WAIT_HEADER1,  
    WAIT_HEADER2,
    READ_LENGTH,
    READ_DATA,     
    READ_CHECKSUM  
} state_t;

typedef struct {
    uint8_t flight_mode; // 1 byte
    int32_t latitude;    // 4 bytes
    int32_t longitude;  // 4 bytes
    float altitude;  // 4 bytes
    float speed;      // 4 bytes
    float batt_voltage; // 4 bytes
    uint8_t  batt_percentage; // 1 byte
    
} __attribute__((packed)) monitor_packet_t; //  22 bytes (1+4+4+4+4+4+1=22 bytes)

typedef enum {
    FLIGHT_MODE_LAND   = 1,
    FLIGHT_MODE_TAKEOFF = 2,
    FLIGHT_MODE_CHARGE  = 3,
} drone_mode_t;

static inline const char* get_flight_mode_str(uint8_t mode) {
    switch(mode) {
        case FLIGHT_MODE_LAND :  return "LAND";
        case FLIGHT_MODE_TAKEOFF: return "TAKEOFF";
        case FLIGHT_MODE_CHARGE:  return "CHARGE";
        default:                  return "UNKNOWN";
    }
}

//define the structure of the command drone packet
typedef struct {
    int32_t lat1;       // 4 Byte
    int32_t lon1;       // 4 Byte
    int32_t lat2;       // 4 Byte
    int32_t lon2;       // 4 Byte
    uint16_t altitude;  // 2 Byte
    uint8_t speed;      // 1 Byte
} __attribute__((packed)) drone_command_t; // 19 Byte
#endif