/**
 * Minimal test for STM32F411 with libopencm3
 * This should work in Renode if the vector table is correct
 */

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>

static volatile uint32_t system_ticks = 0;

void sys_tick_handler(void)
{
    system_ticks++;
}

static void delay_ms(uint32_t ms)
{
    uint32_t start = system_ticks;
    while ((system_ticks - start) < ms);
}

static void clock_setup(void)
{
    /* Use internal 16MHz clock for simplicity in Renode */
    rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_3V3_84MHZ]);
    
    /* Setup SysTick for 1ms interrupts */
    systick_set_frequency(1000, 84000000);
    systick_counter_enable();
    systick_interrupt_enable();
}

static void gpio_setup(void)
{
    /* Enable GPIOC clock for LED (PC13 on most boards) */
    rcc_periph_clock_enable(RCC_GPIOC);
    
    /* Setup PC13 as output */
    gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO13);
    gpio_set_output_options(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, GPIO13);
}

static void uart_setup(void)
{
    /* Enable clocks */
    rcc_periph_clock_enable(RCC_USART1);
    rcc_periph_clock_enable(RCC_GPIOA);
    
    /* Setup GPIO PA9 (TX) and PA10 (RX) */
    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO9 | GPIO10);
    gpio_set_af(GPIOA, GPIO_AF7, GPIO9 | GPIO10);
    gpio_set_output_options(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO9);
    
    /* Setup UART parameters */
    usart_set_baudrate(USART1, 115200);
    usart_set_databits(USART1, 8);
    usart_set_stopbits(USART1, USART_STOPBITS_1);
    usart_set_mode(USART1, USART_MODE_TX);
    usart_set_parity(USART1, USART_PARITY_NONE);
    usart_set_flow_control(USART1, USART_FLOWCONTROL_NONE);
    
    /* Enable */
    usart_enable(USART1);
}

static void uart_puts(const char *s)
{
    while (*s) {
        usart_send_blocking(USART1, *s++);
    }
}

int main(void)
{
    clock_setup();
    gpio_setup();
    uart_setup();
    
    uart_puts("STM32F411 Starting...\r\n");
    uart_puts("Hello from Renode!\r\n");
    
    int counter = 0;
    while (1) {
        gpio_toggle(GPIOC, GPIO13);
        
        uart_puts("Tick ");
        /* Simple number output */
        char buf[10];
        int n = counter++;
        int i = 0;
        if (n == 0) {
            buf[i++] = '0';
        } else {
            int temp = n;
            int digits = 0;
            while (temp > 0) {
                temp /= 10;
                digits++;
            }
            for (int j = digits - 1; j >= 0; j--) {
                int divisor = 1;
                for (int k = 0; k < j; k++) divisor *= 10;
                buf[i++] = '0' + ((n / divisor) % 10);
            }
        }
        buf[i] = '\0';
        uart_puts(buf);
        uart_puts("\r\n");
        
        delay_ms(1000);
    }
    
    return 0;
}
