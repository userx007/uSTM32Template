#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/nvic.h>
#include <FreeRTOS.h>
#include <task.h>

#include "ushell_core.h"
#include "ushell_core_printout.h"
#include "uart_access.h"


static void setup_clock(void) {
    /* Setup 72MHz from 8MHz HSE crystal (standard for STM32F103) */
    rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_72MHZ]);

    /* Alternative: Use internal 8MHz oscillator scaled to 64MHz
     * rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_HSI_64MHZ]);
     */
}

static void setup_gpio(void) {
    rcc_periph_clock_enable(RCC_GPIOC);

    /*
     * STM32F103 uses the older libopencm3 GPIO API (no gpio_mode_setup).
     * gpio_set_mode() combines mode + CNF in a single call.
     * PC13 is the built-in LED on most Blue Pill / Maple Mini boards.
     */
    gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_2_MHZ,
                  GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);
}

void vTaskBlink(void *pvParameters) {
    (void)pvParameters;
    static int i = 0;

    while (1) {
        gpio_toggle(GPIOC, GPIO13);
        uSHELL_PRINTF(i == 0 ? "OFF\n" : "ON\n");
        i = 1 - i;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void vTaskShell(void *pvParameters) {
    (void)pvParameters;

    Microshell::getShellPtr(pluginEntry(), "root")->Run();
}

void vApplicationMallocFailedHook(void) {
    /* Called if a call to pvPortMalloc() fails */
    while (1);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    (void)pcTaskName;
    /* Called if a task overflows its stack */
    while (1);
}

int main(void) {
    setup_clock();
    setup_gpio();
    uart_setup();

    xTaskCreate(vTaskBlink, "Blink", 128, NULL, 2, NULL);
    xTaskCreate(vTaskShell, "Shell", 1024, NULL, 1, NULL);

    vTaskStartScheduler();

    /* Should never reach here */
    while (1);

    return 0;
}
