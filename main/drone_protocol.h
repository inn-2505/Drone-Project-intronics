#ifndef drone_protocol_H
#define drone_protocol_H
#include <string.h>
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
    uint8_t command;     // 1 Byte
    int32_t lat1;       // 4 Byte
    int32_t lon1;       // 4 Byte
    int32_t lat2;       // 4 Byte
    int32_t lon2;       // 4 Byte
    uint16_t altitude;  // 2 Byte
    uint8_t speed;      // 1 Byte
    uint8_t throttle;   // 1 Byte
    uint8_t yaw;        // 1 Byte
    uint8_t pitch;      // 1 Byte
    uint8_t roll;       // 1 Byte
} __attribute__((packed)) drone_command_t; // 24 Byte

typedef enum {
    COMMAND_MODE_ARM   = 1,
    COMMAND_MODE_DISARM = 2,
    COMMAND_MODE_EMERGENCY  = 3,
} command_mode_t;

static inline uint8_t command_str_to_mode(const char *cmd_str) {
    if (strcmp(cmd_str, "ARM") == 0)       return COMMAND_MODE_ARM;
    if (strcmp(cmd_str, "DISARM") == 0)    return COMMAND_MODE_DISARM;
    if (strcmp(cmd_str, "EMERGENCY") == 0) return COMMAND_MODE_EMERGENCY;
    return 0; // ไม่รู้จัก / ค่า default
}

#endif