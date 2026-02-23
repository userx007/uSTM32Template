#include "hd44780_pcf8574.h"
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/i2c.h>
#include <FreeRTOS.h>
#include <task.h>

#define DEBUG_ACTIVE 0

#if (1 == DEBUG_ACTIVE)
#include "ushell_core_printout.h"   /* uSHELL_PRINTF for UART debug */
#endif /* (1 == DEBUG_ACTIVE) */

/* ── HD44780 instruction set ─────────────────────────────────────────────── */
#define HD_CLEARDISPLAY     0x01
#define HD_RETURNHOME       0x02
#define HD_ENTRYMODESET     0x04
#define HD_DISPLAYCONTROL   0x08
#define HD_FUNCTIONSET      0x20
#define HD_SETDDRAMADDR     0x80

#define HD_ENTRY_LEFT       0x02
#define HD_ENTRY_SHIFTDEC   0x00

#define HD_DISPLAY_ON       0x04
#define HD_CURSOR_ON        0x02
#define HD_BLINK_ON         0x01

#define HD_4BITMODE         0x00
#define HD_2LINE            0x08
#define HD_5x8DOTS          0x00

#define I2C_TIMEOUT         100000UL

static const uint8_t ROW_OFFSETS[] = { 0x00, 0x40, 0x14, 0x54 };

#if (1 == DEBUG_ACTIVE)
static void dbg_byte(const char *label, uint8_t val)
{
    static const char h[] = "0123456789ABCDEF";
    char buf[16];
    uint8_t i = 0;
    while (label[i] && i < 8) { buf[i] = label[i]; i++; }
    buf[i++] = '0'; buf[i++] = 'x';
    buf[i++] = h[(val >> 4) & 0xF];
    buf[i++] = h[val & 0xF];
    buf[i++] = '\n'; buf[i] = '\0';
    uSHELL_PRINTF(buf);
}
#endif /*(1 == DEBUG_ACTIVE)*/


/* ── Constructor ─────────────────────────────────────────────────────────── */
HD44780_PCF8574::HD44780_PCF8574(uint8_t i2c_address, uint8_t cols, uint8_t rows)
    : _addr(i2c_address),
      _cols(cols),
      _rows(rows),
      _backlight(LCD_BL),
      _displayCtrl(HD_DISPLAY_ON),
      _i2c_ok(false)
{}

/* ── I2C hardware setup ──────────────────────────────────────────────────── */
void HD44780_PCF8574::i2c_setup(void)
{
    rcc_periph_clock_enable(RCC_I2C1);
    rcc_periph_clock_enable(RCC_GPIOB);

    gpio_set_mode(GPIOB,
                  GPIO_MODE_OUTPUT_50_MHZ,
                  GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN,
                  GPIO6 | GPIO7);

    rcc_periph_reset_pulse(RST_I2C1);
    i2c_peripheral_disable(I2C1);

    i2c_set_clock_frequency(I2C1, 36);
    i2c_set_standard_mode(I2C1);
    i2c_set_ccr(I2C1, 180);
    i2c_set_trise(I2C1, 37);

    i2c_peripheral_enable(I2C1);
}

/* ── Low-level I2C byte write ────────────────────────────────────────────── */
bool HD44780_PCF8574::i2c_write_byte(uint8_t data)
{
    uint32_t t;

    t = I2C_TIMEOUT;
    while (I2C_SR2(I2C1) & I2C_SR2_BUSY)
        if (--t == 0) { _i2c_ok = false; return false; }

    i2c_send_start(I2C1);

    t = I2C_TIMEOUT;
    while (!((I2C_SR1(I2C1) & I2C_SR1_SB) && (I2C_SR2(I2C1) & I2C_SR2_MSL)))
        if (--t == 0) { _i2c_ok = false; return false; }

    i2c_send_7bit_address(I2C1, _addr, I2C_WRITE);

    t = I2C_TIMEOUT;
    while (!(I2C_SR1(I2C1) & I2C_SR1_ADDR)) {
        if (I2C_SR1(I2C1) & I2C_SR1_AF) {
            I2C_SR1(I2C1) &= ~I2C_SR1_AF;
            i2c_send_stop(I2C1);
            _i2c_ok = false;
            return false;
        }
        if (--t == 0) { i2c_send_stop(I2C1); _i2c_ok = false; return false; }
    }
    (void)I2C_SR2(I2C1);

    i2c_send_data(I2C1, data);

    t = I2C_TIMEOUT;
    while (!(I2C_SR1(I2C1) & (I2C_SR1_BTF | I2C_SR1_TxE)))
        if (--t == 0) { i2c_send_stop(I2C1); _i2c_ok = false; return false; }

    i2c_send_stop(I2C1);
    _i2c_ok = true;
    return true;
}

/* ── EN strobe ───────────────────────────────────────────────────────────── */
void HD44780_PCF8574::lcd_pulse_enable(uint8_t data)
{
    i2c_write_byte(data | LCD_EN);
    vTaskDelay(pdMS_TO_TICKS(5));
    i2c_write_byte(data & ~LCD_EN);
    vTaskDelay(pdMS_TO_TICKS(5));
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
    i2c_setup();
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Probe */
    if (!i2c_write_byte(_backlight)) {
#if (1 == DEBUG_ACTIVE)
        uSHELL_PRINTF("LCD: probe FAIL\n");
#endif /*(1 == DEBUG_ACTIVE)*/
        return false;
    }

#if (1 == DEBUG_ACTIVE)
    uSHELL_PRINTF("LCD: probe OK\n");
#endif /*(1 == DEBUG_ACTIVE)*/

    vTaskDelay(pdMS_TO_TICKS(10));

    /*
     * Print expected byte sequence so you can match against oscilloscope.
     * Each write4bits(0xXX) sends three I2C bytes to PCF8574:
     *   [data|BL]  [data|BL|EN]  [data|BL]
     * where BL=0x08, EN=0x04
     *
     * Reset step (0x30):  0x38  0x3C  0x38
     * 4-bit switch (0x20): 0x28  0x2C  0x28
     */
#if (1 == DEBUG_ACTIVE)
    uSHELL_PRINTF("LCD: --- expected I2C bytes ---\n");
    dbg_byte("RS1 data: ", (uint8_t)(0x30 | _backlight));           /* 0x38 */
    dbg_byte("RS1 EN+:  ", (uint8_t)(0x30 | _backlight | LCD_EN));  /* 0x3C */
    dbg_byte("RS1 EN-:  ", (uint8_t)(0x30 | _backlight));           /* 0x38 */
    dbg_byte("4BT data: ", (uint8_t)(0x20 | _backlight));           /* 0x28 */
    dbg_byte("4BT EN+:  ", (uint8_t)(0x20 | _backlight | LCD_EN));  /* 0x2C */
    dbg_byte("4BT EN-:  ", (uint8_t)(0x20 | _backlight));           /* 0x28 */
#endif /*(1 == DEBUG_ACTIVE)*/

    /* 3-step reset */
#if (1 == DEBUG_ACTIVE)
    uSHELL_PRINTF("LCD: reset\n");
#endif /*(1 == DEBUG_ACTIVE)*/

    lcd_write4bits(0x30); vTaskDelay(pdMS_TO_TICKS(10));
    lcd_write4bits(0x30); vTaskDelay(pdMS_TO_TICKS(5));
    lcd_write4bits(0x30); vTaskDelay(pdMS_TO_TICKS(5));

    /* 4-bit mode */
#if (1 == DEBUG_ACTIVE)
    uSHELL_PRINTF("LCD: 4bit mode\n");
#endif /*(1 == DEBUG_ACTIVE)*/

    lcd_write4bits(0x20);
    vTaskDelay(pdMS_TO_TICKS(5));

    /* Function set: 4-bit, 2 line, 5x8 */
#if (1 == DEBUG_ACTIVE)
    uSHELL_PRINTF("LCD: func set\n");
#endif /*(1 == DEBUG_ACTIVE)*/

    command(HD_FUNCTIONSET | HD_4BITMODE | HD_2LINE | HD_5x8DOTS); /* 0x28 */
    vTaskDelay(pdMS_TO_TICKS(5));

    /* Display on, cursor off, blink off */
    _displayCtrl = HD_DISPLAY_ON;
#if (1 == DEBUG_ACTIVE)    
    uSHELL_PRINTF("LCD: display on\n");
#endif /*(1 == DEBUG_ACTIVE)*/

    command(HD_DISPLAYCONTROL | _displayCtrl);  /* 0x0C */
    vTaskDelay(pdMS_TO_TICKS(5));

    clear();  /* 0x01 */

    /* Entry mode */
    command(HD_ENTRYMODESET | HD_ENTRY_LEFT | HD_ENTRY_SHIFTDEC); /* 0x06 */
    vTaskDelay(pdMS_TO_TICKS(5));

#if (1 == DEBUG_ACTIVE)    
    uSHELL_PRINTF("LCD: init done\n");
#endif /*(1 == DEBUG_ACTIVE)*/

    return _i2c_ok;
}

void HD44780_PCF8574::clear(void)
{
    command(HD_CLEARDISPLAY);
    vTaskDelay(pdMS_TO_TICKS(10));
}

void HD44780_PCF8574::home(void)
{
    command(HD_RETURNHOME);
    vTaskDelay(pdMS_TO_TICKS(10));
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
