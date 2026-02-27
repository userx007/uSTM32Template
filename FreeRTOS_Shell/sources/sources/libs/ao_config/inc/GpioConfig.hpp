#ifndef U_GPIO_PIN_CONFIG_HPP
#define U_GPIO_PIN_CONFIG_HPP

#include "FreeRTOS.h"

#if defined(USE_LIBOPENCM3)      // libopencm3 → GPIOX

    // ── LEDs ──────────────────────────────────────────────────────
    //
    #define GPIO_LED_0    	{ GPIOC, GPIO13 }     
    

    // ── Buttons: GPIO ─────────────────────────────────────────────
    //
    #define GPIO_BUTTON_0   { GPIOB, GPIO12 }
    #define GPIO_BUTTON_1   { GPIOB, GPIO13 }


    // ── Buttons: EXTI ─────────────────────────────────────────────
    //                                   line  nvic_irq            prio
#define EXTI_BUTTON_0   EXTI_CFG_FALLING(12,   NVIC_EXTI15_10_IRQ, configMAX_SYSCALL_INTERRUPT_PRIORITY)
#define EXTI_BUTTON_1   EXTI_CFG_FALLING(13,   NVIC_EXTI15_10_IRQ, configMAX_SYSCALL_INTERRUPT_PRIORITY)


#elif defined(USE_STM32HAL)

    #define GPIO_LED_0    	{ GPIOC, GPIO_PIN_13 } // HAL → GPIO_PIN_X

    #define GPIO_BUTTON_0   { GPIOB, GPIO_PIN_12 }
    #define GPIO_BUTTON_1   { GPIOB, GPIO_PIN_13 }

#else
    #error "GPIO: no variant set"
#endif


#endif /*U_GPIO_PIN_CONFIG_HPP*/