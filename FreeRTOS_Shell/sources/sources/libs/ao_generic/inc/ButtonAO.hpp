#pragma once
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
        , m_subscriber(NULL)
        , m_state(ST_IDLE)
        , m_pressTimestamp(0)
        , m_releaseTimestamp(0)
    {}

    void init(ActiveObject *subscriber)
    {
        m_subscriber = subscriber;

        m_ao.init(m_aoCfg.name,
                  &ButtonAO::dispatch,
                  this,
                  m_aoCfg.priority,
                  m_aoCfg.stackWords,
                  m_aoCfg.queueDepth);
    }

    void onISR()
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        const Event e = { SIG_RAW_EDGE, 0 };

        m_ao.postFromISR(e, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }

private:
    ActiveObject  m_ao;
    ButtonConfig  m_cfg;
    AoConfig      m_aoCfg;
    ActiveObject *m_subscriber;

    State         m_state;
    TickType_t    m_pressTimestamp;
    TickType_t    m_releaseTimestamp;

    // ── Trampoline ─────────────────────────────────────────────
    static void dispatch(void *instance, const Event &e)
    {
        static_cast<ButtonAO *>(instance)->handleEvent(e);
    }

    // ── Helpers ────────────────────────────────────────────────
    bool isPressed()
    {
        return m_cfg.activeLow ? m_cfg.pin.isLow()
                               : m_cfg.pin.isHigh();
    }

    void post(Signal sig, uint32_t param = 0)
    {
        const Event e = { sig, param };
        m_subscriber->post(e);
    }

    // ── State machine ──────────────────────────────────────────
    void handleEvent(const Event &e)
    {
        if (e.signal != SIG_RAW_EDGE) return;

        // Debounce — read settled pin state inside the task
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

                    post(SIG_BUTTON_PRESSED);   // Immediate raw press event
                }
                break;
            }

            // ── Finger is down (first press) ───────────────────
            case ST_PRESSED1:
            {
                if (!pressed)   // Released
                {
                    const TickType_t held = xTaskGetTickCount() - m_pressTimestamp;

                    post(SIG_BUTTON_RELEASED, (uint32_t)held);  // Immediate raw release

                    if (held >= m_cfg.longPressTicks)
                    {
                        // Long press — emit immediately, no double-click possible
                        post(SIG_BUTTON_LONG_PRESS, (uint32_t)held);
                        m_state = ST_IDLE;
                    }
                    else
                    {
                        // Short release — start double-click watch window
                        m_releaseTimestamp = xTaskGetTickCount();
                        m_state = ST_WAIT_SECOND;
                        waitForSecondClick();   // Blocking wait (see below)
                    }
                }
                break;
            }

            // ── Finger down again (second press) ───────────────
            case ST_PRESSED2:
            {
                if (!pressed) {
                    // Second release — confirmed double click
                    post(SIG_BUTTON_DOUBLE_CLICK);
                    m_state = ST_IDLE;
                }
                break;
            }

            // ST_WAIT_SECOND is handled in waitForSecondClick()
            default:
                break;
        }
    }

    // ── Double-click window ────────────────────────────────────
    //
    // Called after first release. Polls for a second press within
    // doubleClickTicks. Runs inside the AO's own task — safe to block.
    //
    void waitForSecondClick()
    {
        const TickType_t deadline = m_releaseTimestamp + m_cfg.doubleClickTicks;

        for (;;)
        {
            const TickType_t now       = xTaskGetTickCount();
            const TickType_t remaining = (deadline > now) ? (deadline - now) : 0;

            if (remaining == 0)
            {
                // Window expired — was a single click
                post(SIG_BUTTON_SINGLE_CLICK);
                m_state = ST_IDLE;
                return;
            }

            // Check for second press
            vTaskDelay(pdMS_TO_TICKS(10));   // 10ms poll granularity

            if (isPressed())
            {
                vTaskDelay(m_cfg.debounceTicks);    // Debounce second press

                if (isPressed()) {
                    post(SIG_BUTTON_PRESSED);       // Raw press for second click too
                    m_state = ST_PRESSED2;
                    return;   // Back to handleEvent() for second release
                }
            }
        }
    }
};


/*
onISR (press)           onISR (press)
                    │                       │
    ┌──────┐    ┌───▼──────┐  onISR(rel) ┌──▼──────────┐  onISR(rel)
    │ IDLE ├───▶│ PRESSED1 ├────────────▶│WAIT_SECOND  ├────────────▶ DOUBLE_CLICK
    └──────┘    └───┬──────┘             └──────┬──────┘
                    │ onISR (rel)                │ timeout
                    │ held >= longPress          │
                    ▼                            ▼
               LONG_PRESS                  SINGLE_CLICK
*/