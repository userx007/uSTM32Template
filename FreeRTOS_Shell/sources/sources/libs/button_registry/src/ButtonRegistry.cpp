// ButtonRegistry.cpp
#include "ButtonRegistry.hpp"

ButtonAO *ButtonRegistry::s_slots[ButtonRegistry::MAX_EXTI_LINES] = {};

void ButtonRegistry::registerButton(uint8_t lineNumber, ButtonAO *ao)
{
    if (lineNumber < MAX_EXTI_LINES)
        s_slots[lineNumber] = ao;
}

ButtonAO *ButtonRegistry::find(uint8_t lineNumber)
{
    if (lineNumber < MAX_EXTI_LINES)
        return s_slots[lineNumber];
    return nullptr;
}