// ButtonRegistry.hpp
#ifndef U_BUTTON_REGISTRY_HPP
#define U_BUTTON_REGISTRY_HPP

#include <stdint.h>

class ButtonAO;  // forward declaration

class ButtonRegistry {
public:
    static constexpr uint8_t MAX_EXTI_LINES = 16;

    static void      registerButton(uint8_t lineNumber, ButtonAO *ao);
    static ButtonAO *find(uint8_t lineNumber);

private:
    static ButtonAO *s_slots[MAX_EXTI_LINES];
};

#endif /* U_BUTTON_REGISTRY_HPP */