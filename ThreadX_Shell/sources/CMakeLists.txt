cmake_minimum_required(VERSION 3.20)
project(stm32app C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED)

# Enable warnings project-wide
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    add_compile_options(-Wno-cast-function-type)
elseif(MSVC)
    add_compile_options(/W4)
endif()

# Putty, Zoc
# add_compile_definitions(SERIAL_TERMINAL)

# RENODE
# add_compile_definitions(MY_TERMINAL)

add_compile_definitions(MY_TERMINAL)

# ============== TARGET-SPECIFIC CONFIGURATION ==============
if(STM32_TARGET STREQUAL "STM32F103")
    set(THREADX_PORT "GCC/ARM_CM3")
    set(THREADX_CONFIG_DIR "${CMAKE_SOURCE_DIR}/sources/libs/threadx_config/stm32f103")
    add_compile_definitions(STM32F1)
    add_compile_definitions(STM32F103)
    
elseif(STM32_TARGET STREQUAL "STM32F411")
    set(THREADX_PORT "GCC/ARM_CM4F")
    set(THREADX_CONFIG_DIR "${CMAKE_SOURCE_DIR}/sources/libs/threadx_config/stm32f411")
    add_compile_definitions(STM32F4)
    add_compile_definitions(STM32F411)
endif()

add_subdirectory(threadx)          
add_subdirectory(sources)

add_executable(${PROJECT_NAME}.elf)

# Link libraries
target_link_libraries(${PROJECT_NAME}.elf
    user_main
    ushell_core
    ushell_core_utils
    ushell_user_root
    uart_access
    hd44780
    threadx
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