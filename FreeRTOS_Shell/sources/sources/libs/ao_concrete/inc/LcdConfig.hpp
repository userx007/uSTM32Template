#ifndef U_LCD_CONFIG_HPP
#define U_LCD_CONFIG_HPP

#include <stdint.h>

struct LcdConfig {
    uint8_t i2cAddress;     // PCF8574 I2C address (0x27 or 0x3F)
    uint8_t cols;           // Display width  (e.g. 16)
    uint8_t rows;           // Display height (e.g. 2)
};

static const LcdConfig LCD_CONFIG_DEFAULTS = {
    .i2cAddress = 0x27,
    .cols       = 16,
    .rows       = 2
};

#endif /* U_LCD_CONFIG_HPP */
