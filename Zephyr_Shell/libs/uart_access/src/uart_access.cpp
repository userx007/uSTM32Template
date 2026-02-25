/**
 * @file uart_access.cpp
 * @brief UART interface — Zephyr backend
 *
 * Drop-in replacement for the STM32 HAL version.
 * The public API (uart_setup / uart_getchar / uart_putchar /
 * uart_printf / uart_snprintf) is identical; only the backend changes.
 *
 * Pin mux, clock gating and baud-rate come from the board's devicetree
 * (and prj.conf / app.overlay) — no manual register writes needed.
 *
 * prj.conf:
 *   CONFIG_SERIAL=y
 *   CONFIG_UART_CONSOLE=y
 *   CONFIG_UART_INTERRUPT_DRIVEN=y
 */

#include "uart_access.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
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

/**
 * Resolved at uart_setup() time from the `zephyr,console` chosen node.
 * This is the same UART that printk() uses, matching the board's default
 * debug / shell port — PA9/PA10 on most STM32 boards.
 */
static const struct device *uart_dev = nullptr;

/* ================================================
            public interfaces definition
==================================================*/

/*--------------------------------------------------*/
void uart_setup(void)
{
    uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

    if (!device_is_ready(uart_dev)) {
        /* Nothing we can do without a working UART; trap here in debug. */
        k_panic();
    }
}

/*--------------------------------------------------*/
int uart_getchar(void)
{
    if (!uart_dev) return -1;

    uint8_t byte;
    /* Spin until a character arrives (matches HAL_MAX_DELAY behaviour). */
    while (uart_poll_in(uart_dev, &byte) != 0) {
        k_yield(); /* be cooperative while waiting */
    }
    return (int)byte;
}

/*--------------------------------------------------*/
void uart_putchar(char c)
{
    if (!uart_dev) return;
    uart_poll_out(uart_dev, (unsigned char)c);
}

/*--------------------------------------------------*/
int uart_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            char pad       = ' ';
            int  width     = 0;
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
    /* Pure string formatting — no UART device needed. */
    va_list args;
    va_start(args, fmt);
    int pos = 0;

    while (*fmt && pos < maxlen - 1) {
        if (*fmt == '%') {
            fmt++;
            char pad       = ' ';
            int  width     = 0;
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
    int  i            = 0;
    int  is_negative  = (value < 0);

    if (is_negative) value = -value;

    do { buf[i++] = '0' + (value % 10); value /= 10; } while (value);
    if (is_negative) buf[i++] = '-';

    if (left_align) {
        for (int j = i - 1; j >= 0; j--) uart_putchar(buf[j]);
        for (int j = i; j < width; j++)   uart_putchar(' ');
    } else {
        for (int j = i; j < width; j++)   uart_putchar(pad);
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
    int  i            = 0;
    int  is_negative  = (value < 0);

    if (is_negative) value = -value;

    do { tmp[i++] = '0' + (value % 10); value /= 10; } while (value);
    if (is_negative) tmp[i++] = '-';

    if (left_align) {
        for (int j = i - 1; j >= 0 && *pos < maxlen - 1; j--) buf[(*pos)++] = tmp[j];
        for (int j = i; j < width && *pos < maxlen - 1; j++)   buf[(*pos)++] = ' ';
    } else {
        for (int j = i; j < width && *pos < maxlen - 1; j++)   buf[(*pos)++] = pad;
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
Usage examples (identical to HAL version):
-------------------------------------------------------------
uart_printf("%-15s|\n", "hello");    // "hello          |"
uart_printf("%15s|\n",  "hello");    // "          hello|"
uart_printf("%-10d|\n", 123);        // "123       |"
uart_printf("%10d|\n",  123);        // "       123|"
uart_printf("%-10x|\n", 0xFF);       // "0xFF      |"
uart_printf("%10x|\n",  0xFF);       // "      0xFF|"
*/
