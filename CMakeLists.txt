cmake_minimum_required(VERSION 3.5)
project(stm32app C CXX)

# Putty, Zoc
# add_compile_definitions(SERIAL_TERMINAL)

# RENODE
# add_compile_definitions(MY_TERMINAL)

add_compile_definitions(MY_TERMINAL)

# ============== TARGET-SPECIFIC CONFIGURATION ==============
if(STM32_TARGET STREQUAL "STM32F103")
    set(FREERTOS_PORT "GCC/ARM_CM3")
    set(FREERTOS_CONFIG_DIR "${CMAKE_SOURCE_DIR}/sources/libs/freertos_config/stm32f103")
    add_compile_definitions(STM32F1)
    add_compile_definitions(STM32F103)
    
elseif(STM32_TARGET STREQUAL "STM32F411")
    set(FREERTOS_PORT "GCC/ARM_CM4F")
    set(FREERTOS_CONFIG_DIR "${CMAKE_SOURCE_DIR}/sources/libs/freertos_config/stm32f411")
    add_compile_definitions(STM32F4)
    add_compile_definitions(STM32F411)
endif()
# Define libopencm3 paths
set(LIBOPENCM3_DIR ${CMAKE_SOURCE_DIR}/libopencm3)
set(LIBOPENCM3_BUILD_STAMP ${CMAKE_BINARY_DIR}/libopencm3.stamp)

# Create a custom command to build all libopencm3 libraries
add_custom_command(
    OUTPUT ${LIBOPENCM3_BUILD_STAMP}
    COMMAND make
    COMMAND ${CMAKE_COMMAND} -E touch ${LIBOPENCM3_BUILD_STAMP}
    WORKING_DIRECTORY ${LIBOPENCM3_DIR}
    COMMENT "Building all libopencm3 libraries..."
)

# Create a custom target that depends on the stamp file
add_custom_target(libopencm3_build
    DEPENDS ${LIBOPENCM3_BUILD_STAMP}
)

# Add libopencm3 include directories
include_directories(
    ${LIBOPENCM3_DIR}/include
)

# ============== FREERTOS ==============
# Set heap implementation (can be overridden from command line)
set(FREERTOS_HEAP "heap_4" CACHE STRING "FreeRTOS heap implementation")

# Add FreeRTOS subdirectory
add_subdirectory(FreeRTOS)

# ============== PROJECT SOURCES ==============
add_subdirectory(sources)

# ============== EXECUTABLE ==============
add_executable(${PROJECT_NAME}.elf)

# Make sure libopencm3 is built before linking
add_dependencies(${PROJECT_NAME}.elf libopencm3_build)

# Link libraries
target_link_libraries(${PROJECT_NAME}.elf
    user_main
    ushell_core
    ushell_core_utils
    ushell_user_root
    uart_access
    freertos
    ${LIBOPENCM3_LIB}
)



