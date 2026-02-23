    .syntax unified
    .cpu cortex-m4
    .thumb

    /* System clock and desired tick rate */
    .equ SYSTEM_CLOCK,  168000000   /* 168 MHz */
    .equ SYSTICK_CYCLES, (SYSTEM_CLOCK / 100)  /* 100 Hz => 10 ms tick */

    .global  _tx_initialize_low_level
    .type    _tx_initialize_low_level, %function

_tx_initialize_low_level:
    /* Set PendSV to lowest priority (0xFF) */
    LDR  r0, =0xE000ED22
    MOV  r1, #0xFF
    STRB r1, [r0]

    /* Configure SysTick */
    LDR  r0, =0xE000E014        /* SysTick RELOAD register */
    LDR  r1, =SYSTICK_CYCLES-1
    STR  r1, [r0]

    LDR  r0, =0xE000E010        /* SysTick CTRL register */
    MOV  r1, #0x7               /* Enable, interrupt, use core clock */
    STR  r1, [r0]

    BX   lr
