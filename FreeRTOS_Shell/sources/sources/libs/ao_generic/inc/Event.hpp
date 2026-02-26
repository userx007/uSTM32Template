#pragma once
#include <stdint.h>

enum Signal : uint8_t {
    SIG_NONE = 0,
    SIG_RAW_EDGE,

    SIG_BUTTON_PRESSED,         // Raw press (immediate, always fires)
    SIG_BUTTON_RELEASED,        // Raw release (immediate, always fires)
    SIG_BUTTON_SINGLE_CLICK,    // Confirmed single click (delayed by window)
    SIG_BUTTON_DOUBLE_CLICK,    // Two clicks within window
    SIG_BUTTON_LONG_PRESS,      // Held >= longPressTicks

    SIG_LED_ON,
    SIG_LED_OFF,
    SIG_LED_TOGGLE,
};

struct Event {
    Signal   signal;
    uint32_t param;     // hold duration (ms) for long press, etc.
};