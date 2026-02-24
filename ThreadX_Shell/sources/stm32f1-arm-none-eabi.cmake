
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

set(THREADX_ARCH "cortex_m3")
set(THREADX_TOOLCHAIN "gnu")

# CMake-visible target identifier — use in CMakeLists.txt with:
#   if(STM32_FAMILY STREQUAL "F1")
set(STM32_FAMILY "F1" CACHE STRING "STM32 family: F1 or F4")

set(CMAKE_C_STANDARD          11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
# NOTE: -flto is intentionally omitted. LTO breaks ThreadX assembly port
# files (tx_initialize_low_level.S) and can silently discard weak-symbol
# hooks (tx_application_define) during cross-link. Safe to re-add only if
# you audit every object file in the ThreadX port for LTO compatibility.
set(CMAKE_C_FLAGS "${CPU_FLAGS} -Wall -O2 -Wextra -DSTM32F1")
set(CMAKE_CXX_FLAGS "${CPU_FLAGS} -Wall -O2 -Wextra -DSTM32F1 -fno-exceptions -fno-rtti")
set(CMAKE_ASM_FLAGS "${CPU_FLAGS}")

# Define the linker script
set(CMAKE_EXE_LINKER_FLAGS "-nostartfiles -Wl,--script=${CMAKE_SOURCE_DIR}/linker/stm32f103c8t6.ld,--gc-sections,-Map=${CMAKE_BINARY_DIR}/${PROJECT_NAME}.map --specs=nano.specs --specs=nosys.specs")

# Sysroot — point CMake at the toolchain's own sysroot so the ONLY/NEVER
# find-root modes below actually restrict to cross-compiled artefacts.
# Override on the command line if your toolchain lives elsewhere:
#   cmake -DTOOLCHAIN_DIR=/path/to/arm-gnu-toolchain ...
if(NOT DEFINED TOOLCHAIN_DIR)
    set(TOOLCHAIN_DIR "/opt/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi")
endif()
set(CMAKE_FIND_ROOT_PATH "${TOOLCHAIN_DIR}/${TOOLCHAIN_PREFIX}")

# Don't look for standard system libraries
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Don't try to compile a test program
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
