// A minimal logger that works in both no_std and std environments
// Supports colored output based on severity level
// Now integrated with shell's Writer trait for unified output

#![cfg_attr(not(feature = "hosted"), no_std)]
#![allow(unexpected_cfgs)]
use core::fmt::{self, Write};

#[cfg(not(feature = "hosted"))]
const DEFAULT_BUFFER_SIZE: usize = 128;

#[cfg(feature = "hosted")]
use std::sync::{Mutex, Once};

#[cfg(not(feature = "hosted"))]
use core::cell::RefCell;

#[cfg(not(feature = "hosted"))]
use critical_section::Mutex;

// Re-export dependencies needed by macros
#[cfg(not(feature = "hosted"))]
pub use heapless;

// Re-export core::fmt::Write for macro usage
pub use core::fmt::Write as FmtWrite;

// ANSI color codes
const RESET: &str = "\x1b[0m";
const RED: &str = "\x1b[31m";
const YELLOW: &str = "\x1b[33m";
const GREEN: &str = "\x1b[32m";
const BLUE: &str = "\x1b[94m";
const CYAN: &str = "\x1b[36m";
const GRAY: &str = "\x1b[90m";

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum LogLevel {
    Error,
    Warn,
    Info,
    Debug,
    Verbose,
    Trace,
}

impl LogLevel {
    #[inline]
    pub const fn color(&self) -> &'static str {
        match self {
            LogLevel::Error => RED,
            LogLevel::Warn => YELLOW,
            LogLevel::Info => GREEN,
            LogLevel::Debug => BLUE,
            LogLevel::Verbose => CYAN,
            LogLevel::Trace => GRAY,
        }
    }

    #[inline]
    pub const fn label(&self) -> &'static str {
        match self {
            LogLevel::Error => "ERROR",
            LogLevel::Warn => " WARN",
            LogLevel::Info => " INFO",
            LogLevel::Debug => "DEBUG",
            LogLevel::Verbose => " VERB",
            LogLevel::Trace => "TRACE",
        }
    }
    
    /// Allows early exit before string formatting
    #[inline]
    pub const fn is_enabled(&self, min_level: LogLevel) -> bool {
        (*self as u8) <= (min_level as u8)
    }
}

impl fmt::Display for LogLevel {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}{}{}", self.color(), self.label(), RESET)
    }
}

// ============================================================================
// Unified Writer trait that works for both Logger and Shell
// ============================================================================

/// Universal writer trait for output (used by both logger and shell renderer)
pub trait UnifiedWriter {
    /// Write a string slice
    fn write_str(&mut self, s: &str);

    /// Write raw bytes
    fn write_bytes(&mut self, bytes: &[u8]);

    /// Flush the output (if buffered)
    fn flush(&mut self);
}

// Implement UnifiedWriter for anything that implements fmt::Write
impl<T: fmt::Write> UnifiedWriter for T {
    fn write_str(&mut self, s: &str) {
        let _ = <Self as fmt::Write>::write_str(self, s);
    }

    fn write_bytes(&mut self, bytes: &[u8]) {
        if let Ok(s) = core::str::from_utf8(bytes) {
            let _ = self.write_str(s);
        }
    }

    fn flush(&mut self) {
        // Default implementation - no-op for unbuffered writers
    }
}

/// Trait specifically for log output (extends UnifiedWriter)
/// Note: Send is required to allow the trait to be used in global static loggers
pub trait LogWriter: UnifiedWriter + Write + Send {
    /// Optional: Writer can override to optimize batch writes
    fn write_log(&mut self, level: LogLevel, message: &str, color_entire_line: bool) {
        if color_entire_line {
            UnifiedWriter::write_str(self, level.color());
            UnifiedWriter::write_str(self, "[");
            UnifiedWriter::write_str(self, level.label());
            UnifiedWriter::write_str(self, "] ");
            UnifiedWriter::write_str(self, message);
            UnifiedWriter::write_str(self, RESET);
            UnifiedWriter::write_str(self, "\r\n");
        } else {
            UnifiedWriter::write_str(self, "[");
            let _ = write!(self, "{}", level);
            UnifiedWriter::write_str(self, "] ");
            UnifiedWriter::write_str(self, message);
            UnifiedWriter::write_str(self, "\r\n");
        }
        self.flush();
    }

    /// Write simple message without level prefix (headless mode)
    fn write_simple(&mut self, message: &str) {
        UnifiedWriter::write_str(self, message);
        UnifiedWriter::write_str(self, "\r\n");
        self.flush();
    }
}

// Automatically implement LogWriter for anything that implements UnifiedWriter + Write + Send
impl<T: UnifiedWriter + Write + Send> LogWriter for T {}

/// Logger configuration
pub struct LoggerConfig {
    pub color_entire_line: bool,
    pub min_level: LogLevel,
}

impl Default for LoggerConfig {
    fn default() -> Self {
        Self {
            color_entire_line: false,
            min_level: LogLevel::Info,
        }
    }
}

// ============================================================================
// Buffer size configuration - stored globally
// ============================================================================

#[cfg(not(feature = "hosted"))]
use core::sync::atomic::{AtomicUsize, Ordering};

#[cfg(not(feature = "hosted"))]
static BUFFER_SIZE: AtomicUsize = AtomicUsize::new(DEFAULT_BUFFER_SIZE);

#[cfg(not(feature = "hosted"))]
#[inline]
pub fn set_buffer_size(size: usize) {
    BUFFER_SIZE.store(size, Ordering::Relaxed);
}

#[cfg(not(feature = "hosted"))]
#[inline]
pub fn get_buffer_size() -> usize {
    BUFFER_SIZE.load(Ordering::Relaxed)
}

// ============================================================================
// For hosted environments (std) - use a global static logger
// ============================================================================

#[cfg(feature = "hosted")]
static INIT: Once = Once::new();

#[cfg(feature = "hosted")]
static mut GLOBAL_LOGGER: Option<Mutex<GlobalLogger>> = None;

#[cfg(feature = "hosted")]
struct GlobalLogger {
    config: LoggerConfig,
}

#[cfg(feature = "hosted")]
impl GlobalLogger {
    fn new(config: LoggerConfig) -> Self {
        Self { config }
    }

    fn log(&self, level: LogLevel, message: &str) {
        if !level.is_enabled(self.config.min_level) {
            return;
        }
        
        if self.config.color_entire_line {
            println!("{}[{}] {}{}", level.color(), level.label(), message, RESET);
        } else {
            println!("[{}] {}", level, message);
        }
    }

    #[inline]
    fn log_simple(&self, message: &str) {
        println!("{}", message);
    }
}

#[cfg(feature = "hosted")]
pub fn init_logger(config: LoggerConfig) {
    INIT.call_once(|| unsafe {
        GLOBAL_LOGGER = Some(Mutex::new(GlobalLogger::new(config)));
    });
}

#[cfg(feature = "hosted")]
pub fn set_color_entire_line(enabled: bool) {
    if let Some(logger) = unsafe { &GLOBAL_LOGGER } {
        if let Ok(mut guard) = logger.lock() {
            guard.config.color_entire_line = enabled;
        }
    }
}

#[cfg(feature = "hosted")]
pub fn set_min_level(level: LogLevel) {
    if let Some(logger) = unsafe { &GLOBAL_LOGGER } {
        if let Ok(mut guard) = logger.lock() {
            guard.config.min_level = level;
        }
    }
}

#[cfg(feature = "hosted")]
pub fn log_with_level(level: LogLevel, message: &str) {
    if let Some(logger) = unsafe { &GLOBAL_LOGGER } {
        if let Ok(guard) = logger.lock() {
            guard.log(level, message);
        }
    }
}

#[cfg(feature = "hosted")]
#[inline]
pub fn log_simple_message(message: &str) {
    if let Some(logger) = unsafe { &GLOBAL_LOGGER } {
        if let Ok(guard) = logger.lock() {
            guard.log_simple(message);
        }
    }
}

// ============================================================================
// For no_std environments - use a global logger with writer
// ============================================================================

#[cfg(not(feature = "hosted"))]
struct GlobalLoggerWrapper {
    config: LoggerConfig,
    writer: &'static mut dyn LogWriter,
}

#[cfg(not(feature = "hosted"))]
impl GlobalLoggerWrapper {
    fn new(config: LoggerConfig, writer: &'static mut dyn LogWriter) -> Self {
        Self { config, writer }
    }

    fn log(&mut self, level: LogLevel, message: &str) {
        if !level.is_enabled(self.config.min_level) {
            return;
        }
        
        self.writer
            .write_log(level, message, self.config.color_entire_line);
    }

    #[inline]
    fn log_simple(&mut self, message: &str) {
        self.writer.write_simple(message);
    }
}

#[cfg(not(feature = "hosted"))]
static GLOBAL_LOGGER: Mutex<RefCell<Option<GlobalLoggerWrapper>>> = Mutex::new(RefCell::new(None));

#[cfg(not(feature = "hosted"))]
pub fn init_logger(config: LoggerConfig, writer: &'static mut dyn LogWriter) {
    critical_section::with(|cs| {
        *GLOBAL_LOGGER.borrow_ref_mut(cs) = Some(GlobalLoggerWrapper::new(config, writer));
    });
}

#[cfg(not(feature = "hosted"))]
pub fn set_color_entire_line(enabled: bool) {
    critical_section::with(|cs| {
        if let Some(logger) = GLOBAL_LOGGER.borrow_ref_mut(cs).as_mut() {
            logger.config.color_entire_line = enabled;
        }
    });
}

#[cfg(not(feature = "hosted"))]
pub fn set_min_level(level: LogLevel) {
    critical_section::with(|cs| {
        if let Some(logger) = GLOBAL_LOGGER.borrow_ref_mut(cs).as_mut() {
            logger.config.min_level = level;
        }
    });
}

#[cfg(not(feature = "hosted"))]
pub fn log_with_level(level: LogLevel, message: &str) {
    critical_section::with(|cs| {
        if let Some(logger) = GLOBAL_LOGGER.borrow_ref_mut(cs).as_mut() {
            logger.log(level, message);
        }
    });
}

#[cfg(not(feature = "hosted"))]
#[inline]
pub fn log_simple_message(message: &str) {
    critical_section::with(|cs| {
        if let Some(logger) = GLOBAL_LOGGER.borrow_ref_mut(cs).as_mut() {
            logger.log_simple(message);
        }
    });
}

// ============================================================================
// Get a reference to the global writer for shell use
// ============================================================================

#[cfg(not(feature = "hosted"))]
pub fn with_global_writer<F, R>(f: F) -> Option<R>
where
    F: FnOnce(&mut dyn UnifiedWriter) -> R,
{
    critical_section::with(|cs| {
        if let Some(logger) = GLOBAL_LOGGER.borrow_ref_mut(cs).as_mut() {
            Some(f(logger.writer))
        } else {
            None
        }
    })
}

// ============================================================================
// Legacy Logger for backward compatibility (no_std only)
// ============================================================================

#[cfg(not(feature = "hosted"))]
pub struct Logger<W: LogWriter> {
    writer: W,
    config: LoggerConfig,
}

#[cfg(not(feature = "hosted"))]
impl<W: LogWriter> Logger<W> {
    pub fn new(writer: W, config: LoggerConfig) -> Self {
        Self { writer, config }
    }

    #[inline]
    pub fn set_color_entire_line(&mut self, enabled: bool) {
        self.config.color_entire_line = enabled;
    }

    #[inline]
    pub fn set_min_level(&mut self, level: LogLevel) {
        self.config.min_level = level;
    }

    pub fn log(&mut self, level: LogLevel, message: &str) {
        if level.is_enabled(self.config.min_level) {
            self.writer
                .write_log(level, message, self.config.color_entire_line);
        }
    }

    #[inline]
    pub fn log_simple(&mut self, message: &str) {
        self.writer.write_simple(message);
    }

    #[inline]
    pub fn error(&mut self, message: &str) {
        self.log(LogLevel::Error, message);
    }

    #[inline]
    pub fn warn(&mut self, message: &str) {
        self.log(LogLevel::Warn, message);
    }

    #[inline]
    pub fn info(&mut self, message: &str) {
        self.log(LogLevel::Info, message);
    }

    #[inline]
    pub fn debug(&mut self, message: &str) {
        self.log(LogLevel::Debug, message);
    }

    #[inline]
    pub fn verbose(&mut self, message: &str) {
        self.log(LogLevel::Verbose, message);
    }

    #[inline]
    pub fn trace(&mut self, message: &str) {
        self.log(LogLevel::Trace, message);
    }
}

// ============================================================================
// Unified Macros for both environments - with dynamic buffer sizing
// ============================================================================

#[cfg(not(feature = "hosted"))]
#[doc(hidden)]
pub const fn select_buffer_size(size: usize) -> usize {
    match size {
        0..=64 => 64,
        65..=128 => 128,
        129..=256 => 256,
        257..=512 => 512,
        513..=1024 => 1024,
        1025..=2048 => 2048,
        _ => 4096,
    }
}

// Internal helper macros for different buffer sizes
#[cfg(not(feature = "hosted"))]
#[doc(hidden)]
#[macro_export]
macro_rules! __log_with_size {
    ($level:expr, $size:literal, $($arg:tt)*) => {{
        use $crate::FmtWrite as _;
        let mut msg_buf = $crate::heapless::String::<$size>::new();
        let _ = ::core::write!(&mut msg_buf, $($arg)*);
        $crate::log_with_level($level, msg_buf.as_str());
    }};
}

#[cfg(not(feature = "hosted"))]
#[doc(hidden)]
#[macro_export]
macro_rules! __log_simple_with_size {
    ($size:literal, $($arg:tt)*) => {{
        use $crate::FmtWrite as _;
        let mut msg_buf = $crate::heapless::String::<$size>::new();
        let _ = ::core::write!(&mut msg_buf, $($arg)*);
        $crate::log_simple_message(msg_buf.as_str());
    }};
}

#[macro_export]
macro_rules! log {
    ($level:expr, $($arg:tt)*) => {{
        #[cfg(not(feature = "hosted"))]
        {
            let size = $crate::get_buffer_size();
            match size {
                0..=64 => $crate::__log_with_size!($level, 64, $($arg)*),
                65..=128 => $crate::__log_with_size!($level, 128, $($arg)*),
                129..=256 => $crate::__log_with_size!($level, 256, $($arg)*),
                257..=512 => $crate::__log_with_size!($level, 512, $($arg)*),
                513..=1024 => $crate::__log_with_size!($level, 1024, $($arg)*),
                1025..=2048 => $crate::__log_with_size!($level, 2048, $($arg)*),
                _ => $crate::__log_with_size!($level, 4096, $($arg)*),
            }
        }
        #[cfg(feature = "hosted")]
        {
            $crate::log_with_level($level, &format!($($arg)*));
        }
    }};
}

/// Log with explicit buffer size (bypasses global buffer size in no_std environments)
#[macro_export]
macro_rules! log_with_buffer_size {
    ($level:expr, $size:literal, $($arg:tt)*) => {{
        #[cfg(not(feature = "hosted"))]
        {
            use $crate::FmtWrite as _;
            let mut msg_buf = $crate::heapless::String::<$size>::new();
            let _ = ::core::write!(&mut msg_buf, $($arg)*);
            $crate::log_with_level($level, msg_buf.as_str());
        }
        #[cfg(feature = "hosted")]
        {
            $crate::log_with_level($level, &format!($($arg)*));
        }
    }};
}

#[macro_export]
macro_rules! log_error {
    ($($arg:tt)*) => {
        $crate::log!($crate::LogLevel::Error, $($arg)*)
    };
}

#[macro_export]
macro_rules! log_warn {
    ($($arg:tt)*) => {
        $crate::log!($crate::LogLevel::Warn, $($arg)*)
    };
}

#[macro_export]
macro_rules! log_info {
    ($($arg:tt)*) => {
        $crate::log!($crate::LogLevel::Info, $($arg)*)
    };
}

#[macro_export]
macro_rules! log_debug {
    ($($arg:tt)*) => {
        $crate::log!($crate::LogLevel::Debug, $($arg)*)
    };
}

#[macro_export]
macro_rules! log_verbose {
    ($($arg:tt)*) => {
        $crate::log!($crate::LogLevel::Verbose, $($arg)*)
    };
}

#[macro_export]
macro_rules! log_trace {
    ($($arg:tt)*) => {
        $crate::log!($crate::LogLevel::Trace, $($arg)*)
    };
}

/// Simple/headless logging without level prefix
/// Outputs just the message with a newline, no [INFO], [ERROR], etc.
#[macro_export]
macro_rules! log_simple {
    ($($arg:tt)*) => {{
        #[cfg(not(feature = "hosted"))]
        {
            // Buffer size selection based on initialization
            let size = $crate::get_buffer_size();
            match size {
                0..=64 => $crate::__log_simple_with_size!(64, $($arg)*),
                65..=128 => $crate::__log_simple_with_size!(128, $($arg)*),
                129..=256 => $crate::__log_simple_with_size!(256, $($arg)*),
                257..=512 => $crate::__log_simple_with_size!(512, $($arg)*),
                513..=1024 => $crate::__log_simple_with_size!(1024, $($arg)*),
                1025..=2048 => $crate::__log_simple_with_size!(2048, $($arg)*),
                _ => $crate::__log_simple_with_size!(4096, $($arg)*),
            }
        }
        #[cfg(feature = "hosted")]
        {
            $crate::log_simple_message(&format!($($arg)*));
        }
    }};
}

/// Log simple message with explicit buffer size (bypasses global buffer size in no_std environments)
#[macro_export]
macro_rules! log_simple_with_buffer_size {
    ($size:literal, $($arg:tt)*) => {{
        #[cfg(not(feature = "hosted"))]
        {
            use $crate::FmtWrite as _;
            let mut msg_buf = $crate::heapless::String::<$size>::new();
            let _ = ::core::write!(&mut msg_buf, $($arg)*);
            $crate::log_simple_message(msg_buf.as_str());
        }
        #[cfg(feature = "hosted")]
        {
            $crate::log_simple_message(&format!($($arg)*));
        }
    }};
}