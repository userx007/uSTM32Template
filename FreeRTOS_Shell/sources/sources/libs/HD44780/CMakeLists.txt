
cmake_minimum_required(VERSION 3.3)
project(hd44780)


add_library(${PROJECT_NAME}
    OBJECT
        src/hd44780_pcf8574.cpp
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${PROJECT_SOURCE_DIR}/inc
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        ${LIBOPENCM3_LIB}
        freertos
        ushell_core_config
)