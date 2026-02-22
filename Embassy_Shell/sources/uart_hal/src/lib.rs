// uart_shell_hal.rs
//
// Reusable HAL layer for UART-backed shell infrastructure on STM32 + Embassy.
// Provides:
//   - GlobalUartTx / GlobalUartRx: static UART half-owners
//   - UartWriter: implements core::fmt::Write over blocking TX
//   - uart_write / uart_flush helpers for shell TX closures
//   - uart_rx_task: async task that feeds a byte channel from nb_read()
//   - UART_RX_CHANNEL: the shared channel between RX task and shell reader

#![no_std]

use core::cell::UnsafeCell;
use core::option::Option::{self, None, Some};
use core::result::Result::Ok;

use embassy_stm32::{peripherals};
use embassy_sync::blocking_mutex::raw::CriticalSectionRawMutex;
use embassy_sync::channel::Channel;
use embassy_time::Timer;
use nb;

// ============================================================================
// Global Storage
// ============================================================================

pub struct GlobalUartTx {
    pub tx: UnsafeCell<
        Option<
            embassy_stm32::usart::UartTx<'static, peripherals::USART2, peripherals::DMA1_CH6>,
        >,
    >,
}

pub struct GlobalUartRx {
    pub rx: UnsafeCell<
        Option<
            embassy_stm32::usart::UartRx<
                'static,
                peripherals::USART2,
                embassy_stm32::dma::NoDma,
            >,
        >,
    >,
}

unsafe impl Sync for GlobalUartTx {}
unsafe impl Sync for GlobalUartRx {}

pub static GLOBAL_UART_TX: GlobalUartTx = GlobalUartTx {
    tx: UnsafeCell::new(None),
};

pub static GLOBAL_UART_RX: GlobalUartRx = GlobalUartRx {
    rx: UnsafeCell::new(None),
};

/// UART RX byte channel.
/// Fed by `uart_rx_task`, consumed by the shell's `AsyncReader`.
pub static UART_RX_CHANNEL: Channel<CriticalSectionRawMutex, u8, 1024> = Channel::new();

// ============================================================================
// UartWriter — core::fmt::Write over blocking TX
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
// TX helper closures (pass these to run_shell)
// ============================================================================

/// Write bytes to UART TX (blocking — fast enough for human-speed shells).
pub fn uart_write(bytes: &[u8]) {
    unsafe {
        if let Some(tx) = (*GLOBAL_UART_TX.tx.get()).as_mut() {
            let _ = tx.blocking_write(bytes);
        }
    }
}

/// Flush UART TX.
pub fn uart_flush() {
    unsafe {
        if let Some(tx) = (*GLOBAL_UART_TX.tx.get()).as_mut() {
            let _ = tx.blocking_flush();
        }
    }
}

// ============================================================================
// UART RX Task
//
// Uses nb_read() (non-blocking) to drain the UART FIFO and push bytes into
// UART_RX_CHANNEL. Yields via Timer when no data is available so that other
// embassy tasks (LED, shell, …) get CPU time.
// ============================================================================

#[embassy_executor::task]
pub async fn uart_rx_task() {
    // Brief delay for UART initialization
    Timer::after_millis(100).await;

    let rx = unsafe { (*GLOBAL_UART_RX.rx.get()).take() };

    if let Some(mut rx) = rx {
        loop {
            match rx.nb_read() {
                Ok(byte) => {
                    // Got a byte — push to channel immediately, no delay
                    let _ = UART_RX_CHANNEL.send(byte).await;
                }
                Err(nb::Error::WouldBlock) => {
                    // No data — yield so other tasks can run
                    Timer::after_micros(100).await;
                }
                Err(nb::Error::Other(_)) => {
                    // RX error — brief back-off
                    Timer::after_millis(10).await;
                }
            }
        }
    }
    // If RX was never initialized we simply exit the task silently.
    // Log from the call site if you need diagnostics.
}
