#ifndef __USART_H__
#define __USART_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define UART_RX_BUF_SIZE  128

/* ---- USART2 (shell, PA2=TX / PA3=RX) ---- */
extern UART_HandleTypeDef huart2;
extern uint8_t  uart_rx_buf[UART_RX_BUF_SIZE];
extern uint16_t uart_rx_head;
extern uint16_t uart_rx_tail;

void MX_USART2_UART_Init(void);
void uart_putc(char c);
void uart_puts(const char *s);
int  uart_getc(void);
void uart_inject(const char *s);

/* ---- USART1 (ESP8266, PA9=TX / PA10=RX) ---- */
extern UART_HandleTypeDef huart1;
void MX_USART1_UART_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __USART_H__ */
