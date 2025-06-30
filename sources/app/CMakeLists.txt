
cmake_minimum_required(VERSION 3.10)
project(user_main)

add_library(${PROJECT_NAME}
    OBJECT
        src/main.cpp
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        ${LIBOPENCM3_LIB}
        uart_access
        ushell_core
        ushell_core_utils
        ushell_user_root
)

