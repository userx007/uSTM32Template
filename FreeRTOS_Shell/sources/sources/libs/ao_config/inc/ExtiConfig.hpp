#ifndef U_EXTI_CONFIG_HPP
#define U_EXTI_CONFIG_HPP

#include <stdint.h>

#if defined(USE_LIBOPENCM3)
#include <libopencm3/stm32/exti.h>
#include <libopencm3/cm3/nvic.h>

struct ExtiConfig {
    uint32_t extiLine;   // EXTI12, EXTI0, etc.
    uint32_t trigger;    // EXTI_TRIGGER_FALLING / BOTH
    uint8_t  nvicIrq;    // NVIC_EXTI15_10_IRQ, etc.
    uint8_t  nvicPrio;
    uint8_t  lineNumber; // numeric 0-15, used by registry
};

// Helpers for readability in GpioConfig.hpp
#define EXTI_CFG_FALLING(line, nvicIrq_, prio) \
    { EXTI##line, EXTI_TRIGGER_FALLING, nvicIrq_, prio, line }

#define EXTI_CFG_BOTH(line, nvicIrq_, prio) \
    { EXTI##line, EXTI_TRIGGER_BOTH, nvicIrq_, prio, line }

#endif
#endif /* U_EXTI_CONFIG_HPP */