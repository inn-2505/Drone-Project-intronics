#ifndef uart_H
#define uart_H

void init_uart(void);
void tx_task(void *pvParameters);
void rx_task(void* pvParameters);
uint8_t calc_checksum(uint8_t len, const uint8_t *data);
void uart_send(const uint8_t *data, size_t length);
void on_packet(const uint8_t *data, size_t length);
void convert2json(const uint8_t *data, char *json_buffer, size_t buffer_size);


#endif