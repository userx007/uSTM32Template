#ifndef U_GPIO_EVENTS_HPP
#define U_GPIO_EVENTS_HPP

#include <stdint.h>
#include "GpioPin.hpp"

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
    uint32_t param;     // hold duration (ticks) for long press, etc.
};

// Callback fired by ButtonAO on every cooked event.
// sig       — what happened (PRESSED, RELEASED, SINGLE_CLICK, ...)
// buttonPin — which button fired (port + pin = unique identity)
// param     — hold duration in ticks for LONG_PRESS/RELEASED, 0 otherwise
typedef void (*ButtonCallbackFn)(Signal         sig,
                                 const GpioPin &buttonPin,
                                 uint32_t       param);

#endif /* U_GPIO_EVENTS_HPP */