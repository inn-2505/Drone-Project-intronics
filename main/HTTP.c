#include "HTTP.h"
#include "config.h"       
#include <string.h>
#include <sys/param.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
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

//char command_mode[16] = "command";
//char type[32] = "READY";
float latitude = 0.0;
float longitude = 0.0;

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
    char json_payload[512];
    ESP_LOGI(TAG, "HTTP Sender Task is running and waiting for Queue...");
    
    while (1) {
       if (xQueueReceive(http_queue, json_payload, portMAX_DELAY) == pdPASS) {
            ESP_LOGI(TAG, "Queue item found! Sending HTTP POST: %s", json_payload);
            
        ESP_LOGI(TAG, "SENDING JSON Telemetry: %s", json_payload);
        // Perform HTTP POST to Django server
        send_http_post((uint8_t *)json_payload, strlen(json_payload));

        // ดีเลย์ 5 วินาทีก่อนส่งรอบถัดไป
        vTaskDelay(5000 / portTICK_PERIOD_MS); 
        }
    }
    vTaskDelete(NULL);
}


void udp_receiver_task(void *pvParameters) 
{
    char rx_buffer[256];
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;
    struct sockaddr_storage source_addr;
    drone_command_t cmd_to_send;

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
                    cJSON *latitude_item = cJSON_GetObjectItem(root, "latitude");
                    cJSON *longitude_item = cJSON_GetObjectItem(root, "longitude");
                    cJSON *altitude_item = cJSON_GetObjectItem(root, "altitude");
                    cJSON *arm_item = cJSON_GetObjectItem(root, "arm");
                    // Verify required fields exist
                    if (latitude_item != NULL && longitude_item != NULL && altitude_item != NULL) {
                        
                        // Extract float values (handling both string and number formats)
                        if (cJSON_IsNumber(latitude_item)) {
                            cmd_to_send.latitude = (float)latitude_item->valuedouble;
                        } else {
                            cmd_to_send.latitude = (float)atof(latitude_item->valuestring);
                        }
                        if (cJSON_IsNumber(longitude_item)) {
                            cmd_to_send.longitude = (float)longitude_item->valuedouble;
                        } else {
                            cmd_to_send.longitude = (float)atof(longitude_item->valuestring);
                        }
                        if (cJSON_IsNumber(altitude_item)) {
                            cmd_to_send.altitude = (float)altitude_item->valuedouble;
                        } else {
                            cmd_to_send.altitude = (float)atof(altitude_item->valuestring);
                        }
                        // Extract arm state (optional field, default is 0)
                        if (arm_item != NULL) {
                            cmd_to_send.arm_state = (uint8_t)arm_item->valueint;
                        } else {
                            cmd_to_send.arm_state = 0;
                        }
                        ESP_LOGI(TAG, "Parsed Target: Lat: %.6f, Lon: %.6f, Alt: %.1f", 
                                 cmd_to_send.latitude, cmd_to_send.longitude, cmd_to_send.altitude);
                        // Push the command struct into the UART TX Queue
                        if (xQueueSend(uart_tx_queue, &cmd_to_send, pdMS_TO_TICKS(100)) != pdPASS) {
                            ESP_LOGW(TAG, "UART TX queue is full, command dropped!");
                        }
                    } 
                    else {
                        ESP_LOGE(TAG, "JSON format invalid! Missing latitude, longitude, or altitude.");
                    }
                    // อย่าลืมลบ object เพื่อคืน Memory
                    cJSON_Delete(root);
                } 

                else 
                {
                    ESP_LOGE(TAG, "JSON parsing error: invalid JSON format");
                }
            }
        }

        if (sock != -1) {
            shutdown(sock, 0);
            close(sock);
        }
    }
}
