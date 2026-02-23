cmake_minimum_required(VERSION 3.20)
project(threadx_app C ASM)

# Toolchain settings (or use a toolchain file)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ARM)

set(CPU_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")
set(CMAKE_C_FLAGS   "${CPU_FLAGS} -O2 -g -ffunction-sections -fdata-sections")
set(CMAKE_ASM_FLAGS "${CPU_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CPU_FLAGS} -T${CMAKE_SOURCE_DIR}/STM32F4xx.ld \
    -Wl,--gc-sections -specs=nosys.specs")

# --- ThreadX library ---
set(THREADX_ARCH "cortex_m4")
set(THREADX_TOOLCHAIN "gnu")

add_subdirectory(threadx)          # Exposes 'threadx' CMake target

# --- Application ---
add_executable(${PROJECT_NAME}
    startup_stm32f4xx.s
    src/tx_initialize_low_level.s
    src/main.c
    src/app_threadx.c
    src/syscalls.c
)

target_link_libraries(${PROJECT_NAME} 
    PRIVATE 
        threadx
)

target_include_directories(${PROJECT_NAME} 
    PRIVATE 
        src 
        threadx/common/inc
        threadx_user_cfg
)

target_compile_definitions(${PROJECT_NAME} 
    PRIVATE 
        TX_INCLUDE_USER_DEFINE_FILE
)