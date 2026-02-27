cmake_minimum_required(VERSION 3.3)
project(isr_handlers)


add_library(${PROJECT_NAME}
    OBJECT
        src/isr_handlers.cpp
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
       inc
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        ${LIBOPENCM3_LIB}
        ao_generic
        ao_config
        button_registry
)