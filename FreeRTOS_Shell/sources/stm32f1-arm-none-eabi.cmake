
###########################################################################################################
# Toolchain file for building STM32 firmware using arm-none-eabi-gcc on Linux
#
# Usage:
# cmake -DCMAKE_TOOLCHAIN_FILE=stm32f1-arm-none-eabi-toolchain.cmake -B build_stm32f1 -S .
# cmake --build build_stm32f1
###########################################################################################################

# Target system
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m3)

# CMake-visible target identifier — use in CMakeLists.txt with:
#   if(STM32_FAMILY STREQUAL "F1")
set(STM32_FAMILY "F1" CACHE STRING "STM32 family: F1 or F4")


# Toolchain prefix
set(TOOLCHAIN_PREFIX arm-none-eabi)

# Compilers
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}-gcc)

# Optional: GDB for debugging
set(CMAKE_GDB ${TOOLCHAIN_PREFIX}-gdb)

# Flags
set(CPU_FLAGS "-mcpu=cortex-m3 -mthumb")
set(CMAKE_C_FLAGS "${CPU_FLAGS} -Wall -O2 -Wextra -flto -I ${CMAKE_SOURCE_DIR}/libopencm3/include -DSTM32F1")
set(CMAKE_CXX_FLAGS "${CPU_FLAGS} -Wall -O2 -Wextra -flto -I ${CMAKE_SOURCE_DIR}/libopencm3/include -DSTM32F1 -fno-exceptions -fno-rtti")

# Define the linker script
set(CMAKE_EXE_LINKER_FLAGS "-nostartfiles -Wl,--script=${CMAKE_SOURCE_DIR}/linker/stm32f103c8t6.ld,--gc-sections,-Map=${CMAKE_BINARY_DIR}/${PROJECT_NAME}.map --specs=nano.specs --specs=nosys.specs")

# Don't look for standard system libraries
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Don't try to compile a test program
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)


# libopencm3 library
set(LIBOPENCM3_DIR ${CMAKE_SOURCE_DIR}/libopencm3 CACHE PATH "Path to libopencm3")
set(LIBOPENCM3_LIB ${LIBOPENCM3_DIR}/lib/libopencm3_stm32f1.a CACHE FILEPATH "libopencm3 static library")

add_compile_definitions(USE_LIBOPENCM3)

