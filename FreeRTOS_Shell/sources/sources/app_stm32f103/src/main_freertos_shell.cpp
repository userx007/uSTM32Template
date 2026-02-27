#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/exti.h>
#include <FreeRTOS.h>
#include <task.h>

#include "ushell_core.h"
#include "uart_access.h"

#include "LcdAO.hpp"
#include "LedAO.hpp"
#include "ButtonAO.hpp"
#include "ao_defs.hpp"


// ── Active Object instances ────────────────────────────────────

static LedAO    ledAO(LED_0);
static LcdAO    lcdAO(LCD_0);

// ── Blink task ─────────────────────────────────────────────────
//
// No longer touches GPIO directly — posts to LedAO and LcdAO.
// This task is intentionally kept as a plain FreeRTOS task since
// its only job is to drive periodic events into the two AOs.
//
static void vTaskBlink(void *pvParameters)
{
    (void)pvParameters;

    bool ledOn = false;

    for (;;)
    {
        // Toggle LED via LedAO
        const Event ev = { SIG_LED_TOGGLE, 0 };
        ledAO.getAO()->post(ev);

        // Update LCD via LcdAO
        lcdAO.print(1, 0, ledOn ? "LED: OFF        "
                                : "LED: ON         ");
        ledOn = !ledOn;

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}


// ── Shell task ─────────────────────────────────────────────────
static void vTaskShell(void *pvParameters)
{
    (void)pvParameters;
    Microshell::getShellPtr(pluginEntry(), "root")->Run();
}

// ── FreeRTOS hooks ─────────────────────────────────────────────
void vApplicationIdleHook(void)          
{ 
    __asm volatile("wfi"); 
}


void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    while (1);
}

void vApplicationMallocFailedHook(void)
{
    while(1) {
        gpio_toggle(GPIOC, GPIO13);
        for(int i = 0; i < 5000000; i++) {
            __asm volatile("nop");   // ← prevents optimizer from removing the loop
        }
    }
}

// ── Hardware init ──────────────────────────────────────────────
static void setup_clock(void)
{
    rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_72MHZ]);
}

static void setup_gpio(void)
{
    rcc_periph_clock_enable(RCC_GPIOB);  // ← belt-and-suspenders for buttons
    rcc_periph_clock_enable(RCC_GPIOC);
    rcc_periph_clock_enable(RCC_AFIO);   // ← needed for EXTI remapping

    // PC13 — onboard LED (active-low, driven by LedAO)
    gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_2_MHZ,
                  GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);
}

// ── Main ───────────────────────────────────────────────────────
int main(void)
{
    setup_clock();
    setup_gpio();
    uart_setup();

    static ButtonAO buttonAO_0(BUTTON_0);
    static ButtonAO buttonAO_1(BUTTON_1);

    buttonAO_0.init();
    buttonAO_1.init();
    ledAO.init();
    lcdAO.init();

    xTaskCreate(vTaskBlink, "Blink", 128,  NULL, 2, NULL);
    xTaskCreate(vTaskShell, "Shell", 512, NULL, 1, NULL);

    vTaskStartScheduler();

    while (1);
    return 0;
}

