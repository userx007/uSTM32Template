#ifndef U_BUTTON_CONFIG_HPP
#define U_BUTTON_CONFIG_HPP

#include "GpioPin.hpp"
#include "ExtiConfig.hpp"
#include "GpioEvent.hpp"        
#include "FreeRTOS.h"

struct ButtonConfig {
    GpioPin           pin;
    ExtiConfig        exti;    
    TickType_t        debounceTicks;
    TickType_t        longPressTicks;
    TickType_t        doubleClickTicks;
    bool              activeLow;
    ButtonCallbackFn  callback;         
};

#endif /* U_BUTTON_CONFIG_HPP */