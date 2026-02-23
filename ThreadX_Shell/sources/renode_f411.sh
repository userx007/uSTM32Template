#!/bin/bash

BUILD_DIR=$(pwd)/build_stm32f411
cp stm32_f411.resc ${BUILD_DIR}

cd ${BUILD_DIR}

# Run renode with absolute path
renode --console ${BUILD_DIR}/stm32_f411.resc