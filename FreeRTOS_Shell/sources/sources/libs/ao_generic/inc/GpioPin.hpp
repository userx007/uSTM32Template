#ifndef U_GPIO_PIN_HANDLING_HPP
#define U_GPIO_PIN_HANDLING_HPP

#ifdef USE_LIBOPENCM3

#include <libopencm3/stm32/gpio.h>
#include <stdint.h>

struct GpioPin {
    uint32_t port;   // e.g. GPIOA, GPIOB ...
    uint16_t pin;    // e.g. GPIO0, GPIO1 ... (libopencm3 uses GPIO0 not GPIO_PIN_0)

    void setHigh() const { gpio_set(port, pin);    }
    void setLow()  const { gpio_clear(port, pin);  }
    void toggle()  const { gpio_toggle(port, pin); }
    bool isLow()   const { return gpio_get(port, pin) == 0;  }
    bool isHigh()  const { return gpio_get(port, pin) != 0;  }
};



#elif USE_STM32HAL

#if defined(STM32F4)
#  include "stm32f4xx_hal.h"
#elif defined(STM32F1)
#  include "stm32f1xx_hal.h"
#endif

struct GpioPin {
    GPIO_TypeDef *port;
    uint16_t      pin;

    // Convenience helpers so callers don't touch HAL directly
    void setHigh()  const { HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);   }
    void setLow()   const { HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET); }
    void toggle()   const { HAL_GPIO_TogglePin(port, pin);                }
    bool isLow()    const { return HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET; }
    bool isHigh()   const { return HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET;   }
};



#else
#error "GPIO: no variant set"
#endif /* USE_LIBOPENCM3, USE_STM32HAL */

#endif /*U_GPIO_PIN_HANDLING_HPP*/