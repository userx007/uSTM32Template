#pragma once

#include <stdint.h>
#include <stddef.h>

/*
 * HD44780 LCD driver via PCF8574 I2C expander
 * Target: STM32F103 + libopencm3
 *
 * PCF8574 → HD44780 pin mapping (standard backpack wiring):
 *   P0 → RS   (Register Select)
 *   P1 → RW   (Read/Write, tied LOW = write only)
 *   P2 → EN   (Enable strobe)
 *   P3 → BL   (Backlight, active HIGH)
 *   P4 → D4
 *   P5 → D5
 *   P6 → D6
 *   P7 → D7
 *
 * I2C1 pins on STM32F103:
 *   PB6 → SCL
 *   PB7 → SDA
 *   (Requires 4.7kΩ pull-up resistors to 3.3V on both lines)
 *
 * PCF8574 default I2C address: 0x27 (A2=A1=A0=1)
 * PCF8574A default I2C address: 0x3F (A2=A1=A0=1)
 */

/* PCF8574 bit positions */
#define LCD_RS  (1 << 0)
#define LCD_RW  (1 << 1)
#define LCD_EN  (1 << 2)
#define LCD_BL  (1 << 3)
#define LCD_D4  (1 << 4)
#define LCD_D5  (1 << 5)
#define LCD_D6  (1 << 6)
#define LCD_D7  (1 << 7)

/* Common display sizes */
#define LCD_COLS  16
#define LCD_ROWS   2

class HD44780_PCF8574 {
public:
    /**
     * @param i2c_address  7-bit I2C address of the PCF8574 (e.g. 0x27)
     * @param cols         Number of display columns (default 16)
     * @param rows         Number of display rows (default 2)
     */
    HD44780_PCF8574(uint8_t i2c_address = 0x27,
                    uint8_t cols = LCD_COLS,
                    uint8_t rows = LCD_ROWS);

    /** Initialise I2C peripheral and the LCD. Call once before any other method. */
    void init(void);

    /** Clear display and return cursor to home. */
    void clear(void);

    /** Return cursor to home position without clearing. */
    void home(void);

    /** Move cursor to column col, row row (both zero-indexed). */
    void setCursor(uint8_t col, uint8_t row);

    /** Print a null-terminated string at the current cursor position. */
    void print(const char *str);

    /** Print a single character at the current cursor position. */
    void write(char c);

    /** Turn the backlight on or off. */
    void setBacklight(bool on);

    /** Turn the display on or off (retains content). */
    void displayOn(bool on);

    /** Show or hide the cursor underline. */
    void cursorOn(bool on);

    /** Enable or disable cursor blinking. */
    void blinkOn(bool on);

private:
    uint8_t _addr;
    uint8_t _cols;
    uint8_t _rows;
    uint8_t _backlight;     /* Current backlight bit state (LCD_BL or 0) */
    uint8_t _displayCtrl;   /* Tracks display/cursor/blink bits */

    void     i2c_setup(void);
    void     i2c_write_byte(uint8_t data);

    void     lcd_send(uint8_t value, uint8_t mode);   /* mode: 0=cmd, LCD_RS=data */
    void     lcd_write4bits(uint8_t nibble);
    void     lcd_pulse_enable(uint8_t data);

    void     command(uint8_t cmd);
};
