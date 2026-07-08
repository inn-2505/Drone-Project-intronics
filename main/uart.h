#ifndef uart_H
#define uart_H

void init_uart(void);
void tx_task(void *pvParameters);
void rx_task(void* pvParameters);
void parse_msp_packet(uint8_t cmd, uint8_t *payload, uint8_t size);
void send_msp_command(uint8_t cmd, uint8_t *payload, uint8_t size);

#endif