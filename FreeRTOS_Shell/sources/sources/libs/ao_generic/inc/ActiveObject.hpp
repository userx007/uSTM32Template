#pragma once
#include "GpioEvent.hpp"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

typedef void (*DispatchFn)(void *instance, const Event &e);

class ActiveObject {
public:
    ActiveObject()
        : m_queue(NULL)
        , m_task(NULL)
        , m_dispatchFn(NULL)
        , m_owner(NULL)
    {}

    void init(const char  *name,
              DispatchFn   dispatchFn,
              void        *ownerInstance,
              UBaseType_t  priority,
              uint32_t     stackWords,
              uint8_t      queueDepth)
    {
        m_dispatchFn = dispatchFn;
        m_owner      = ownerInstance;

        m_queue = xQueueCreate(queueDepth, sizeof(Event));
        configASSERT(m_queue != NULL);

        xTaskCreate(eventLoop, name, stackWords, this, priority, &m_task);
        configASSERT(m_task != NULL);
    }

    void post(const Event &e)
    {
        xQueueSend(m_queue, &e, 0);
    }

    void postFromISR(const Event &e, BaseType_t *pxHigherPriorityTaskWoken)
    {
        xQueueSendFromISR(m_queue, &e, pxHigherPriorityTaskWoken);
    }

private:
    QueueHandle_t  m_queue;
    TaskHandle_t   m_task;
    DispatchFn     m_dispatchFn;
    void          *m_owner;

    static void eventLoop(void *pvParams)
    {
        ActiveObject *self = static_cast<ActiveObject *>(pvParams);
        Event e;

        for (;;) {
            if (xQueueReceive(self->m_queue, &e, portMAX_DELAY) == pdPASS) {
                self->m_dispatchFn(self->m_owner, e);
            }
        }
    }
};