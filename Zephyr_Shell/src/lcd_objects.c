/*
 * lcd_objects.c — definitions of all Zephyr kernel objects for the LCD
 * subsystem.
 *
 * MUST remain a .c file.  K_MSGQ_DEFINE and friends use C99 designated
 * initialisers and STRUCT_SECTION_ITERABLE; they do not initialise
 * correctly from a C++ translation unit (wrong linker section, corrupted
 * used_msgs / pointer fields).
 */

#include "lcd_objects.h"

/* ── Message queue ───────────────────────────────────────────────────── */
/*
 * Alignment = 4: Cortex-M performs word-aligned loads/stores internally
 * in k_msgq_put/get.  alignof(LcdMessage_t) would be 1 (all byte fields),
 * which caused the buffer to land on an odd address and every received
 * message to read as zeros even though the put side had correct data.
 */
K_MSGQ_DEFINE(lcd_queue, sizeof(LcdMessage_t), LCD_QUEUE_CAPACITY, 4);

/* ── LCD-ready semaphore ─────────────────────────────────────────────── */
K_SEM_DEFINE(lcd_ready_sem, 0, 1);

/* ── Thread stacks ───────────────────────────────────────────────────── */
K_THREAD_STACK_DEFINE(led_stack_area,   LED_STACK_SIZE);
K_THREAD_STACK_DEFINE(lcd_stack_area,   LCD_STACK_SIZE);
K_THREAD_STACK_DEFINE(shell_stack_area, SHELL_STACK_SIZE);

/* ── Thread control blocks ───────────────────────────────────────────── */
struct k_thread led_thread_data;
struct k_thread lcd_thread_data;
struct k_thread shell_thread_data;
