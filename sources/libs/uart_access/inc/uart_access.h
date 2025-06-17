#ifndef UART_ACCESS_H
#define UART_ACCESS_H

#include <stdarg.h>

void uart_setup(void);
void uart_putchar(char c);
int  uart_getchar(void);
void uart_printf(const char *fmt, ...);
int  uart_snprintf(char *buf, int maxlen, const char *fmt, ...);


#endif /*UART_ACCESS_H*/