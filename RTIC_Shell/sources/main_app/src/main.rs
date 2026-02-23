#![no_std]
#![no_main]

// ---------------------------------------------------------------------------
// Application-level LED toggle counter (business logic, stays here)
// ---------------------------------------------------------------------------
static LED_TOGGLE_COUNT: core::sync::atomic::AtomicU32 =
    core::sync::atomic::AtomicU32::new(0);

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

// Shell plumbing
use ushell_config::*;
use ushell_dispatcher::{generate_commands_dispatcher, generate_shortcuts_dispatcher};
use ushell_usercode::commands as uc;
use ushell_usercode::shortcuts as us;

// Logger
use ushell_logger::{log_simple, log_info, log_error};
use ushell_logger::{init_logger, LogLevel, LoggerConfig};

// Shell input components
use ushell_input::input::parser::InputParser;
use ushell_input::input::key_reader::embedded::AnsiKeyParser;
use ushell_input::input::key_reader::Key;
use ushell_input::input::renderer::CallbackWriter;

// ---------------------------------------------------------------------------
// UART HAL — all UART concerns live here
// ---------------------------------------------------------------------------
use uart_hal::{
    // Size constants used by RTIC shared-struct type parameters
    RX_QUEUE_SIZE,
    TX_BUFFER_SIZE,
    // Concrete HAL types (saves main from spelling out long paths)
    UartTx,
    UartRx,
    // Runtime helpers
    write_bytes,
    flush_noop,
    handle_tx_ready,
    init_uart_globals,
    // Global fmt::Write instance for the logger
    LOGGER_WRITER,
    // Shell ↔ queue bridge
    RxQueueReader,
};

// ---------------------------------------------------------------------------
// Code-generated shell dispatchers
// ---------------------------------------------------------------------------
generate_commands_dispatcher! {
    mod commands;
    hexstr_size       = crate::MAX_HEXSTR_LEN;
    error_buffer_size = crate::ERROR_BUFFER_SIZE;
    path              = "../ushell_usercode/src/commands.cfg"
}

generate_shortcuts_dispatcher! {
    mod shortcuts;
    error_buffer_size = crate::ERROR_BUFFER_SIZE;
    path              = "../ushell_usercode/src/shortcuts.cfg"
}

// ---------------------------------------------------------------------------
// RTIC application
// ---------------------------------------------------------------------------
#[app(device = stm32f4xx_hal::pac, peripherals = true, dispatchers = [EXTI0])]
mod app {
    use super::*;

    // ---- Shared resources (touched by multiple tasks / ISRs) --------------
    #[shared]
    struct Shared {
        uart_tx:      UartTx,
        tx_buffer:    Deque<u8, TX_BUFFER_SIZE>,
        rx_queue:     Queue<u8, RX_QUEUE_SIZE>,
        shell_pending: bool, // prevents redundant shell_task::spawn() calls
    }

    // ---- Local resources (single owner) -----------------------------------
    #[local]
    struct Local {
        uart_rx:     UartRx,
        led:         Pin<'C', 13, Output<PushPull>>,
        blink_timer: CounterHz<pac::TIM2>,
        shell:       ShellCtx,
    }

    // -----------------------------------------------------------------------
    // init — hardware setup only
    // -----------------------------------------------------------------------
    #[init]
    fn init(ctx: init::Context) -> (Shared, Local) {
        let dp = ctx.device;

        // Clocks: 100 MHz sysclk
        let rcc    = dp.RCC.constrain();
        let clocks = rcc.cfgr
            .sysclk(100.MHz())
            .pclk1(50.MHz())
            .pclk2(100.MHz())
            .freeze();

        // LED on PC13
        let gpioc = dp.GPIOC.split();
        let led   = gpioc.pc13.into_push_pull_output();

        // USART2 — PA2 = TX, PA3 = RX @ 115200 8N1
        let gpioa  = dp.GPIOA.split();
        let serial = Serial::new(
            dp.USART2,
            (gpioa.pa2.into_alternate(), gpioa.pa3.into_alternate()),
            SerialConfig::default().baudrate(115200.bps()),
            &clocks,
        ).unwrap();

        let (uart_tx, mut uart_rx) = serial.split();
        uart_rx.listen(); // arm RX interrupt

        // LED blink timer — 1 Hz
        let mut blink_timer = Timer::new(dp.TIM2, &clocks).counter_hz();
        blink_timer.start(1.Hz()).unwrap();
        blink_timer.listen(stm32f4xx_hal::timer::Event::Update);

        // Allocate RTIC shared buffers
        let tx_buffer: Deque<u8, TX_BUFFER_SIZE> = Deque::new();
        let rx_queue:  Queue<u8, RX_QUEUE_SIZE>  = Queue::new();

        // Wire logger to the UART writer.
        // NOTE: write_bytes is a no-op until init_uart_globals is called from
        // shell_task, so the first log lines are intentionally deferred.
        unsafe {
            init_logger(
                LoggerConfig { color_entire_line: true, min_level: LogLevel::Debug },
                &mut *core::ptr::addr_of_mut!(LOGGER_WRITER),
            );
        }

        // Spawn shell_task once so it can run its one-time UART global init
        shell_task::spawn().ok();

        (
            Shared { uart_tx, tx_buffer, rx_queue, shell_pending: true },
            Local  { uart_rx, led, blink_timer, shell: ShellCtx::new() },
        )
    }

    // -----------------------------------------------------------------------
    // USART2 ISR — RX ingestion + TX draining
    // -----------------------------------------------------------------------
    #[task(
        binds = USART2,
        local  = [uart_rx],
        shared = [uart_tx, tx_buffer, rx_queue, shell_pending],
        priority = 3,
    )]
    fn usart2_isr(mut ctx: usart2_isr::Context) {
        // --- RX path -------------------------------------------------------
        if ctx.local.uart_rx.is_rx_not_empty() {
            match ctx.local.uart_rx.read() {
                Ok(byte) => {
                    ctx.shared.rx_queue.lock(|q| { let _ = q.enqueue(byte); });

                    // Spawn the shell task only when it is not already queued.
                    ctx.shared.shell_pending.lock(|pending| {
                        if !*pending {
                            *pending = true;
                            shell_task::spawn().ok();
                        }
                    });
                }
                Err(_) => { /* framing / overrun errors — ignore or count */ }
            }
        }

        // --- TX path (delegated entirely to uart_hal) ----------------------
        ctx.shared.uart_tx.lock(|uart_tx| {
            ctx.shared.tx_buffer.lock(|tx_buf| {
                handle_tx_ready(uart_tx, tx_buf);
            });
        });
    }

    // -----------------------------------------------------------------------
    // TIM2 ISR — LED blink (business logic)
    // -----------------------------------------------------------------------
    #[task(
        binds = TIM2,
        local  = [led, blink_timer, state: bool = false],
        priority = 2,
    )]
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

    // -----------------------------------------------------------------------
    // Shell task — one-time UART global init, then byte-by-byte processing
    // -----------------------------------------------------------------------
    #[task(
        shared = [uart_tx, tx_buffer, rx_queue, shell_pending],
        local  = [shell, initialized: bool = false],
        priority = 1,
    )]
    async fn shell_task(mut ctx: shell_task::Context) {
        // --- One-time global init ------------------------------------------
        // Wire the RTIC-owned tx_buffer and uart_tx into uart_hal's global
        // state so that write_bytes (and the logger) can send bytes without
        // holding any RTIC lock at call-site.
        //
        // Safety: RTIC shared resources are pinned in static storage for the
        // lifetime of the programme. transmute extends the borrow to 'static,
        // which is sound here because we run this block exactly once and do
        // not move or drop the resources afterwards.
        if !*ctx.local.initialized {
            unsafe {
                ctx.shared.tx_buffer.lock(|tx_buf| {
                    ctx.shared.uart_tx.lock(|uart_tx| {
                        init_uart_globals(
                            core::mem::transmute::<
                                &mut Deque<u8, TX_BUFFER_SIZE>,
                                &'static mut Deque<u8, TX_BUFFER_SIZE>,
                            >(tx_buf),
                            core::mem::transmute::<&mut UartTx, &'static mut UartTx>(uart_tx),
                        );
                    });
                });
            }

            // Logger is now operational — emit welcome banner
            log_simple!("System initialized");
            log_simple!("UART configured with step-based shell");
            log_simple!("Starting step-based shell...");
            log_simple!("Type '##' for available commands");

            *ctx.local.initialized = true;
        }

        // --- Process all queued RX bytes -----------------------------------
        ctx.shared.rx_queue.lock(|rx_queue| {
            let mut reader = RxQueueReader::new(rx_queue);
            while !reader.is_empty() {
                if !ctx.local.shell.step(&mut reader) {
                    log_info!("Shell exited");
                    break;
                }
            }
        });

        // Release the pending flag so the ISR may re-spawn us on new input
        ctx.shared.shell_pending.lock(|pending| { *pending = false; });
    }

    // -----------------------------------------------------------------------
    // Idle
    // -----------------------------------------------------------------------
    #[idle]
    fn idle(_: idle::Context) -> ! {
        loop {
            cortex_m::asm::wfi();
        }
    }
}

// ---------------------------------------------------------------------------
// Shell context — wraps InputParser for step-based processing
// (shell concern, not UART concern — stays in main.rs)
// ---------------------------------------------------------------------------

struct ShellCtx {
    parser: InputParser<
        'static,
        CallbackWriter<fn(&[u8]), fn()>,
        { commands::NUM_COMMANDS },          // NAC — autocomplete candidates
        { commands::MAX_FUNCTION_NAME_LEN }, // FNL — function name length
        { INPUT_MAX_LEN },                   // IML — input max length
        { HISTORY_TOTAL_CAPACITY },          // HTC — history buffer capacity
    >,
    key_parser:  AnsiKeyParser,
    pending_key: Option<Key>,
}

impl ShellCtx {
    fn new() -> Self {
        // write_bytes / flush_noop are plain fn-pointers — no closure captures.
        let writer = CallbackWriter::new(
            write_bytes as fn(&[u8]),
            flush_noop  as fn(),
        );
        let parser = InputParser::new(
            writer,
            commands::get_commands(),
            commands::get_datatypes(),
            shortcuts::get_shortcuts(),
            PROMPT,
        );
        Self { parser, key_parser: AnsiKeyParser::new(), pending_key: None }
    }

    /// Process one byte from `reader` and advance the parser state machine.
    /// Returns `false` when the shell signals it should stop running.
    fn step(&mut self, reader: &mut RxQueueReader) -> bool {
        // Feed one raw byte through the ANSI key decoder
        if let Some(byte) = reader.read_byte() {
            if let Some(key) = self.key_parser.parse_byte(byte) {
                self.pending_key = Some(key);
            }
        }

        // Advance the parser; dispatch commands when a line is complete
        self.parser.parse_input(
            || self.pending_key.take(),
            |s: &str| write_bytes(s.as_bytes()),
            |input| {
                let mut error_buffer: String<ERROR_BUFFER_SIZE> = String::new();

                let result = if shortcuts::is_supported_shortcut(input.as_str()) {
                    shortcuts::dispatch(input.as_str(), &mut error_buffer)
                } else {
                    commands::dispatch(input.as_str(), &mut error_buffer)
                };

                match result {
                    Ok(_)  => log_info!("Success"),
                    Err(e) => log_error!("Error: {}", e),
                }
            },
        )
    }
}
