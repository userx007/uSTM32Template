## Step-by-Step Guide: STM32F4xx with Embassy-rs

### 1. Prerequisites & Tool Installation

First, install Rust and required tools:

```bash
# Install Rust (if not already installed)
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source $HOME/.cargo/env

# Install the ARM Cortex-M4F target (STM32F4 uses Cortex-M4F with FPU)
rustup target add thumbv7em-none-eabihf

# Install cargo-binutils for creating binaries
cargo install cargo-binutils
rustup component add llvm-tools-preview

# Install probe-rs for flashing (optional, for real hardware)
cargo install probe-rs-tools

# Install flip-link for stack overflow protection
cargo install flip-link
```

### 2. Install Renode

```bash
wget https://packages.microsoft.com/config/debian/12/packages-microsoft-prod.deb
sudo dpkg -i packages-microsoft-prod.deb
sudo apt update
sudo apt install dotnet-runtime-8.0

# On Linux (Ubuntu/Debian)
wget https://github.com/renode/renode/releases/download/v1.16.0/renode_1.16.0_amd64.deb
sudo apt install ./renode_1.16.0_amd64.deb

# On macOS
brew install --cask renode

# Or download from: https://github.com/renode/renode/releases
```

### 3. Create New Embassy Project

```bash
# Create new project
cargo new stm32f4-embassy-minimal
cd stm32f4-embassy-minimal

# Create necessary directories
mkdir .cargo
```

### 4. Configure Cargo

Create `.cargo/config.toml`:

```toml
[target.thumbv7em-none-eabihf]
runner = "probe-rs run --chip STM32F407VGTx"
rustflags = [
  "-C", "link-arg=-Tlink.x",
  "-C", "link-arg=-Tdefmt.x",
]

[build]
target = "thumbv7em-none-eabihf"

[env]
DEFMT_LOG = "info"
```

### 5. Configure Dependencies

Edit `Cargo.toml`:

```toml
[package]
name = "stm32f4-embassy-minimal"
version = "0.1.0"
edition = "2021"

[dependencies]
# Use stm32f407vg for STM32F407 Discovery board
# Other options: stm32f401, stm32f411, stm32f429, etc.
embassy-stm32 = { version = "0.1.0", features = ["stm32f407vg", "time-driver-any", "unstable-pac", "memory-x"] }
embassy-executor = { version = "0.6.0", features = ["arch-cortex-m", "executor-thread", "defmt", "integrated-timers"] }
embassy-time = { version = "0.3.0", features = ["defmt", "defmt-timestamp-uptime", "tick-hz-32_768"] }
embassy-sync = { version = "0.6.0", features = ["defmt"] }

cortex-m = { version = "0.7.7", features = ["inline-asm", "critical-section-single-core"] }
cortex-m-rt = "0.7.3"

defmt = "0.3"
defmt-rtt = "0.4"
panic-probe = { version = "0.3", features = ["print-defmt"] }

[profile.release]
debug = 2
lto = true
opt-level = "z"

[profile.dev]
debug = 2
opt-level = 1
```

### 6. Create the Minimal Firmware

Create `src/main.rs`:

```rust
#![no_std]
#![no_main]

use defmt::*;
use embassy_executor::Spawner;
use embassy_stm32::gpio::{Level, Output, Speed};
use embassy_time::Timer;
use {defmt_rtt as _, panic_probe as _};

#[embassy_executor::main]
async fn main(_spawner: Spawner) {
    let p = embassy_stm32::init(Default::default());
    
    info!("Embassy STM32F4 Minimal Example Starting!");

    // Configure PD12 as output (Green LED on STM32F407 Discovery board)
    // For other boards, adjust the pin:
    // - STM32F401 Nucleo: PA5
    // - STM32F411 Discovery: PD13 (orange), PD14 (red), PD15 (blue)
    let mut led = Output::new(p.PD12, Level::Low, Speed::Low);

    loop {
        info!("LED ON");
        led.set_high();
        Timer::after_millis(500).await;

        info!("LED OFF");
        led.set_low();
        Timer::after_millis(500).await;
    }
}
```

### 7. Create Memory Configuration

Create `memory.x`:

```ld
/* STM32F407VG memory layout */
MEMORY
{
  FLASH : ORIGIN = 0x08000000, LENGTH = 1024K
  RAM : ORIGIN = 0x20000000, LENGTH = 128K
}

/* For other STM32F4 variants, adjust as needed:
 * STM32F401RE: FLASH = 512K, RAM = 96K
 * STM32F411RE: FLASH = 512K, RAM = 128K
 * STM32F429ZI: FLASH = 2048K, RAM = 256K (192K + 64K CCM)
 */
```

### 8. Build the Firmware

```bash
# Build the project
cargo build --release

# Create binary file for Renode
cargo objcopy --release -- -O binary target/thumbv7em-none-eabihf/release/stm32f4-embassy-minimal.bin

# Or create hex file
cargo objcopy --release -- -O ihex target/thumbv7em-none-eabihf/release/stm32f4-embassy-minimal.hex
```

### 9. Create Renode Configuration

Create `stm32f4-embassy.resc`:

```renode
# Create machine
mach create "stm32f4"

# Load STM32F4 platform (adjust for your specific chip)
machine LoadPlatformDescription @platforms/boards/stm32f4_discovery-kit.repl

# Set binary file path
$bin?=@target/thumbv7em-none-eabihf/release/stm32f4-embassy-minimal.bin

# Load binary to flash
sysbus LoadBinary $bin 0x08000000

# Set CPU PC to reset vector
cpu PC 0x08000004

# Create UART analyzer window
showAnalyzer sysbus.usart2

# Start emulation
start
```

Alternatively, create a more interactive script `stm32f4-debug.resc`:

```renode
# Setup logging
logLevel -1 sysbus.usart2

# Create machine
mach create "stm32f4"
machine LoadPlatformDescription @platforms/boards/stm32f4_discovery-kit.repl

# Load ELF file (with debug symbols)
sysbus LoadELF @target/thumbv7em-none-eabihf/release/stm32f4-embassy-minimal

# Create UART window
showAnalyzer sysbus.usart2

# Enable GDB server (optional)
machine StartGdbServer 3333

# Start emulation
start
```

### 10. Run in Renode

```bash
# Start Renode
renode

# In Renode console, load your script
(monitor) include @stm32f4-embassy.resc

# Or load directly from command line
renode stm32f4-embassy.resc
```

### 11. Additional Renode Commands

Once running in Renode:

```renode
# Pause emulation
pause

# Resume emulation
start

# Show current state
machine

# Show peripherals
sysbus

# Reset machine
machine Reset

# Enable logging
logLevel 0

# Show CPU registers
sysbus.cpu GetRegisters

# Monitor GPIO state
sysbus.gpioPortD

# Watch specific peripheral
logLevel 0 sysbus.usart2
```

### 12. Debugging with GDB (Optional)

In one terminal:
```bash
renode stm32f4-debug.resc
```

In another terminal:
```bash
arm-none-eabi-gdb target/thumbv7em-none-eabihf/release/stm32f4-embassy-minimal
(gdb) target remote :3333
(gdb) load
(gdb) continue
```

### Project Structure

Your final project should look like:
```
stm32f4-embassy-minimal/
├── .cargo/
│   └── config.toml
├── src/
│   └── main.rs
├── Cargo.toml
├── memory.x
├── stm32f4-embassy.resc
└── stm32f4-debug.resc
```

### STM32F4 Variant-Specific Notes

#### STM32F401 (Nucleo-64)
```toml
# In Cargo.toml
embassy-stm32 = { version = "0.1.0", features = ["stm32f401re", ...] }
```
```ld
/* In memory.x */
FLASH : ORIGIN = 0x08000000, LENGTH = 512K
RAM : ORIGIN = 0x20000000, LENGTH = 96K
```
LED Pin: PA5

#### STM32F411 (Discovery)
```toml
# In Cargo.toml
embassy-stm32 = { version = "0.1.0", features = ["stm32f411re", ...] }
```
```ld
/* In memory.x */
FLASH : ORIGIN = 0x08000000, LENGTH = 512K
RAM : ORIGIN = 0x20000000, LENGTH = 128K
```
LED Pins: PD13 (orange), PD14 (red), PD15 (blue)

#### STM32F429 (Discovery)
```toml
# In Cargo.toml
embassy-stm32 = { version = "0.1.0", features = ["stm32f429zi", ...] }
```
```ld
/* In memory.x */
FLASH : ORIGIN = 0x08000000, LENGTH = 2048K
RAM : ORIGIN = 0x20000000, LENGTH = 192K
/* CCM RAM at 0x10000000, LENGTH = 64K (optional, for DMA-free buffers) */
```
LED Pin: PG13

### Troubleshooting Tips

1. **Build errors**: Ensure you have the correct target installed with `rustup target list --installed`
2. **Linking errors**: Check that `memory.x` is in the project root
3. **Renode can't find binary**: Use absolute paths or check working directory
4. **No output in Renode**: Make sure defmt-rtt is properly configured
5. **Wrong chip variant**: Double-check your STM32F4 part number and update the feature flag in Cargo.toml
6. **FPU issues**: STM32F4 has an FPU, so make sure you're using `thumbv7em-none-eabihf` (hard-float) target

### Use Renode's RTT support (Advanced)

```renode
mach create "stm32f4"
machine LoadPlatformDescription @platforms/boards/stm32f4_discovery-kit.repl

# Enable RTT
machine CreateRttChannel "defmt" 0

sysbus LoadELF @target/thumbv7em-none-eabihf/release/stm32f4-embassy-minimal

# Show RTT output
showAnalyzer rtt

machine StartGdbServer 3333
start
```

### Use Renode's UART

```renode
mach create "stm32f4"
machine LoadPlatformDescription @platforms/boards/stm32f4_discovery-kit.repl

logLevel 3 sysbus

sysbus LoadELF @target/thumbv7em-none-eabihf/release/stm32f4-embassy-minimal

# Show USART output
showAnalyzer sysbus.usart2

machine StartGdbServer 3333
start
```

## What you need for real hardware:

### 1. **Hardware connections:**
For USART2 (common default on Discovery boards):
- **PA2 (TX)** → Connect to your USB-to-Serial adapter's RX pin
- **PA3 (RX)** → Connect to your USB-to-Serial adapter's TX pin
- **GND** → Connect to your adapter's GND

For USART1:
- **PA9 (TX)** → Connect to your USB-to-Serial adapter's RX pin
- **PA10 (RX)** → Connect to your USB-to-Serial adapter's TX pin
- **GND** → Connect to your adapter's GND

### 2. **Terminal settings:**
- **Baud rate:** 115200 (Embassy's default)
- **Data bits:** 8
- **Stop bits:** 1
- **Parity:** None
- **Flow control:** None

### 3. **Example terminal commands:**

**Linux/Mac:**
```bash
screen /dev/ttyUSB0 115200
# or
minicom -D /dev/ttyUSB0 -b 115200
# or
picocom -b 115200 /dev/ttyUSB0
```

**Windows:**
- Use PuTTY or TeraTerm, select your COM port, set baud to 115200

### 4. **Flashing to real hardware:**

Using probe-rs (recommended):
```bash
# Flash and run
cargo run --release

# Or just flash
probe-rs download --chip STM32F407VGTx target/thumbv7em-none-eabihf/release/stm32f4-embassy-minimal
```

Using OpenOCD:
```bash
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg -c "program target/thumbv7em-none-eabihf/release/stm32f4-embassy-minimal.elf verify reset exit"
```

## Key Differences from STM32F1:

1. **Target Architecture**: `thumbv7em-none-eabihf` (Cortex-M4F with hardware FPU) vs `thumbv7m-none-eabi` (Cortex-M3)
2. **More RAM and Flash**: Typically 512KB-2MB flash and 96KB-256KB RAM
3. **Hardware FPU**: Can use floating-point operations efficiently
4. **More peripherals**: Additional timers, ADCs, DACs, etc.
5. **Higher clock speeds**: Up to 168-180 MHz vs 72 MHz on F1
6. **Different GPIO ports**: Often uses different default LED pins

---

## Step-by-Step Guide: STM32F411 with Embassy-rs

### 1. Prerequisites & Tool Installation

First, install Rust and required tools:

```bash
# Install Rust (if not already installed)
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source $HOME/.cargo/env

# Install the ARM Cortex-M4F target (STM32F411 uses Cortex-M4F with FPU)
rustup target add thumbv7em-none-eabihf

# Install cargo-binutils for creating binaries
cargo install cargo-binutils
rustup component add llvm-tools-preview

# Install probe-rs for flashing (recommended for real hardware)
cargo install probe-rs-tools

# Install flip-link for stack overflow protection
cargo install flip-link
```

### 2. Install Renode

```bash
wget https://packages.microsoft.com/config/debian/12/packages-microsoft-prod.deb
sudo dpkg -i packages-microsoft-prod.deb
sudo apt update
sudo apt install dotnet-runtime-8.0

# On Linux (Ubuntu/Debian)
wget https://github.com/renode/renode/releases/download/v1.16.0/renode_1.16.0_amd64.deb
sudo apt install ./renode_1.16.0_amd64.deb

# On macOS
brew install --cask renode

# Or download from: https://github.com/renode/renode/releases
```

### 3. Create New Embassy Project

```bash
# Create new project
cargo new stm32f411-embassy-minimal
cd stm32f411-embassy-minimal

# Create necessary directories
mkdir .cargo
```

### 4. Configure Cargo

Create `.cargo/config.toml`:

```toml
[target.thumbv7em-none-eabihf]
# For STM32F411RE (Nucleo-64) or STM32F411CE (BlackPill)
runner = "probe-rs run --chip STM32F411RETx"

rustflags = [
  "-C", "link-arg=-Tlink.x",
  "-C", "link-arg=-Tdefmt.x",
]

[build]
target = "thumbv7em-none-eabihf"

[env]
DEFMT_LOG = "info"
```

### 5. Configure Dependencies

Edit `Cargo.toml`:

```toml
[package]
name = "stm32f411-embassy-minimal"
version = "0.1.0"
edition = "2021"

[dependencies]
# STM32F411RE for Nucleo board or STM32F411CE for BlackPill
embassy-stm32 = { version = "0.1.0", features = ["stm32f411re", "time-driver-any", "unstable-pac", "memory-x"] }
embassy-executor = { version = "0.6.0", features = ["arch-cortex-m", "executor-thread", "defmt", "integrated-timers"] }
embassy-time = { version = "0.3.0", features = ["defmt", "defmt-timestamp-uptime", "tick-hz-32_768"] }
embassy-sync = { version = "0.6.0", features = ["defmt"] }

cortex-m = { version = "0.7.7", features = ["inline-asm", "critical-section-single-core"] }
cortex-m-rt = "0.7.3"

defmt = "0.3"
defmt-rtt = "0.4"
panic-probe = { version = "0.3", features = ["print-defmt"] }

[profile.release]
debug = 2
lto = true
opt-level = "z"

[profile.dev]
debug = 2
opt-level = 1
```

### 6. Create the Minimal Firmware

Create `src/main.rs`:

```rust
#![no_std]
#![no_main]

use defmt::*;
use embassy_executor::Spawner;
use embassy_stm32::gpio::{Level, Output, Speed};
use embassy_time::Timer;
use {defmt_rtt as _, panic_probe as _};

#[embassy_executor::main]
async fn main(_spawner: Spawner) {
    let p = embassy_stm32::init(Default::default());
    
    info!("Embassy STM32F411 Minimal Example Starting!");

    // LED pin selection based on your board:
    // STM32F411 Nucleo-64: PA5 (LD2 - green LED)
    // STM32F411 BlackPill: PC13 (onboard LED)
    // STM32F411 Discovery: PD13 (orange), PD14 (red), PD15 (blue), PD12 (green)
    
    let mut led = Output::new(p.PA5, Level::Low, Speed::Low);  // Change pin as needed

    loop {
        info!("LED ON");
        led.set_high();
        Timer::after_millis(500).await;

        info!("LED OFF");
        led.set_low();
        Timer::after_millis(500).await;
    }
}
```

### 7. Create Memory Configuration

Create `memory.x`:

```ld
/* STM32F411RE/CE memory layout */
MEMORY
{
  FLASH : ORIGIN = 0x08000000, LENGTH = 512K
  RAM : ORIGIN = 0x20000000, LENGTH = 128K
}
```

### 8. Build the Firmware

```bash
# Build the project
cargo build --release

# Create binary file for Renode
cargo objcopy --release -- -O binary target/thumbv7em-none-eabihf/release/stm32f411-embassy-minimal.bin

# Or create hex file
cargo objcopy --release -- -O ihex target/thumbv7em-none-eabihf/release/stm32f411-embassy-minimal.hex
```

### 9. Create Renode Configuration

Create `stm32f411-embassy.resc`:

```renode
# Create machine
mach create "stm32f411"

# Load STM32F411 platform
# Note: Use stm32f4 generic platform or discovery board
machine LoadPlatformDescription @platforms/boards/stm32f4_discovery-kit.repl

# Set binary file path
$bin?=@target/thumbv7em-none-eabihf/release/stm32f411-embassy-minimal.bin

# Load binary to flash
sysbus LoadBinary $bin 0x08000000

# Set CPU PC to reset vector
cpu PC 0x08000004

# Create UART analyzer window
showAnalyzer sysbus.usart2

# Start emulation
start
```

Alternatively, create a more interactive script `stm32f411-debug.resc`:

```renode
# Setup logging
logLevel -1 sysbus.usart2

# Create machine
mach create "stm32f411"
machine LoadPlatformDescription @platforms/boards/stm32f4_discovery-kit.repl

# Load ELF file (with debug symbols)
sysbus LoadELF @target/thumbv7em-none-eabihf/release/stm32f411-embassy-minimal

# Create UART window
showAnalyzer sysbus.usart2

# Enable GDB server (optional)
machine StartGdbServer 3333

# Start emulation
start
```

### 10. Run in Renode

```bash
# Start Renode
renode

# In Renode console, load your script
(monitor) include @stm32f411-embassy.resc

# Or load directly from command line
renode stm32f411-embassy.resc
```

### 11. Additional Renode Commands

Once running in Renode:

```renode
# Pause emulation
pause

# Resume emulation
start

# Show current state
machine

# Show peripherals
sysbus

# Reset machine
machine Reset

# Enable logging
logLevel 0

# Show CPU registers
sysbus.cpu GetRegisters

# Monitor GPIO state
sysbus.gpioPortA

# Watch UART traffic
logLevel 0 sysbus.usart2
```

### 12. Debugging with GDB (Optional)

In one terminal:
```bash
renode stm32f411-debug.resc
```

In another terminal:
```bash
arm-none-eabi-gdb target/thumbv7em-none-eabihf/release/stm32f411-embassy-minimal
(gdb) target remote :3333
(gdb) load
(gdb) continue
```

### Project Structure

Your final project should look like:
```
stm32f411-embassy-minimal/
├── .cargo/
│   └── config.toml
├── src/
│   └── main.rs
├── Cargo.toml
├── memory.x
├── stm32f411-embassy.resc
└── stm32f411-debug.resc
```

### STM32F411 Board-Specific LED Pins

Choose the correct LED pin based on your board:

#### STM32F411 Nucleo-64 (most common)
```rust
let mut led = Output::new(p.PA5, Level::Low, Speed::Low);  // LD2 - green LED
```

#### STM32F411 BlackPill
```rust
let mut led = Output::new(p.PC13, Level::Low, Speed::Low);  // Onboard LED
```
**Note:** BlackPill LED is active LOW, so you may want to invert the logic:
```rust
let mut led = Output::new(p.PC13, Level::High, Speed::Low);  // Start OFF
// Then use set_low() to turn ON, set_high() to turn OFF
```

#### STM32F411 Discovery
```rust
let mut led_green = Output::new(p.PD12, Level::Low, Speed::Low);
let mut led_orange = Output::new(p.PD13, Level::Low, Speed::Low);
let mut led_red = Output::new(p.PD14, Level::Low, Speed::Low);
let mut led_blue = Output::new(p.PD15, Level::Low, Speed::Low);
```

### Troubleshooting Tips

1. **Build errors**: Ensure you have the correct target installed with `rustup target list --installed`
2. **Linking errors**: Check that `memory.x` is in the project root
3. **Renode can't find binary**: Use absolute paths or check working directory
4. **No output in Renode**: Make sure defmt-rtt is properly configured
5. **Wrong chip variant**: 
   - For STM32F411RE (Nucleo): Use feature `"stm32f411re"`
   - For STM32F411CE (BlackPill): Use feature `"stm32f411ce"`
6. **FPU issues**: STM32F411 has an FPU, so make sure you're using `thumbv7em-none-eabihf` (hard-float) target
7. **LED not visible**: Check if your board's LED is active HIGH or active LOW

### Use Renode's RTT support (Advanced)

```renode
mach create "stm32f411"
machine LoadPlatformDescription @platforms/boards/stm32f4_discovery-kit.repl

# Enable RTT
machine CreateRttChannel "defmt" 0

sysbus LoadELF @target/thumbv7em-none-eabihf/release/stm32f411-embassy-minimal

# Show RTT output
showAnalyzer rtt

machine StartGdbServer 3333
start
```

### Use Renode's UART

```renode
mach create "stm32f411"
machine LoadPlatformDescription @platforms/boards/stm32f4_discovery-kit.repl

logLevel 3 sysbus

sysbus LoadELF @target/thumbv7em-none-eabihf/release/stm32f411-embassy-minimal

# Show USART output
showAnalyzer sysbus.usart2

machine StartGdbServer 3333
start
```

## What you need for real hardware:

### 1. **Hardware connections:**

For USART2 (common on Nucleo boards):
- **PA2 (TX)** → Connect to your USB-to-Serial adapter's RX pin
- **PA3 (RX)** → Connect to your USB-to-Serial adapter's TX pin
- **GND** → Connect to your adapter's GND

For USART1:
- **PA9 (TX)** → Connect to your USB-to-Serial adapter's RX pin
- **PA10 (RX)** → Connect to your USB-to-Serial adapter's TX pin
- **GND** → Connect to your adapter's GND

**Note:** Many Nucleo boards have built-in ST-Link with virtual COM port, so you may not need an external USB-to-Serial adapter!

### 2. **Terminal settings:**
- **Baud rate:** 115200 (Embassy's default)
- **Data bits:** 8
- **Stop bits:** 1
- **Parity:** None
- **Flow control:** None

### 3. **Example terminal commands:**

**Linux/Mac:**
```bash
screen /dev/ttyUSB0 115200
# or
minicom -D /dev/ttyUSB0 -b 115200
# or
picocom -b 115200 /dev/ttyUSB0

# For Nucleo with built-in ST-Link:
screen /dev/ttyACM0 115200
```

**Windows:**
- Use PuTTY or TeraTerm, select your COM port, set baud to 115200

### 4. **Flashing to real hardware:**

#### Using probe-rs (recommended):
```bash
# Flash and run (with RTT logging)
cargo run --release

# Or just flash
probe-rs download --chip STM32F411RETx target/thumbv7em-none-eabihf/release/stm32f411-embassy-minimal

# For BlackPill (STM32F411CE):
probe-rs download --chip STM32F411CEUx target/thumbv7em-none-eabihf/release/stm32f411-embassy-minimal
```

#### Using OpenOCD:
```bash
# For Nucleo boards with ST-Link:
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg -c "program target/thumbv7em-none-eabihf/release/stm32f411-embassy-minimal.elf verify reset exit"
```

#### Using STM32CubeProgrammer:
1. Export binary: `cargo objcopy --release -- -O binary firmware.bin`
2. Open STM32CubeProgrammer
3. Connect to your board
4. Load `firmware.bin` at address `0x08000000`
5. Click "Download"

### 5. **Board-specific notes:**

#### STM32F411 Nucleo-64:
- Has built-in ST-Link V2-1 programmer/debugger
- Virtual COM port available via USB
- No external programmer needed
- User LED on PA5
- User button on PC13

#### STM32F411 BlackPill:
- No built-in programmer
- Need external ST-Link, J-Link, or DFU mode for flashing
- LED on PC13 (active LOW)
- Can use USB DFU bootloader for programming
- Crystal: 25 MHz (important for clock configuration)

## STM32F411 Features & Clock Configuration

The STM32F411 is a high-performance Cortex-M4F MCU with:
- **Clock speed:** Up to 100 MHz
- **Flash:** 512 KB
- **RAM:** 128 KB
- **FPU:** Single-precision floating-point unit
- **Peripherals:** USB OTG FS, I2C, SPI, USART, Timers, ADC, etc.

### Example with custom clock configuration:

```rust
use embassy_stm32::Config;
use embassy_stm32::rcc::{Pll, PllSource, Sysclk};

#[embassy_executor::main]
async fn main(_spawner: Spawner) {
    let mut config = Config::default();
    
    // Configure for 96 MHz (safe for USB)
    config.rcc.pll = Some(Pll {
        source: PllSource::HSE,  // Use external crystal
        prediv: 25,              // 25 MHz / 25 = 1 MHz
        mul: 192,                // 1 MHz * 192 = 192 MHz VCO
        divp: Some(2),           // 192 MHz / 2 = 96 MHz (SYSCLK)
        divq: Some(4),           // 192 MHz / 4 = 48 MHz (USB)
        divr: None,
    });
    config.rcc.sys = Sysclk::PLL1_P;
    
    let p = embassy_stm32::init(config);
    
    // Your code here...
}
```

## Key Features of STM32F411:

1. **100 MHz CPU**: Faster than F1 series (72 MHz)
2. **Hardware FPU**: Efficient floating-point math
3. **Low power modes**: Multiple sleep and stop modes
4. **USB OTG FS**: Can act as USB device or host
5. **DSP instructions**: Good for signal processing
6. **DMA**: 2 controllers for efficient data transfers

## Comparison: STM32F411 vs STM32F103

| Feature | STM32F411 | STM32F103 |
|---------|-----------|-----------|
| Core | Cortex-M4F | Cortex-M3 |
| Max Clock | 100 MHz | 72 MHz |
| FPU | Yes (single) | No |
| Flash | 512 KB | 64-128 KB |
| RAM | 128 KB | 20 KB |
| USB | OTG FS | Full-speed |
| Price | ~$3-4 | ~$2-3 |
| Power | Lower | Higher |

This gives you a complete working Embassy setup for the STM32F411!

---

# STM32F411 Embassy Blink & UART Example

A minimal Embassy-based example for STM32F411CEU6 (BlackPill board) featuring:
- Asynchronous LED blinking on PC13
- UART output on USART2 (PA2/TX, PA3/RX)
- Defmt logging via RTT

## Hardware Configuration

### STM32F411CEU6 (BlackPill)
- **LED**: PC13 (onboard LED, active low)
- **UART2 TX**: PA2
- **UART2 RX**: PA3
- **Flash**: 512KB
- **RAM**: 128KB

### Connections
- Connect a USB-to-Serial adapter to PA2 (TX) and PA3 (RX)
- UART settings: 115200 baud, 8N1 (default)

## Prerequisites

1. Install Rust and required tools:
```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
rustup target add thumbv7em-none-eabihf
cargo install probe-rs --features cli
```

2. Install required system packages (Linux):
```bash
# Ubuntu/Debian
sudo apt install libusb-1.0-0-dev libudev-dev

# For udev rules (allows non-root access to debug probes)
curl -L https://probe.rs/files/69-probe-rs.rules -o /tmp/69-probe-rs.rules
sudo mv /tmp/69-probe-rs.rules /etc/udev/rules.d/
sudo udevadm control --reload
sudo udevadm trigger
```

## Building

```bash
cargo build --release
```

## Flashing and Running

### With probe-rs (recommended)
```bash
cargo run --release
```

This will:
1. Build the project
2. Flash it to the STM32F411
3. Start RTT logging (you'll see defmt logs in the terminal)

### Manual flashing
```bash
cargo objcopy --release -- -O binary target.bin
probe-rs download --chip STM32F411CEUx target.bin
```

## Monitoring

### RTT Logs (defmt)
When you run `cargo run`, you'll see log messages like:
```
INFO  Starting STM32F411 Embassy example
INFO  LED ON
INFO  Sent UART message: count=0
INFO  LED OFF
INFO  LED ON
INFO  Sent UART message: count=1
```

### UART Output
Connect to the UART with a serial terminal:
```bash
# Linux
screen /dev/ttyUSB0 115200
# or
minicom -D /dev/ttyUSB0 -b 115200

# macOS
screen /dev/tty.usbserial-* 115200

# Windows (use PuTTY or TeraTerm)
```

You should see:
```
Hello from Embassy! Count: 0
Hello from Embassy! Count: 1
Hello from Embassy! Count: 2
...
```

## Project Structure

```
.
├── Cargo.toml          # Dependencies and project config
├── memory.x            # Linker script for STM32F411CEU6
├── build.rs            # Build script to configure linker
├── .cargo/
│   └── config.toml     # Cargo build configuration
└── main.rs             # Application code
```

## Code Overview

The example uses Embassy's async runtime with two tasks:

1. **blink_led**: Toggles PC13 every 500ms
2. **uart_task**: Sends a counter message every 2 seconds via UART2

Both tasks run concurrently thanks to Embassy's executor.

## Troubleshooting

### probe-rs can't find the chip
```bash
# List connected probes
probe-rs list

# Try specifying the probe explicitly
probe-rs run --chip STM32F411CEUx --probe <probe-id>
```

### Permission denied errors (Linux)
Make sure udev rules are installed (see Prerequisites).

### LED not blinking
- The onboard LED on PC13 is active LOW (LED on when pin is LOW)
- Check if your board has a different LED pin

### No UART output
- Verify TX/RX connections (cross-connection if needed)
- Check baud rate matches (115200)
- Verify UART2 pins: PA2 (TX), PA3 (RX)

## Customization

### Change LED pin
Edit `main.rs`, line where `blink_led` is spawned:
```rust
spawner.spawn(blink_led(p.PA5)).unwrap();  // Change to PA5
```

### Change UART baud rate
Edit `uart_task` function:
```rust
let mut config = Config::default();
config.baudrate = 9600;  // Change baud rate
```

### Adjust timing
Modify `Timer::after_millis()` or `Timer::after_secs()` calls.

## Resources

- [Embassy Documentation](https://embassy.dev/)
- [STM32F4 Reference Manual](https://www.st.com/resource/en/reference_manual/dm00119316.pdf)
- [probe-rs Documentation](https://probe.rs/)
