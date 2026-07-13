#ifndef HTTP_H
#define HTTP_H

#include <stdint.h>

void send_http_post(const uint8_t *data, int len);
void http_sender_task(void *pvParameters);
void udp_receiver_task(void *pvParameters);
void convert2uart(const char *json_string);
#endif