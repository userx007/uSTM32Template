cmake_minimum_required(VERSION 3.5)
project(threadx_port)

if(STM32_FAMILY STREQUAL "F1")
    set(CPU_VARIANT "stm32f103")
else()
    set(CPU_VARIANT "stm32f411")
endif()


add_library(${PROJECT_NAME}
    STATIC
        ${CPU_VARIANT}/src/tx_initialize_low_level.S
        shared/syscalls.c
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CPU_VARIANT}/inc
)

