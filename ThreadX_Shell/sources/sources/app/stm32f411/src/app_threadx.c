#include "tx_api.h"
#include "uart_access.h"

#if defined(STM32F4)
#  include "stm32f4xx_hal.h"
#elif defined(STM32F1)
#  include "stm32f1xx_hal.h"
#endif

/*
 * Onboard LED pinout:
 *   STM32F411CEU6 (Black Pill) : PC13 — active LOW
 *   STM32F103C8T6 (Blue Pill)  : PC13 — active LOW
 *
 * Adjust GPIO_PIN / GPIOx below if your board differs.
 */
#define LED_GREEN 0

/* ── LED init ────────────────────────────────────────────────────────────── */
static void led_init(void)
{
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {};
    gpio.Pin   = GPIO_PIN_13;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &gpio);

    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET); /* LED off (active LOW) */
}

/* ── LED toggle (called from led_thread) ─────────────────────────────────── */
void toggle_led(int led)
{
    (void)led; /* only one LED — extend with a switch if you add more */
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
}

/* ── Thread control blocks ───────────────────────────────────────────────── */
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
        uart_printf("Tick: %d\r\n", tick++);
        tx_thread_sleep(100);  /* 100 ticks = 1 s at 100 Hz */
    }
}

/* ── Kernel entry point ──────────────────────────────────────────────────── */
void tx_application_define(void *first_unused_memory)
{
    (void)first_unused_memory;

    led_init();

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