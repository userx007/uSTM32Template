#include "ButtonAO.hpp"
#include "LedAO.hpp"

static const ButtonConfig BTN1_CFG = {
    .pin              = { GPIOA, GPIO_PIN_0 },
    .debounceTicks    = pdMS_TO_TICKS(20),
    .longPressTicks   = pdMS_TO_TICKS(1000),
    .doubleClickTicks = pdMS_TO_TICKS(300),    // ← new
    .activeLow        = true
};

static const LedConfig LED1_CFG = {
    .pin        = { GPIOC, GPIO_PIN_13 },
    .activeHigh = true
};

static LedAO    ledAO   (LED1_CFG);
static ButtonAO buttonAO(BTN1_CFG);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

    ledAO.init();
    buttonAO.init(ledAO.getAO());

    vTaskStartScheduler();
    for (;;);
}

extern "C" void EXTI0_IRQHandler(void)
{
    buttonAO.onISR();
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}

/*
---

## Signal Timeline
```
Single click:
  pin: ▔▔▔╲___╱▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔
           │   │        │
           │   │        └─ [300ms window expires] → SIG_BUTTON_SINGLE_CLICK
           │   └─────────────────────────────────── SIG_BUTTON_RELEASED
           └─────────────────────────────────────── SIG_BUTTON_PRESSED

Double click:
  pin: ▔▔▔╲___╱▔╲___╱▔▔▔▔▔▔▔▔▔▔▔▔▔
           │   │ │   │
           │   │ │   └── SIG_BUTTON_DOUBLE_CLICK
           │   │ └────── SIG_BUTTON_PRESSED  (2nd)
           │   └──────── SIG_BUTTON_RELEASED (1st)
           └──────────── SIG_BUTTON_PRESSED  (1st)

Long press:
  pin: ▔▔▔╲_________╱▔▔▔▔▔▔▔▔▔▔▔▔▔
           │         │
           │         └── SIG_BUTTON_LONG_PRESS (+ SIG_BUTTON_RELEASED)
           └──────────── SIG_BUTTON_PRESSED
                         [no single/double click emitted]
*/                         