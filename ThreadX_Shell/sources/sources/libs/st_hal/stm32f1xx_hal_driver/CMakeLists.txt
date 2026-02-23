add_library(stm32f1xx_hal STATIC
    Src/stm32f1xx_hal.c
    Src/stm32f1xx_hal_cortex.c
    Src/stm32f1xx_hal_rcc.c
    Src/stm32f1xx_hal_rcc_ex.c
    Src/stm32f1xx_hal_gpio.c
    Src/stm32f1xx_hal_gpio_ex.c
    Src/stm32f1xx_hal_dma.c
    Src/stm32f1xx_hal_uart.c
    Src/stm32f1xx_hal_pwr.c
)

target_include_directories(stm32f1xx_hal PUBLIC
    Inc
)

target_link_libraries(stm32f1xx_hal PUBLIC
    cmsis_device_f1
)

target_include_directories(stm32f1xx_hal PUBLIC
    ${CMAKE_SOURCE_DIR}/sources/libs/threadx_config/stm32f103
)