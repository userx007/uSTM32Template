#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

/* ── LED ──────────────────────────────────────────────────────────────────
 *
 * Zephyr uses the devicetree alias "led0" to resolve the LED node.
 * GPIO_DT_SPEC_GET reads the gpios property (pin + flags) at compile time.
 * No hardcoded pin numbers — the board file owns that.
 */
#define LED_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);

/* ── Thread stacks ────────────────────────────────────────────────────────
 *
 * K_THREAD_STACK_DEFINE allocates stack in a special memory section with
 * correct alignment. Do NOT use plain arrays — they lack the guard pages
 * and alignment that Zephyr requires.
 */
#define LED_STACK_SIZE   512
#define UART_STACK_SIZE  1024
#define LED_PRIORITY     5
#define UART_PRIORITY    6    /* Lower number = higher priority in Zephyr
                                 (opposite convention to ThreadX's 0=highest
                                  but same direction: lower int = higher prio) */

K_THREAD_STACK_DEFINE(led_stack_area,  LED_STACK_SIZE);
K_THREAD_STACK_DEFINE(uart_stack_area, UART_STACK_SIZE);

static struct k_thread led_thread_data;
static struct k_thread uart_thread_data;


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

    while (1) {
        gpio_pin_toggle_dt(&led);
        k_msleep(500);   /* 500 ms — takes real milliseconds, not ticks */
    }
}


/* ── UART thread ──────────────────────────────────────────────────────── */
static void uart_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    uint32_t count = 0;

    while (1) {
        printk("Tick: %u\n", count++);
        k_msleep(1000);  /* 1 second */
    }
}


/* ── main ─────────────────────────────────────────────────────────────── */
int main(void)
{
    /* Zephyr has already run: clock init, SysTick, UART console.
       main() is itself a thread (the main thread) at priority 0. */

    k_thread_create(
        &led_thread_data,               /* Thread control block   */
        led_stack_area,                 /* Stack buffer           */
        K_THREAD_STACK_SIZEOF(led_stack_area),  /* Stack size     */
        led_thread,                     /* Entry function         */
        NULL, NULL, NULL,               /* p1, p2, p3 parameters  */
        LED_PRIORITY,                   /* Priority               */
        0,                              /* Options                */
        K_NO_WAIT                       /* Start immediately      */
    );

    k_thread_name_set(&led_thread_data, "led");

    k_thread_create(
        &uart_thread_data,
        uart_stack_area,
        K_THREAD_STACK_SIZEOF(uart_stack_area),
        uart_thread,
        NULL, NULL, NULL,
        UART_PRIORITY,
        0,
        K_NO_WAIT
    );

    k_thread_name_set(&uart_thread_data, "uart");

    /* main() can return — Zephyr's idle thread takes over.
       Or you can loop here if you have work to do in the main thread. */
    return 0;
}