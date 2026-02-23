add_library(cmsis_device_f1 STATIC
    Source/Templates/system_stm32f1xx.c
)

target_include_directories(cmsis_device_f1 PUBLIC
    Include
    ${CMAKE_SOURCE_DIR}/sources/libs/st_hal/CMSIS_5/CMSIS/Core/Include
)

target_compile_definitions(cmsis_device_f1 PUBLIC
    STM32F103xB       # adjust to your exact F1 chip
)