#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/nvic.h> 
#include <FreeRTOS.h>
#include <task.h>

#include "ushell_core.h"
#include "ushell_core_printout.h"
#include "uart_access.h"


static void setup_clock(void) {
    /* Setup 100MHz from 8MHz HSE crystal */
    rcc_clock_setup_pll(&rcc_hse_8mhz_3v3[RCC_CLOCK_3V3_84MHZ]);
    
    /* Alternative: Use 25MHz HSE (common on some F411 boards)
     * rcc_clock_setup_pll(&rcc_hse_25mhz_3v3[RCC_CLOCK_3V3_84MHZ]);
     */
    
    /* Alternative: Use internal 16MHz oscillator
     * rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_3V3_84MHZ]);
     */
}

static void setup_gpio(void) {
    rcc_periph_clock_enable(RCC_GPIOC);
    
    /* Configure PC13 as output */
    gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO13);
    gpio_set_output_options(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, GPIO13);
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

    // Ensure FreeRTOS can manage interrupts properly
    //NVIC_SetPriorityGrouping(NVIC_PRIGROUP_GROUP4_NOSUB); // 4 bits for pre-emption priority

    xTaskCreate(vTaskBlink, "Blink", 128, NULL, 2, NULL);
    xTaskCreate(vTaskShell, "Shell", 1024, NULL, 1, NULL);

    
    vTaskStartScheduler();
    
    /* Should never reach here */
    while (1);
    
    return 0;
}