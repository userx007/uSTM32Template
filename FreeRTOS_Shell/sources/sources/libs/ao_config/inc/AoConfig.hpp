#ifndef U_AO_CONFIG_HPP
#define U_AO_CONFIG_HPP

#include "FreeRTOS.h"

// Passed to init() so callers can tune priorities/stack per instance
struct AoConfig {
    const char  *name;
    UBaseType_t  priority;
    uint32_t     stackWords;
    uint8_t      queueDepth;
};

// Sensible defaults — override per instance as needed
static constexpr AoConfig BUTTON_AO_DEFAULTS = { "ButtonAO", 3, 96, 8  };
static constexpr AoConfig LED_AO_DEFAULTS    = { "LedAO",    2, 128, 8  };

#endif /*U_AO_CONFIG_HPP*/