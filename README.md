# uSTM32Template
Template for building STM32 projects using libcm3

## Install the toolchain

```
wget https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz

sudo tar -xvf arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz -C /opt

echo 'export PATH="/opt/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi/bin:$PATH"' >> ~/.bashrc

source ~/.bashrc
```    

### check the installation status
```
    cat ~/.bashrc
    arm-none-eabi-gcc --version
```    

### clone the libopencm3 repository
```
    git clone https://github.com/libopencm3/libopencm3.git
```
---

## Integrate FreeRTOS 

### **Step 1: Add FreeRTOS Source**

First, add FreeRTOS as a submodule or download it:

```bash
# Option A: Using git submodule (recommended)
git submodule add https://github.com/FreeRTOS/FreeRTOS-Kernel.git FreeRTOS

# Option B: Or download and extract manually
# Download from https://github.com/FreeRTOS/FreeRTOS-Kernel
```

Your directory structure will become:
```
.
├── FreeRTOS/
├── libopencm3/
├── sources/
└── ...
```

### **Step 2: Create FreeRTOSConfig.h**

Create `sources/libs/FreeRTOSConfig.h`:

```c
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* STM32F103C8T6 is Cortex-M3 running at 72MHz typically */
#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configUSE_TICKLESS_IDLE                 0
#define configCPU_CLOCK_HZ                      72000000
#define configTICK_RATE_HZ                      1000
#define configMAX_PRIORITIES                    5
#define configMINIMAL_STACK_SIZE                128
#define configMAX_TASK_NAME_LEN                 16
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_TASK_NOTIFICATIONS            1
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configQUEUE_REGISTRY_SIZE               10
#define configUSE_QUEUE_SETS                    0
#define configUSE_TIME_SLICING                  0
#define configUSE_NEWLIB_REENTRANT              0
#define configENABLE_BACKWARD_COMPATIBILITY     0
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 5

/* Memory allocation */
#define configSUPPORT_STATIC_ALLOCATION         0
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configTOTAL_HEAP_SIZE                   ((size_t)(10 * 1024))

/* Hook function related definitions */
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_MALLOC_FAILED_HOOK            1

/* Co-routine definitions */
#define configUSE_CO_ROUTINES                   0
#define configMAX_CO_ROUTINE_PRIORITIES         2

/* Software timer definitions */
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               3
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            configMINIMAL_STACK_SIZE

/* Cortex-M specific definitions */
#define configPRIO_BITS                         4  /* STM32F103 has 4 bits */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    5
#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/* Optional functions */
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_xResumeFromISR                  1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetIdleTaskHandle          1
#define INCLUDE_eTaskGetState                   1
#define INCLUDE_xEventGroupSetBitFromISR        1
#define INCLUDE_xTimerPendFunctionCall          1
#define INCLUDE_xTaskAbortDelay                 1
#define INCLUDE_xTaskGetHandle                  1

/* Cortex-M3 handlers used by FreeRTOS */
#define vPortSVCHandler     sv_call_handler
#define xPortPendSVHandler  pend_sv_handler
#define xPortSysTickHandler sys_tick_handler

#endif /* FREERTOS_CONFIG_H */
```

### **Step 3: Update Root CMakeLists.txt**

Modify your `CMakeLists.txt` to include FreeRTOS:

```cmake
cmake_minimum_required(VERSION 3.16)

project(stm32app C ASM)

# ... your existing setup ...

# FreeRTOS source files
set(FREERTOS_DIR ${CMAKE_SOURCE_DIR}/FreeRTOS)
set(FREERTOS_PORT_DIR ${FREERTOS_DIR}/portable/GCC/ARM_CM3)

set(FREERTOS_SOURCES
    ${FREERTOS_DIR}/tasks.c
    ${FREERTOS_DIR}/queue.c
    ${FREERTOS_DIR}/list.c
    ${FREERTOS_DIR}/timers.c
    ${FREERTOS_DIR}/event_groups.c
    ${FREERTOS_DIR}/stream_buffer.c
    ${FREERTOS_DIR}/portable/MemMang/heap_4.c  # or heap_1/2/3/5 depending on needs
    ${FREERTOS_PORT_DIR}/port.c
)

# Add FreeRTOS includes
include_directories(
    ${FREERTOS_DIR}/include
    ${FREERTOS_PORT_DIR}
    ${CMAKE_SOURCE_DIR}/sources/libs  # For FreeRTOSConfig.h
)

add_subdirectory(sources)

# ... rest of your CMakeLists.txt ...
```

### **Step 4: Update sources/CMakeLists.txt**

Add FreeRTOS sources to your build:

```cmake
# Add FreeRTOS sources to your executable
add_executable(${PROJECT_NAME}
    # Your existing sources
    app/main.c
    # ... other sources ...
    
    # FreeRTOS sources
    ${FREERTOS_SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/app
    ${CMAKE_CURRENT_SOURCE_DIR}/libs
    # FreeRTOS includes already added in root CMakeLists.txt
)
```

### **Step 5: Update Your Application Code**

Modify your `sources/app/main.c` to use FreeRTOS:

```c
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <FreeRTOS.h>
#include <task.h>

static void setup_clock(void) {
    rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_72MHZ]);
}

static void setup_gpio(void) {
    rcc_periph_clock_enable(RCC_GPIOC);
    gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_2_MHZ,
                  GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);
}

void vTaskBlink(void *pvParameters) {
    (void)pvParameters;
    
    while (1) {
        gpio_toggle(GPIOC, GPIO13);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void vApplicationMallocFailedHook(void) {
    /* Called if a call to pvPortMalloc() fails */
    while (1);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    (void)pcTaskName;
    /* Called if a task overflows its stack */
    while (1);
}

int main(void) {
    setup_clock();
    setup_gpio();
    
    xTaskCreate(vTaskBlink, "Blink", 128, NULL, 1, NULL);
    
    vTaskStartScheduler();
    
    /* Should never reach here */
    while (1);
    
    return 0;
}
```

### **Step 6: Adjust Linker Script (if needed)**

Your `linker/stm32f103c8t6.ld` should already allocate enough RAM. Verify you have sufficient heap space for FreeRTOS (10KB in the config above).

### **Step 7: Build**

```bash
./build.sh
```

### **Important Notes:**

1. **Heap Selection**: I used `heap_4.c` which is generally recommended. Choose based on your needs:
   - `heap_1.c`: Simple, no free()
   - `heap_2.c`: Free but no coalescence
   - `heap_3.c`: Wraps malloc/free
   - `heap_4.c`: Coalescence, best general purpose
   - `heap_5.c`: Multiple memory regions

2. **Stack Size**: Monitor stack usage, adjust `configMINIMAL_STACK_SIZE` and task stack sizes as needed

3. **System Clock**: Ensure `configCPU_CLOCK_HZ` matches your actual clock speed

4. **Interrupt Priorities**: Make sure interrupts that call FreeRTOS API have priority ≤ `configMAX_SYSCALL_INTERRUPT_PRIORITY`

Would you like me to help with any specific part of this integration or create complete example files?