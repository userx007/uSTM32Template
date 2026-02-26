#ifndef U_LED_CONFIG_HPP
#define U_LED_CONFIG_HPP

#include "GpioPin.hpp"
#include "GpioConfig.hpp"

struct LedConfig {
    GpioPin pin;
    bool    activeHigh;
};

static const LedConfig LED_CONFIG_DEFAULTS = {
    .pin        = GPIO_LED_1,
    .activeHigh = false     // PC13 blue pill LED is active-low
};

#endif /* U_LED_CONFIG_HPP */