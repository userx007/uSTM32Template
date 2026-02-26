#ifndef U_LED_CONFIG_HPP
#define U_LED_CONFIG_HPP

#include "GpioPin.hpp"

struct LedConfig {
    GpioPin pin;
    bool    activeHigh;
};

#endif /* U_LED_CONFIG_HPP */