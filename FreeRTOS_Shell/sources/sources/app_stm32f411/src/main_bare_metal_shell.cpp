#include "libopencm3/stm32/rcc.h"
#include "libopencm3/stm32/gpio.h"

#include "ushell_core.h"
#include "uart_access.h"


int main(void)
{
    uart_setup();
    Microshell::getShellPtr(pluginEntry(), "root")->Run();
    return 0;
}