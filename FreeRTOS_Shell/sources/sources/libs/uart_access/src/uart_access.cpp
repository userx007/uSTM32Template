#include "uart_access.h"
#include "libopencm3/stm32/rcc.h"
#include "libopencm3/stm32/gpio.h"
#include "libopencm3/stm32/usart.h"

#include <stdarg.h>
#include <stdint.h>

/* ================================================
            private interfaces declaration
==================================================*/

static void print_int(int value, int width, char pad, int left_align);
static void print_hex(unsigned int value, int width, char pad, int left_align);
static void print_int_to_buf(char *buf, int *pos, int maxlen, int value, int width, char pad, int left_align);
static void print_hex_to_buf(char *buf, int *pos, int maxlen, unsigned int value, int width, char pad, int left_align);

/* ================================================
            public interfaces ddefinition
==================================================*/


/*--------------------------------------------------*/
void uart_setup(void)
{
    /* Enable clocks for USART1 and GPIOA */
    rcc_periph_clock_enable(RCC_USART1);
    rcc_periph_clock_enable(RCC_GPIOA);

#if defined(STM32F1)
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ,
                  GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO_USART1_TX);
    gpio_set_mode(GPIOA, GPIO_MODE_INPUT,
                  GPIO_CNF_INPUT_FLOAT, GPIO_USART1_RX);
#endif /*defined(STM32F1)*/

#if defined(STM32F4)
/*
    STM32F411 USART1 Configuration:
    USART1 TX: PA9 (Alternate Function 7)
    USART1 RX: PA10 (Alternate Function 7)
*/
    
    /* Configure PA9 as USART1_TX - Alternate Function */
    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO9);
    gpio_set_af(GPIOA, GPIO_AF7, GPIO9);  /* AF7 is USART1 for STM32F411 */
    gpio_set_output_options(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO9);
    
    /* Configure PA10 as USART1_RX - Alternate Function */
    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO10);
    gpio_set_af(GPIOA, GPIO_AF7, GPIO10);  /* AF7 is USART1 for STM32F411 */
#endif /*defined(STM32F4)*/

    /* Setup USART1 parameters */
    usart_set_baudrate(USART1, 115200);
    usart_set_databits(USART1, 8);
    usart_set_stopbits(USART1, USART_STOPBITS_1);
    usart_set_mode(USART1, USART_MODE_TX_RX);
    usart_set_parity(USART1, USART_PARITY_NONE);
    usart_set_flow_control(USART1, USART_FLOWCONTROL_NONE);

    /* Enable USART1 */
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
                case 'u':
                    print_int(va_arg(args, unsigned int), width, pad, left_align);
                    break;
                case 'd':
                    print_int(va_arg(args, int), width, pad, left_align);
                    break;
                case 'x':
                case 'X':                
                    print_hex(va_arg(args, unsigned int), width, pad, left_align);
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
                        while (*temp && pos < maxlen - 1) {
                            buf[pos++] = *temp++;
                        }
                        // Add padding on the right
                        for (int i = len; i < width && pos < maxlen - 1; i++) {
                            buf[pos++] = pad;
                        }
                    } 
                    // Right-aligned: print padding first, then string
                    else {
                        // Add padding on the left
                        for (int i = len; i < width && pos < maxlen - 1; i++) {
                            buf[pos++] = pad;
                        }
                        while (*s && pos < maxlen - 1) {
                            buf[pos++] = *s++;
                        }
                    }
                    break;
                }
                case 'u':
                    print_int_to_buf(buf, &pos, maxlen, va_arg(args, unsigned int), width, pad, left_align);
                    break;
                case 'd':
                    print_int_to_buf(buf, &pos, maxlen, va_arg(args, int), width, pad, left_align);
                    break;
                case 'x':
                case 'X':
                    print_hex_to_buf(buf, &pos, maxlen, va_arg(args, unsigned int), width, pad, left_align);
                    break;
                case 'c':
                    if (pos < maxlen - 1) {
                        buf[pos++] = (char)va_arg(args, int);
                    }
                    break;
                default:
                    if (pos < maxlen - 1) buf[pos++] = '%';
                    if (pos < maxlen - 1) buf[pos++] = *fmt;
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
            private interfaces definition
==================================================*/


/*--------------------------------------------------*/
static void print_int(int value, int width, char pad, int left_align)
{
    char buffer[12];
    int i = 0;
    int is_negative = 0;
    
    if (value < 0) {
        is_negative = 1;
        value = -value;
    }
    
    do {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    } while (value);
    
    if (is_negative) {
        buffer[i++] = '-';
    }
    
    // Left-aligned: print number first, then padding
    if (left_align) {
        for (int j = i - 1; j >= 0; j--) {
            uart_putchar(buffer[j]);
        }
        for (int j = i; j < width; j++) {
            uart_putchar(pad);
        }
    }
    // Right-aligned: padding first, then number
    else {
        for (int j = i; j < width; j++) {
            uart_putchar(pad);
        }
        for (int j = i - 1; j >= 0; j--) {
            uart_putchar(buffer[j]);
        }
    }
}



/*--------------------------------------------------*/
static void print_hex(unsigned int value, int width, char pad, int left_align)
{
    const char *hex = "0123456789ABCDEF";
    char buffer[8];
    int i = 0;
    
    do {
        buffer[i++] = hex[value & 0xF];
        value >>= 4;
    } while (value);
    
    // Left-aligned: 0x + number first, then padding
    if (left_align) {
        uart_putchar('0');
        uart_putchar('x');
        for (int j = i - 1; j >= 0; j--) {
            uart_putchar(buffer[j]);
        }
        for (int j = i + 2; j < width; j++) {
            uart_putchar(pad);
        }
    }
    // Right-aligned: padding first, then 0x + number
    else {
        for (int j = i + 2; j < width; j++) {
            uart_putchar(pad);
        }
        uart_putchar('0');
        uart_putchar('x');
        for (int j = i - 1; j >= 0; j--) {
            uart_putchar(buffer[j]);
        }
    }
}



/*--------------------------------------------------*/
static void print_int_to_buf(char *buf, int *pos, int maxlen, int value, int width, char pad, int left_align)
{
    char tmp[12];
    int i = 0;
    int is_negative = 0;
    
    if (value < 0) {
        is_negative = 1;
        value = -value;
    }
    
    do {
        tmp[i++] = '0' + (value % 10);
        value /= 10;
    } while (value);
    
    if (is_negative) {
        tmp[i++] = '-';
    }
    
    // Left-aligned: number first, then padding
    if (left_align) {
        for (int j = i - 1; j >= 0 && *pos < maxlen - 1; j--) {
            buf[(*pos)++] = tmp[j];
        }
        for (int j = i; j < width && *pos < maxlen - 1; j++) {
            buf[(*pos)++] = pad;
        }
    }
    // Right-aligned: padding first, then number
    else {
        for (int j = i; j < width && *pos < maxlen - 1; j++) {
            buf[(*pos)++] = pad;
        }
        for (int j = i - 1; j >= 0 && *pos < maxlen - 1; j--) {
            buf[(*pos)++] = tmp[j];
        }
    }
}



/*--------------------------------------------------*/
static void print_hex_to_buf(char *buf, int *pos, int maxlen, unsigned int value, int width, char pad, int left_align)
{
    const char *hex = "0123456789ABCDEF";
    char tmp[8];
    int i = 0;
    
    do {
        tmp[i++] = hex[value & 0xF];
        value >>= 4;
    } while (value);
    
    // Left-aligned: 0x + number first, then padding
    if (left_align) {
        if (*pos < maxlen - 2) {
            buf[(*pos)++] = '0';
            buf[(*pos)++] = 'x';
        }
        for (int j = i - 1; j >= 0 && *pos < maxlen - 1; j--) {
            buf[(*pos)++] = tmp[j];
        }
        for (int j = i + 2; j < width && *pos < maxlen - 1; j++) {
            buf[(*pos)++] = pad;
        }
    }
    // Right-aligned: padding first, then 0x + number
    else {
        for (int j = i + 2; j < width && *pos < maxlen - 1; j++) {
            buf[(*pos)++] = pad;
        }
        if (*pos < maxlen - 2) {
            buf[(*pos)++] = '0';
            buf[(*pos)++] = 'x';
        }
        for (int j = i - 1; j >= 0 && *pos < maxlen - 1; j--) {
            buf[(*pos)++] = tmp[j];
        }
    }
}

/*
Usage examples:
-------------------------------------------------------------
uart_printf("%-15s|\n", "hello");       // "hello          |"
uart_printf("%15s|\n", "hello");        // "          hello|"
uart_printf("%-10s|\n", "test");        // "test      |"
uart_printf("%s|\n", "no padding");     // "no padding|"
uart_printf("%-10d|\n", 123);           // "123       |"
uart_printf("%10d|\n", 123);            // "       123|"
uart_printf("%-10x|\n", 0xFF);          // "0xFF      |"
uart_printf("%10x|\n", 0xFF);           // "      0xFF|"

*/