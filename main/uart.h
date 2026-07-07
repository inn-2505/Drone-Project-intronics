#ifndef uart_H
#define uart_H

void init_uart(void);
void tx_task(void *pvParameters);
void rx_task(void* pvParameters);
#endif