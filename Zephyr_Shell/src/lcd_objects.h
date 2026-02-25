#pragma once

/*
 * lcd_objects.h — shared declarations for Zephyr kernel objects used by
 * the LCD subsystem.
 *
 * RULE: All K_MSGQ_DEFINE / K_SEM_DEFINE / K_THREAD_STACK_DEFINE calls
 * live in lcd_objects.c (a plain C file).  These macros use C99 designated
 * initialisers and Zephyr's STRUCT_SECTION_ITERABLE linker magic; placing
 * them in a .cpp translation unit corrupts the initialisation (wrong section
 * placement, used_msgs starts at 32, pointers misaligned).
 */

#include <zephyr/kernel.h>

/* ── Sizing constants (shared between .c and .cpp) ──────────────────── */
#define LCD_QUEUE_CAPACITY  32
#define LCD_MSG_LEN         32

#define LED_STACK_SIZE      1024
#define LCD_STACK_SIZE      4096
#define SHELL_STACK_SIZE    2048

/* ── Message type ────────────────────────────────────────────────────── */
/*
 * _pad[2] makes the struct 36 bytes and naturally 4-byte aligned on ARM.
 * sizeof(LcdMessage_t) must equal lcd_queue.msg_size — if you change this
 * struct, do a pristine rebuild (west build -p always) so both sides agree.
 */
typedef struct {
    uint8_t row;
    uint8_t col;
    uint8_t _pad[2];
    char    text[LCD_MSG_LEN];
} LcdMessage_t;

/* ── Kernel object declarations ──────────────────────────────────────── */
#ifdef __cplusplus
extern "C" {
#endif

extern struct k_msgq lcd_queue;
extern struct k_sem  lcd_ready_sem;

extern struct k_thread led_thread_data;
extern struct k_thread lcd_thread_data;
extern struct k_thread shell_thread_data;

/* Must declare with explicit size so K_THREAD_STACK_SIZEOF(sym)
 * can apply sizeof() to a complete array type in main.cpp. */
extern k_thread_stack_t led_stack_area[LED_STACK_SIZE];
extern k_thread_stack_t lcd_stack_area[LCD_STACK_SIZE];
extern k_thread_stack_t shell_stack_area[SHELL_STACK_SIZE];

#ifdef __cplusplus
}
#endif
