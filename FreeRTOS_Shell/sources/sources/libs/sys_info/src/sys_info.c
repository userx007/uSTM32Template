#include "FreeRTOS.h"
#include "task.h"
#include "portable.h"
#include "ushell_core_printout.h"


static void printStackWatermarks(void)
{
    TaskStatus_t tasks[10];
    UBaseType_t  count = uxTaskGetSystemState(tasks, 10, NULL);

    uSHELL_PRINTF("%-16s %s\r\n", "Task", "Free words");
    uSHELL_PRINTF("-----------------------------\r\n");
    for (UBaseType_t i = 0; i < count; i++) {
        uSHELL_PRINTF("  %-16s %u\r\n",
            tasks[i].pcTaskName,
            tasks[i].usStackHighWaterMark);
    }
}

// FreeRTOSConfig.h requirement: configUSE_TRACE_FACILITY 1
static void printTaskStates(void)
{
    static const char *stateNames[] = {
        "RUNNING", "READY", "BLOCKED", "SUSPENDED", "DELETED"
    };

    TaskStatus_t tasks[10];
    UBaseType_t  count = uxTaskGetSystemState(tasks, 10, NULL);

    uSHELL_PRINTF("%-16s %-10s %s\r\n", "Task", "State", "Priority");
    uSHELL_PRINTF("------------------------------------\r\n");
    for (UBaseType_t i = 0; i < count; i++) {
        uSHELL_PRINTF("  %-16s %-10s %u\r\n",
            tasks[i].pcTaskName,
            stateNames[tasks[i].eCurrentState],
            tasks[i].uxCurrentPriority);
    }
}

// Always available, no config needed
static void printHeapStats(void)
{
    HeapStats_t stats;
    vPortGetHeapStats(&stats);

    uSHELL_PRINTF("Heap stats:\r\n");
    uSHELL_PRINTF("  Total heap:        %u bytes\r\n", configTOTAL_HEAP_SIZE);
    uSHELL_PRINTF("  Free now:          %u bytes\r\n", stats.xAvailableHeapSpaceInBytes);
    uSHELL_PRINTF("  Min ever free:     %u bytes\r\n", stats.xMinimumEverFreeBytesRemaining);
    uSHELL_PRINTF("  Free blocks:       %u\r\n",       stats.xNumberOfFreeBlocks);
    uSHELL_PRINTF("  Largest block:     %u bytes\r\n", stats.xSizeOfLargestFreeBlockInBytes);
    uSHELL_PRINTF("  Smallest block:    %u bytes\r\n", stats.xSizeOfSmallestFreeBlockInBytes);
    uSHELL_PRINTF("  Alloc calls:       %u\r\n",       stats.xNumberOfSuccessfulAllocations);
    uSHELL_PRINTF("  Free calls:        %u\r\n",       stats.xNumberOfSuccessfulFrees);
}

static void printUptime(void)
{
    TickType_t ticks = xTaskGetTickCount();
    uint32_t   ms    = ticks;           // 1 tick = 1ms with configTICK_RATE_HZ=1000
    uint32_t   sec   = ms / 1000;
    uint32_t   min   = sec / 60;

    uSHELL_PRINTF("Uptime: %02u:%02u.%03u (ticks: %u)\r\n",
        min, sec % 60, ms % 1000, ticks);
}

static void printTaskCount(void)
{
    uSHELL_PRINTF("Tasks:\r\n");
    uSHELL_PRINTF("  Running now:   %u\r\n", uxTaskGetNumberOfTasks());
    uSHELL_PRINTF("  Scheduler:     %s\r\n",
        xTaskGetSchedulerState() == taskSCHEDULER_RUNNING ? "RUNNING" :
        xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED ? "SUSPENDED" : "NOT STARTED");
}


#ifdef __cplusplus
extern "C" {
#endif

void sysinfo(void)
{
    uSHELL_PRINTF("\r\n=== System Info ===\r\n");
    printUptime();
    uSHELL_PRINTF("\r\n");
    printTaskCount();
    uSHELL_PRINTF("\r\n");
    printHeapStats();
    uSHELL_PRINTF("\r\n");
    printTaskStates();
    uSHELL_PRINTF("\r\n");
    printStackWatermarks();
    uSHELL_PRINTF("==================\r\n");
}

#ifdef __cplusplus
}
#endif
