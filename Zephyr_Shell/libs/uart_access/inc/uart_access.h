#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file uart_access.h
 * @brief UART interface — Zephyr backend
 *
 * Drop-in replacement for the STM32 HAL version.
 * Same public API; underneath it uses Zephyr's UART poll driver.
 *
 * prj.conf requirements:
 *   CONFIG_SERIAL=y
 *   CONFIG_UART_CONSOLE=y
 *   CONFIG_UART_INTERRUPT_DRIVEN=y
 */

/** Initialise the UART handle (resolves zephyr,console chosen node). */
void uart_setup(void);

/** Blocking single-character receive. Returns byte or -1 on error. */
int  uart_getchar(void);

/** Blocking single-character transmit. */
void uart_putchar(char c);

/**
 * Minimal printf over UART.
 * Supports: %s  %d  %x/%X  %c  + width / zero-pad / left-align.
 */
int  uart_printf(const char *fmt, ...);

/**
 * Minimal snprintf into caller-supplied buffer.
 * Same specifiers as uart_printf(). Always NUL-terminates.
 * Returns number of characters written (excluding NUL).
 */
int  uart_snprintf(char *buf, int maxlen, const char *fmt, ...);

#ifdef __cplusplus
}
#endif
