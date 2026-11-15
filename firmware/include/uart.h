#pragma once
#include <stdint.h>
/* USART6 = PC (programacion/CPS), USART2 = GPS (solo RX) */
void uart_pc_init(uint32_t baud);
void uart_pc_putc(uint8_t c);
void uart_pc_write(const void *buf, int len);
int  uart_pc_getc(void);           /* -1 si no hay datos */
int  uart_pc_available(void);
void uart_gps_init(uint32_t baud);
void uart_gps_stop(void);
int  uart_gps_getc(void);
