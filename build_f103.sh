#!/bin/bash
mkdir -p build_stm32f103
cd build_stm32f103
cmake -DCMAKE_TOOLCHAIN_FILE=../stm32-arm-none-eabi.cmake -DSTM32_TARGET=STM32F103 ..
make -j$(nproc)