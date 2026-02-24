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

#define MS_TO_TICKS(ms)  ((ms) * TX_TIMER_TICKS_PER_SECOND / 1000)

/*
 * Onboard LED pinout:
 *   STM32F411CEU6 (Black Pill) : PC13 — active LOW
 *   STM32F103C8T6 (Blue Pill)  : PC13 — active LOW
 */


/* ── Interfaces declaration ─────────────────────────────────────────────── */

bool queue_is_valid(TX_QUEUE *q);
void LCD_Post(uint8_t row, uint8_t col, const char *text);


/* ── LED ────────────────────────────────────────────────────────────────── */

static TX_THREAD led_thread;
static ULONG led_stack[512 / sizeof(ULONG)];

/* LED init */
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

/* LED toggle (called from led_thread) */
void toggle_led(void)
{
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
}

/* Thread entry function */
static void led_thread_entry(ULONG initial_input)
{
    (void)initial_input;
    static int i = 0;

    while (1) {
        toggle_led();
        LCD_Post(1, 0, (i = 1 - i) == 0 ? "LED: OFF        " : "LED: ON         ");
        tx_thread_sleep(MS_TO_TICKS(2000));   /* Sleep 2000 ms */
    }
}


/* ── SHELL ──────────────────────────────────────────────────────────────── */

static TX_THREAD shell_thread;
static ULONG shell_stack[1024 / sizeof(ULONG)];

static void shell_thread_entry(ULONG initial_input)
{
    (void)initial_input;
    Microshell::getShellPtr(pluginEntry(), "root")->Run();
}


/* ── LCD ────────────────────────────────────────────────────────────────── */

#define LCD_MSG_LEN   32   /* Max characters per message */

typedef struct {
    uint8_t row;
    uint8_t col;
    char    text[LCD_MSG_LEN];
} LcdMessage_t;

#define LCD_QUEUE_MSG_WORDS     ((sizeof(LcdMessage_t) + sizeof(ULONG) - 1) / sizeof(ULONG))
#define LCD_QUEUE_CAPACITY      5          

static TX_THREAD lcd_thread;
static TX_QUEUE  lcd_queue;
static ULONG lcd_stack[2048 / sizeof(ULONG)];
static ULONG lcd_queue_storage[LCD_QUEUE_CAPACITY * LCD_QUEUE_MSG_WORDS];

void LCD_Post(uint8_t row, uint8_t col, const char *text) {

    if (!queue_is_valid(&lcd_queue)) {
        uSHELL_PRINTF("Queue is invalid..\n");
        return;
    }

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

    UINT status = tx_queue_send(&lcd_queue, &msg, TX_NO_WAIT);
    if (status != TX_SUCCESS){
        uSHELL_PRINTF("Failed to send LCD message\n");
    }
}

static void lcd_thread_entry(ULONG initial_input)
{
    (void)initial_input;

    static HD44780_PCF8574 lcd(0x27, 16, 2);   /* PCF8574 at 0x27, 16x2 display */

    if (!lcd.init()) {
        /* I2C probe failed: wrong address, missing component, or
         * PICSimLab PCF8574 not connected on I2C1 (PB6=SCL, PB7=SDA).
         * Try 0x3F if you have a PCF8574A backpack. */
        uSHELL_PRINTF("LCD I2C FAIL - check address & wiring\n");
        while (!lcd.ok()) {
            uSHELL_PRINTF("LCD retry...\n");
            tx_thread_sleep(MS_TO_TICKS(50)); /* 50 ms */
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
        if (tx_queue_receive(&lcd_queue, &msg, TX_WAIT_FOREVER) == TX_SUCCESS) {
            lcd.setCursor(msg.col, msg.row);
            lcd.print(msg.text);
        }
    }    
}


#ifdef __cplusplus
 extern "C" {
#endif

/* ───────────────────────────────────────────────────────────────────────── */
/* ── Kernel entry point ─────────────────────────────────────────────────── */
/* ───────────────────────────────────────────────────────────────────────── */
void tx_application_define(void *first_unused_memory)
{
    (void)first_unused_memory;
    UINT status;

    led_init();

    status = tx_queue_create(
        &lcd_queue,                     /* Control block          */
        (CHAR*)"LCD Queue",             /* Name                   */
        LCD_QUEUE_MSG_WORDS,            /* message size           */
        lcd_queue_storage,              /* Storage buffer         */
        sizeof(lcd_queue_storage)       /* Buffer size in bytes   */
    );

    if (status != TX_SUCCESS){
        uSHELL_PRINTF("Failed to create LCD queue\n");
        while (1); /* Handle error — queue creation failed */
    }

    /* ── LED ───────────────────────────────────── */
    /* Create LED thread: priority 10, preemption-threshold 10 (disabled),
       no time-slice, auto-start */
    status = tx_thread_create(
        &led_thread,            /* Control block */
        (CHAR*)"LED Thread",           /* Name (debug) */
        led_thread_entry,       /* Entry function */
        0,                      /* Entry input */
        led_stack,              /* Stack base */
        sizeof(led_stack),      /* Stack size in bytes */
        29,                     /* Priority (0=highest, 31=lowest by default) */
        29,                     /* Preemption-threshold */
        TX_NO_TIME_SLICE,       /* Time-slice */
        TX_AUTO_START           /* Auto-start */
    );

    if (status != TX_SUCCESS) {
        uSHELL_PRINTF("Failed to create LED thread\n");
        while (1) {} /* Handle error — typically halt or assert */
    }

    /* ── LCD ───────────────────────────────────── */
    /* Create UART thread at lower priority */
    status = tx_thread_create(
        &lcd_thread,            /* Control block */
        (CHAR*)"LCD Thread",    /* Name (debug) */
        lcd_thread_entry,       /* Entry function */
        0,                      /* Entry input */
        lcd_stack,              /* Stack base */
        sizeof(lcd_stack),      /* Stack size in bytes */
        30,                     /* Priority (0=highest, 31=lowest by default) */
        30,                     /* Preemption-threshold */
        TX_NO_TIME_SLICE,       /* Time-slice */
        TX_AUTO_START           /* Auto-start */
    );

    if (status != TX_SUCCESS) {
        uSHELL_PRINTF("Failed to create LCD thread\n");
        while (1) {} /* Handle error — typically halt or assert */
    }

    /* ── SHELL ─────────────────────────────────── */
    /* Create Shell thread at lower priority */
    status = tx_thread_create(
        &shell_thread,          /* Control block */
        (CHAR*)"SHELL Thread",  /* Name (debug) */
        shell_thread_entry,     /* Entry function */
        0,                      /* Entry input */
        shell_stack,            /* Stack base */
        sizeof(shell_stack),    /* Stack size in bytes */
        31,                     /* Priority (0=highest, 31=lowest by default) */
        31,                     /* Preemption-threshold */
        TX_NO_TIME_SLICE,       /* Time-slice */
        TX_AUTO_START           /* Auto-start */
    );

    if (status != TX_SUCCESS) {
        uSHELL_PRINTF("Failed to create SHELL thread\n");
        while (1) {} /* Handle error — typically halt or assert */
    }    
}

#ifdef __cplusplus
}
#endif

/* ───────────────────────────────────────────────────────────────────────── */
/* ── Helpers ────────────────────────────────────────────────────────────── */
/* ───────────────────────────────────────────────────────────────────────── */

bool queue_is_valid(TX_QUEUE *q)
{
    return tx_queue_info_get(q, NULL, NULL, NULL, NULL, NULL, NULL) == TX_SUCCESS;
}
