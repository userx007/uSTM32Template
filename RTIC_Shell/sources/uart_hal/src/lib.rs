//! # uart_hal
//!
//! Standalone, no_std UART abstraction for RTIC-based STM32F4 applications.
//!
//! ## Responsibilities
//! - Owns the global TX ring-buffer / UART-Tx pointer state.
//! - Exposes plain function pointers (`write_bytes`, `flush_noop`) that can be
//!   handed directly to `CallbackWriter` or any other sink.
//! - Provides a ready-made `fmt::Write` impl (`UartWriter`) for logger integration.
//! - Provides `RxQueueReader` so the shell can drain the RTIC-owned RX queue
//!   without knowing about the queue internals.
//! - Provides `handle_tx_ready`, a single-call ISR helper that drains one byte
//!   from the TX buffer and manages the TX-interrupt arm/disarm logic.
//! - Provides `init_uart_globals` for the one-time wiring of RTIC shared
//!   resources into the global state.
//!
//! ## What this crate does NOT do
//! - It does not configure clocks, pins, or the USART peripheral.
//! - It does not know about the shell, commands, or any business logic.
//! - It does not spawn or manage RTIC tasks.

#![no_std]

use stm32f4xx_hal::{pac, serial::{Tx, Rx}};

// These traits are not included in the blanket `prelude::*`; they must be
// imported explicitly.  The compiler error messages name them precisely.
use stm32f4xx_hal::prelude::_stm32f4xx_hal_serial_TxListen; // .listen() / .unlisten()
use stm32f4xx_hal::prelude::_stm32f4xx_hal_serial_TxISR;    // .is_tx_empty()
use stm32f4xx_hal::prelude::_embedded_hal_serial_nb_Write;   // .write(byte)

use heapless::{Deque, spsc::Queue};

// ---------------------------------------------------------------------------
// Public size constants
// ---------------------------------------------------------------------------

/// Capacity of the interrupt-driven RX byte queue.
pub const RX_QUEUE_SIZE: usize = 128;

/// Capacity of the software TX ring buffer that feeds the USART TX interrupt.
pub const TX_BUFFER_SIZE: usize = 512;

// ---------------------------------------------------------------------------
// Concrete HAL type aliases (re-exported so main.rs stays free of hal details)
// ---------------------------------------------------------------------------

/// The USART2 TX half, as produced by `serial.split()`.
pub type UartTx = Tx<pac::USART2>;

/// The USART2 RX half, as produced by `serial.split()`.
pub type UartRx = Rx<pac::USART2>;

// ---------------------------------------------------------------------------
// Internal global state
// ---------------------------------------------------------------------------

struct GlobalUartState {
    tx_buffer: core::cell::UnsafeCell<Option<&'static mut Deque<u8, TX_BUFFER_SIZE>>>,
    uart_tx:   core::cell::UnsafeCell<Option<&'static mut UartTx>>,
}

// Safety: accesses are coordinated by RTIC's priority-based interrupt masking.
// The UnsafeCells are written exactly once (in init_uart_globals) before any
// reader can observe them.
unsafe impl Sync for GlobalUartState {}

static mut GLOBAL_UART: GlobalUartState = GlobalUartState {
    tx_buffer: core::cell::UnsafeCell::new(None),
    uart_tx:   core::cell::UnsafeCell::new(None),
};

// ---------------------------------------------------------------------------
// Global logger writer instance
// ---------------------------------------------------------------------------

/// A zero-sized `fmt::Write` implementor backed by [`write_bytes`].
///
/// Declare a `static mut` of this in your application and pass a `&mut` to
/// `init_logger`:
///
/// ```ignore
/// init_logger(cfg, unsafe { &mut *core::ptr::addr_of_mut!(uart_hal::LOGGER_WRITER) });
/// ```
pub static mut LOGGER_WRITER: UartWriter = UartWriter;

// ---------------------------------------------------------------------------
// One-time initialisation
// ---------------------------------------------------------------------------

/// Register the RTIC-owned `tx_buffer` and `uart_tx` with the global state.
///
/// Must be called **exactly once**, from the RTIC task that holds locks on
/// both resources.  Use `core::mem::transmute` to extend lifetimes to
/// `'static` — this is sound because RTIC shared resources live for the
/// entire programme lifetime.
///
/// # Safety
/// - Both references must remain valid for `'static`.
/// - Must be called before the first call to [`write_bytes`].
/// - Must be called exactly once.
pub unsafe fn init_uart_globals(
    tx_buf:  &'static mut Deque<u8, TX_BUFFER_SIZE>,
    uart_tx: &'static mut UartTx,
) {
    *(*core::ptr::addr_of_mut!(GLOBAL_UART.tx_buffer)).get() = Some(tx_buf);
    *(*core::ptr::addr_of_mut!(GLOBAL_UART.uart_tx)).get()   = Some(uart_tx);
}

// ---------------------------------------------------------------------------
// Public write / flush — suitable as bare function pointers
// ---------------------------------------------------------------------------

/// Enqueue `bytes` into the TX ring buffer and arm the TX interrupt.
///
/// This is a plain `fn` (not a closure) so it can be stored in a
/// `CallbackWriter<fn(&[u8]), fn()>` or any other function-pointer slot.
///
/// Silently drops bytes that exceed the buffer capacity.
/// No-ops silently before [`init_uart_globals`] has been called.
pub fn write_bytes(bytes: &[u8]) {
    // Safety: write_bytes is called only from tasks at or below the USART ISR
    // priority.  The ISR exclusively pops (pop_front) while we push
    // (push_back), so there is no aliased mutable access to the Deque.
    //
    // Deref note: `Option<&'static mut T>::as_mut()` yields
    // `Option<&mut &'static mut T>`, not `Option<&mut T>`.
    // We therefore deref one extra level with `**uart_tx` to obtain the plain
    // `&mut UartTx` that the HAL trait methods expect.
    unsafe {
        let tx_buf_ptr = core::ptr::addr_of!(GLOBAL_UART.tx_buffer);
        let tx_ptr     = core::ptr::addr_of!(GLOBAL_UART.uart_tx);

        if let Some(tx_buf) = (*(*tx_buf_ptr).get()).as_mut() {
            if let Some(uart_tx) = (*(*tx_ptr).get()).as_mut() {
                for &b in bytes {
                    if tx_buf.push_back(b).is_err() {
                        break; // buffer full — drop the remainder
                    }
                }
                // uart_tx : &mut &'static mut UartTx  →  **  →  &mut UartTx
                (**uart_tx).listen();
            }
        }
    }
}

/// No-op flush — TX draining is handled entirely by the USART TX interrupt.
///
/// Provided as a companion to [`write_bytes`] for APIs that require a paired
/// `fn()` flush pointer (e.g. `CallbackWriter`).
pub fn flush_noop() {}

// ---------------------------------------------------------------------------
// ISR TX helper
// ---------------------------------------------------------------------------

/// Drive the TX side of the USART interrupt.
///
/// Call this from your USART ISR whenever the TX data register is empty.
/// Pops one byte from `tx_buf`, writes it to `uart_tx`, and keeps the TX
/// interrupt armed.  When the buffer empties the interrupt is disarmed, so
/// the ISR stops re-entering.
///
/// # Example (inside `usart2_isr`)
/// ```ignore
/// ctx.shared.uart_tx.lock(|uart_tx| {
///     ctx.shared.tx_buffer.lock(|tx_buf| {
///         uart_hal::handle_tx_ready(uart_tx, tx_buf);
///     });
/// });
/// ```
pub fn handle_tx_ready(uart_tx: &mut UartTx, tx_buf: &mut Deque<u8, TX_BUFFER_SIZE>) {
    if uart_tx.is_tx_empty() {
        match tx_buf.pop_front() {
            Some(byte) => {
                let _ = uart_tx.write(byte);
                uart_tx.listen();   // keep armed while data remains
            }
            None => {
                uart_tx.unlisten(); // buffer drained — silence the TX interrupt
            }
        }
    }
}

// ---------------------------------------------------------------------------
// fmt::Write for logger integration
// ---------------------------------------------------------------------------

/// Zero-sized type implementing `fmt::Write` by forwarding to [`write_bytes`].
///
/// Intended for use with `ushell_logger::init_logger` (or any logger that
/// accepts a `&mut dyn fmt::Write`).
pub struct UartWriter;

impl core::fmt::Write for UartWriter {
    #[inline]
    fn write_str(&mut self, s: &str) -> core::fmt::Result {
        write_bytes(s.as_bytes());
        Ok(())
    }
}

// ---------------------------------------------------------------------------
// RX queue reader
// ---------------------------------------------------------------------------

/// A thin, lifetime-scoped wrapper around the RTIC-owned RX byte queue.
///
/// Construct inside the shell task while holding the `rx_queue` lock, then
/// pass to the shell's `step` method for byte-by-byte consumption.
///
/// # Example
/// ```ignore
/// ctx.shared.rx_queue.lock(|rx_queue| {
///     let mut reader = RxQueueReader::new(rx_queue);
///     while !reader.is_empty() {
///         shell.step(&mut reader);
///     }
/// });
/// ```
pub struct RxQueueReader<'a> {
    queue: &'a mut Queue<u8, RX_QUEUE_SIZE>,
}

impl<'a> RxQueueReader<'a> {
    /// Wrap the RTIC-owned RX queue for the duration of a lock scope.
    pub fn new(queue: &'a mut Queue<u8, RX_QUEUE_SIZE>) -> Self {
        Self { queue }
    }

    /// Dequeue and return the next byte, or `None` if empty.
    pub fn read_byte(&mut self) -> Option<u8> {
        self.queue.dequeue()
    }

    /// Returns `true` when no bytes are waiting.
    pub fn is_empty(&self) -> bool {
        self.queue.is_empty()
    }
}
