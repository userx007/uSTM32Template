#ifndef UART_ACCESS_H
#define UART_ACCESS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>

void uart_setup(void);
int  uart_getchar(void);
void uart_putchar(char c);
int  uart_printf (const char *format, ...);
int  uart_snprintf(char *buf, int maxlen, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /*UART_ACCESS_H*/