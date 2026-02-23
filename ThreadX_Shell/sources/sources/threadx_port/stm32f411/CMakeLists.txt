cmake_minimum_required(VERSION 3.5)

project(threadx_port)

add_library(${PROJECT_NAME}
    STATIC
        src/startup_stm32f4xx.s
        src/syscalls.c
        src/tx_initialize_low_level.s
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${PROJECT_SOURCE_DIR}/inc

)
