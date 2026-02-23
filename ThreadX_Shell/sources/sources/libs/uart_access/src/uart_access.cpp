#include "uart_access.h"

#if defined(STM32F1)
#  include "stm32f1xx_hal.h"
#elif defined(STM32F4)
#  include "stm32f4xx_hal.h"
#else
#  error "Define STM32F1 or STM32F4 in your build system"
#endif

#include <stdarg.h>

/* ================================================
            private interfaces declaration
==================================================*/

static void print_int(int value, int width, char pad, int left_align);
static void print_hex(unsigned int value, int width, char pad, int left_align);
static void print_int_to_buf(char *buf, int *pos, int maxlen, int value, int width, char pad, int left_align);
static void print_hex_to_buf(char *buf, int *pos, int maxlen, unsigned int value, int width, char pad, int left_align);

/* ================================================
            module-level state
==================================================*/

static UART_HandleTypeDef huart1;

/* ================================================
            HAL MSP hook  (GPIO + clock wiring)
==================================================*/

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1)
        return;

#if defined(STM32F1)
    /*
     * STM32F1: USART1 TX=PA9, RX=PA10
     * GPIOA and USART1 clocks
     */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {};

    /* TX – alternate function push-pull */
    gpio.Pin   = GPIO_PIN_9;
    gpio.Mode  = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* RX – floating input */
    gpio.Pin  = GPIO_PIN_10;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);

#elif defined(STM32F4)
    /*
     * STM32F4: USART1 TX=PA9 AF7, RX=PA10 AF7
     */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {};
    gpio.Pin       = GPIO_PIN_9 | GPIO_PIN_10;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &gpio);
#endif
}

/* ================================================
            public interfaces definition
==================================================*/

/*--------------------------------------------------*/
void uart_setup(void)
{
    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;

    HAL_UART_Init(&huart1);
}

/*--------------------------------------------------*/
int uart_getchar(void)
{
    uint8_t byte;
    HAL_UART_Receive(&huart1, &byte, 1, HAL_MAX_DELAY);
    return (int)byte;
}

/*--------------------------------------------------*/
void uart_putchar(char c)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&c, 1, HAL_MAX_DELAY);
}

/*--------------------------------------------------*/
int uart_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            char pad      = ' ';
            int  width    = 0;
            int  left_align = 0;

            if (*fmt == '-') { left_align = 1; fmt++; }
            if (*fmt == '0') { pad = '0';      fmt++; }

            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt - '0');
                fmt++;
            }

            switch (*fmt) {
                case 's': {
                    const char *s = va_arg(args, const char *);
                    int len = 0;
                    const char *tmp = s;
                    while (*tmp++) len++;

                    if (left_align) {
                        tmp = s;
                        while (*tmp) uart_putchar(*tmp++);
                        for (int i = len; i < width; i++) uart_putchar(' ');
                    } else {
                        for (int i = len; i < width; i++) uart_putchar(pad);
                        while (*s) uart_putchar(*s++);
                    }
                    break;
                }
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
            char pad      = ' ';
            int  width    = 0;
            int  left_align = 0;

            if (*fmt == '-') { left_align = 1; fmt++; }
            if (*fmt == '0') { pad = '0';      fmt++; }

            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt - '0');
                fmt++;
            }

            switch (*fmt) {
                case 's': {
                    const char *s = va_arg(args, const char *);
                    int len = 0;
                    const char *tmp = s;
                    while (*tmp++) len++;

                    if (left_align) {
                        tmp = s;
                        while (*tmp && pos < maxlen - 1) buf[pos++] = *tmp++;
                        for (int i = len; i < width && pos < maxlen - 1; i++) buf[pos++] = ' ';
                    } else {
                        for (int i = len; i < width && pos < maxlen - 1; i++) buf[pos++] = pad;
                        while (*s && pos < maxlen - 1) buf[pos++] = *s++;
                    }
                    break;
                }
                case 'd':
                    print_int_to_buf(buf, &pos, maxlen, va_arg(args, int), width, pad, left_align);
                    break;
                case 'x':
                case 'X':
                    print_hex_to_buf(buf, &pos, maxlen, va_arg(args, unsigned int), width, pad, left_align);
                    break;
                case 'c':
                    if (pos < maxlen - 1) buf[pos++] = (char)va_arg(args, int);
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
    char buf[12];
    int  i           = 0;
    int  is_negative = (value < 0);

    if (is_negative) value = -value;

    do { buf[i++] = '0' + (value % 10); value /= 10; } while (value);
    if (is_negative) buf[i++] = '-';

    if (left_align) {
        for (int j = i - 1; j >= 0; j--) uart_putchar(buf[j]);
        for (int j = i; j < width; j++)  uart_putchar(' ');
    } else {
        for (int j = i; j < width; j++)  uart_putchar(pad);
        for (int j = i - 1; j >= 0; j--) uart_putchar(buf[j]);
    }
}

/*--------------------------------------------------*/
static void print_hex(unsigned int value, int width, char pad, int left_align)
{
    const char hex[] = "0123456789ABCDEF";
    char buf[8];
    int  i = 0;

    do { buf[i++] = hex[value & 0xF]; value >>= 4; } while (value);

    if (left_align) {
        uart_putchar('0'); uart_putchar('x');
        for (int j = i - 1; j >= 0; j--) uart_putchar(buf[j]);
        for (int j = i + 2; j < width; j++) uart_putchar(' ');
    } else {
        for (int j = i + 2; j < width; j++) uart_putchar(pad);
        uart_putchar('0'); uart_putchar('x');
        for (int j = i - 1; j >= 0; j--) uart_putchar(buf[j]);
    }
}

/*--------------------------------------------------*/
static void print_int_to_buf(char *buf, int *pos, int maxlen, int value, int width, char pad, int left_align)
{
    char tmp[12];
    int  i           = 0;
    int  is_negative = (value < 0);

    if (is_negative) value = -value;

    do { tmp[i++] = '0' + (value % 10); value /= 10; } while (value);
    if (is_negative) tmp[i++] = '-';

    if (left_align) {
        for (int j = i - 1; j >= 0 && *pos < maxlen - 1; j--) buf[(*pos)++] = tmp[j];
        for (int j = i; j < width && *pos < maxlen - 1; j++)  buf[(*pos)++] = ' ';
    } else {
        for (int j = i; j < width && *pos < maxlen - 1; j++)  buf[(*pos)++] = pad;
        for (int j = i - 1; j >= 0 && *pos < maxlen - 1; j--) buf[(*pos)++] = tmp[j];
    }
}

/*--------------------------------------------------*/
static void print_hex_to_buf(char *buf, int *pos, int maxlen, unsigned int value, int width, char pad, int left_align)
{
    const char hex[] = "0123456789ABCDEF";
    char tmp[8];
    int  i = 0;

    do { tmp[i++] = hex[value & 0xF]; value >>= 4; } while (value);

    if (left_align) {
        if (*pos < maxlen - 2) { buf[(*pos)++] = '0'; buf[(*pos)++] = 'x'; }
        for (int j = i - 1; j >= 0 && *pos < maxlen - 1; j--) buf[(*pos)++] = tmp[j];
        for (int j = i + 2; j < width && *pos < maxlen - 1; j++) buf[(*pos)++] = ' ';
    } else {
        for (int j = i + 2; j < width && *pos < maxlen - 1; j++) buf[(*pos)++] = pad;
        if (*pos < maxlen - 2) { buf[(*pos)++] = '0'; buf[(*pos)++] = 'x'; }
        for (int j = i - 1; j >= 0 && *pos < maxlen - 1; j--) buf[(*pos)++] = tmp[j];
    }
}

/*
Usage examples:
-------------------------------------------------------------
uart_printf("%-15s|\n", "hello");    // "hello          |"
uart_printf("%15s|\n",  "hello");    // "          hello|"
uart_printf("%-10d|\n", 123);        // "123       |"
uart_printf("%10d|\n",  123);        // "       123|"
uart_printf("%-10x|\n", 0xFF);       // "0xFF      |"
uart_printf("%10x|\n",  0xFF);       // "      0xFF|"
*/
