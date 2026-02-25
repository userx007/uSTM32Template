#!/bin/bash

rm -rf build/*

west build -p always -b stm32_min_dev -- -DBOARD_ROOT=.

# ensure this export
# export ZEPHYR_BASE=~/zephyrproject/zephyr