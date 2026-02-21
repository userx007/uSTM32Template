#include "hd44780_pcf8574.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/i2c.h>

#include <FreeRTOS.h>
#include <task.h>   /* vTaskDelay */

/* ── HD44780 instruction set ─────────────────────────────────────────────── */
#define HD_CLEARDISPLAY     0x01
#define HD_RETURNHOME       0x02
#define HD_ENTRYMODESET     0x04
#define HD_DISPLAYCONTROL   0x08
#define HD_CURSORSHIFT      0x10
#define HD_FUNCTIONSET      0x20
#define HD_SETCGRAMADDR     0x40
#define HD_SETDDRAMADDR     0x80

/* Entry mode flags */
#define HD_ENTRY_LEFT       0x02
#define HD_ENTRY_SHIFTDEC   0x00

/* Display control flags */
#define HD_DISPLAY_ON       0x04
#define HD_CURSOR_ON        0x02
#define HD_BLINK_ON         0x01

/* Function set flags */
#define HD_4BITMODE         0x00
#define HD_2LINE            0x08
#define HD_5x8DOTS          0x00

/* Row start addresses for up to 4 rows */
static const uint8_t ROW_OFFSETS[] = { 0x00, 0x40, 0x14, 0x54 };

/* ── Constructor ─────────────────────────────────────────────────────────── */
HD44780_PCF8574::HD44780_PCF8574(uint8_t i2c_address, uint8_t cols, uint8_t rows)
    : _addr(i2c_address),
      _cols(cols),
      _rows(rows),
      _backlight(LCD_BL),
      _displayCtrl(HD_DISPLAY_ON)
{}

/* ── I2C hardware setup ──────────────────────────────────────────────────── */
void HD44780_PCF8574::i2c_setup(void)
{
    /* Clock gates */
    rcc_periph_clock_enable(RCC_I2C1);
    rcc_periph_clock_enable(RCC_GPIOB);

    /* PB6 = SCL, PB7 = SDA — alternate function open-drain */
    gpio_set_mode(GPIOB,
                  GPIO_MODE_OUTPUT_50_MHZ,
                  GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN,
                  GPIO6 | GPIO7);

    /* Reset I2C1 via RCC (i2c_reset() not available on F1 libopencm3) */
    rcc_periph_reset_pulse(RST_I2C1);
    i2c_peripheral_disable(I2C1);

    /* APB1 clock is 36 MHz when core runs at 72 MHz */
    i2c_set_clock_frequency(I2C1, 36);

    /* Standard mode 100 kHz:
     * CCR = Fpclk / (2 * Fscl) = 36000000 / (2 * 100000) = 180 */
    i2c_set_standard_mode(I2C1);
    i2c_set_ccr(I2C1, 180);

    /* Trise = (Fpclk / 1000000) + 1 = 37 for standard mode */
    i2c_set_trise(I2C1, 37);

    i2c_peripheral_enable(I2C1);
}

/* ── Low-level I2C byte write to PCF8574 ─────────────────────────────────── */
void HD44780_PCF8574::i2c_write_byte(uint8_t data)
{
    /* Wait until bus is free */
    while ((I2C_SR2(I2C1) & I2C_SR2_BUSY));

    i2c_send_start(I2C1);
    while (!((I2C_SR1(I2C1) & I2C_SR1_SB)
          && (I2C_SR2(I2C1) & I2C_SR2_MSL)));

    /* Send 7-bit address with write bit */
    i2c_send_7bit_address(I2C1, _addr, I2C_WRITE);
    while (!(I2C_SR1(I2C1) & I2C_SR1_ADDR));
    /* Clear ADDR by reading SR2 */
    (void)I2C_SR2(I2C1);

    i2c_send_data(I2C1, data);
    while (!(I2C_SR1(I2C1) & (I2C_SR1_BTF | I2C_SR1_TxE)));

    i2c_send_stop(I2C1);
}

/* ── EN strobe ───────────────────────────────────────────────────────────── */
void HD44780_PCF8574::lcd_pulse_enable(uint8_t data)
{
    i2c_write_byte(data | LCD_EN);          /* EN high */
    vTaskDelay(1);                          /* > 450 ns hold */
    i2c_write_byte(data & ~LCD_EN);         /* EN low  */
    vTaskDelay(1);                          /* > 37 µs settle */
}

/* ── Send one nibble (upper 4 bits of `nibble` map to D4–D7) ─────────────── */
void HD44780_PCF8574::lcd_write4bits(uint8_t nibble)
{
    i2c_write_byte(nibble | _backlight);
    lcd_pulse_enable(nibble | _backlight);
}

/* ── Send a full byte as two nibbles ─────────────────────────────────────── */
void HD44780_PCF8574::lcd_send(uint8_t value, uint8_t mode)
{
    uint8_t high = (value & 0xF0)          | mode;   /* upper nibble */
    uint8_t low  = ((value << 4) & 0xF0)  | mode;   /* lower nibble */
    lcd_write4bits(high);
    lcd_write4bits(low);
}

void HD44780_PCF8574::command(uint8_t cmd)
{
    lcd_send(cmd, 0);   /* RS = 0 → instruction register */
}

/* ── Public API ──────────────────────────────────────────────────────────── */

void HD44780_PCF8574::init(void)
{
    i2c_setup();

    /* HD44780 power-on initialisation sequence (4-bit mode, §2.4 datasheet) */
    vTaskDelay(pdMS_TO_TICKS(50));          /* Wait >40ms after Vcc rises */

    i2c_write_byte(_backlight);             /* Backlight on, all data low */

    /* Three-step reset to guarantee 4-bit mode regardless of prior state */
    lcd_write4bits(0x30);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_write4bits(0x30);
    vTaskDelay(pdMS_TO_TICKS(1));
    lcd_write4bits(0x30);
    vTaskDelay(pdMS_TO_TICKS(1));

    /* Switch to 4-bit mode */
    lcd_write4bits(0x20);
    vTaskDelay(pdMS_TO_TICKS(1));

    /* Function set: 4-bit, 2 lines, 5x8 font */
    command(HD_FUNCTIONSET | HD_4BITMODE | HD_2LINE | HD_5x8DOTS);
    vTaskDelay(pdMS_TO_TICKS(1));

    /* Display on, no cursor, no blink */
    _displayCtrl = HD_DISPLAY_ON;
    command(HD_DISPLAYCONTROL | _displayCtrl);

    clear();

    /* Entry mode: left-to-right, no display shift */
    command(HD_ENTRYMODESET | HD_ENTRY_LEFT | HD_ENTRY_SHIFTDEC);
    vTaskDelay(pdMS_TO_TICKS(1));
}

void HD44780_PCF8574::clear(void)
{
    command(HD_CLEARDISPLAY);
    vTaskDelay(pdMS_TO_TICKS(2));           /* Clear takes up to 1.64ms */
}

void HD44780_PCF8574::home(void)
{
    command(HD_RETURNHOME);
    vTaskDelay(pdMS_TO_TICKS(2));
}

void HD44780_PCF8574::setCursor(uint8_t col, uint8_t row)
{
    if (row >= _rows) row = _rows - 1;
    if (col >= _cols) col = _cols - 1;
    command(HD_SETDDRAMADDR | (col + ROW_OFFSETS[row]));
}

void HD44780_PCF8574::write(char c)
{
    lcd_send(static_cast<uint8_t>(c), LCD_RS);   /* RS = 1 → data register */
}

void HD44780_PCF8574::print(const char *str)
{
    while (*str) {
        write(*str++);
    }
}

void HD44780_PCF8574::setBacklight(bool on)
{
    _backlight = on ? LCD_BL : 0;
    i2c_write_byte(_backlight);             /* Apply immediately */
}

void HD44780_PCF8574::displayOn(bool on)
{
    if (on) _displayCtrl |=  HD_DISPLAY_ON;
    else    _displayCtrl &= ~HD_DISPLAY_ON;
    command(HD_DISPLAYCONTROL | _displayCtrl);
}

void HD44780_PCF8574::cursorOn(bool on)
{
    if (on) _displayCtrl |=  HD_CURSOR_ON;
    else    _displayCtrl &= ~HD_CURSOR_ON;
    command(HD_DISPLAYCONTROL | _displayCtrl);
}

void HD44780_PCF8574::blinkOn(bool on)
{
    if (on) _displayCtrl |=  HD_BLINK_ON;
    else    _displayCtrl &= ~HD_BLINK_ON;
    command(HD_DISPLAYCONTROL | _displayCtrl);
}
