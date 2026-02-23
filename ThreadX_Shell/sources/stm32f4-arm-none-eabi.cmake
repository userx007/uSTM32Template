
###########################################################################################################
# Toolchain file for building STM32 firmware using arm-none-eabi-gcc on Linux
#
# Usage:
# cmake -DCMAKE_TOOLCHAIN_FILE=stm32f4-arm-none-eabi-toolchain.cmake -B build_stm32f4 -S .
# cmake --build build_stm32f4
###########################################################################################################

# Target system
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m4)

set(THREADX_ARCH "cortex_m4")
set(THREADX_TOOLCHAIN "gnu")

# CMake-visible target identifier — use in CMakeLists.txt with:
#   if(STM32_FAMILY STREQUAL "F4")
set(STM32_FAMILY "F4" CACHE STRING "STM32 family: F1 or F4")

# Toolchain prefix
set(TOOLCHAIN_PREFIX arm-none-eabi)

# Compilers
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}-gcc)

# Optional: GDB for debugging
set(CMAKE_GDB ${TOOLCHAIN_PREFIX}-gdb)

# Flags
set(CPU_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")
set(CMAKE_C_FLAGS "${CPU_FLAGS} -Wall -O2 -Wextra -DSTM32F4")
set(CMAKE_CXX_FLAGS "${CPU_FLAGS} -Wall -O2 -Wextra -DSTM32F4 -fno-exceptions -fno-rtti")
set(CMAKE_ASM_FLAGS "${CPU_FLAGS}")

# Define the linker script
set(CMAKE_EXE_LINKER_FLAGS "-nostartfiles -Wl,--script=${CMAKE_SOURCE_DIR}/linker/stm32f411ceu6.ld,--gc-sections,-Map=${CMAKE_BINARY_DIR}/${PROJECT_NAME}.map --specs=nano.specs --specs=nosys.specs")

# Don't look for standard system libraries
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Don't try to compile a test program
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
