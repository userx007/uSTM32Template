/*
 * main.c — STM32F1 / STM32F4 + Azure RTOS ThreadX
 *
 * Responsibilities:
 *   1. HAL_Init()            – flash latency, SysTick 1 ms base
 *   2. SystemClock_Config()  – bring up PLL (72 MHz F1 / 100 MHz F4)
 *   3. uart_init()           – UART peripheral before any uart_printf
 *   4. tx_kernel_enter()     – hand control to ThreadX (never returns)
 *
 * NOTE: Do NOT start SysTick yourself. ThreadX takes ownership of it
 *       inside tx_kernel_enter() via tx_initialize_low_level().
 */

#if defined(STM32F4)
#  include "stm32f4xx_hal.h"
#elif defined(STM32F1)
#  include "stm32f1xx_hal.h"
#else
#  error "Define STM32F4 or STM32F1 in your build system."
#endif

#include "tx_api.h"
#include "uart_access.h"

/* ── Forward declarations ───────────────────────────────────────────────── */
void tx_application_define(void *first_unused_memory);   /* app_threadx.c */
static void SystemClock_Config(void);

/* ── main ───────────────────────────────────────────────────────────────── */
int main(void)
{
    /* 1. Init HAL (resets peripherals, sets flash latency for current clock,
          configures SysTick at 1 kHz — ThreadX will reprogram it later). */
    HAL_Init();

    /* 2. Configure PLL and switch to the target system clock. */
    SystemClock_Config();

    /* 3. Init UART so uart_printf() works from the very first thread tick. */
    uart_setup();

    /* 4. Start the ThreadX kernel — calls tx_application_define() then
          schedules threads.  Never returns. */
    tx_kernel_enter();

    /* Unreachable */
    return 0;
}

/* ── Clock configuration ────────────────────────────────────────────────── */

#if defined(STM32F4)
/*
 * STM32F411CEU6 — target 100 MHz from 25 MHz HSE (Black Pill crystal).
 *
 *   HSE 25 MHz -> /M=25 -> 1 MHz PLL input
 *   x N=200    -> 200 MHz VCO
 *   / P=2      -> 100 MHz SYSCLK
 *   / Q=4      ->  50 MHz USB/SDIO (adjust if you need 48 MHz USB)
 *
 * APB1 max = 50 MHz  -> /2  -> 50 MHz
 * APB2 max = 100 MHz -> /1  -> 100 MHz
 */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_ON;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM       = 25;
    osc.PLL.PLLN       = 200;
    osc.PLL.PLLP       = RCC_PLLP_DIV2;
    osc.PLL.PLLQ       = 4;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
        while (1) {}   /* clock fault — halt */

    RCC_ClkInitTypeDef clk = {0};
    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                         RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    /* Flash latency: 3 WS for 100 MHz at VCC 3.3 V (ref. manual Table 6) */
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_3) != HAL_OK)
        while (1) {}
}

#elif defined(STM32F1)
/*
 * STM32F103C8T6 — target 72 MHz from 8 MHz HSE (Blue Pill crystal).
 *
 *   HSE 8 MHz -> PLL x 9 -> 72 MHz SYSCLK
 *
 * APB1 max = 36 MHz  -> /2  -> 36 MHz
 * APB2 max = 72 MHz  -> /1  -> 72 MHz
 */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_ON;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL     = RCC_PLL_MUL9;   /* 8 x 9 = 72 MHz */
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
        while (1) {}

    RCC_ClkInitTypeDef clk = {0};
    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                         RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;   /* 36 MHz max on APB1 */
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    /* Flash latency: 2 WS for 72 MHz (ref. manual Table 6) */
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK)
        while (1) {}
}
#endif