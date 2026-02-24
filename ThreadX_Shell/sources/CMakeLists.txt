cmake_minimum_required(VERSION 3.20)
project(stm32app C CXX ASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Extra warnings not already covered by the toolchain flags
add_compile_options(-Wpedantic -Wno-cast-function-type)

# Terminal backend
add_compile_definitions(MY_TERMINAL)

# ============== TARGET-SPECIFIC CONFIGURATION ==============
if(STM32_FAMILY STREQUAL "F1")
    set(THREADX_CONFIG_DIR "${CMAKE_SOURCE_DIR}/sources/threadx_port/stm32f103/inc/")
    set(STARTUP_FILE "${CMAKE_SOURCE_DIR}/startup/startup_stm32f103xb.s")
    add_compile_definitions(STM32F103)

elseif(STM32_FAMILY STREQUAL "F4")
    set(THREADX_CONFIG_DIR "${CMAKE_SOURCE_DIR}/sources/threadx_port/stm32f411/inc/")
    set(STARTUP_FILE "${CMAKE_SOURCE_DIR}/startup/startup_stm32f4xx.s")
    add_compile_definitions(STM32F411)

else()
    message(FATAL_ERROR "Unknown STM32_FAMILY '${STM32_FAMILY}'. Set it in your toolchain file.")
endif()

add_subdirectory(threadx)
add_subdirectory(sources)

add_executable(${PROJECT_NAME}.elf
    ${STARTUP_FILE}
)

target_link_libraries(${PROJECT_NAME}.elf
    threadx
    threadx_port   
    user_main
    ushell_core
    ushell_core_utils
    ushell_user_root
    uart_access
    hd44780
)

target_link_options(${PROJECT_NAME}.elf 
    PRIVATE
        -Wl,--whole-archive
        $<TARGET_FILE:threadx_port>
        -Wl,--no-whole-archive
)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND arm-none-eabi-objcopy -O binary
            $<TARGET_FILE:${PROJECT_NAME}.elf>
            ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin
    COMMAND arm-none-eabi-objcopy -O ihex
            $<TARGET_FILE:${PROJECT_NAME}.elf>
            ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.hex
    COMMENT "Converting ELF to BIN and HEX..."
)