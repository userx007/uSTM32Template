#pragma once
#include "FreeRTOS.h"

// Passed to init() so callers can tune priorities/stack per instance
struct AoConfig {
    const char  *name;
    UBaseType_t  priority;
    uint32_t     stackWords;
    uint8_t      queueDepth;
};

// Sensible defaults — override per instance as needed
static const AoConfig BUTTON_AO_DEFAULTS = { "ButtonAO", 3, 256, 8  };
static const AoConfig LED_AO_DEFAULTS    = { "LedAO",    2, 128, 8  };