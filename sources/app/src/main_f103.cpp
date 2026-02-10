#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <FreeRTOS.h>
#include <task.h>

static void setup_clock(void) {
    rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_72MHZ]);
}

static void setup_gpio(void) {
    rcc_periph_clock_enable(RCC_GPIOC);
    gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_2_MHZ,
                  GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);
}

void vTaskBlink(void *pvParameters) {
    (void)pvParameters;
    
    while (1) {
        gpio_toggle(GPIOC, GPIO13);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
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
    
    xTaskCreate(vTaskBlink, "Blink", 128, NULL, 1, NULL);
    
    vTaskStartScheduler();
    
    /* Should never reach here */
    while (1);
    
    return 0;
}