#include "sim.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "config.h"
#include "uart.h"
#include "drone_protocol.h"
#include <string.h>

// อ้างอิงชื่อ TAG และพอร์ตให้ตรงกับโปรเจกต์ของคุณ
static const char *TAG = "DRONE_SIM";
void drone_sim_task(void *pvParameters)
{
    uint8_t sim_counter = 0;
    
    // ขนาดโครงสร้างข้อมูลจริง = 22 Byte
    size_t data_len = sizeof(monitor_packet_t); 
    // ขนาดรวมทั้งแพ็กเกจ = 2 + 1 + 22 + 1 = 26 Byte
    size_t total_length = 2 + 1  + data_len + 1; 
    
    uint8_t tx_buffer[total_length];

    ESP_LOGI(TAG, "🤖 [UART SIM] Drone Simulator Started! (22-Byte Struct Mode)");
    int i = 1;
    while (1) {
        sim_counter++;

        // 1. 📝 สร้างและหยอดข้อมูลจำลองลงโครงสร้าง 22 ไบต์ของคุณ
        monitor_packet_t sim_packet;
        sim_packet.flight_mode = i;
        i++;
        sim_packet.latitude        = (int32_t)(13.756300 * 1000000);
        sim_packet.longitude       = (int32_t)(100.501800 * 1000000);
        sim_packet.altitude        = 50 + (sim_counter % 5); // ความสูงขยับ 50-54 เมตร
        sim_packet.speed           = 20 + (sim_counter % 3); // ความเร็วขยับ 20-22 km/h
        
        // 🌟 ใส่ค่าแบตเตอรี่แบบ float ตรงๆ เบสิคตามที่คุณต้องการ
        // จำลองสถานการณ์แบตเตอรี่ค่อยๆ ลดลงทีละนิด
        sim_packet.batt_voltage    = 12.6f - (sim_counter * 0.01f); 
        sim_packet.batt_percentage = 100 - (sim_counter % 10);

        // 2. 📦 บรรจุลงสายพานส่งข้อมูล (tx_buffer)
        tx_buffer[0] = HEADER1; // ช่อง 0 ยัด Header
        tx_buffer[1] = HEADER2; // ช่อง 1 ยัด Header
        tx_buffer[2] = data_len; // ช่อง 2 ยัด Payload Length (22 Byte)
        
        // ช่อง 2 ถึง 23 ก๊อปปี้ข้อมูลสตรัคลงไป
        memcpy(&tx_buffer[3], &sim_packet, data_len); 

        // 3. 🧮 คำนวณ Checksum ทั้งก้อน (Header 2 Byte + Data 22 Byte = 24 Byte)
        size_t check_len = total_length - 1; 
        uint8_t checksum_val = calc_checksum(check_len, tx_buffer);
        
        // ช่องที่ 25 (ตัวสุดท้าย) ยัด Checksum ปิดท้าย
        tx_buffer[total_length - 1] = checksum_val;

        // 4. ปริ้นต์ Log ตรวจสอบความถูกต้องก่อนยิงออก
        ESP_LOGI(TAG, "[SIM TX] Sending -> Mode: %d | Volt: %.2fV | Checksum: 0x%02X", 
                 sim_packet.flight_mode, 
                 sim_packet.batt_voltage, 
                 tx_buffer[total_length - 1]);

        // 5. 🚀 ยิงออกพอร์ต UART ทั้งหมด 26 ไบต์รวดเดียว
        uart_write_bytes(UART_PORT_NUM, (const char *)tx_buffer, total_length);

        
        vTaskDelay(pdMS_TO_TICKS(200));
        if (i > 3) {
            i = 1;
        }
    }
}