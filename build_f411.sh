#!/bin/bash
mkdir -p build_stm32f411
cd build_stm32f411
cmake -DCMAKE_TOOLCHAIN_FILE=../stm32-arm-none-eabi.cmake -DSTM32_TARGET=STM32F411 ..
make -j$(nproc)