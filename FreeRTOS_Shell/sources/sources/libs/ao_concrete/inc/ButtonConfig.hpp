#pragma once
#include "GpioPin.hpp"
#include "FreeRTOS.h"

struct ButtonConfig {
    GpioPin     pin;
    TickType_t  debounceTicks;
    TickType_t  longPressTicks;
    TickType_t  doubleClickTicks;  // Max gap between two clicks
    bool        activeLow;
};

static const ButtonConfig BUTTON_CONFIG_DEFAULTS = {
    .pin              = { GPIOA, GPIO_PIN_0 },
    .debounceTicks    = pdMS_TO_TICKS(20),
    .longPressTicks   = pdMS_TO_TICKS(1000),
    .doubleClickTicks = pdMS_TO_TICKS(300),
    .activeLow        = true
};