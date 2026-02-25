#!/bin/bash

west build -b stm32_min_dev -- -DBOARD_ROOT=.

# ensure this export
# export ZEPHYR_BASE=~/zephyrproject/zephyr