
## Install pipx

```bash
sudo apt install pipx
pipx ensurepath

# or using pip

sudo apt install python3-pip
# alternative
# 	wget https://bootstrap.pypa.io/get-pip.py
	# python3 get-pip.py

# then

pip install --user pipx
pipx ensurepath
```

## Install Zephyr

```bash
# Install west in an isolated pipx environment
pipx install west

# Initialise a new Zephyr workspace
west init ~/zephyrproject
cd ~/zephyrproject
west update

# Install the Zephyr SDK (toolchain: arm-zephyr-eabi-gcc)
wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.8/zephyr-sdk-0.16.8_linux-x86_64.tar.xz
tar xf zephyr-sdk-0.16.8_linux-x86_64.tar.xz
cd zephyr-sdk-0.16.8
./setup.sh

# Inject Zephyr's Python dependencies into west's pipx environment
pipx runpip west install -r ~/zephyrproject/zephyr/scripts/requirements.txt
```

## Build the project

```bash
# Blue Pill (STM32F103C8T6)
west build -b stm32_min_dev -- -DBOARD_ROOT=. 

# Black Pill (STM32F411CEU6)
west build -b blackpill_f411ce

# Clean build
west build -b blackpill_f411ce --pristine

# Flash via ST-Link
west flash

# Flash via DFU (no ST-Link needed, Blue/Black Pill support this)
west flash --runner dfu-util
```

## Usefull west commands

```bash
# See all supported STM32 boards
west boards | grep stm32

# Interactive Kconfig menu (like Linux menuconfig)
west build -t menuconfig

# RAM and flash usage reports
west build -t ram_report
west build -t rom_report

# Open a serial console at 115200
west espressif monitor   # or use minicom / picocom directly
```