#include "ao_defs.hpp"
#include "ushell_core_printout.h"


// -- buttons callbacks forward declaration -----------------------------------

static void onButtonEvent_0(Signal sig, const GpioPin &btn, uint32_t param);
static void onButtonEvent_1(Signal sig, const GpioPin &btn, uint32_t param);


// -- buttons configuration ---------------------------------------------------

const ButtonConfig BUTTON_0 = {
    .pin              = GPIO_BUTTON_0,
    .exti             = EXTI_BUTTON_0,
    .debounceTicks    = pdMS_TO_TICKS(20),
    .longPressTicks   = pdMS_TO_TICKS(1000),
    .doubleClickTicks = pdMS_TO_TICKS(300),
    .activeLow        = true,
    .callback         = onButtonEvent_0
};

const ButtonConfig BUTTON_1 = {
    .pin              = GPIO_BUTTON_1,
    .exti             = EXTI_BUTTON_1,      
    .debounceTicks    = pdMS_TO_TICKS(20),
    .longPressTicks   = pdMS_TO_TICKS(1000),
    .doubleClickTicks = pdMS_TO_TICKS(300),
    .activeLow        = true,
    .callback         = onButtonEvent_1
};


// -- LCD configuration -------------------------------------------------------

const LcdConfig LCD_0 = {
    .i2cAddress = 0x27,
    .cols       = 16,
    .rows       = 2
};


// -- LED configuration -------------------------------------------------------

const LedConfig LED_0 = {
    .pin        = GPIO_LED_0,
    .activeHigh = false     // PC13 blue pill LED is active-low
};


// -- buttons callbacks implementation ----------------------------------------

static void onButtonEvent_0(Signal sig, const GpioPin &btn, uint32_t param)
{
    (void)btn;      // Ignored here — use it to multiplex if >1 button
    (void)param;    // Available for long-press duration etc.

//  const Event e = { SIG_LED_TOGGLE, 0 };   // default

    switch (sig)
    {
        case SIG_BUTTON_SINGLE_CLICK:
        {
            //const Event ev = { SIG_LED_TOGGLE, 0 };
            //ledAO.getAO()->post(ev);  
            uSHELL_PRINTF("0: SINGLE_CLICK\n");
            break;
        }
        case SIG_BUTTON_DOUBLE_CLICK:
        {
            //const Event ev = { SIG_LED_OFF, 0 };
            //ledAO.getAO()->post(ev);
            uSHELL_PRINTF("0: DOUBLE_CLICK\n");
            break;
        }
        case SIG_BUTTON_LONG_PRESS:
        {
            // const Event ev = { SIG_LED_ON, 0 };
            //ledAO.getAO()->post(ev);
            uSHELL_PRINTF("0: LONG_PRESS\n");
            break;
        }
        default:
            break;      // PRESSED / RELEASED ignored here
    }
}

static void onButtonEvent_1(Signal sig, const GpioPin &btn, uint32_t param)
{
    (void)btn;      // Ignored here — use it to multiplex if >1 button
    (void)param;    // Available for long-press duration etc.

//  const Event e = { SIG_LED_TOGGLE, 0 };   // default

    switch (sig)
    {
        case SIG_BUTTON_SINGLE_CLICK:
        {
            //const Event ev = { SIG_LED_TOGGLE, 0 };
            //ledAO.getAO()->post(ev);  
            uSHELL_PRINTF("1: SINGLE_CLICK\n");
            break;
        }
        case SIG_BUTTON_DOUBLE_CLICK:
        {
            //const Event ev = { SIG_LED_OFF, 0 };
            //ledAO.getAO()->post(ev);
            uSHELL_PRINTF("1: DOUBLE_CLICK\n");
            break;
        }
        case SIG_BUTTON_LONG_PRESS:
        {
            // const Event ev = { SIG_LED_ON, 0 };
            //ledAO.getAO()->post(ev);
            uSHELL_PRINTF("1: LONG_PRESS\n");
            break;
        }
        default:
            break;      // PRESSED / RELEASED ignored here
    }
}