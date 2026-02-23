#include "tx_api.h"
#include <stdio.h>

#define LED_GREEN 0

void toggle_led( int led );

/* Thread control blocks (can also be dynamically allocated) */
static TX_THREAD led_thread;
static TX_THREAD uart_thread;

/* Stack areas */
static ULONG led_stack[512 / sizeof(ULONG)];
static ULONG uart_stack[1024 / sizeof(ULONG)];

/* --- Thread entry functions --- */

static void led_thread_entry(ULONG initial_input)
{
    (void)initial_input;
    while (1)
    {
        toggle_led(LED_GREEN);
        tx_thread_sleep(50);   /* Sleep 50 ticks = 500 ms at 100 Hz */
    }
}

static void uart_thread_entry(ULONG initial_input)
{
    (void)initial_input;
    UINT tick = 0;
    while (1)
    {
        printf("Tick: %u\r\n", tick++);
        tx_thread_sleep(100);  /* 1 second */
    }
}

/* --- Kernel entry point (called by tx_kernel_enter) --- */

void tx_application_define(void *first_unused_memory)
{
    UINT status;

    /* Create LED thread: priority 10, preemption-threshold 10 (disabled),
       no time-slice, auto-start */
    status = tx_thread_create(
        &led_thread,            /* Control block */
        "LED Thread",           /* Name (debug) */
        led_thread_entry,       /* Entry function */
        0,                      /* Entry input */
        led_stack,              /* Stack base */
        sizeof(led_stack),      /* Stack size in bytes */
        10,                     /* Priority (0=highest, 31=lowest by default) */
        10,                     /* Preemption-threshold */
        TX_NO_TIME_SLICE,       /* Time-slice */
        TX_AUTO_START           /* Auto-start */
    );

    if (status != TX_SUCCESS)
    {
        /* Handle error — typically halt or assert */
        while (1) {}
    }

    /* Create UART thread at lower priority */
    status = tx_thread_create(
        &uart_thread, "UART Thread", uart_thread_entry, 0,
        uart_stack, sizeof(uart_stack),
        15, 15, TX_NO_TIME_SLICE, TX_AUTO_START
    );

    if (status != TX_SUCCESS)
    {
        while (1) {}
    }
}