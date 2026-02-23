#![no_std]
#![no_main]

use core::default::Default;

use embassy_executor::Spawner;
use embassy_stm32::gpio::{Level, Output, Speed};
use embassy_stm32::usart::{Config, Uart};
use embassy_stm32::{bind_interrupts, peripherals, usart};
use embassy_sync::blocking_mutex::raw::CriticalSectionRawMutex;
use embassy_sync::signal::Signal;
use embassy_time::Timer;
use panic_halt as _;
use static_cell::StaticCell;

use ushell_dispatcher::{generate_commands_dispatcher, generate_shortcuts_dispatcher};
use ushell_usercode::commands as uc;
use ushell_usercode::shortcuts as us;

use ushell2::runner::{run_shell, AsyncReader, ShellConfig};
use ushell2::{log_info, log_simple};
use ushell2::logger::{init_logger, LogLevel, LoggerConfig};

use uart_hal::{
    uart_flush, uart_write,
    uart_rx_task,
    GLOBAL_UART_RX, GLOBAL_UART_TX, UART_RX_CHANNEL,
    UartWriter,
};

// ============================================================================
// Shell Configuration Constants
// All of these are tuning knobs for the shell runtime. Adjust as needed.
// ============================================================================

pub const PROMPT: &str = ">> ";
pub const MAX_INPUT_LEN: usize = 128;
pub const MAX_HEXSTR_LEN: usize = 64;
pub const MAX_HISTORY_CAPACITY: usize = 256;
pub const MAX_ERROR_BUFFER_SIZE: usize = 32;

// ============================================================================
// Shell Dispatcher Code Generation
// ============================================================================

generate_commands_dispatcher! {
    mod commands;
    hexstr_size = crate::MAX_HEXSTR_LEN;
    error_buffer_size = crate::MAX_ERROR_BUFFER_SIZE;
    path = "../ushell/ushell_usercode/src/commands.cfg"
}

generate_shortcuts_dispatcher! {
    mod shortcuts;
    error_buffer_size = crate::MAX_ERROR_BUFFER_SIZE;
    path = "../ushell/ushell_usercode/src/shortcuts.cfg"
}

bind_interrupts!(struct Irqs {
    USART2 => usart::InterruptHandler<peripherals::USART2>;
});

// ============================================================================
// Safe Static Initialization
//
// StaticCell guarantees single-init semantics without requiring unsafe blocks
// at the call site. The underlying value is only written once, at startup.
// ============================================================================

static UART_WRITER: StaticCell<UartWriter> = StaticCell::new();

// Signal sent from `main` to `shell_task` once hardware is fully configured.
static SYSTEM_READY: Signal<CriticalSectionRawMutex, ()> = Signal::new();

// ============================================================================
// Main Entry Point
// ============================================================================

#[embassy_executor::main]
async fn main(spawner: Spawner) {
    let p = embassy_stm32::init(Default::default());

    let config = Config::default();

    let uart = Uart::new(
        p.USART2,
        p.PA3, // RX
        p.PA2, // TX
        Irqs,
        p.DMA1_CH6,                          // TX DMA
        embassy_stm32::dma::NoDma,           // No RX DMA — works better in Renode
        config,
    )
    .expect("Failed to initialize USART2");

    // Split UART into independent TX and RX halves
    let (tx, rx) = uart.split();

    // Safety: we are in `main`, before any tasks are spawned that access
    // GLOBAL_UART_TX / GLOBAL_UART_RX, so there is no aliasing risk here.
    // These statics are only written once.
    unsafe {
        *GLOBAL_UART_TX.tx.get() = Some(tx);
        *GLOBAL_UART_RX.rx.get() = Some(rx);
    }

    // Initialize the logger using a safely-allocated static UartWriter.
    // UART_WRITER.init() panics if called more than once, which is what we want.
    let writer = UART_WRITER.init(UartWriter::new());
    init_logger(
        LoggerConfig {
            color_entire_line: true,
            min_level: LogLevel::Debug,
        },
        writer,
    );

    log_simple!("System initialized");
    log_simple!("UART configured with async shell (nb_read)");

    // Spawn tasks. `expect` gives a more debuggable panic than `unwrap` if
    // the executor runs out of task slots. This panics via `panic_halt`.
    spawner
        .spawn(blink_led(p.PC13))
        .expect("Failed to spawn blink_led");
    spawner
        .spawn(uart_rx_task())
        .expect("Failed to spawn uart_rx_task");
    spawner
        .spawn(shell_task())
        .expect("Failed to spawn shell_task");

    // Signal all tasks that hardware setup is complete.
    SYSTEM_READY.signal(());

    log_info!("All tasks spawned successfully");
}

// ============================================================================
// LED Blinker Task
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
// Shell Processing Task
// ============================================================================

#[embassy_executor::task]
async fn shell_task() {
    // Wait until main() confirms hardware is fully initialized before
    // attempting any UART or channel access.
    SYSTEM_READY.wait().await;

    log_simple!("Starting async shell...");
    log_simple!("Type '##' for available commands");

    let reader = AsyncReader::new(
        // Non-blocking: try to pull a byte from the RX channel
        || UART_RX_CHANNEL.try_receive().ok(),
        // Yield for 50 µs between empty reads to avoid busy-looping
        || Timer::after_micros(50),
        // Yield to the executor after 100 consecutive empty reads —
        // a good balance between latency and cooperative scheduling
        100,
    );

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
        { commands::MAX_COMMANDS_PER_LETTER }, // max autocomplete candidates per letter
        { commands::MAX_FUNCTION_NAME_LEN },   // function name buffer size
        { MAX_INPUT_LEN },                     // input line buffer size
        { MAX_HISTORY_CAPACITY },              // history ring buffer capacity
        { MAX_ERROR_BUFFER_SIZE },             // error message buffer size
        _,                                     // reader type — inferred
    >(uart_write, uart_flush, reader, config)
    .await;

    log_info!("Shell exited");
}
