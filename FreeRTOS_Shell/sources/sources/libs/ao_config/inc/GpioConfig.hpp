#ifndef U_GPIO_PIN_CONFIG_HPP
#define U_GPIO_PIN_CONFIG_HPP

#if defined(USE_LIBOPENCM3)      // libopencm3 → GPIOX

    #define GPIO_LED_0    	{ GPIOC, GPIO13 }     
    #define GPIO_BUTTON_0 	{ GPIOA, GPIO0 }

#elif defined(USE_STM32HAL)

    #define GPIO_LED_0    	{ GPIOC, GPIO_PIN_13 } // HAL → GPIO_PIN_X
    #define GPIO_BUTTON_0 	{ GPIOA, GPIO_PIN_0 }

#else
    #error "GPIO: no variant set"
#endif


#endif /*U_GPIO_PIN_CONFIG_HPP*/