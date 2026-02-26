#pragma once
#include "ActiveObject.hpp"
#include "LedConfig.hpp"
#include "AoConfig.hpp"

class LedAO {
public:
    LedAO(const LedConfig &ledCfg,
          const AoConfig  &aoCfg = LED_AO_DEFAULTS)
        : m_cfg(ledCfg)
        , m_aoCfg(aoCfg)
        , m_state(false)
    {}

    void init()
    {
        m_ao.init(m_aoCfg.name,
                  &LedAO::dispatch,
                  this,
                  m_aoCfg.priority,
                  m_aoCfg.stackWords,
                  m_aoCfg.queueDepth);
    }

    ActiveObject *getAO() { return &m_ao; }

private:
    ActiveObject m_ao;
    LedConfig    m_cfg;
    AoConfig     m_aoCfg;
    bool         m_state;

    static void dispatch(void *instance, const Event &e)
    {
        static_cast<LedAO *>(instance)->handleEvent(e);
    }

    void setLed(bool on)
    {
        m_state = on;
        if (on)
            m_cfg.activeHigh ? m_cfg.pin.setHigh() : m_cfg.pin.setLow();
        else
            m_cfg.activeHigh ? m_cfg.pin.setLow()  : m_cfg.pin.setHigh();
    }

    void handleEvent(const Event &e)
    {
        switch (e.signal)
        {
            // Raw events — react immediately if desired
            case SIG_BUTTON_PRESSED:                            break;  // Ignore raw
            case SIG_BUTTON_RELEASED:                           break;  // Ignore raw

            // Cooked click events
            case SIG_BUTTON_SINGLE_CLICK:   setLed(!m_state);  break;  // Toggle on single
            case SIG_BUTTON_DOUBLE_CLICK:   setLed(false);     break;  // Off on double
            case SIG_BUTTON_LONG_PRESS:     setLed(true);      break;  // On  on long

            // Direct LED commands
            case SIG_LED_ON:                setLed(true);      break;
            case SIG_LED_OFF:               setLed(false);     break;
            case SIG_LED_TOGGLE:            setLed(!m_state);  break;

            default:                                           break;
        }
    }
};