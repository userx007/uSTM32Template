
cmake_minimum_required(VERSION 3.3)
project(uart_access)


add_library(${PROJECT_NAME}
    OBJECT
        src/uart_access.cpp
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${PROJECT_SOURCE_DIR}/inc
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        ${LIBOPENCM3_LIB}
        ushell_core_config
)