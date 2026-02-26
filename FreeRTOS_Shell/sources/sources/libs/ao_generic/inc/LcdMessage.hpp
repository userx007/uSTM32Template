#ifndef U_LCD_MESSAGE_HPP
#define U_LCD_MESSAGE_HPP

#include <stdint.h>

#define LCD_MSG_LEN  32     // Max characters per message

struct LcdMessage {
    uint8_t row;
    uint8_t col;
    char    text[LCD_MSG_LEN];

    // Convenience constructor — safe string copy, no strncpy dependency
    static LcdMessage make(uint8_t row, uint8_t col, const char *str)
    {
        LcdMessage m;
        m.row = row;
        m.col = col;

        uint8_t i = 0;
        while (str[i] && i < LCD_MSG_LEN - 1) {
            m.text[i] = str[i];
            i++;
        }
        m.text[i] = '\0';

        return m;
    }
};

#endif /* U_LCD_MESSAGE_HPP */
