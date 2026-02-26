#pragma once
#include "GpioPin.hpp"

struct LedConfig {
    GpioPin pin;
    bool    activeHigh;   // true = SET turns LED on, false = RESET turns it on
};

static const LedConfig LED_CONFIG_DEFAULTS = {
    .pin        = { GPIOC, GPIO_PIN_13 },
    .activeHigh = true
};