#![no_std]
#![no_main]

// ---------------------------------------------------------------------------
// Application-level business logic
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
use heapless::{Deque, spsc::Queue};

use ushell_dispatcher::{generate_commands_dispatcher, generate_shortcuts_dispatcher};
use ushell_usercode::commands as uc;
use ushell_usercode::shortcuts as us;

use ushell2::{log_info, log_simple};
use ushell2::logger::{init_logger, LogLevel, LoggerConfig};

// ---------------------------------------------------------------------------
// UART HAL
// ---------------------------------------------------------------------------
use uart_hal::{
    RX_QUEUE_SIZE, TX_BUFFER_SIZE,
    UartTx, UartRx,
    handle_tx_ready,
    init_uart_globals,
    LOGGER_WRITER,
    RxQueueReader,
};

// ---------------------------------------------------------------------------
// Shell context — InputParser / AnsiKeyParser / dispatch wiring
// ---------------------------------------------------------------------------
use ushell_ctx::{ShellCtx, ShellConfig};

// ============================================================================
// Shell configuration constants
// ============================================================================

pub const PROMPT:                &str  = ">> ";
pub const MAX_INPUT_LEN:        usize  = 128;
pub const MAX_HEXSTR_LEN:       usize  = 64;
pub const MAX_HISTORY_CAPACITY: usize  = 256;
pub const MAX_ERROR_BUFFER_SIZE: usize = 32;

// ============================================================================
// Generated dispatcher modules
// ============================================================================

generate_commands_dispatcher! {
    mod commands;
    hexstr_size       = crate::MAX_HEXSTR_LEN;
    error_buffer_size = crate::MAX_ERROR_BUFFER_SIZE;
    path              = "../ushell/ushell_usercode/src/commands.cfg"
}

generate_shortcuts_dispatcher! {
    mod shortcuts;
    error_buffer_size = crate::MAX_ERROR_BUFFER_SIZE;
    path              = "../ushell/ushell_usercode/src/shortcuts.cfg"
}

// ---------------------------------------------------------------------------
// Type alias — spells out the const params once, used everywhere else
// ---------------------------------------------------------------------------
type MyShell = ShellCtx<
    { commands::MAX_COMMANDS_PER_LETTER }, // NAC — max autocomplete candidates
    { commands::MAX_FUNCTION_NAME_LEN   }, // FNL — function name buffer length
    { MAX_INPUT_LEN                     }, // IML — input line buffer length
    { MAX_HISTORY_CAPACITY              }, // HTC — history ring-buffer capacity
    { MAX_ERROR_BUFFER_SIZE             }, // E   — error message buffer size
>;

// ============================================================================
// RTIC application
// ============================================================================
#[app(device = stm32f4xx_hal::pac, peripherals = true, dispatchers = [EXTI0])]
mod app {
    use super::*;

    #[shared]
    struct Shared {
        uart_tx:       UartTx,
        tx_buffer:     Deque<u8, TX_BUFFER_SIZE>,
        rx_queue:      Queue<u8, RX_QUEUE_SIZE>,
        shell_pending: bool,
    }

    #[local]
    struct Local {
        uart_rx:     UartRx,
        led:         Pin<'C', 13, Output<PushPull>>,
        blink_timer: CounterHz<pac::TIM2>,
        shell:       MyShell,
    }

    // -----------------------------------------------------------------------
    // init — hardware setup only
    // -----------------------------------------------------------------------
    #[init]
    fn init(ctx: init::Context) -> (Shared, Local) {
        let dp = ctx.device;

        let rcc    = dp.RCC.constrain();
        let clocks = rcc.cfgr
            .sysclk(100.MHz())
            .pclk1(50.MHz())
            .pclk2(100.MHz())
            .freeze();

        let gpioc = dp.GPIOC.split();
        let led   = gpioc.pc13.into_push_pull_output();

        let gpioa  = dp.GPIOA.split();
        let serial = Serial::new(
            dp.USART2,
            (gpioa.pa2.into_alternate(), gpioa.pa3.into_alternate()),
            SerialConfig::default().baudrate(115200.bps()),
            &clocks,
        ).unwrap();

        let (uart_tx, mut uart_rx) = serial.split();
        uart_rx.listen();

        let mut blink_timer = Timer::new(dp.TIM2, &clocks).counter_hz();
        blink_timer.start(1.Hz()).unwrap();
        blink_timer.listen(stm32f4xx_hal::timer::Event::Update);

        let tx_buffer: Deque<u8, TX_BUFFER_SIZE> = Deque::new();
        let rx_queue:  Queue<u8, RX_QUEUE_SIZE>  = Queue::new();

        // Wire logger — write_bytes is a no-op until init_uart_globals runs
        // in shell_task, so early log calls are silently dropped.
        unsafe {
            init_logger(
                LoggerConfig { color_entire_line: true, min_level: LogLevel::Debug },
                &mut *core::ptr::addr_of_mut!(LOGGER_WRITER),
            );
        }

        // Build shell from config — all InputParser/AnsiKeyParser internals
        // are hidden inside ushell_ctx; main only sees function pointers.
        let shell = MyShell::new(ShellConfig {
            get_commands:        commands::get_commands,
            get_datatypes:       commands::get_datatypes,
            get_shortcuts:       shortcuts::get_shortcuts,
            is_shortcut:         shortcuts::is_supported_shortcut,
            command_dispatcher:  commands::dispatch,
            shortcut_dispatcher: shortcuts::dispatch,
            prompt:              PROMPT,
        });

        shell_task::spawn().ok();

        (
            Shared { uart_tx, tx_buffer, rx_queue, shell_pending: true },
            Local  { uart_rx, led, blink_timer, shell },
        )
    }

    // -----------------------------------------------------------------------
    // USART2 ISR — RX ingestion + TX draining
    // -----------------------------------------------------------------------
    #[task(
        binds  = USART2,
        local  = [uart_rx],
        shared = [uart_tx, tx_buffer, rx_queue, shell_pending],
        priority = 3,
    )]
    fn usart2_isr(mut ctx: usart2_isr::Context) {
        if ctx.local.uart_rx.is_rx_not_empty() {
            match ctx.local.uart_rx.read() {
                Ok(byte) => {
                    ctx.shared.rx_queue.lock(|q| { let _ = q.enqueue(byte); });
                    ctx.shared.shell_pending.lock(|pending| {
                        if !*pending {
                            *pending = true;
                            shell_task::spawn().ok();
                        }
                    });
                }
                Err(_) => {}
            }
        }

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
        binds  = TIM2,
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
    // Shell task — one-time UART global init, then byte processing
    // -----------------------------------------------------------------------
    #[task(
        shared = [uart_tx, tx_buffer, rx_queue, shell_pending],
        local  = [shell, initialized: bool = false],
        priority = 1,
    )]
    async fn shell_task(mut ctx: shell_task::Context) {
        if !*ctx.local.initialized {
            // Safety: RTIC shared resources live for 'static. transmute is
            // sound because this block runs exactly once and the resources are
            // never moved or dropped afterwards.
            unsafe {
                ctx.shared.tx_buffer.lock(|tx_buf| {
                    ctx.shared.uart_tx.lock(|uart_tx| {
                        init_uart_globals(
                            core::mem::transmute::<
                                &mut Deque<u8, TX_BUFFER_SIZE>,
                                &'static mut Deque<u8, TX_BUFFER_SIZE>,
                            >(tx_buf),
                            core::mem::transmute::<
                                &mut UartTx,
                                &'static mut UartTx,
                            >(uart_tx),
                        );
                    });
                });
            }

            log_simple!("System initialized");
            log_simple!("UART configured with step-based shell");
            log_simple!("Starting step-based shell...");
            log_simple!("Type '##' for available commands");

            *ctx.local.initialized = true;
        }

        ctx.shared.rx_queue.lock(|rx_queue| {
            let mut reader = RxQueueReader::new(rx_queue);
            while !reader.is_empty() {
                if !ctx.local.shell.step(&mut reader) {
                    log_info!("Shell exited");
                    break;
                }
            }
        });

        ctx.shared.shell_pending.lock(|pending| { *pending = false; });
    }

    #[idle]
    fn idle(_: idle::Context) -> ! {
        loop { cortex_m::asm::wfi(); }
    }
}
