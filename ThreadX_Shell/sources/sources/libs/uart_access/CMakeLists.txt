cmake_minimum_required(VERSION 3.3)
project(uart_access)


add_library(${PROJECT_NAME}
    STATIC
        src/uart_access.cpp
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${PROJECT_SOURCE_DIR}/inc
)

if(STM32_FAMILY STREQUAL "F4")
    target_link_libraries(${PROJECT_NAME}
        PUBLIC 
            stm32f4xx_hal
    )
elseif(STM32_FAMILY STREQUAL "F1")
    target_link_libraries(${PROJECT_NAME} 
        PUBLIC 
            stm32f1xx_hal
    )
endif()

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        ushell_core_config
)