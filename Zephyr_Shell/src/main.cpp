#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#include <new>

#include "lcd_objects.h"   /* kernel objects defined in lcd_objects.c */
#include "hd44780_pcf8574.h"
#include "ushell_core.h"
#include "ushell_core_printout.h"
#include "uart_access.h"

#define ENABLE_LCD      1U
#define ENABLE_LED      1U
#define ENABLE_SHELL    1U

/* ── LCD public API — forward declaration ─────────────────────────────── */
/* Provide a no-op fallback when LCD is disabled so any caller
 * (e.g. the LED thread) still links cleanly regardless of ENABLE_LCD. */
#if (1 == ENABLE_LCD)
void LCD_Post(uint8_t row, uint8_t col, const char *text);
#else
static inline void LCD_Post(uint8_t /*row*/, uint8_t /*col*/, const char * /*text*/) {}
#endif


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
 * LCD must be the most urgent of the three so it
 * always drains the queue before the LED thread can post the next item.
 *
 * LCD     4  — services the display queue; must run before LED can post
 * LED     5  — sleeps 99% of the time in k_msleep(3000)
 * Shell   6  — lowest, wakes instantly on any UART keypress
 */
/* Stack sizes are defined in lcd_objects.h */

#define LCD_PRIORITY        4   /* highest of the three — owns the queue  */
#define LED_PRIORITY        5
#define SHELL_PRIORITY      6

/* ── LCD hardware constants ───────────────────────────────────────────────
 *
  * Try 0x3F if you have a PCF8574A backpack instead of PCF8574.
 */
#define LCD_I2C_ADDR        0x27
#define LCD_COLS            16
#define LCD_ROWS            2

/* ── LCD message queue, semaphore, stacks, and thread data ──────────────
 * Defined in lcd_objects.c (must be a .c file — see that file for why).
 * Declared via lcd_objects.h, already included above.
 */

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


/* ── Helper macro: silence unused thread-arg warnings ────────────────────
 */
#define THREAD_UNUSED_ARGS  ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3)


#if (1 == ENABLE_LED)
/* ── LED thread ───────────────────────────────────────────────────────── */
static void led_thread(void *p1, void *p2, void *p3)
{
    THREAD_UNUSED_ARGS;

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

    int i = 0;
    while (1) {
        gpio_pin_toggle_dt(&led);
        LCD_Post(1, 0, (i = 1 - i) == 0 ? "LED: ON         " : "LED: OFF        ");
        k_msleep(3000);
    }
}
#endif /*(1 == ENABLE_LED)*/


#if (1 == ENABLE_SHELL)
/* ── Shell thread ─────────────────────────────────────────────────────── */
static void shell_thread(void *p1, void *p2, void *p3)
{
    THREAD_UNUSED_ARGS;

    Microshell::getShellPtr(pluginEntry(), "root")->Run();
}
#endif /*(1 == ENABLE_SHELL)*/


/* ── LCD post (public API) ────────────────────────────────────────────── */
#if (1 == ENABLE_LCD)
void LCD_Post(uint8_t row, uint8_t col, const char *text)
{
    LcdMessage_t msg;

    /* Zero-initialise the whole struct first, then copy.
     *
     *  a) strlcpy is not available in picolibc unless CONFIG_POSIX_API=y;
     *     without it the linker resolves to a weak stub that copies nothing,
     *     leaving msg.text as uninitialised stack garbage (or all-zeros if
     *     the frame happened to be zeroed).
     *  b) Partial field assignment (row, col, text individually) left the
     *     _pad bytes uninitialised.  Some compiler optimisation levels can
     *     reorder or coalesce the assignments in ways that interact badly
     *     with the k_msgq memcpy.
     *
     * Zero-initialising the struct first guarantees every byte sent through
     * the queue is deterministic, and the manual copy loop works on every
     * Zephyr libc variant without any Kconfig requirement. */
    memset(&msg, 0, sizeof(msg));
    {
        size_t i = 0;
        while (text[i] && i < sizeof(msg.text) - 1) {
            msg.text[i] = text[i];
            i++;
        }
        /* msg.text[i] is already '\0' from the memset */
    }
    msg.row = row;
    msg.col = col;

    /* K_NO_WAIT: never block the caller. */
    int ret = k_msgq_put(&lcd_queue, &msg, K_NO_WAIT);
    if (ret != 0) {
        printk("LCD queue full — message dropped (row=%d)\n", row);
    }
}


/* ── LCD thread ───────────────────────────────────────────────────────── */
static void lcd_thread_entry(void *p1, void *p2, void *p3)
{
    THREAD_UNUSED_ARGS;

    printk("LCD thread started\n");

    /* Construct the driver in-place now that I2C is ready.
     * Placement new: no heap allocation, object lives in lcd_buf.
    */
    lcd = new (lcd_buf) HD44780_PCF8574(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS);

    printk("LCD object constructed\n");

    /* 
     * înit() idempotency guard:
     * Some HD44780 drivers re-initialise I2C peripheral state on every
     * call to init(), which can leave the bus in an undefined condition
     * if called while already running.  We call init() exactly once per
     * retry, only after confirming ok() is still false.
     * If your driver's init() is safe to call repeatedly this is a no-op
     * improvement; if it is not, this prevents a subtle I2C bus hang.
     *
     * 20 × 200 ms = 4 seconds maximum wait.
     */

    #define LCD_INIT_RETRIES    20
    #define LCD_INIT_RETRY_MS   200

    {
        int retries = 0;
        while (retries < LCD_INIT_RETRIES) {
            if (lcd->init()) {
                break;             /* success — stop retrying immediately */
            }
            printk("LCD I2C FAIL — retry %d/%d (check addr 0x%02X & wiring)\n",
                   ++retries, LCD_INIT_RETRIES, LCD_I2C_ADDR);
            k_msleep(LCD_INIT_RETRY_MS);
        }
    }

    if (!lcd->ok()) {
        printk("LCD gave up after %d retries — running without display\n",
               LCD_INIT_RETRIES);

        /* Explicitly destroy the placement-new object before
         * returning so any resources it holds are released cleanly. */
        lcd->~HD44780_PCF8574();
        lcd = nullptr;

        k_sem_give(&lcd_ready_sem);   /* unblock LED regardless */
        return;
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

    printk("LCD entering message loop\n");   /* sentinel — must appear in log */

    LcdMessage_t msg;
    while (1) {
        /* Block forever until a message arrives.
         * Any printk here will confirm the thread is alive and draining. */
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

    printk("Entered main\n");

#if (1 == ENABLE_LED)
    /* ── LED ──────────────────────────────────────────────────────────── */
    printk("Starting led thread\n");
    /* Check k_thread_create return value — a NULL tid means the
     * thread was not created (e.g. out of thread objects or bad params). */
    k_tid_t led_tid = k_thread_create(
        &led_thread_data,
        led_stack_area,
        K_THREAD_STACK_SIZEOF(led_stack_area),
        led_thread,
        NULL, NULL, NULL,
        LED_PRIORITY,
        0,
        K_NO_WAIT
    );
    if (!led_tid) {
        printk("ERROR: failed to create LED thread\n");
    } else {
        k_thread_name_set(&led_thread_data, "led");
    }
#endif /*(1 == ENABLE_LED)*/


    /* ── LCD ──────────────────────────────────────────────────────────── */
#if (1 == ENABLE_LCD)
    printk("Starting lcd thread\n");
    k_tid_t lcd_tid = k_thread_create(
        &lcd_thread_data,
        lcd_stack_area,
        K_THREAD_STACK_SIZEOF(lcd_stack_area),
        lcd_thread_entry,
        NULL, NULL, NULL,
        LCD_PRIORITY,
        0,
        K_NO_WAIT
    );
    if (!lcd_tid) {
        printk("ERROR: failed to create LCD thread\n");
        /* Give the semaphore so the LED thread is never permanently blocked
         * if LCD thread creation itself fails. */
        k_sem_give(&lcd_ready_sem);
    } else {
        k_thread_name_set(&lcd_thread_data, "lcd");
    }
#endif /*(1 == ENABLE_LCD)*/


#if (1 == ENABLE_SHELL)
    /* ── Shell ────────────────────────────────────────────────────────── */
    printk("Starting shell thread\n");
    k_tid_t shell_tid = k_thread_create(
        &shell_thread_data,
        shell_stack_area,
        K_THREAD_STACK_SIZEOF(shell_stack_area),
        shell_thread,
        NULL, NULL, NULL,
        SHELL_PRIORITY,
        0,
        K_NO_WAIT
    );
    if (!shell_tid) {
        printk("ERROR: failed to create shell thread\n");
    } else {
        k_thread_name_set(&shell_thread_data, "shell");
    }
#endif /*(1 == ENABLE_SHELL)*/


    printk("All threads started — entering idle\n");

    /* main() returns — Zephyr's idle thread takes over */
    return 0;
}
