#ifndef U_GPIO_PIN_HPP
#define U_GPIO_PIN_HPP

#if defined(USE_LIBOPENCM3)             // ← consistent use of defined()
#include <libopencm3/stm32/gpio.h>
#include <stdint.h>

struct GpioPin {
    uint32_t port;
    uint16_t pin;

    void setHigh() const { gpio_set(port, pin);           }
    void setLow()  const { gpio_clear(port, pin);         }
    void toggle()  const { gpio_toggle(port, pin);        }
    bool isLow()   const { return gpio_get(port, pin) == 0;  }
    bool isHigh()  const { return gpio_get(port, pin) != 0;  }
};

#elif defined(USE_STM32HAL)             // ← consistent use of defined()

#if defined(STM32F4)
    #include "stm32f4xx_hal.h"
#elif defined(STM32F1)
    #include "stm32f1xx_hal.h"
#else
    #error "USE_STM32HAL set but no STM32 family defined (STM32F1, STM32F4 ...)"
#endif

struct GpioPin {
    GPIO_TypeDef *port;
    uint16_t      pin;

    void setHigh() const { HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);          }
    void setLow()  const { HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);        }
    void toggle()  const { HAL_GPIO_TogglePin(port, pin);                       }
    bool isLow()   const { return HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET; }
    bool isHigh()  const { return HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET;   }
};

#else
    #error "GpioPin: define either USE_LIBOPENCM3 or USE_STM32HAL"
#endif

#endif /* U_GPIO_PIN_HPP */