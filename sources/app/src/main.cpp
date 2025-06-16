#include "libopencm3/stm32/rcc.h"
#include "libopencm3/stm32/gpio.h"

#include "ushell_core.h"


#if 0
static void delay(uint32_t value)
{
  for (uint32_t i = 0; i < value; i++)
  {
    __asm__("nop"); /* Do nothing. */
  }
}
#endif

int main(void)
{
    const char *pstrRootName = "root";
    Microshell::getShellPtr(pluginEntry(), pstrRootName)->Run();
#if 0
    /*
     * This is just a demo program so you can test if your setup works.
     * You may remove this and replace it with your actual program
     */
    rcc_periph_clock_enable(RCC_GPIOC);
    gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);

    while(1) {
        gpio_set(GPIOC, GPIO13);
        delay(100000);
        gpio_clear(GPIOC, GPIO13);
        delay(100000);
    }
#endif

}