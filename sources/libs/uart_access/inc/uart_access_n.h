#ifndef UART_ACCESS_H
#define UART_ACCESS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>

/* Initialize UART1 on STM32F411 (PA9=TX, PA10=RX, 115200 baud) */
void uart_setup(void);

/* Get a character from UART (blocking) */
int uart_getchar(void);

/* Send a character to UART (blocking) */
void uart_putchar(char c);

/* Printf-like function for UART output */
int uart_printf(const char *fmt, ...);

/* snprintf-like function for string formatting */
int uart_snprintf(char *buf, int maxlen, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /*UART_ACCESS_H*/
