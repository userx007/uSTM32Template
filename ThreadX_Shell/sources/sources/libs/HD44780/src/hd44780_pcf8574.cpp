#include "hd44780_pcf8574.h"

#if defined(STM32F1)
#  include "stm32f1xx_hal.h"
#elif defined(STM32F4)
#  include "stm32f4xx_hal.h"
#else
#  error "Define STM32F1 or STM32F4 in your build system"
#endif

#include "tx_api.h"   /* tx_thread_sleep, TX_TIMER_TICKS_PER_SECOND */

#define DEBUG_ACTIVE 0

#if (1 == DEBUG_ACTIVE)
#  include "ushell_core_printout.h"
#endif

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

#define I2C_TIMEOUT_MS    10UL

/* HAL uses 8-bit address (7-bit << 1) */
#define I2C_ADDR_8BIT(a)  ((a) << 1)

static const uint8_t ROW_OFFSETS[] = { 0x00, 0x40, 0x14, 0x54 };

/* ── ThreadX delay helper ────────────────────────────────────────────────── */
static inline void lcd_delay_ms(uint32_t ms)
{
    /* Convert ms to ThreadX ticks (rounded up to at least 1 tick) */
    ULONG ticks = (ms * TX_TIMER_TICKS_PER_SECOND + 999UL) / 1000UL;
    if (ticks == 0) ticks = 1;
    tx_thread_sleep(ticks);
}

/* ── Module-level HAL handle ─────────────────────────────────────────────── */
static I2C_HandleTypeDef hi2c1;

/* ── HAL MSP hook ────────────────────────────────────────────────────────── */
void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance != I2C1)
        return;

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {};

#if defined(STM32F1)
    /*
     * STM32F1: PB6=SCL, PB7=SDA — alternate function open-drain
     * No GPIO_Alternate field on F1; AF is implicit for I2C pins.
     */
    gpio.Pin   = GPIO_PIN_6 | GPIO_PIN_7;
    gpio.Mode  = GPIO_MODE_AF_OD;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);

#elif defined(STM32F4)
    /*
     * STM32F4: PB6=SCL AF4, PB7=SDA AF4
     */
    gpio.Pin       = GPIO_PIN_6 | GPIO_PIN_7;
    gpio.Mode      = GPIO_MODE_AF_OD;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &gpio);
#endif
}

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
    hi2c1.Instance             = I2C1;
    hi2c1.Init.ClockSpeed      = 100000;          /* 100 kHz standard mode */
    hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1     = 0;
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLED;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLED;
    hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLED;

    HAL_I2C_Init(&hi2c1);
}

/* ── Low-level I2C byte write ────────────────────────────────────────────── */
bool HD44780_PCF8574::i2c_write_byte(uint8_t data)
{
    HAL_StatusTypeDef status =
        HAL_I2C_Master_Transmit(&hi2c1,
                                I2C_ADDR_8BIT(_addr),
                                &data, 1,
                                I2C_TIMEOUT_MS);
    _i2c_ok = (status == HAL_OK);
    return _i2c_ok;
}

/* ── EN strobe ───────────────────────────────────────────────────────────── */
void HD44780_PCF8574::lcd_pulse_enable(uint8_t data)
{
    i2c_write_byte(data | LCD_EN);
    lcd_delay_ms(1);
    i2c_write_byte(data & ~LCD_EN);
    lcd_delay_ms(1);
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
    lcd_delay_ms(100);

    /* Probe — just send backlight byte to check ACK */
    if (!i2c_write_byte(_backlight)) {
#if (1 == DEBUG_ACTIVE)
        uSHELL_PRINTF("LCD: probe FAIL\n");
#endif
        return false;
    }

#if (1 == DEBUG_ACTIVE)
    uSHELL_PRINTF("LCD: probe OK\n");
#endif

    lcd_delay_ms(10);

    /* 3-step reset sequence */
    lcd_write4bits(0x30); lcd_delay_ms(10);
    lcd_write4bits(0x30); lcd_delay_ms(5);
    lcd_write4bits(0x30); lcd_delay_ms(5);

    /* Switch to 4-bit mode */
    lcd_write4bits(0x20);
    lcd_delay_ms(5);

    /* Function set: 4-bit, 2-line, 5x8 */
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

#if (1 == DEBUG_ACTIVE)
    uSHELL_PRINTF("LCD: init done\n");
#endif

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
