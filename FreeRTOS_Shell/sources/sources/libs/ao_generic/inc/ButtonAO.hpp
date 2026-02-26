#ifndef U_BUTTON_AO_HPP
#define U_BUTTON_AO_HPP

#include "ActiveObject.hpp"
#include "ButtonConfig.hpp"
#include "AoConfig.hpp"

class ButtonAO {
public:

    // ── State machine states ───────────────────────────────────
    enum State : uint8_t {
        ST_IDLE,
        ST_PRESSED1,        // First press, finger down
        ST_WAIT_SECOND,     // First release, waiting for second press
        ST_PRESSED2,        // Second press, finger down
    };

    // ── Constructor ────────────────────────────────────────────
    ButtonAO(const ButtonConfig &btnCfg,
             const AoConfig     &aoCfg = BUTTON_AO_DEFAULTS)
        : m_cfg(btnCfg)
        , m_aoCfg(aoCfg)
        , m_state(ST_IDLE)
        , m_pressTimestamp(0)
        , m_releaseTimestamp(0)
    {}

    void init()
    {
        m_ao.init(m_aoCfg.name,
                  &ButtonAO::dispatch,
                  this,
                  m_aoCfg.priority,
                  m_aoCfg.stackWords,
                  m_aoCfg.queueDepth);
    }

    // Call this from the GPIO EXTI ISR
    void onISR()
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        const Event e = { SIG_RAW_EDGE, 0 };

        m_ao.postFromISR(e, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }

private:
    ActiveObject  m_ao;
    ButtonConfig  m_cfg;        // Owns the callback + pin identity
    AoConfig      m_aoCfg;

    State         m_state;
    TickType_t    m_pressTimestamp;
    TickType_t    m_releaseTimestamp;

    // ── Trampoline ─────────────────────────────────────────────
    static void dispatch(void *instance, const Event &e)
    {
        static_cast<ButtonAO *>(instance)->handleEvent(e);
    }

    // ── Helpers ────────────────────────────────────────────────
    bool isPressed() const
    {
        return m_cfg.activeLow ? m_cfg.pin.isLow()
                               : m_cfg.pin.isHigh();
    }

    // Fire callback — passes button identity so handler knows which button
    void notify(Signal sig, uint32_t param = 0) const
    {
        if (m_cfg.callback != NULL) {
            m_cfg.callback(sig, m_cfg.pin, param);
        }
    }

    // ── State machine ──────────────────────────────────────────
    void handleEvent(const Event &e)
    {
        if (e.signal != SIG_RAW_EDGE) return;

        vTaskDelay(m_cfg.debounceTicks);
        const bool pressed = isPressed();

        switch (m_state)
        {
            // ── Waiting for any activity ───────────────────────
            case ST_IDLE:
            {
                if (pressed) {
                    m_pressTimestamp = xTaskGetTickCount();
                    m_state = ST_PRESSED1;
                    notify(SIG_BUTTON_PRESSED);
                }
                break;
            }

            // ── Finger down (first press) ──────────────────────
            case ST_PRESSED1:
            {
                if (!pressed)
                {
                    const TickType_t held = xTaskGetTickCount() - m_pressTimestamp;

                    notify(SIG_BUTTON_RELEASED, (uint32_t)held);

                    if (held >= m_cfg.longPressTicks)
                    {
                        notify(SIG_BUTTON_LONG_PRESS, (uint32_t)held);
                        m_state = ST_IDLE;
                    }
                    else
                    {
                        m_releaseTimestamp = xTaskGetTickCount();
                        m_state = ST_WAIT_SECOND;
                        waitForSecondClick();
                    }
                }
                break;
            }

            // ── Second press detected ──────────────────────────
            case ST_PRESSED2:
            {
                if (!pressed) {
                    notify(SIG_BUTTON_DOUBLE_CLICK);
                    m_state = ST_IDLE;
                }
                break;
            }

            default:
                break;
        }
    }

    // ── Double-click window — blocking poll inside the AO task ─
    void waitForSecondClick()
    {
        const TickType_t deadline = m_releaseTimestamp + m_cfg.doubleClickTicks;

        for (;;)
        {
            const TickType_t now       = xTaskGetTickCount();
            const TickType_t remaining = (deadline > now) ? (deadline - now) : 0;

            if (remaining == 0)
            {
                notify(SIG_BUTTON_SINGLE_CLICK);
                m_state = ST_IDLE;
                return;
            }

            vTaskDelay(pdMS_TO_TICKS(10));

            if (isPressed())
            {
                vTaskDelay(m_cfg.debounceTicks);

                if (isPressed()) {
                    notify(SIG_BUTTON_PRESSED);
                    m_state = ST_PRESSED2;
                    return;
                }
            }
        }
    }
};

#endif /* U_BUTTON_AO_HPP */