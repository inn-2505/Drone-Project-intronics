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
#include "drone_protocol.h"
#include "cJSON.h"

static const char *TAG = "NETWORKCONTROL";


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
        .timeout_ms = 1000,
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
        // รอรับข้อความในคิว หากไม่มีข้อมูลจะหลับรอ (Block) แบบไม่กินกำลัง CPU
        if (xQueueReceive(http_queue, json_payload, portMAX_DELAY) == pdPASS) {
            ESP_LOGI(TAG, "Queue item found! Sending HTTP POST: %s", json_payload);
            
            // สั่งส่งข้อมูล JSON ยิงหา Server Django
            send_http_post((uint8_t *)json_payload, strlen(json_payload));
        }
    }
    
    vTaskDelete(NULL);
    
}

/*
 =============================================================
           ทิศทาง 2: JSON string (จาก UDP) --> ส่งเข้า
            uart_tx_queue ให้ tx_task ยิงออก UART ต่อ
 =============================================================
 */

void convert2uart(const char *json_string)
{
    cJSON *root = cJSON_Parse(json_string);
    if (root == NULL) {
        ESP_LOGE(TAG, "JSON parse error: invalid format");
        return;
    }
 
    // ดึงค่าแต่ละ key ออกมา  
    cJSON *command_item   = cJSON_GetObjectItem(root, "command");
    cJSON *lat_1_item     = cJSON_GetObjectItem(root, "lat1");
    cJSON *long_1_item    = cJSON_GetObjectItem(root, "lon1");
    cJSON *lat_2_item     = cJSON_GetObjectItem(root, "lat2");
    cJSON *long_2_item    = cJSON_GetObjectItem(root, "lon2");
    cJSON *altitude_item  = cJSON_GetObjectItem(root, "alt");
    cJSON *speed_item     = cJSON_GetObjectItem(root, "spd");
    cJSON *throttle_item  = cJSON_GetObjectItem(root, "throttle");
    cJSON *yaw_item       = cJSON_GetObjectItem(root, "yaw");
    cJSON *pitch_item     = cJSON_GetObjectItem(root, "pitch");
    cJSON *roll_item      = cJSON_GetObjectItem(root, "roll");

    // เช็คว่า field ที่ต้องมีครบไหม 
    if (!command_item || !lat_1_item || !long_1_item || !lat_2_item || !long_2_item || !altitude_item || !speed_item || !throttle_item || !yaw_item || !pitch_item || !roll_item) {
        ESP_LOGE(TAG, "JSON missing required fields!");
        cJSON_Delete(root);
        return;
    }
 
    drone_command_t payload;
    
    if (command_item != NULL && cJSON_IsString(command_item)) {
        const char *cmd_str = command_item->valuestring;
        payload.command = command_str_to_mode(cmd_str);
    } else {
        ESP_LOGE(TAG, "Missing or invalid 'command' field (expected string)");
        payload.command = 0;
    }
    payload.lat1     = (int32_t)(lat_1_item->valuedouble * 1000000);
    payload.lon1     = (int32_t)(long_1_item->valuedouble * 1000000);
    payload.lat2     = (int32_t)(lat_2_item->valuedouble * 1000000);
    payload.lon2     = (int32_t)(long_2_item->valuedouble * 1000000);
    payload.altitude = (uint16_t)altitude_item->valueint;
    payload.speed    = (uint8_t)speed_item->valueint;
    payload.throttle = (uint8_t)throttle_item->valueint;
    payload.yaw      = (uint8_t)yaw_item->valueint;
    payload.pitch    = (uint8_t)pitch_item->valueint;
    payload.roll     = (uint8_t)roll_item->valueint;

    if (xQueueSend(uart_tx_queue, &payload, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGW(TAG, "uart_tx_queue full, command dropped!");
    }
    // คืนหน่วยความจำของ cJSON
    cJSON_Delete(root);

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
                ESP_LOGI(TAG, "Received UDP JSON (%d bytes): %s", len, rx_buffer);
                convert2uart(rx_buffer); // แปลง JSON string เป็น byte array
                
            }
        }

        if (sock != -1) {
            shutdown(sock, 0);
            close(sock);
        }
    }
}
