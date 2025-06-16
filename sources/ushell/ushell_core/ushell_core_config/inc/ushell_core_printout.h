#ifndef USHELL_CORE_PRINTOUT_H
#define USHELL_CORE_PRINTOUT_H

/* forward declarations to avoid direct dependency of uShell by implementation*/
#include <stdarg.h>
int  uart_getchar       (void);
void uart_putchar       (char c);
void uart_printf        (const char *format, ...);
int  uart_snprintf      (char *buf, int maxlen, const char *fmt, ...);
#define uSHELL_PRINTF   uart_printf
#define uSHELL_SNPRINTF uart_snprintf
#define uSHELL_GETCH()  uart_getchar()
#define uSHELL_PUTCH(x) uart_putchar(x)

#endif /* USHELL_CORE_PRINTOUT_H */
