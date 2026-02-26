#ifndef U_BUTTON_CONFIG_HPP
#define U_BUTTON_CONFIG_HPP

#include "GpioPin.hpp"
#include "GpioConfig.hpp"
#include "GpioEvent.hpp"        // ButtonCallbackFn
#include "FreeRTOS.h"

struct ButtonConfig {
    GpioPin           pin;
    TickType_t        debounceTicks;
    TickType_t        longPressTicks;
    TickType_t        doubleClickTicks;
    bool              activeLow;
    ButtonCallbackFn  callback;         // Called on every cooked event
};

static const ButtonConfig BUTTON_CONFIG_DEFAULTS = {
    .pin              = GPIO_BUTTON_0,
    .debounceTicks    = pdMS_TO_TICKS(20),
    .longPressTicks   = pdMS_TO_TICKS(1000),
    .doubleClickTicks = pdMS_TO_TICKS(300),
    .activeLow        = true,
    .callback         = NULL            // Must be set by caller
};

#endif /* U_BUTTON_CONFIG_HPP */