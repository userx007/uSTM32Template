#![no_std]
#![no_main]

use core::cell::UnsafeCell;
use core::default::Default;
use core::option::Option::{self, None, Some};
use core::result::Result::Ok;

use embassy_executor::Spawner;
use embassy_stm32::gpio::{Level, Output, Speed};
use embassy_stm32::usart::{Config, Uart};
use embassy_stm32::{bind_interrupts, peripherals, usart};
use embassy_sync::channel::Channel;
use embassy_sync::blocking_mutex::raw::CriticalSectionRawMutex;
use embassy_time::Timer;
use nb;  // For non-blocking error handling
use panic_halt as _;

use ushell_config::*;
use ushell_dispatcher::{generate_commands_dispatcher, generate_shortcuts_dispatcher};
use ushell_usercode::commands as uc;
use ushell_usercode::shortcuts as us;

use ushell_logger::{log_simple, log_info};
use ushell_logger::{init_logger, LogLevel, LoggerConfig};

// Import the async-enabled shell components
use ushell2::{run_shell, AsyncReader, ShellConfig};

generate_commands_dispatcher! {
    mod commands;
    hexstr_size = crate::MAX_HEXSTR_LEN;
    error_buffer_size = crate::ERROR_BUFFER_SIZE;
    path = "../ushell_usercode/src/commands.cfg"
}

generate_shortcuts_dispatcher! {
    mod shortcuts;
    error_buffer_size = crate::ERROR_BUFFER_SIZE;
    path = "../ushell_usercode/src/shortcuts.cfg"
}

bind_interrupts!(struct Irqs {
    USART2 => usart::InterruptHandler<peripherals::USART2>;
});

// ============================================================================
// Global Storage
// ============================================================================

struct GlobalUartTx {
    tx: UnsafeCell<
        Option<
            embassy_stm32::usart::UartTx<'static, peripherals::USART2, peripherals::DMA1_CH6>,
        >,
    >,
}

struct GlobalUartRx {
    rx: UnsafeCell<
        Option<
            embassy_stm32::usart::UartRx<'static, peripherals::USART2, embassy_stm32::dma::NoDma>,
        >,
    >,
}

unsafe impl Sync for GlobalUartTx {}
unsafe impl Sync for GlobalUartRx {}

static GLOBAL_UART_TX: GlobalUartTx = GlobalUartTx {
    tx: UnsafeCell::new(None),
};

static GLOBAL_UART_RX: GlobalUartRx = GlobalUartRx {
    rx: UnsafeCell::new(None),
};

/// UART RX Channel
/// The uart_rx_task feeds this channel asynchronously
/// The shell's AsyncReader consumes from it
static UART_RX_CHANNEL: Channel<CriticalSectionRawMutex, u8, 1024> = Channel::new();

static mut GLOBAL_WRITER: UartWriter = UartWriter;

// ============================================================================
// UART Writer Implementation
// ============================================================================

pub struct UartWriter;

unsafe impl Send for UartWriter {}

impl Default for UartWriter {
    fn default() -> Self {
        Self::new()
    }
}

impl UartWriter {
    pub const fn new() -> Self {
        Self
    }

    fn write_bytes_internal(&mut self, bytes: &[u8]) {
        unsafe {
            if let Some(tx) = (*GLOBAL_UART_TX.tx.get()).as_mut() {
                let _ = tx.blocking_write(bytes);
            }
        }
    }
}

impl core::fmt::Write for UartWriter {
    fn write_str(&mut self, s: &str) -> core::fmt::Result {
        self.write_bytes_internal(s.as_bytes());
        Ok(())
    }
}

// ============================================================================
// Main Entry Point
// ============================================================================

#[embassy_executor::main]
async fn main(spawner: Spawner) {
    let p = embassy_stm32::init(Default::default());

    let usart: peripherals::USART2 = p.USART2;
    let tx_pin: peripherals::PA2 = p.PA2;
    let rx_pin: peripherals::PA3 = p.PA3;
    let tx_dma: peripherals::DMA1_CH6 = p.DMA1_CH6;

    let config = Config::default();

    let uart = Uart::new(
        usart,
        rx_pin,
        tx_pin,
        Irqs,
        tx_dma,
        embassy_stm32::dma::NoDma,  // No DMA for RX - works better in Renode
        config,
    )
    .unwrap();

    // Split UART into TX and RX to avoid conflicts
    let (tx, rx) = uart.split();

    unsafe {
        *GLOBAL_UART_TX.tx.get() = Some(tx);
        *GLOBAL_UART_RX.rx.get() = Some(rx);
    }

    unsafe {
        init_logger(
            LoggerConfig {
                color_entire_line: true,
                min_level: LogLevel::Debug,
            },
            &mut *core::ptr::addr_of_mut!(GLOBAL_WRITER),
        );
    }

    log_simple!("System initialized");
    log_simple!("UART configured with async shell (nb_read)");

    // Spawn all async tasks
    spawner.spawn(blink_led(p.PC13)).unwrap();
    spawner.spawn(uart_rx_task()).unwrap();
    spawner.spawn(shell_task()).unwrap();

    log_info!("All tasks spawned successfully");
}

// ============================================================================
// LED Blinker Task - Demonstrates async task cooperation
// ============================================================================

#[embassy_executor::task]
async fn blink_led(pin: peripherals::PC13) {
    let mut led = Output::new(pin, Level::High, Speed::Low);

    log_info!("LED task started");

    loop {
        led.set_high();
        log_info!("LED ON");
        Timer::after_millis(500).await;

        led.set_low();
        log_info!("LED OFF");
        Timer::after_millis(500).await;
    }
}

// ============================================================================
// UART RX Task - Non-blocking interrupt-driven reception
// ============================================================================
// This approach provides proper async behavior without DMA:
// - nb_read() checks for data without blocking
// - If data available: read it immediately
// - If no data: yield to other tasks (LED, shell, etc.)
// - Works perfectly in Renode and on real hardware
// ============================================================================


#[embassy_executor::task]
async fn uart_rx_task() {
    // Brief delay for UART initialization
    Timer::after_millis(100).await;

    log_info!("UART RX task started");

    // Take ownership of RX from global storage
    let rx = unsafe {
        (*GLOBAL_UART_RX.rx.get()).take()
    };

    if let Some(mut rx) = rx {
        loop {
            // Use nb_read for non-blocking check
            match rx.nb_read() {
                Ok(byte) => {
                    // Got a byte, send to channel
                    let _ = UART_RX_CHANNEL.send(byte).await;
                    // No delay here - immediately check for next byte
                }
                Err(nb::Error::WouldBlock) => {
                    // No data available - yield to other tasks
                    // This is the key: yield when there's nothing to do
                    Timer::after_micros(100).await;
                }
                Err(nb::Error::Other(_)) => {
                    // Error reading
                    Timer::after_millis(10).await;
                }
            }
        }
    } else {
        log_info!("UART RX not initialized!");
    }
}

// ============================================================================
// Shell Processing Task
// ============================================================================

#[embassy_executor::task]
async fn shell_task() {
    // Wait for system initialization
    Timer::after_millis(150).await;

    log_simple!("Starting async shell...");
    log_simple!("Type '##' for available commands");

    // ========================================================================
    // TX Functions (blocking is OK for TX - typically fast enough)
    // ========================================================================

    fn uart_write(bytes: &[u8]) {
        unsafe {
            if let Some(tx) = (*GLOBAL_UART_TX.tx.get()).as_mut() {
                let _ = tx.blocking_write(bytes);
            }
        }
    }

    fn uart_flush() {
        unsafe {
            if let Some(tx) = (*GLOBAL_UART_TX.tx.get()).as_mut() {
                let _ = tx.blocking_flush();
            }
        }
    }

    // ========================================================================
    // Create AsyncReader - The Heart of Async Support
    // ========================================================================

    let reader = AsyncReader::new(
        // Try to read from channel (non-blocking)
        || UART_RX_CHANNEL.try_receive().ok(),
        // Yield function - returns Future that yields for 50 microseconds
        // This is enough time for other tasks to run
        || Timer::after_micros(50),
        // Yield threshold - yield after 100 consecutive empty reads
        // Lower = more responsive to other tasks, but more overhead
        // Higher = less overhead, but other tasks may be starved
        // 100 is a good balance for human-speed input
        100,
    );

    // ========================================================================
    // Shell Configuration
    // ========================================================================

    let config = ShellConfig {
        get_commands: commands::get_commands,
        get_datatypes: commands::get_datatypes,
        get_shortcuts: shortcuts::get_shortcuts,
        is_shortcut: shortcuts::is_supported_shortcut,
        command_dispatcher: commands::dispatch,
        shortcut_dispatcher: shortcuts::dispatch,
        prompt: PROMPT,
    };

    // ====================================================================
    // Run Shell
    // ====================================================================

    run_shell::<
        { commands::NUM_COMMANDS },
        { commands::MAX_FUNCTION_NAME_LEN },
        { INPUT_MAX_LEN },
        { HISTORY_TOTAL_CAPACITY },
        { ERROR_BUFFER_SIZE },
        _,
    >(uart_write, uart_flush, reader, config)
    .await; // async mode
    
    log_info!("Shell exited");
}
