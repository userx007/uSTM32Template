###########################################################################################################
# Toolchain file for building STM32 firmware using arm-none-eabi-gcc on Linux
#
# Usage:
# cmake -DCMAKE_TOOLCHAIN_FILE=stm32-arm-none-eabi-toolchain.cmake -B build -S .
# cmake --build build
###########################################################################################################

# Target system
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m3)

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
set(CMAKE_C_FLAGS_INIT "${CPU_FLAGS} -Wall -O2")
set(CMAKE_CXX_FLAGS_INIT "${CPU_FLAGS} -Wall -O2")
set(CMAKE_ASM_FLAGS_INIT "${CPU_FLAGS}")

# Don't look for standard system libraries
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
