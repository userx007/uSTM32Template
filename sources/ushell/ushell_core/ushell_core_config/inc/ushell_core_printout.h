#ifndef USHELL_CORE_PRINTOUT_H
#define USHELL_CORE_PRINTOUT_H

#include <stdarg.h>
char uart_getchar       (void);
void uart_putchar       (char data);
int  uart_printf        (const char *format, ...);
#define uSHELL_PRINTF   printf
#define uSHELL_SNPRINTF snprintf
#define uSHELL_GETCH()  uart_getchar()
#define uSHELL_PUTCH(x) uart_putchar(x)

#endif /* USHELL_CORE_PRINTOUT_H */
