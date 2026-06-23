#ifndef __USART_H__
#define __USART_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define UART_RX_BUF_SIZE  128

extern UART_HandleTypeDef huart2;
extern uint8_t  uart_rx_buf[UART_RX_BUF_SIZE];
extern uint16_t uart_rx_head;
extern uint16_t uart_rx_tail;

void MX_USART2_UART_Init(void);
void uart_putc(char c);
void uart_puts(const char *s);

/* Call this to check for available bytes (non-blocking) */
int  uart_getc(void);

#ifdef __cplusplus
}
#endif

#endif /* __USART_H__ */
