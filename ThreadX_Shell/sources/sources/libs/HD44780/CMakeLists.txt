
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
        threadx
        ushell_core_config
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
