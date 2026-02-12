#![no_std]
#![no_main]

static LED_TOGGLE_COUNT: core::sync::atomic::AtomicU32 = core::sync::atomic::AtomicU32::new(0);

use panic_halt as _;
use rtic::app;
use stm32f4xx_hal::{
    pac,
    prelude::*,
    gpio::{Output, PushPull, Pin},
    serial::{Config as SerialConfig, Serial},
    timer::{Flag as TimerFlag, CounterHz, Timer},
};
use heapless::{Deque, spsc::Queue, String};

use ushell_config::*;
use ushell_dispatcher::{generate_commands_dispatcher, generate_shortcuts_dispatcher};
use ushell_usercode::commands as uc;
use ushell_usercode::shortcuts as us;

use ushell_logger::{log_simple, log_info, log_error};
use ushell_logger::{init_logger, LogLevel, LoggerConfig};

// Import the shell components from ushell_input
// NOTE: Make sure ushell2 is compiled WITHOUT the "async" feature!
use ushell_input::input::parser::InputParser;
use ushell_input::input::key_reader::embedded::AnsiKeyParser;
use ushell_input::input::key_reader::Key;
use ushell_input::input::renderer::CallbackWriter;

// Configuration constants
const UART_RX_QUEUE_SIZE: usize = 128;
const UART_TX_BUFFER_SIZE: usize = 512;

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

type UartTx = stm32f4xx_hal::serial::Tx<pac::USART2>;
type UartRx = stm32f4xx_hal::serial::Rx<pac::USART2>;

// Global UART writer for logger
static mut GLOBAL_WRITER: UartWriterRtic = UartWriterRtic;

// Global storage for UART TX resource
struct GlobalUartResources {
    tx_buffer: core::cell::UnsafeCell<Option<&'static mut Deque<u8, UART_TX_BUFFER_SIZE>>>,
    uart_tx: core::cell::UnsafeCell<Option<&'static mut UartTx>>,
}

unsafe impl Sync for GlobalUartResources {}

static mut GLOBAL_UART: GlobalUartResources = GlobalUartResources {
    tx_buffer: core::cell::UnsafeCell::new(None),
    uart_tx: core::cell::UnsafeCell::new(None),
};

// ============================================================================
// UART Writer Functions (must be function pointers, not closures!)
// ============================================================================

fn uart_write_bytes(bytes: &[u8]) {
    unsafe {
        let tx_buffer_ptr = core::ptr::addr_of!(GLOBAL_UART.tx_buffer);
        let uart_tx_ptr = core::ptr::addr_of!(GLOBAL_UART.uart_tx);
        
        if let Some(tx_buf) = (*(*tx_buffer_ptr).get()).as_mut() {
            if let Some(uart_tx) = (*(*uart_tx_ptr).get()).as_mut() {
                for &byte in bytes {
                    if tx_buf.push_back(byte).is_err() {
                        break;
                    }
                }
                uart_tx.listen();
            }
        }
    }
}

fn uart_flush_noop() {
    // No-op flush - handled by interrupt
}

#[app(device = stm32f4xx_hal::pac, peripherals = true, dispatchers = [EXTI0])]
mod app {
    use super::*;

    #[shared]
    struct Shared {
        uart_tx: UartTx,
        tx_buffer: Deque<u8, UART_TX_BUFFER_SIZE>,
        rx_queue: Queue<u8, UART_RX_QUEUE_SIZE>,
    }

    #[local]
    struct Local {
        uart_rx: UartRx,
        led: Pin<'C', 13, Output<PushPull>>,
        blink_timer: CounterHz<pac::TIM2>,
        shell: ShellCtx,
    }

    #[init]
    fn init(ctx: init::Context) -> (Shared, Local) {
        let dp = ctx.device;

        // Setup clocks - 100 MHz sysclk
        let rcc = dp.RCC.constrain();
        let clocks = rcc.cfgr
            .sysclk(100.MHz())
            .pclk1(50.MHz())
            .pclk2(100.MHz())
            .freeze();

        // Setup LED pin (PC13)
        let gpioc = dp.GPIOC.split();
        let led = gpioc.pc13.into_push_pull_output();

        // Setup UART2 pins (PA2=TX, PA3=RX)
        let gpioa = dp.GPIOA.split();
        let tx_pin = gpioa.pa2.into_alternate();
        let rx_pin = gpioa.pa3.into_alternate();

        // Configure UART2 - 115200 baud, 8N1
        let serial_config = SerialConfig::default()
            .baudrate(115200.bps());

        let serial = Serial::new(
            dp.USART2,
            (tx_pin, rx_pin),
            serial_config,
            &clocks,
        ).unwrap();

        // Split serial before configuring interrupts
        let (uart_tx, mut uart_rx) = serial.split();
        
        // Enable UART RX interrupt
        uart_rx.listen();

        // Setup LED blink timer (TIM2) - 1 Hz
        let mut blink_timer = Timer::new(dp.TIM2, &clocks).counter_hz();
        blink_timer.start(1.Hz()).unwrap();
        blink_timer.listen(stm32f4xx_hal::timer::Event::Update);

        // Create TX buffer and RX queue
        let tx_buffer: Deque<u8, UART_TX_BUFFER_SIZE> = Deque::new();
        let rx_queue: Queue<u8, UART_RX_QUEUE_SIZE> = Queue::new();

        // Initialize the global logger with static UART writer
        unsafe {
            init_logger(
                LoggerConfig {
                    color_entire_line: true,
                    min_level: LogLevel::Debug,
                },
                &mut *core::ptr::addr_of_mut!(GLOBAL_WRITER),
            );
        }

        // Create shell context
        let shell = ShellCtx::new();

        // Spawn shell_task once to initialize globals and print welcome messages
        shell_task::spawn().ok();

        (
            Shared {
                uart_tx,
                tx_buffer,
                rx_queue,
            },
            Local {
                uart_rx,
                led,
                blink_timer,
                shell,
            },
        )
    }

    // ========================================================================
    // USART2 Interrupt Handler - Feeds RX queue and spawns shell task
    // ========================================================================
    #[task(binds = USART2, local = [uart_rx], shared = [uart_tx, tx_buffer, rx_queue], priority = 3)]
    fn usart2_isr(mut ctx: usart2_isr::Context) {
        let uart_rx = ctx.local.uart_rx;

        // Handle RX: Read received byte and store in queue
        if uart_rx.is_rx_not_empty() {
            match uart_rx.read() {
                Ok(byte) => {
                    // Store in RX queue
                    ctx.shared.rx_queue.lock(|rx_queue| {
                        let _ = rx_queue.enqueue(byte);
                    });
                    
                    // Spawn shell task to process the byte
                    shell_task::spawn().ok();
                }
                Err(_) => {
                    // Handle UART error
                }
            }
        }

        // Handle TX: Send byte from buffer if available
        ctx.shared.uart_tx.lock(|uart_tx| {
            if uart_tx.is_tx_empty() {
                ctx.shared.tx_buffer.lock(|tx_buf| {
                    match tx_buf.pop_front() {
                        Some(byte) => {
                            // Send byte
                            let _ = uart_tx.write(byte);
                            // Keep TX interrupt enabled
                            uart_tx.listen();
                        }
                        None => {
                            // No more data, disable TX interrupt
                            uart_tx.unlisten();
                        }
                    }
                });
            }
        });
    }

    // ========================================================================
    // LED Blink Task - Simple toggle every timer tick
    // ========================================================================
    #[task(binds = TIM2, local = [led, blink_timer, state: bool = false], priority = 2)]
    fn led_blink(ctx: led_blink::Context) {
        ctx.local.blink_timer.clear_flags(TimerFlag::Update);
            
        LED_TOGGLE_COUNT.fetch_add(1, core::sync::atomic::Ordering::Relaxed);            

        if *ctx.local.state {
            ctx.local.led.set_high();
            log_info!("LED ON");
        } else {
            ctx.local.led.set_low();
            log_info!("LED OFF");
        }
        
        *ctx.local.state = !*ctx.local.state;
    }

    // ========================================================================
    // Shell Processing Task - Processes one step and exits
    // ========================================================================
    #[task(shared = [uart_tx, tx_buffer, rx_queue], local = [shell, initialized: bool = false], priority = 1)]
    async fn shell_task(mut ctx: shell_task::Context) {
        // One-time initialization of global UART resources
        if !*ctx.local.initialized {
            unsafe {
                ctx.shared.tx_buffer.lock(|tx_buf| {
                    let tx_buffer_ptr = core::ptr::addr_of_mut!(GLOBAL_UART.tx_buffer);
                    *(*tx_buffer_ptr).get() = Some(core::mem::transmute(tx_buf));
                });
                ctx.shared.uart_tx.lock(|uart_tx| {
                    let uart_tx_ptr = core::ptr::addr_of_mut!(GLOBAL_UART.uart_tx);
                    *(*uart_tx_ptr).get() = Some(core::mem::transmute(uart_tx));
                });
            }
            
            // Now logging will work - print welcome messages
            log_simple!("System initialized");
            log_simple!("UART configured with step-based shell");
            log_simple!("Starting step-based shell...");
            log_simple!("Type '##' for available commands");
            
            *ctx.local.initialized = true;
        }

        // Process all available bytes in the queue
        ctx.shared.rx_queue.lock(|rx_queue| {
            let mut reader = RticQueueReader { queue: rx_queue };

            while !reader.is_empty() {
                let keep_running = ctx.local.shell.step(&mut reader);

                if !keep_running {
                    log_info!("Shell exited");
                    break;
                }
            }
        });
    }

    #[idle]
    fn idle(_: idle::Context) -> ! {
        loop {
            cortex_m::asm::wfi();
        }
    }
}

// ============================================================================
// Shell Context - Wraps the shell parser to process one step at a time
// ============================================================================

struct ShellCtx {
    parser: InputParser<
        'static,
        CallbackWriter<fn(&[u8]), fn()>,
        { commands::NUM_COMMANDS },          // NAC - Number of Autocomplete Candidates
        { commands::MAX_FUNCTION_NAME_LEN }, // FNL - Function Name Length
        { INPUT_MAX_LEN },                   // IML - Input Maximum Length
        { HISTORY_TOTAL_CAPACITY },          // HTC - History Total Capacity
    >,
    key_parser: AnsiKeyParser,
    pending_key: Option<Key>,
}

impl ShellCtx {
    fn new() -> Self {
        // Create the writer for the parser using FUNCTION POINTERS
        let writer = CallbackWriter::new(
            uart_write_bytes as fn(&[u8]),
            uart_flush_noop as fn(),
        );

        // Create the parser with all required configuration
        let parser = InputParser::new(
            writer,
            commands::get_commands(),
            commands::get_datatypes(),
            shortcuts::get_shortcuts(),
            PROMPT,
        );

        Self {
            parser,
            key_parser: AnsiKeyParser::new(),
            pending_key: None,
        }
    }

    fn step(&mut self, reader: &mut RticQueueReader) -> bool {
        // Read a byte if available
        if let Some(byte) = reader.read_byte() {
            // Parse ANSI escape sequences into keys
            if let Some(key) = self.key_parser.parse_byte(byte) {
                self.pending_key = Some(key);
            }
        }

        // Process the input with the parser
        self.parser.parse_input(
            || self.pending_key.take(),
            |s: &str| uart_write_bytes(s.as_bytes()),
            |input| {
                // Execute command or shortcut using error buffer
                let mut error_buffer: String<ERROR_BUFFER_SIZE> = String::new();
                
                let result = if shortcuts::is_supported_shortcut(input.as_str()) {
                    shortcuts::dispatch(input.as_str(), &mut error_buffer)
                } else {
                    commands::dispatch(input.as_str(), &mut error_buffer)
                };

                match result {
                    Ok(_) => log_info!("Success"),
                    Err(e) => log_error!("Error: {}", e),
                }
            },
        )
    }
}

// ============================================================================
// RTIC Queue Reader - Simple wrapper for the RX queue
// ============================================================================

struct RticQueueReader<'a> {
    queue: &'a mut Queue<u8, UART_RX_QUEUE_SIZE>,
}

impl<'a> RticQueueReader<'a> {
    fn read_byte(&mut self) -> Option<u8> {
        self.queue.dequeue()
    }
    
    fn is_empty(&self) -> bool {
        self.queue.is_empty()
    }
}

// ============================================================================
// UART Writer for Logger Integration
// ============================================================================

pub struct UartWriterRtic;

impl core::fmt::Write for UartWriterRtic {
    fn write_str(&mut self, s: &str) -> core::fmt::Result {
        uart_write_bytes(s.as_bytes());
        Ok(())
    }
}
