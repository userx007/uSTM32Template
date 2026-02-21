#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/nvic.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include "hd44780_pcf8574.h"
#include "ushell_core.h"
#include "ushell_core_printout.h"
#include "uart_access.h"

/* ── LCD message queue ───────────────────────────────────────────────────── */

#define LCD_MSG_LEN   32   /* Max characters per message */

typedef struct {
    uint8_t row;
    uint8_t col;
    char    text[LCD_MSG_LEN];
} LcdMessage_t;

/* Other tasks post to this queue to display text */
static QueueHandle_t xLcdQueue = nullptr;



static void setup_clock(void) {
    /* Setup 72MHz from 8MHz HSE crystal (standard for STM32F103) */
    rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_72MHZ]);
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


/* ── Helper: post a message to the LCD task ──────────────────────────────── */

void LCD_Post(uint8_t row, uint8_t col, const char *text) {
    if (!xLcdQueue) 
        return;

    LcdMessage_t msg;
    msg.row = row;
    msg.col = col;

    /* Safe string copy */
    uint8_t i = 0;
    while (text[i] && i < LCD_MSG_LEN - 1) {
        msg.text[i] = text[i];
        i++;
    }
    msg.text[i] = '\0';

    xQueueSend(xLcdQueue, &msg, pdMS_TO_TICKS(10));
}

/* ── LCD task ────────────────────────────────────────────────────────────── */

void vTaskLCD(void *pvParameters) {
    (void)pvParameters;

    static HD44780_PCF8574 lcd(0x27, 16, 2);   /* PCF8574 at 0x27, 16x2 display */

    if (!lcd.init()) {
        /* I2C probe failed: wrong address, missing component, or
         * PICSimLab PCF8574 not connected on I2C1 (PB6=SCL, PB7=SDA).
         * Try 0x3F if you have a PCF8574A backpack. */
        uSHELL_PRINTF("LCD I2C FAIL - check address & wiring\n");
        while (!lcd.ok()) {
            uSHELL_PRINTF("LCD retry...\n");
            vTaskDelay(pdMS_TO_TICKS(2000));
            lcd.init();
        }
    }

    uSHELL_PRINTF("LCD OK\n");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("System Ready");
    lcd.setCursor(0, 1);
    lcd.print("STM32F103");

    LcdMessage_t msg;

    while (1) {
        /* Block until a message arrives (no timeout = wait forever) */
        if (xQueueReceive(xLcdQueue, &msg, portMAX_DELAY) == pdTRUE) {
            lcd.setCursor(msg.col, msg.row);
            lcd.print(msg.text);
        }
    }
}


void vTaskBlink(void *pvParameters) {
    (void)pvParameters;
    static int i = 0;

    while (1) {
        gpio_toggle(GPIOC, GPIO13);

        LCD_Post(1, 0, i == 0 ? "LED: OFF        " : "LED: ON         ");

        i = 1 - i;

        vTaskDelay(pdMS_TO_TICKS(2000));
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

void vApplicationIdleHook(void) {
    __asm volatile("wfi");
}


int main(void) {
    setup_clock();
    setup_gpio();
    uart_setup();

    /* Create the queue before any task that uses it */
    xLcdQueue = xQueueCreate(8, sizeof(LcdMessage_t));



    xTaskCreate(vTaskLCD,   "LCD",   512, NULL, 3, NULL);   /* Highest: owns I2C */
    xTaskCreate(vTaskBlink, "Blink", 128, NULL, 2, NULL);
    xTaskCreate(vTaskShell, "Shell", 1024, NULL, 1, NULL);

    vTaskStartScheduler();

    /* Should never reach here */
    while (1);

    return 0;
}
