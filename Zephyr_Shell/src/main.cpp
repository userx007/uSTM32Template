#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#include <new> 

#include "hd44780_pcf8574.h"
#include "ushell_core.h"
#include "ushell_core_printout.h"
#include "uart_access.h"

#define ENABLE_LCD      1U
#define ENABLE_LED      1U
#define ENABLE_SHELL    0U

/* ── LCD public API — forward declaration ─────────────────────────────── */
void LCD_Post(uint8_t row, uint8_t col, const char *text);


/* ── LED ──────────────────────────────────────────────────────────────────
 *
 * Zephyr uses the devicetree alias "led0" to resolve the LED node.
 * GPIO_DT_SPEC_GET reads the gpios property (pin + flags) at compile time.
 * No hardcoded pin numbers — the board file owns that.
 */
#define LED_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);


/* ── Thread config ────────────────────────────────────────────────────────
 *
 * Lower number = higher priority in Zephyr.
 *
 * LCD     5  — must init hardware first, blocks on k_msgq_get when idle
 * LED     6  — sleeps 99% of the time in k_msleep(2000)
 * Shell   7  — lowest, but wakes instantly on any UART keypress
 *
 * All three spend nearly all their time in blocking calls, so priority
 * only matters for the brief moments they are simultaneously runnable.
 * The shell being "lowest" costs nothing in practice.
 */
#define LED_STACK_SIZE      1024
#define LCD_STACK_SIZE      2048
#define SHELL_STACK_SIZE    2048


#define LED_PRIORITY        5
#define LCD_PRIORITY        6   
#define SHELL_PRIORITY      7   

/* ── LCD message queue ────────────────────────────────────────────────────
 *
 * K_MSGQ_DEFINE allocates storage statically and initialises at boot.
 * No runtime create call needed.
 */
#define LCD_QUEUE_CAPACITY  32
#define LCD_MSG_LEN         32

typedef struct {
    uint8_t row;
    uint8_t col;
    char    text[LCD_MSG_LEN];
} LcdMessage_t;

K_MSGQ_DEFINE(lcd_queue, sizeof(LcdMessage_t), LCD_QUEUE_CAPACITY, alignof(LcdMessage_t));


/* ── LCD-ready semaphore ──────────────────────────────────────────────────
 *
 * LED thread blocks on this until the LCD thread has finished hardware
 * init. Prevents the queue filling up during LCD init/retry, and also
 * unblocks gracefully if the LCD is absent (retry limit reached).
 */
K_SEM_DEFINE(lcd_ready_sem, 0, 1);


/* ── Thread stacks ────────────────────────────────────────────────────────
 *
 * K_THREAD_STACK_DEFINE places the stack in a dedicated section with the
 * alignment and guard pages Zephyr requires. Never use plain arrays.
 */
K_THREAD_STACK_DEFINE(led_stack_area,   LED_STACK_SIZE);
K_THREAD_STACK_DEFINE(shell_stack_area, SHELL_STACK_SIZE);
K_THREAD_STACK_DEFINE(lcd_stack_area,   LCD_STACK_SIZE);

static struct k_thread led_thread_data;
static struct k_thread lcd_thread_data;
static struct k_thread shell_thread_data;


/* ── LCD driver storage ───────────────────────────────────────────────────
 *
 * We deliberately avoid a file-scope C++ object here. With
 * CONFIG_STATIC_INIT_GNU=y, file-scope constructors run before main(),
 * at which point the I2C subsystem may not be ready, leaving the object
 * in a broken state.
 *
 * Instead we use a raw byte buffer and placement new inside the thread,
 * where the I2C driver is guaranteed to be fully initialised.
 * No heap is used — the object lives in this static buffer forever.
 */
#if (1 == ENABLE_LCD)    
static uint8_t lcd_buf[sizeof(HD44780_PCF8574)] alignas(HD44780_PCF8574);
static HD44780_PCF8574 *lcd = nullptr;
#endif /*(1 == ENABLE_LCD)*/

#if (1 == ENABLE_LED)    
/* ── LED thread ───────────────────────────────────────────────────────── */
static void led_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    if (!gpio_is_ready_dt(&led)) {
        printk("LED GPIO not ready\n");
        return;
    }
    gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);

    /* Block until LCD init is complete (or has given up).
     * Either way the semaphore will be given, so LED always starts. */
#if (1 == ENABLE_LCD)    
    k_sem_take(&lcd_ready_sem, K_FOREVER);
#endif /*(1 == ENABLE_LCD)*/

    static int i = 0;
    while (1) {
        gpio_pin_toggle_dt(&led);
        LCD_Post(1, 0, (i = 1 - i) == 0 ? "LED: OFF        " : "LED: ON         ");
        k_msleep(3000);
    }
}
#endif /*(1 == ENABLE_LED)*/


#if (1 == ENABLE_SHELL)    
/* ── Shell thread ─────────────────────────────────────────────────────── */
static void shell_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    Microshell::getShellPtr(pluginEntry(), "root")->Run();
}
#endif /*(1 == ENABLE_SHELL)*/

/* ── LCD post (public API) ────────────────────────────────────────────── */
#if (1 == ENABLE_LCD)    
void LCD_Post(uint8_t row, uint8_t col, const char *text)
{
    LcdMessage_t msg;
    msg.row = row;
    msg.col = col;

    /* Safe string copy — no strncpy dependency */
    uint8_t i = 0;
    while (text[i] && i < LCD_MSG_LEN - 1) {
        msg.text[i] = text[i];
        i++;
    }
    msg.text[i] = '\0';

    /* K_NO_WAIT: never block the caller.
     * printk is safe here — may be called before the shell is running. */
    int ret = k_msgq_put(&lcd_queue, &msg, K_NO_WAIT);
    if (ret != 0) {
        printk("LCD queue full — message dropped (row=%d)\n", row);
    }
}


/* ── LCD thread ───────────────────────────────────────────────────────── */
static void lcd_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    printk("LCD thread started\n");

    /* Construct the driver in-place now that I2C is ready.
     * Placement new: no heap allocation, object lives in lcd_buf. */
    lcd = new (lcd_buf) HD44780_PCF8574(0x27, 16, 2);

    printk("LCD object constructed\n");

    if (!lcd->init()) {
        /* I2C probe failed — wrong address, missing component, or
         * PCF8574 not wired on I2C1 (PB6=SCL, PB7=SDA).
         * Try 0x3F if you have a PCF8574A backpack. */
        printk("LCD I2C FAIL - check address & wiring\n");

        /* Retry with a hard limit so a missing LCD never blocks the
         * LED thread permanently via the semaphore.
         * 20 × 200 ms = 4 seconds maximum wait. */
        int retries = 0;
        while (!lcd->ok() && retries < 20) {
            printk("LCD retry %d...\n", ++retries);
            k_msleep(200);
            lcd->init();
        }

        if (!lcd->ok()) {
            printk("LCD gave up — running without display\n");
            k_sem_give(&lcd_ready_sem);   /* unblock LED regardless */
            return;
        }
    }

    printk("LCD OK\n");
    lcd->clear();
    lcd->setCursor(0, 0);
    lcd->print("System Ready");
    lcd->setCursor(0, 1);
    lcd->print("STM32F103");

    /* Signal LED thread: display is ready, start posting messages */
    k_sem_give(&lcd_ready_sem);
    printk("Semaphore given — LED should start now\n");

    LcdMessage_t msg;
    while (1) {
        /* Block forever until a message arrives */
        if (k_msgq_get(&lcd_queue, &msg, K_FOREVER) == 0) {
            lcd->setCursor(msg.col, msg.row);
            lcd->print(msg.text);
        }
    }
}
#endif /*(1 == ENABLE_LCD)*/

/* ── main ─────────────────────────────────────────────────────────────── */
int main(void)
{
    /* Zephyr has already initialised clocks, SysTick, and the UART
     * console. main() runs as a thread at priority 0 and will be
     * preempted as soon as the created threads are scheduled. */

    uart_setup();

    printk("Entered main..\n");

#if (1 == ENABLE_LED)    
    /* ── LED ──────────────────────────────────────────────────────────── */    
    printk("Starting led..\n");
    k_thread_create(
        &led_thread_data,
        led_stack_area,
        K_THREAD_STACK_SIZEOF(led_stack_area),
        led_thread,
        NULL, NULL, NULL,
        LED_PRIORITY,
        0,
        K_NO_WAIT
    );
    k_thread_name_set(&led_thread_data, "led");
#endif /*(1 == ENABLE_LED)*/

    /* ── LCD ──────────────────────────────────────────────────────────── */
#if (1 == ENABLE_LCD)    
    printk("Starting lcd..\n");
    k_thread_create(
        &lcd_thread_data,
        lcd_stack_area,
        K_THREAD_STACK_SIZEOF(lcd_stack_area),
        lcd_thread_entry,
        NULL, NULL, NULL,
        LCD_PRIORITY,
        0,
        K_NO_WAIT
    );
    k_thread_name_set(&lcd_thread_data, "lcd");
#endif /*(1 == ENABLE_LCD)*/


#if (1 == ENABLE_SHELL)
    /* ── Shell ────────────────────────────────────────────────────────── */
    printk("Starting shell..\n");

    k_thread_create(
        &shell_thread_data,
        shell_stack_area,
        K_THREAD_STACK_SIZEOF(shell_stack_area),
        shell_thread,
        NULL, NULL, NULL,
        SHELL_PRIORITY,
        0,
        K_NO_WAIT
    );
    k_thread_name_set(&shell_thread_data, "shell");
#endif /*(1 == ENABLE_SHELL)*/


    printk("Starting idle..\n");

    /* main() returns — Zephyr's idle thread takes over */
    return 0;
}
