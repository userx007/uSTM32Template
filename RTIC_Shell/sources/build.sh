#!/bin/bash

cargo build --release 

# Create binary file for Renode
cargo objcopy --release -- -O binary target/thumbv7em-none-eabihf/release/stm32f4-embassy-shell.bin

# Or create hex file
cargo objcopy --release -- -O ihex target/thumbv7em-none-eabihf/release/stm32f4-embassy-shell.hex