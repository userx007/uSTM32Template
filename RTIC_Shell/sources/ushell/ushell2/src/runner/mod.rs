//! # UShell - Unified Async/Sync Shell Framework
//!
//! A no_std compatible shell implementation with unified async/sync interface.
//!
//! ## Features
//!
//! - `async`: Enable async/await support with Embassy or other async runtimes
//!   - Without feature: Uses blocking function pointers
//!   - With feature: True async with `.await` for efficient task cooperation
//!
//! ## Architecture
//!
//! The shell uses a `UartReader` trait that abstracts byte reading:
//! - **Async mode** (`async` feature): `UartReader::read_byte()` returns `impl Future`
//! - **Sync mode** (no feature): `UartReader::read_byte()` polls a function pointer
//!
//! This provides a unified `run_shell()` function that works in both environments.

#![no_std]
#![no_implicit_prelude]

extern crate core;
extern crate heapless;

use core::ops::FnMut;
use core::option::Option::{self, None, Some};
use core::result::Result::{self, Err, Ok};
use heapless::String;

use crate::input::key_reader::embedded::AnsiKeyParser;
use crate::input::key_reader::Key;
use crate::input::parser::InputParser;
use crate::input::renderer::CallbackWriter;
use crate::{log_error, log_info};

#[cfg(feature = "hosted")]
use crate::terminal::RawMode;

// ============================================================================
// Unified Reader Trait
// ============================================================================

/// Unified trait for reading bytes from UART/serial input.
///
/// This trait provides a common interface for both async and sync environments:
/// - In async mode (`async` feature enabled): Returns a Future that yields
/// - In sync mode (default): Polls a function pointer and returns immediately
pub trait UartReader {
    /// Read a single byte from UART.
    ///
    /// # Async Mode (`async` feature)
    /// Returns a Future that:
    /// - Yields to executor while waiting for data
    /// - Resolves to `Some(u8)` when data arrives
    /// - May resolve to `None` on timeout/error
    ///
    /// # Sync Mode (default)
    /// Returns immediately:
    /// - `Some(u8)` if data available
    /// - `None` if no data (non-blocking poll)
    #[cfg(feature = "async")]
    fn read_byte(&mut self) -> impl core::future::Future<Output = Option<u8>>;

    #[cfg(not(feature = "async"))]
    fn read_byte(&mut self) -> Option<u8>;
}

// ============================================================================
// Sync Implementation: Polling Reader
// ============================================================================

#[cfg(not(feature = "async"))]
mod sync_impl {
    use super::UartReader;
    use ::core::ops::FnMut;
    use ::core::option::Option::{self, None, Some};

    /// Synchronous UART reader using function pointer polling.
    ///
    /// This is the default implementation for non-async environments.
    /// It wraps a function pointer that polls for available data.
    pub struct PollingReader<F>
    where
        F: FnMut() -> Option<u8>,
    {
        read_fn: F,
    }

    impl<F> PollingReader<F>
    where
        F: FnMut() -> Option<u8>,
    {
        /// Create a new polling reader from a function pointer.
        ///
        /// The function should:
        /// - Return `Some(byte)` if data is available
        /// - Return `None` if no data (non-blocking)
        #[inline]
        pub const fn new(read_fn: F) -> Self {
            Self { read_fn }
        }
    }

    impl<F> UartReader for PollingReader<F>
    where
        F: FnMut() -> Option<u8>,
    {
        #[inline]
        fn read_byte(&mut self) -> Option<u8> {
            (self.read_fn)()
        }
    }
}

// ============================================================================
// Async Implementation: Channel Reader
// ============================================================================

#[cfg(feature = "async")]
mod async_impl {
    use super::*;
    use ::core::option::Option::{self, None, Some};

    /// Async UART reader that properly yields to executor.
    ///
    /// This implementation uses an async channel or similar mechanism
    /// to receive data without blocking the executor.
    pub struct AsyncReader<F, Y>
    where
        F: FnMut() -> Option<u8>,
        Y: core::future::Future<Output = ()>,
    {
        try_read_fn: F,
        yield_fn: fn() -> Y,
        empty_count: u32,
        yield_threshold: u32,
    }

    impl<F, Y> AsyncReader<F, Y>
    where
        F: FnMut() -> Option<u8>,
        Y: core::future::Future<Output = ()>,
    {
        /// Create a new async reader.
        ///
        /// # Parameters
        ///
        /// - `try_read_fn`: Function to attempt non-blocking read (e.g., channel.try_receive())
        /// - `yield_fn`: Function that returns a Future to yield to executor
        /// - `yield_threshold`: Number of consecutive empty reads before yielding
        ///
        /// # Example
        ///
        /// ```no_run
        /// use embassy_time::Timer;
        ///
        /// let reader = AsyncReader::new(
        ///     || RX_CHANNEL.try_receive().ok(),
        ///     || Timer::after_micros(10),
        ///     100,
        /// );
        /// ```
        #[inline]
        pub const fn new(try_read_fn: F, yield_fn: fn() -> Y, yield_threshold: u32) -> Self {
            Self {
                try_read_fn,
                yield_fn,
                empty_count: 0,
                yield_threshold,
            }
        }
    }

    impl<F, Y> UartReader for AsyncReader<F, Y>
    where
        F: FnMut() -> Option<u8>,
        Y: core::future::Future<Output = ()>,
    {
        async fn read_byte(&mut self) -> Option<u8> {
            // Try to read data
            if let Some(byte) = (self.try_read_fn)() {
                self.empty_count = 0;
                return Some(byte);
            }

            // No data available, track consecutive empty reads
            self.empty_count += 1;

            // Yield to executor after threshold
            if self.empty_count >= self.yield_threshold {
                ((self.yield_fn)()).await;
                self.empty_count = 0;
            }

            None
        }
    }
}

// ============================================================================
// Shell Configuration
// ============================================================================

pub struct ShellConfig<const IML: usize, const EBS: usize> {
    pub get_commands: fn() -> &'static [(&'static str, &'static str)],
    pub get_datatypes: fn() -> &'static str,
    pub get_shortcuts: fn() -> &'static str,
    pub is_shortcut: fn(&str) -> bool,
    pub command_dispatcher: for<'a> fn(&'a str, &'a mut String<EBS>) -> Result<(), &'a str>,
    pub shortcut_dispatcher: for<'a> fn(&'a str, &'a mut String<EBS>) -> Result<(), &'a str>,
    pub prompt: &'static str,
}

// ============================================================================
// Unified Shell Runner
// ============================================================================

/// Run the shell with unified async/sync interface.
///
/// This function works in both async and sync environments:
/// - **Async mode**: Properly yields to executor while waiting for input
/// - **Sync mode**: Polls for input without yielding (original behavior)
///
/// # Type Parameters
///
/// - `NAC`: Number of Autocomplete Candidates
/// - `FNL`: Function Name Length
/// - `IML`: Input Maximum Length
/// - `HTC`: History Total Capacity
/// - `R`: UART reader implementing `UartReader` trait
///
/// # Example (Async)
///
/// ```no_run
/// #[embassy_executor::task]
/// async fn shell_task() {
///     let reader = AsyncReader::new(
///         || RX_CHANNEL.try_receive().ok(),
///         || Timer::after_micros(10),
///         100,
///     );
///     
///     run_shell(uart_write, uart_flush, reader, config).await;
/// }
/// ```
///
/// # Example (Sync)
///
/// ```no_run
/// fn shell_task() {
///     let reader = PollingReader::new(|| uart_nb_read().ok());
///     
///     run_shell(uart_write, uart_flush, reader, config);
/// }
/// ```
#[cfg(feature = "async")]
pub async fn run_shell<
    const NAC: usize,
    const FNL: usize,
    const IML: usize,
    const HTC: usize,
    const EBS: usize,
    R: UartReader,
>(
    write_fn: fn(&[u8]),
    flush_fn: fn(),
    mut reader: R,
    config: ShellConfig<IML, EBS>,
) {
    let writer = CallbackWriter::new(write_fn, flush_fn);

    // Get static data references once before loop instead of calling every iteration
    let commands = (config.get_commands)();
    let datatypes = (config.get_datatypes)();
    let shortcuts = (config.get_shortcuts)();

    let mut parser = InputParser::<CallbackWriter<fn(&[u8]), fn()>, NAC, FNL, IML, HTC>::new(
        writer,
        commands,
        datatypes,
        shortcuts,
        config.prompt,
    );

    let mut key_parser = AnsiKeyParser::new();
    let mut pending_key: Option<Key> = None;

    loop {
        // Async read - yields to executor when no data available
        if let Some(byte) = reader.read_byte().await {
            if let Some(key) = key_parser.parse_byte(byte) {
                pending_key = Some(key);
            }
        }

        // Process pending key
        let continue_running = parser.parse_input(
            || pending_key.take(),
            |s: &str| {
                write_fn(s.as_bytes());
            },
            |input: &String<IML>| {
                // Pass input as &str to avoid potential string copies
                exec::<EBS>(
                    input.as_str(),
                    config.is_shortcut,
                    config.command_dispatcher,
                    config.shortcut_dispatcher,
                )
            },
        );

        if !continue_running {
            break;
        }
    }
}

#[cfg(not(feature = "async"))]
pub fn run_shell<
    const NAC: usize,
    const FNL: usize,
    const IML: usize,
    const HTC: usize,
    const EBS: usize,
    R: UartReader,
>(
    write_fn: fn(&[u8]),
    flush_fn: fn(),
    mut reader: R,
    config: ShellConfig<IML, EBS>,
) {
    let writer = CallbackWriter::new(write_fn, flush_fn);

    // Get static data references once before loop
    let commands = (config.get_commands)();
    let datatypes = (config.get_datatypes)();
    let shortcuts = (config.get_shortcuts)();

    let mut parser = InputParser::<CallbackWriter<fn(&[u8]), fn()>, NAC, FNL, IML, HTC>::new(
        writer,
        commands,
        datatypes,
        shortcuts,
        config.prompt,
    );

    let mut key_parser = AnsiKeyParser::new();
    let mut pending_key: Option<Key> = None;

    loop {
        // Sync read - polls without yielding
        if let Some(byte) = reader.read_byte() {
            if let Some(key) = key_parser.parse_byte(byte) {
                pending_key = Some(key);
            }
        }

        // Process pending key
        // Avoid closure allocation in write callback
        let continue_running = parser.parse_input(
            || pending_key.take(),
            |s: &str| {
                write_fn(s.as_bytes());
            },
            |input: &String<IML>| {
                // Pass input as &str to avoid potential string copies
                exec::<EBS>(
                    input.as_str(),
                    config.is_shortcut,
                    config.command_dispatcher,
                    config.shortcut_dispatcher,
                )
            },
        );

        if !continue_running {
            break;
        }
    }
}

// ============================================================================
// Command Execution
// ============================================================================

#[inline]
fn exec<const EBS: usize>(
    input_str: &str,
    is_shortcut: fn(&str) -> bool,
    command_dispatcher: for<'a> fn(&'a str, &'a mut String<EBS>) -> Result<(), &'a str>,
    shortcut_dispatcher: for<'a> fn(&'a str, &'a mut String<EBS>) -> Result<(), &'a str>,
) {
    let mut error_buffer: String<EBS> = String::new();

    let result = if is_shortcut(input_str) {
        shortcut_dispatcher(input_str, &mut error_buffer)
    } else {
        command_dispatcher(input_str, &mut error_buffer)
    };

    match result {
        Ok(_) => log_info!("Success"),
        Err(e) => log_error!("Error: {}", e),
    }
}

// ============================================================================
// Re-exports for convenience
// ============================================================================

#[cfg(not(feature = "async"))]
pub use sync_impl::PollingReader as SyncReader;

#[cfg(feature = "async")]
pub use async_impl::AsyncReader;
