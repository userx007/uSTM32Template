#![no_std]
#![no_main]

use core::default::Default;

use embassy_executor::Spawner;
use embassy_stm32::gpio::{Level, Output, Speed};
use embassy_stm32::usart::{Config, Uart};
use embassy_stm32::{bind_interrupts, peripherals, usart};
use embassy_time::Timer;
use panic_halt as _;

use ushell_config::*;
use ushell_dispatcher::{generate_commands_dispatcher, generate_shortcuts_dispatcher};
use ushell_usercode::commands as uc;
use ushell_usercode::shortcuts as us;

use ushell_logger::{log_simple, log_info};
use ushell_logger::{init_logger, LogLevel, LoggerConfig};

use ushell2::{run_shell, AsyncReader, ShellConfig};

// HAL library — everything UART/channel/writer-related lives here
use uart_hal::{
    uart_flush, uart_write,
    uart_rx_task,
    GLOBAL_UART_RX, GLOBAL_UART_TX, UART_RX_CHANNEL,
    UartWriter,
};

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

static mut GLOBAL_WRITER: UartWriter = UartWriter::new();

// ============================================================================
// Main Entry Point
// ============================================================================

#[embassy_executor::main]
async fn main(spawner: Spawner) {
    let p = embassy_stm32::init(Default::default());

    let config = Config::default();
    let uart = Uart::new(
        p.USART2,
        p.PA3,          // RX
        p.PA2,          // TX
        Irqs,
        p.DMA1_CH6,     // TX DMA
        embassy_stm32::dma::NoDma,  // No DMA for RX — works better in Renode
        config,
    )
    .unwrap();

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

    spawner.spawn(blink_led(p.PC13)).unwrap();
    spawner.spawn(uart_rx_task()).unwrap();
    spawner.spawn(shell_task()).unwrap();

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
// Shell Task
// ============================================================================

#[embassy_executor::task]
async fn shell_task() {
    Timer::after_millis(150).await;

    log_simple!("Starting async shell...");
    log_simple!("Type '##' for available commands");

    let reader = AsyncReader::new(
        // Try to pull a byte from the RX channel (non-blocking)
        || UART_RX_CHANNEL.try_receive().ok(),
        // Yield for 50 µs between empty reads
        || Timer::after_micros(50),
        // Yield after 100 consecutive empty reads — good balance for human input
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

    run_shell::<
        { commands::NUM_COMMANDS },
        { commands::MAX_FUNCTION_NAME_LEN },
        { INPUT_MAX_LEN },
        { HISTORY_TOTAL_CAPACITY },
        { ERROR_BUFFER_SIZE },
        _,
    >(uart_write, uart_flush, reader, config)
    .await;

    log_info!("Shell exited");
}
