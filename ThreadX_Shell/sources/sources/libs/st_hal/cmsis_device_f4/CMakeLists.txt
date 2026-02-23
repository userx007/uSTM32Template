add_library(cmsis_device_f4 STATIC
    Source/Templates/system_stm32f4xx.c
)

target_include_directories(cmsis_device_f4 PUBLIC
    Include
    ${CMAKE_SOURCE_DIR}/sources/libs/st_hal/CMSIS_5/CMSIS/Core/Include
)

target_compile_definitions(cmsis_device_f4 PUBLIC
    STM32F411xE
)