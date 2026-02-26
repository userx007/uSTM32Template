#include "ao_defs.hpp"

const ButtonConfig BUTTON_0 = {
    .pin              = GPIO_BUTTON_0,
    .debounceTicks    = pdMS_TO_TICKS(20),
    .longPressTicks   = pdMS_TO_TICKS(1000),
    .doubleClickTicks = pdMS_TO_TICKS(300),
    .activeLow        = true,
    .callback         = NULL            // Must be set by caller
};


const LcdConfig LCD_0 = {
    .i2cAddress = 0x27,
    .cols       = 16,
    .rows       = 2
};


const LedConfig LED_0 = {
    .pin        = GPIO_LED_0,
    .activeHigh = false     // PC13 blue pill LED is active-low
};
