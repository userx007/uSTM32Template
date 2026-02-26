#ifndef U_BUTTON_CONFIG_HPP
#define U_BUTTON_CONFIG_HPP

#include "GpioPin.hpp"
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

#endif /* U_BUTTON_CONFIG_HPP */