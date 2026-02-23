/* tx_user.h — Project-specific ThreadX configuration */

#ifndef TX_USER_H
#define TX_USER_H

/* Maximum thread priority levels (default: 32, max: 1024) */
#define TX_MAX_PRIORITIES               32

/* Timer tick rate — must match tx_initialize_low_level.s */
/* Default is already set in tx_api.h but can override here */
/* #define TX_TIMER_TICKS_PER_SECOND    100 */

/* Enable event trace (requires a trace buffer) */
/* #define TX_ENABLE_EVENT_TRACE */

/* Enable stack-fill for debugging (0xEF pattern) */
#define TX_ENABLE_STACK_CHECKING

/* Enable performance counters (useful during development) */
#define TX_THREAD_ENABLE_PERFORMANCE_INFO
#define TX_SEMAPHORE_ENABLE_PERFORMANCE_INFO
#define TX_QUEUE_ENABLE_PERFORMANCE_INFO

/* Disable preemption-threshold feature to save memory if unused */
/* #define TX_DISABLE_PREEMPTION_THRESHOLD */

/* Disable notify callbacks to reduce code size */
/* #define TX_DISABLE_NOTIFY_CALLBACKS */

#endif /* TX_USER_H */