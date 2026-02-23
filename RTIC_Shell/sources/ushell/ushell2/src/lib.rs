#![no_std]
#![allow(unused)]

pub extern crate heapless;

#[cfg(any(
    feature = "history-persistence",
    feature = "heap-history",
    feature = "heap-input-buffer"
))]
extern crate std;

pub mod autocomplete;
pub mod history;
pub mod input;
pub mod logger;
pub mod runner;
pub mod terminal;

// Re-export commonly used types for easier importing
pub use input::parser::InputParser;
pub use terminal::RawMode;

// Re-export items needed by logging macros
// (macros use $crate:: which refers to the crate root)
pub use logger::{
    get_buffer_size,
    log_with_level, 
    log_simple_message,
    LogLevel,
    FmtWrite,
    UnifiedWriter,
};