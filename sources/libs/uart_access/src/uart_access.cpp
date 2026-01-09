#include "uart_access.h"
#include "libopencm3/stm32/rcc.h"
#include "libopencm3/stm32/gpio.h"
#include "libopencm3/stm32/usart.h"

#include <stdarg.h>
#include <stdint.h>

/* ================================================
            private interfaces declaration
==================================================*/

static void print_int(int value, int width, char pad);
static void print_hex(unsigned int value, int width, char pad);
static void print_int_to_buf(char *buf, int *pos, int maxlen, int value, int width, char pad);
static void print_hex_to_buf(char *buf, int *pos, int maxlen, unsigned int value, int width, char pad);


/* ================================================
            public interfaces ddefinition
==================================================*/


/*--------------------------------------------------*/
void uart_setup(void)
{
    rcc_periph_clock_enable(RCC_USART1);
    rcc_periph_clock_enable(RCC_GPIOA);

    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ,
                  GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO_USART1_TX);
    gpio_set_mode(GPIOA, GPIO_MODE_INPUT,
                  GPIO_CNF_INPUT_FLOAT, GPIO_USART1_RX);

    usart_set_baudrate(USART1, 115200);
    usart_set_databits(USART1, 8);
    usart_set_stopbits(USART1, USART_STOPBITS_1);
    usart_set_mode(USART1, USART_MODE_TX_RX);
    usart_set_parity(USART1, USART_PARITY_NONE);
    usart_set_flow_control(USART1, USART_FLOWCONTROL_NONE);

    usart_enable(USART1);
}



/*--------------------------------------------------*/
int uart_getchar(void)
{
    while (!(USART_SR(USART1) & USART_SR_RXNE)); // Wait for data
    return usart_recv(USART1);
}



/*--------------------------------------------------*/
void uart_putchar(char c)
{
    usart_send_blocking(USART1, c);
}



/*--------------------------------------------------*/
int uart_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            char pad = ' ';
            int width = 0;
            int left_align = 0;  // Flag for left alignment
            
            // Check for left alignment flag '-'
            if (*fmt == '-') {
                left_align = 1;
                fmt++;
            }
            
            // Check for zero padding
            if (*fmt == '0') {
                pad = '0';
                fmt++;
            }
            
            // Parse width
            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt - '0');
                fmt++;
            }
            
            switch (*fmt) {
                case 's': {
                    const char *s = va_arg(args, const char *);
                    int len = 0;
                    const char *temp = s;
                    
                    // Calculate string length
                    while (*temp++) len++;
                    
                    // Left-aligned: print string first, then padding
                    if (left_align) {
                        temp = s;
                        while (*temp) {
                            uart_putchar(*temp++);
                        }
                        // Add padding on the right
                        for (int i = len; i < width; i++) {
                            uart_putchar(pad);
                        }
                    } 
                    // Right-aligned: print padding first, then string
                    else {
                        // Add padding on the left
                        for (int i = len; i < width; i++) {
                            uart_putchar(pad);
                        }
                        while (*s) {
                            uart_putchar(*s++);
                        }
                    }
                    break;
                }
                case 'd':
                    print_int(va_arg(args, int), width, pad);
                    break;
                case 'x':
                    print_hex(va_arg(args, unsigned int), width, pad);
                    break;
                case 'c':
                    uart_putchar((char)va_arg(args, int));
                    break;
                default:
                    uart_putchar('%');
                    uart_putchar(*fmt);
                    break;
            }
        } else {
            uart_putchar(*fmt);
        }
        fmt++;
    }
    va_end(args);
    return 0;
}


/*--------------------------------------------------*/
int uart_snprintf(char *buf, int maxlen, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int pos = 0;

    while (*fmt && pos < maxlen - 1) {
        if (*fmt == '%') {
            fmt++;
            char pad = ' ';
            int width = 0;
            if (*fmt == '0') {
                pad = '0';
                fmt++;
            }
            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt - '0');
                fmt++;
            }
            switch (*fmt) {
                case 's': {
                    const char *s = va_arg(args, const char *);
                    while (*s && pos < maxlen - 1) {
                        buf[pos++] = *s++;
                    }
                    break;
                }
                case 'd':
                    print_int_to_buf(buf, &pos, maxlen, va_arg(args, int), width, pad);
                    break;
                case 'x':
                    print_hex_to_buf(buf, &pos, maxlen, va_arg(args, unsigned int), width, pad);
                    break;
                case 'c':
                    buf[pos++] = (char)va_arg(args, int);
                    break;
                default:
                    buf[pos++] = '%';
                    buf[pos++] = *fmt;
                    break;
            }
        } else {
            buf[pos++] = *fmt;
        }
        fmt++;
    }

    buf[pos] = '\0';
    va_end(args);
    return pos;
}



/* ================================================
            private interfaces ddefinition
==================================================*/


/*--------------------------------------------------*/
static void print_int(int value, int width, char pad)
{
    char buffer[12];
    int i = 0;
    if (value < 0) {
        uart_putchar('-');
        value = -value;
        width--;
    }
    do {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    } while (value);
    while (width-- > i) {
        uart_putchar(pad);
    }
    while (i--) {
        uart_putchar(buffer[i]);
    }
}



/*--------------------------------------------------*/
static void print_hex(unsigned int value, int width, char pad)
{
    const char *hex = "0123456789ABCDEF";
    char buffer[8];
    int i = 0;
    do {
        buffer[i++] = hex[value & 0xF];
        value >>= 4;
    } while (value);
    while (width-- > i + 2) {
        uart_putchar(pad);
    }
    uart_putchar('0');
    uart_putchar('x');
    while (i--) {
        uart_putchar(buffer[i]);
    }
}



/*--------------------------------------------------*/
static void print_int_to_buf(char *buf, int *pos, int maxlen, int value, int width, char pad)
{
    char tmp[12];
    int i = 0;
    if (value < 0) {
        if (*pos < maxlen - 1) buf[(*pos)++] = '-';
        value = -value;
        width--;
    }
    do {
        tmp[i++] = '0' + (value % 10);
        value /= 10;
    } while (value);
    while (width-- > i && *pos < maxlen - 1) {
        buf[(*pos)++] = pad;
    }
    while (i-- && *pos < maxlen - 1) {
        buf[(*pos)++] = tmp[i];
    }
}



/*--------------------------------------------------*/
static void print_hex_to_buf(char *buf, int *pos, int maxlen, unsigned int value, int width, char pad)
{
    const char *hex = "0123456789ABCDEF";
    char tmp[8];
    int i = 0;
    do {
        tmp[i++] = hex[value & 0xF];
        value >>= 4;
    } while (value);
    if (*pos < maxlen - 2) {
        buf[(*pos)++] = '0';
        buf[(*pos)++] = 'x';
    }
    while (width-- > i + 2 && *pos < maxlen - 1) {
        buf[(*pos)++] = pad;
    }
    while (i-- && *pos < maxlen - 1) {
        buf[(*pos)++] = tmp[i];
    }
}


