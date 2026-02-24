cmake_minimum_required(VERSION 3.5)

project(threadx_port)

add_library(${PROJECT_NAME}
    STATIC
        src/tx_initialize_low_level.S
        src/syscalls.c
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${PROJECT_SOURCE_DIR}/inc

)
