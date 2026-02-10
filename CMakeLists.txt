cmake_minimum_required(VERSION 3.5)

# ============== TARGET SELECTION ==============
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=../stm32-arm-none-eabi.cmake -DSTM32_TARGET=STM32F103 ..
#    or: cmake -DCMAKE_TOOLCHAIN_FILE=../stm32-arm-none-eabi.cmake -DSTM32_TARGET=STM32F411 ..
set(STM32_TARGET "STM32F103" CACHE STRING "STM32 Target: STM32F103 or STM32F411")
set_property(CACHE STM32_TARGET PROPERTY STRINGS "STM32F103" "STM32F411")

message(STATUS "=========================================")
message(STATUS "STM32 Target: ${STM32_TARGET}")
message(STATUS "=========================================")

# Validate target selection
if(NOT STM32_TARGET STREQUAL "STM32F103" AND NOT STM32_TARGET STREQUAL "STM32F411")
    message(FATAL_ERROR "Invalid STM32_TARGET: ${STM32_TARGET}. Must be STM32F103 or STM32F411")
endif()

project(stm32app C CXX ASM)

# Putty, Zoc
# add_compile_definitions(SERIAL_TERMINAL)
# RENODE
add_compile_definitions(MY_TERMINAL)

# ============== TARGET-SPECIFIC CONFIGURATION ==============
if(STM32_TARGET STREQUAL "STM32F103")
    # STM32F103 Configuration (Cortex-M3)
    set(MCU_FAMILY "stm32f1")
    set(MCU_LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/linker/stm32f103cbt6.ld")
    set(FREERTOS_PORT "GCC/ARM_CM3")
    set(FREERTOS_CONFIG_DIR "${CMAKE_SOURCE_DIR}/sources/libs/stm32f103")
    set(MCU_CPU "cortex-m3")
    set(MCU_FPU_FLAGS "")
    set(MCU_FLOAT_ABI "soft")
    add_compile_definitions(STM32F1)
    add_compile_definitions(STM32F103)
    
elseif(STM32_TARGET STREQUAL "STM32F411")
    # STM32F411 Configuration (Cortex-M4F)
    set(MCU_FAMILY "stm32f4")
    set(MCU_LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/linker/stm32f411ceu6.ld")
    set(FREERTOS_PORT "GCC/ARM_CM4F")
    set(FREERTOS_CONFIG_DIR "${CMAKE_SOURCE_DIR}/sources/libs/stm32f411")
    set(MCU_CPU "cortex-m4")
    set(MCU_FPU_FLAGS "-mfpu=fpv4-sp-d16")
    set(MCU_FLOAT_ABI "hard")
    add_compile_definitions(STM32F4)
    add_compile_definitions(STM32F411)
endif()

# Display configuration
message(STATUS "Configuration:")
message(STATUS "  MCU Family: ${MCU_FAMILY}")
message(STATUS "  MCU CPU: ${MCU_CPU}")
message(STATUS "  FreeRTOS Port: ${FREERTOS_PORT}")
message(STATUS "  Linker Script: ${MCU_LINKER_SCRIPT}")
message(STATUS "  FreeRTOS Config: ${FREERTOS_CONFIG_DIR}")
message(STATUS "  Float ABI: ${MCU_FLOAT_ABI}")

# ============== COMPILER FLAGS ==============
# IMPORTANT: Append to existing flags from toolchain file, don't replace them
set(TARGET_SPECIFIC_FLAGS 
    "-mcpu=${MCU_CPU}"
    "-mthumb"
    "-mfloat-abi=${MCU_FLOAT_ABI}"
    ${MCU_FPU_FLAGS}
)

# Convert list to string
string(REPLACE ";" " " TARGET_SPECIFIC_FLAGS_STR "${TARGET_SPECIFIC_FLAGS}")

# Append to existing flags from toolchain
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TARGET_SPECIFIC_FLAGS_STR}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TARGET_SPECIFIC_FLAGS_STR}")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} ${TARGET_SPECIFIC_FLAGS_STR}")

# Linker flags
set(CMAKE_EXE_LINKER_FLAGS 
    "${CMAKE_EXE_LINKER_FLAGS} \
    -T${MCU_LINKER_SCRIPT} \
    -mcpu=${MCU_CPU} \
    -mthumb \
    -mfloat-abi=${MCU_FLOAT_ABI} \
    ${MCU_FPU_FLAGS} \
    -specs=nano.specs \
    -specs=nosys.specs \
    -lc \
    -lm \
    -lnosys \
    -Wl,--gc-sections \
    -Wl,-Map=${PROJECT_NAME}.map,--cref \
    -Wl,--print-memory-usage"
)

# ============== LIBOPENCM3 ==============
set(LIBOPENCM3_DIR ${CMAKE_SOURCE_DIR}/libopencm3)
set(LIBOPENCM3_LIB_FILE ${LIBOPENCM3_DIR}/lib/libopencm3_${MCU_FAMILY}.a)
set(LIBOPENCM3_LIB ${LIBOPENCM3_LIB_FILE})

# Create a custom command to build libopencm3
add_custom_command(
    OUTPUT ${LIBOPENCM3_LIB_FILE}
    COMMAND make
    WORKING_DIRECTORY ${LIBOPENCM3_DIR}
    COMMENT "Building libopencm3 for ${MCU_FAMILY}..."
)

# Create a custom target that depends on the library file
add_custom_target(libopencm3_build
    DEPENDS ${LIBOPENCM3_LIB_FILE}
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
    freertos
    ${LIBOPENCM3_LIB}
    ushell_core
    ushell_core_utils
    ushell_user_root
    uart_access
    user_main
)

# ============== POST-BUILD ==============
# Generate hex and bin files
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex ${PROJECT_NAME}.elf ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary ${PROJECT_NAME}.elf ${PROJECT_NAME}.bin
    COMMENT "Generating hex and bin files..."
)

# Print size information
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_SIZE} ${PROJECT_NAME}.elf
    COMMENT "Size information:"
)

# ============== BUILD INFO ==============
message(STATUS "=========================================")
message(STATUS "Build configuration complete!")
message(STATUS "To build:")
message(STATUS "  cd build")
message(STATUS "  cmake -DCMAKE_TOOLCHAIN_FILE=../stm32-arm-none-eabi.cmake -DSTM32_TARGET=${STM32_TARGET} ..")
message(STATUS "  make")
message(STATUS "=========================================")