#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

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
 * PCF8574  default I2C address: 0x27  (A2=A1=A0=1)
 * PCF8574A default I2C address: 0x3F  (A2=A1=A0=1)
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

#define LCD_COLS  16
#define LCD_ROWS   2

class HD44780_PCF8574 {
public:
    HD44780_PCF8574(uint8_t i2c_address = 0x27,
                    uint8_t cols = LCD_COLS,
                    uint8_t rows = LCD_ROWS);

    /**
     * Initialise I2C and the LCD.
     * @return true  on success
     * @return false if the PCF8574 did not acknowledge (wrong address,
     *               not connected, or PICSimLab component absent)
     */
    bool init(void);

    void clear(void);
    void home(void);
    void setCursor(uint8_t col, uint8_t row);
    void print(const char *str);
    void write(char c);
    void setBacklight(bool on);
    void displayOn(bool on);
    void cursorOn(bool on);
    void blinkOn(bool on);

    /** True if the last I2C transaction succeeded. */
    bool ok(void) const { return _i2c_ok; }

private:
    uint8_t _addr;
    uint8_t _cols;
    uint8_t _rows;
    uint8_t _backlight;
    uint8_t _displayCtrl;
    bool    _i2c_ok;

    void i2c_setup(void);
    bool i2c_write_byte(uint8_t data);

    void lcd_send(uint8_t value, uint8_t mode);
    void lcd_write4bits(uint8_t nibble);
    void lcd_pulse_enable(uint8_t data);
    void command(uint8_t cmd);
};
