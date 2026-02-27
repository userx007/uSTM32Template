#include "ButtonRegistry.hpp"
#include "ButtonAO.hpp"

extern "C" {
#include <libopencm3/stm32/exti.h>
}

// Generic dispatcher — call for every line in a shared IRQ group
static void dispatch_exti_line(uint8_t line)
{
    uint32_t mask = (1u << line);
    if (exti_get_flag_status(mask)) {
        exti_reset_request(mask);
        ButtonAO *ao = ButtonRegistry::find(line);
        if (ao) ao->onISR();
    }
}

extern "C" void exti0_isr(void)    { dispatch_exti_line(0);  }
extern "C" void exti1_isr(void)    { dispatch_exti_line(1);  }
extern "C" void exti2_isr(void)    { dispatch_exti_line(2);  }
extern "C" void exti3_isr(void)    { dispatch_exti_line(3);  }
extern "C" void exti4_isr(void)    { dispatch_exti_line(4);  }

extern "C" void exti9_5_isr(void)
{
    for (uint8_t line = 5; line <= 9; ++line)
        dispatch_exti_line(line);
}

extern "C" void exti15_10_isr(void)
{
    for (uint8_t line = 10; line <= 15; ++line)
        dispatch_exti_line(line);
}