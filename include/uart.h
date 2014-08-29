#ifndef _uart_included_h_
#define _uart_included_h_

#include <stdint.h>

#define BAUD_RATE       115200

void uart_init(uint32_t baud);
void uart_putchar(uint8_t c);
void uart_print(char* fmt, ...);
uint8_t uart_getchar(void);
uint8_t uart_available(void);

#endif
