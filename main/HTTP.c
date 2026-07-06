#include "HTTP.h"
#include "config.h"       
#include <string.h>
#include <sys/param.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include <stdio.h>

#include "cJSON.h"

static const char *TAG = "NETWORKCONTROL";

char flight_mode[16] = "GUIDED";
char current_status[32] = "READY";
char msg_timestamp[32] = "2026-07-03:15.00.00";

// HTTP Event Handler 
static esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        default:
            break;
    }
    return ESP_OK;
}
//HTTP POST
void send_http_post(const uint8_t *data, int len) {
    esp_http_client_config_t config = {
        .url = DJANGO_API_URL,
        .event_handler = _http_event_handler,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 5000,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    
    // SETTING Content-Type TO binary payload OR application/json (ขึ้นอยู่กับการรับข้อมูลของ Django)
    esp_http_client_set_header(client, "Content-Type", "application/json");
    
    // Payload
    esp_http_client_set_post_field(client, (const char *)data, len);
    // HTTP POST Request
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "SEND HTTP POST SUCCCESS! Status Code: %d", status_code);
    } else {
        ESP_LOGE(TAG, "SEND HTTP POST FAILED: %s", esp_err_to_name(err));
    }
    //CLEAR RAM HTTP Client
    esp_http_client_cleanup(client);
}



// Task READ FROM UART AND HTTP POST
void http_sender_task(void *pvParameters) {
    // uint8_t *rx_data = (uint8_t *) malloc(BUF_SIZE);
    // if (rx_data == NULL) {
    //     ESP_LOGE(TAG, "FULL MEMORY: Failed to allocate memory for UART buffer");
    //     vTaskDelete(NULL);
    // }
    // while (1) {
    //     // READ FROM UART2 (WAIT FOR 100ms)
    //     int len = uart_read_bytes(UART_PORT_NUM, rx_data, BUF_SIZE, 100 / portTICK_PERIOD_MS);
        
    //     if (len > 0) {
    //         ESP_LOGI(TAG, "READ FROM UART: %d bytes", len);
    //         // SEND HTTP POST
    //         send_http_post(rx_data, len);
    //     }
        
    //     // ข้อควรระวัง: HTTP มี overhead สูง ควรตั้งหน่วงเวลาไม่ให้ถี่เกินไป เพื่อไม่ให้ CPU/Network ทำงานหนักเกิน
    //     vTaskDelay(500 / portTICK_PERIOD_MS); 
    // }
    
    // free(rx_data);
    // vTaskDelete(NULL);
    
    //dummy variable for testing
    //char test_msg[64];
    //int counter = 0;

    
    static char json_payload[512];
    char flight_mode[16] = "SLEEP";
    double lat = 13.7563;
    double lon = 100.5018;
    float alt = 100.0;
    float speed = 10.0;
    float roll = 0.0;
    float pitch = 0.0;
    float yaw = 0.0;
    float battery_voltage = 16.8;
    int battery_percentage = 100;
    while (1) {
        // test dummy 
        lat += 0.0001;
        lon += 0.0001;
        alt += 0.5;
        if (alt > 150.0) alt = 100.0;
        speed += 0.1;
        if (speed > 15.0) speed = 10.0;
        
        roll = (float)(rand() % 10 - 5) / 10.0f; // -0.5 ถึง +0.5
        pitch = (float)(rand() % 10 - 5) / 10.0f; // -0.5 ถึง +0.5
        yaw = (float)(rand() % 360);
        
        battery_voltage -= 0.05f;
        if (battery_voltage < 14.0f) battery_voltage = 16.8f;
        battery_percentage = (int)((battery_voltage - 14.0f) / (16.8f - 14.0f) * 100.0f);
        // ประกอบโครงสร้าง JSON ให้ตรงตาม Key ใน Django Model ของเพื่อน
        snprintf(json_payload, sizeof(json_payload),
                 "{"
                 "\"flight_mode\":\"%s\","  
                 "\"latitude\":%.6f,"
                 "\"longitude\":%.6f,"
                 "\"altitude\":%.1f,"
                 "\"speed\":%.1f,"
                 "\"roll\":%.2f,"
                 "\"pitch\":%.2f,"
                 "\"yaw\":%.1f,"
                 "\"battery_voltage\":%.2f,"
                 "\"battery_percentage\":%d"
                 "}",
                 flight_mode, lat, lon, alt, speed, roll, pitch, yaw, battery_voltage, battery_percentage);
        ESP_LOGI(TAG, "SENDING JSON Telemetry: %s", json_payload);

        send_http_post((uint8_t *)json_payload, strlen(json_payload));

        // ดีเลย์ 5 วินาทีก่อนส่งรอบถัดไป
        vTaskDelay(5000 / portTICK_PERIOD_MS); 
    }
    vTaskDelete(NULL);
    
}

void udp_receiver_task(void *pvParameters) 
{
    char rx_buffer[256];
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;
    struct sockaddr_storage source_addr;

    while (1) 
    {
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(UDP_PORT);

        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0) 
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0) 
        {
            close(sock);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        ESP_LOGI(TAG, "UDP Command Listener is ready on port %d...", UDP_PORT);

        while (1)
        {
            socklen_t socklen = sizeof(source_addr);
            // บรรทัดนี้จะหยุดรอแบบไม่กิน CPU ทันทีที่มีแพ็กเกจยิงมา มันจะทำงานในระดับมิลลิวินาที (ทันที!)
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

            if (len < 0) 
            {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                break; 
            } 
            else 
            {
                rx_buffer[len] = 0; // ปิดท้ายสตริง
                ESP_LOGI(TAG, "UDP Received JSON String: %s", rx_buffer);

                // 1. แปลงข้อความดิบให้กลายเป็นออบเจกต์ cJSON
                cJSON *root = cJSON_Parse(rx_buffer);
                
                if (root != NULL) {
                    // 2. ดึงค่าแต่ละ Key ออกมา
                    cJSON *mode_item = cJSON_GetObjectItem(root, "flight_mode");
                    cJSON *status_item = cJSON_GetObjectItem(root, "status");
                    cJSON *time_item = cJSON_GetObjectItem(root, "timestamp"); // 👈 รับค่าที่เป็นข้อความวันที่

                    // 3. เช็คเงื่อนไข: คราวนี้ต้องเป็น String ทั้ง 3 ตัวเลย (cJSON_IsString)
                    if (cJSON_IsString(mode_item) && cJSON_IsString(status_item) && cJSON_IsString(time_item)) {
                        
                        // 🚀 อัปเดตข้อมูลหลัก (Global Variables) ด้วยการ Copy สตริงทันที!
                        strncpy(flight_mode, mode_item->valuestring, sizeof(flight_mode) - 1);
                        strncpy(current_status, status_item->valuestring, sizeof(current_status) - 1);
                        strncpy(msg_timestamp, time_item->valuestring, sizeof(msg_timestamp) - 1); // 👈 ก๊อปปี้ข้อความวันที่ลงตัวแปรหลัก
                        
                        ESP_LOGW(TAG, "🔥 COMMAND APPLIED!");
                        ESP_LOGW(TAG, "Mode: %s | Status: %s | Time: %s", flight_mode, current_status, msg_timestamp);
                        
                    } else {
                        ESP_LOGE(TAG, "JSON format invalid! Missing fields or wrong data types (Must be all strings).");
                    }

                    // 4. สั่งคืนหน่วยความจำให้ระบบ
                    cJSON_Delete(root);
                    
                } else {
                    ESP_LOGE(TAG, "Error parsing invalid JSON text.");
                }
            }
        }

        if (sock != -1) {
            shutdown(sock, 0);
            close(sock);
        }
    }
}
