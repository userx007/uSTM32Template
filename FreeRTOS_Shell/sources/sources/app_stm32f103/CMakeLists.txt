
cmake_minimum_required(VERSION 3.10)
project(user_main)

add_library(${PROJECT_NAME}
    OBJECT
        src/main_freertos_shell.cpp
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        uart_access
        ushell_core
        ushell_core_utils
        ushell_user_root
        freertos
)

