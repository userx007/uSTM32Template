#![no_std]
#![allow(unused)]

extern crate heapless;
extern crate ushell_logger;

#[cfg(any(
    feature = "history-persistence",
    feature = "heap-history",
    feature = "heap-input-buffer"
))]
extern crate std;

pub mod autocomplete;
pub mod history;
pub mod input;
pub mod terminal;

// Re-export commonly used types for easier importing
pub use input::parser::InputParser;
pub use terminal::RawMode;
