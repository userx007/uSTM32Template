#ifndef U_LCD_AO_HPP
#define U_LCD_AO_HPP

#include "LcdConfig.hpp"
#include "LcdMessage.hpp"
#include "AoConfig.hpp"
#include "hd44780_pcf8574.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

// ── Default AO config for LCD ──────────────────────────────────
// Defined here so AoConfig.hpp stays generic (no LCD dependency)
static const AoConfig LCD_AO_DEFAULTS = { "LcdAO", 3, 512, 8 };

// ─────────────────────────────────────────────────────────────────
// LcdAO
//
// Does NOT use the generic ActiveObject base — its queue carries
// LcdMessage (row + col + text), not the generic Event type.
// The structural pattern (composed queue + task + trampoline) is
// identical to ActiveObject, just typed differently.
// ─────────────────────────────────────────────────────────────────
class LcdAO {
public:

    LcdAO(const LcdConfig &lcdCfg,
          const AoConfig  &aoCfg = LCD_AO_DEFAULTS)
        : m_lcdCfg(lcdCfg)
        , m_aoCfg(aoCfg)
        , m_queue(NULL)
        , m_task(NULL)
        , m_lcd(lcdCfg.i2cAddress, lcdCfg.cols, lcdCfg.rows)
    {}

    // Call once before vTaskStartScheduler()
    void init()
    {
        m_queue = xQueueCreate(m_aoCfg.queueDepth, sizeof(LcdMessage));
        configASSERT(m_queue != NULL);

        xTaskCreate(eventLoop,
                    m_aoCfg.name,
                    m_aoCfg.stackWords,
                    this,
                    m_aoCfg.priority,
                    &m_task);
        configASSERT(m_task != NULL);
    }

    // Post from any task — non-blocking (drops if queue full)
    void post(const LcdMessage &msg)
    {
        xQueueSend(m_queue, &msg, 0);
    }

    // Convenience: build and post in one call
    void print(uint8_t row, uint8_t col, const char *text)
    {
        post(LcdMessage::make(row, col, text));
    }

    // Post from ISR
    void postFromISR(const LcdMessage &msg,
                     BaseType_t       *pxHigherPriorityTaskWoken)
    {
        xQueueSendFromISR(m_queue, &msg, pxHigherPriorityTaskWoken);
    }

private:
    LcdConfig        m_lcdCfg;
    AoConfig         m_aoCfg;
    QueueHandle_t    m_queue;
    TaskHandle_t     m_task;
    HD44780_PCF8574  m_lcd;

    // ── Private task — owns all LCD hardware access ────────────
    static void eventLoop(void *pvParams)
    {
        LcdAO *self = static_cast<LcdAO *>(pvParams);
        self->run();
    }

    void run()
    {
        // ── Hardware init with retry ───────────────────────────
        while (!m_lcd.init()) {
            vTaskDelay(pdMS_TO_TICKS(2000));
        }

        m_lcd.clear();
        m_lcd.setCursor(0, 0);
        m_lcd.print("System Ready");
        m_lcd.setCursor(0, 1);
        m_lcd.print("STM32F103");

        // ── Event loop ─────────────────────────────────────────
        LcdMessage msg;

        for (;;) {
            if (xQueueReceive(m_queue, &msg, portMAX_DELAY) == pdTRUE) {
                m_lcd.setCursor(msg.col, msg.row);
                m_lcd.print(msg.text);
            }
        }
    }
};

#endif /* U_LCD_AO_HPP */
