/**
 * @file hd44780_pcf8574.cpp
 * @brief HD44780 LCD driver via PCF8574 I2C expander — Zephyr backend
 *
 * Drop-in replacement for the STM32 HAL + ThreadX version.
 * Public API is unchanged; the following are replaced:
 *
 *   HAL / ThreadX                  →  Zephyr
 *   ─────────────────────────────────────────────────────
 *   I2C_HandleTypeDef / HAL_I2C_*  →  struct device + i2c_write()
 *   HAL_I2C_MspInit (GPIO/clocks)  →  devicetree (automatic)
 *   tx_thread_sleep()              →  k_msleep()
 *   uSHELL_PRINTF / HAL guards     →  printk / printk  (Zephyr logging)
 */

#include "hd44780_pcf8574.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(hd44780, LOG_LEVEL_DBG);

/* ── HD44780 instruction set ─────────────────────────────────────────────── */
#define HD_CLEARDISPLAY   0x01
#define HD_RETURNHOME     0x02
#define HD_ENTRYMODESET   0x04
#define HD_DISPLAYCONTROL 0x08
#define HD_FUNCTIONSET    0x20
#define HD_SETDDRAMADDR   0x80

#define HD_ENTRY_LEFT     0x02
#define HD_ENTRY_SHIFTDEC 0x00

#define HD_DISPLAY_ON     0x04
#define HD_CURSOR_ON      0x02
#define HD_BLINK_ON       0x01

#define HD_4BITMODE       0x00
#define HD_2LINE          0x08
#define HD_5x8DOTS        0x00

static const uint8_t ROW_OFFSETS[] = { 0x00, 0x40, 0x14, 0x54 };

/* ── Delay helper ────────────────────────────────────────────────────────── */
static inline void lcd_delay_ms(uint32_t ms)
{
    k_msleep(ms);
}

/* ── Constructor ─────────────────────────────────────────────────────────── */
HD44780_PCF8574::HD44780_PCF8574(uint8_t i2c_address, uint8_t cols, uint8_t rows)
    : _addr(i2c_address),
      _cols(cols),
      _rows(rows),
      _backlight(LCD_BL),
      _displayCtrl(HD_DISPLAY_ON),
      _i2c_ok(false),
      _i2c_dev(nullptr)
{}

/* ── Low-level I2C byte write ────────────────────────────────────────────── */
bool HD44780_PCF8574::i2c_write_byte(uint8_t data)
{
    if (!_i2c_dev) return false;

    /*
     * i2c_write(dev, buf, len, addr) — Zephyr takes the 7-bit address
     * directly; no manual left-shift needed (unlike HAL).
     */
    int ret = i2c_write(_i2c_dev, &data, 1, _addr);
    _i2c_ok = (ret == 0);
    return _i2c_ok;
}

/* ── EN strobe ───────────────────────────────────────────────────────────── */
void HD44780_PCF8574::lcd_pulse_enable(uint8_t data)
{
    i2c_write_byte(data | LCD_EN);
    lcd_delay_ms(5);
    i2c_write_byte(data & ~LCD_EN);
    lcd_delay_ms(5);
}

/* ── Send one nibble ─────────────────────────────────────────────────────── */
void HD44780_PCF8574::lcd_write4bits(uint8_t nibble)
{
    i2c_write_byte(nibble | _backlight);
    lcd_pulse_enable(nibble | _backlight);
}

/* ── Send full byte as two nibbles ───────────────────────────────────────── */
void HD44780_PCF8574::lcd_send(uint8_t value, uint8_t mode)
{
    uint8_t high = (value & 0xF0)         | mode;
    uint8_t low  = ((value << 4) & 0xF0) | mode;
    lcd_write4bits(high);
    lcd_write4bits(low);
}

void HD44780_PCF8574::command(uint8_t cmd)
{
    lcd_send(cmd, 0);
}

/* ── Public API ──────────────────────────────────────────────────────────── */

bool HD44780_PCF8574::init(void)
{
    /*
     * Resolve the I2C bus from the devicetree `i2c1` node label.
     * The board file (or app.overlay) must have `&i2c1 { status = "okay"; }`.
     * No clock gating or GPIO mux calls needed — Zephyr handles those.
     */
    _i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c1));

    printk("LCD: HD44780_PCF8574::init()\n");

    if (!device_is_ready(_i2c_dev)) {
        printk("LCD: I2C bus not ready\n");
        _i2c_ok = false;
        return false;
    }

    lcd_delay_ms(10);

    /* Probe — send backlight byte and check ACK */
    if (!i2c_write_byte(_backlight)) {
        printk("LCD: probe FAIL (no ACK at 0x%02X)\n", _addr);
        return false;
    }

    printk("LCD: probe OK at 0x%02X\n", _addr);
    lcd_delay_ms(10);

    /* 3-step reset sequence (HD44780 datasheet §4.4) */
    lcd_write4bits(0x30); lcd_delay_ms(10);
    lcd_write4bits(0x30); lcd_delay_ms(5);
    lcd_write4bits(0x30); lcd_delay_ms(5);

    /* Switch to 4-bit mode */
    lcd_write4bits(0x20);
    lcd_delay_ms(5);

    /* Function set: 4-bit, 2-line, 5×8 dots */
    command(HD_FUNCTIONSET | HD_4BITMODE | HD_2LINE | HD_5x8DOTS);
    lcd_delay_ms(5);

    /* Display on, cursor off, blink off */
    _displayCtrl = HD_DISPLAY_ON;
    command(HD_DISPLAYCONTROL | _displayCtrl);
    lcd_delay_ms(5);

    clear();

    /* Entry mode: left-to-right, no shift */
    command(HD_ENTRYMODESET | HD_ENTRY_LEFT | HD_ENTRY_SHIFTDEC);
    lcd_delay_ms(5);

    printk("LCD: init done\n");
    return _i2c_ok;
}

void HD44780_PCF8574::clear(void)
{
    command(HD_CLEARDISPLAY);
    lcd_delay_ms(10);
}

void HD44780_PCF8574::home(void)
{
    command(HD_RETURNHOME);
    lcd_delay_ms(10);
}

void HD44780_PCF8574::setCursor(uint8_t col, uint8_t row)
{
    if (row >= _rows) row = _rows - 1;
    if (col >= _cols) col = _cols - 1;
    command(HD_SETDDRAMADDR | (col + ROW_OFFSETS[row]));
}

void HD44780_PCF8574::write(char c)
{
    lcd_send(static_cast<uint8_t>(c), LCD_RS);
}

void HD44780_PCF8574::print(const char *str)
{
    while (*str) write(*str++);
}

void HD44780_PCF8574::setBacklight(bool on)
{
    _backlight = on ? LCD_BL : 0;
    i2c_write_byte(_backlight);
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
