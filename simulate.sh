#!/bin/bash

cp stm32.resc build

# Get current directory
CRT_DIR=$(pwd)
BUILD_DIR=${CRT_DIR}/build
cd ${BUILD_DIR}

# Run renode with absolute path
renode --console ${BUILD_DIR}/stm32.resc