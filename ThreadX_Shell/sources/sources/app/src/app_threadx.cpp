#include "tx_api.h"

#include "hd44780_pcf8574.h"
#include "ushell_core.h"
#include "ushell_core_printout.h"
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
void toggle_led(void)
{
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
}

/* ── Thread control blocks ───────────────────────────────────────────────── */
static TX_THREAD led_thread;
static TX_THREAD shell_thread;

/* Stack areas */
static ULONG led_stack[512 / sizeof(ULONG)];
static ULONG shell_stack[1024 / sizeof(ULONG)];

/* --- Thread entry functions --- */

static void led_thread_entry(ULONG initial_input)
{
    (void)initial_input;
    while (1)
    {
        toggle_led();
        tx_thread_sleep(1000);   /* Sleep 50 ticks = 500 ms at 100 Hz */
    }
}

static void shell_thread_entry(ULONG initial_input)
{
    (void)initial_input;
    Microshell::getShellPtr(pluginEntry(), "root")->Run();
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
        (CHAR*)"LED Thread",           /* Name (debug) */
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
        &shell_thread,          /* Control block */
        (CHAR*)"SHELL Thread",         /* Name (debug) */
        shell_thread_entry,     /* Entry function */
        0,                      /* Entry input */
        shell_stack,             /* Stack base */
        sizeof(shell_stack),     /* Stack size in bytes */
        15,                     /* Priority (0=highest, 31=lowest by default) */
        15,                     /* Preemption-threshold */
        TX_NO_TIME_SLICE,       /* Time-slice */
        TX_AUTO_START           /* Auto-start */
    );

    if (status != TX_SUCCESS)
    {
        while (1) {}
    }
}