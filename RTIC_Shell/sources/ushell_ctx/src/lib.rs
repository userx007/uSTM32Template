//! # ushell_ctx
//!
//! Encapsulates the synchronous, step-based shell processing context for
//! RTIC-based applications.
//!
//! ## What this crate owns
//! - [`ShellCtx`] — wraps `InputParser` + `AnsiKeyParser` and exposes a
//!   single `step()` method that the RTIC shell task calls in a loop.
//! - [`ShellConfig`] — plain struct of function pointers that the application
//!   fills in from its generated dispatchers, then hands to `ShellCtx::new()`.
//!
//! ## What this crate does NOT do
//! - Hardware or UART configuration (that is `uart_hal`'s job).
//! - Task scheduling or RTIC concerns (that stays in `main.rs`).
//! - Command / shortcut definitions (those live in `ushell_usercode`).
//!
//! ## Const-generic parameters
//!
//! `ShellCtx` is generic over five `usize` constants — all sourced from the
//! generated dispatcher modules or from application constants:
//!
//! | Parameter | Meaning                                      | Typical source                        |
//! |-----------|----------------------------------------------|---------------------------------------|
//! | `NAC`     | Max autocomplete candidates (per letter)     | `commands::MAX_COMMANDS_PER_LETTER`   |
//! | `FNL`     | Function name buffer length                  | `commands::MAX_FUNCTION_NAME_LEN`     |
//! | `IML`     | Input line max length                        | app constant `MAX_INPUT_LEN`          |
//! | `HTC`     | History ring-buffer total capacity           | app constant `MAX_HISTORY_CAPACITY`   |
//! | `E`       | Error message buffer size (heapless String)  | app constant `MAX_ERROR_BUFFER_SIZE`  |
//!
//! In `main.rs` create a type alias so you only write the numbers once:
//!
//! ```ignore
//! type MyShell = ushell_ctx::ShellCtx<
//!     { commands::MAX_COMMANDS_PER_LETTER },
//!     { commands::MAX_FUNCTION_NAME_LEN   },
//!     { MAX_INPUT_LEN                     },
//!     { MAX_HISTORY_CAPACITY              },
//!     { MAX_ERROR_BUFFER_SIZE             },
//! >;
//! ```

#![no_std]

//use heapless::String;


use ushell2::input::parser::InputParser;
use ushell2::input::key_reader::embedded::AnsiKeyParser;
use ushell2::input::key_reader::Key;
use ushell2::input::renderer::CallbackWriter;

use uart_hal::{write_bytes, flush_noop, RxQueueReader};

use ushell2::{log_info, log_error};

// ---------------------------------------------------------------------------
// Concrete function-pointer type aliases
//
// These are derived directly from `InputParser::new`'s signature in parser.rs:
//
//   shell_commands:  &'static [(&'static str, &'static str)]
//   shell_datatypes: &'static str
//   shell_shortcuts: &'static str
//
// The generated dispatcher functions match these exactly — no generics needed.
// ---------------------------------------------------------------------------

/// Returns the static command table passed to `InputParser`.
pub type GetCommandsFn  = fn() -> &'static [(&'static str, &'static str)];

/// Returns the static datatype-description string passed to `InputParser`.
pub type GetDatatypesFn = fn() -> &'static str;

/// Returns the static shortcuts-description string passed to `InputParser`.
pub type GetShortcutsFn = fn() -> &'static str;

/// Predicate: returns `true` when `input` matches a known shortcut prefix.
pub type IsShortcutFn   = fn(input: &str) -> bool;

/// Dispatcher: executes `input` as a command or shortcut, writing any error
/// into `error_buf`.  `E` is the heapless `String` capacity for error messages.
pub type DispatchFn<const E: usize> =
    for<'a> fn(&'a str, &'a mut heapless::String<E>) -> Result<(), &'a str>;

// ---------------------------------------------------------------------------
// ShellConfig — application-supplied wiring
// ---------------------------------------------------------------------------

/// All the application-level wiring that [`ShellCtx`] needs but cannot know
/// by itself.  Populate this from the generated dispatcher modules and pass it
/// to [`ShellCtx::new`].
///
/// `E` is the error-message buffer size (heapless `String` capacity); it must
/// match the `MAX_ERROR_BUFFER_SIZE` constant used in the dispatcher macros.
///
/// # Example
/// ```ignore
/// let config = ShellConfig {
///     get_commands:        commands::get_commands,
///     get_datatypes:       commands::get_datatypes,
///     get_shortcuts:       shortcuts::get_shortcuts,
///     is_shortcut:         shortcuts::is_supported_shortcut,
///     command_dispatcher:  commands::dispatch,
///     shortcut_dispatcher: shortcuts::dispatch,
///     prompt:              PROMPT,
/// };
/// let shell: MyShell = ShellCtx::new(config);
/// ```
pub struct ShellConfig<const E: usize> {
    /// Returns `&'static [(&'static str, &'static str)]` — the command table.
    pub get_commands:        GetCommandsFn,
    /// Returns `&'static str` — human-readable datatype descriptions.
    pub get_datatypes:       GetDatatypesFn,
    /// Returns `&'static str` — human-readable shortcut descriptions.
    pub get_shortcuts:       GetShortcutsFn,
    /// Returns `true` when the input string is a known shortcut.
    pub is_shortcut:         IsShortcutFn,
    /// Dispatches a command line; writes an error message into `error_buf` on failure.
    pub command_dispatcher:  DispatchFn<E>,
    /// Dispatches a shortcut line; writes an error message into `error_buf` on failure.
    pub shortcut_dispatcher: DispatchFn<E>,
    /// The prompt string displayed before each input line (e.g. `">> "`).
    pub prompt:              &'static str,
}

// ---------------------------------------------------------------------------
// ShellCtx
// ---------------------------------------------------------------------------

/// Step-based shell context for synchronous (RTIC) environments.
///
/// Wraps [`InputParser`] and [`AnsiKeyParser`], storing the dispatch
/// function-pointers alongside the parser so `step()` can execute commands
/// without any knowledge of the application's command table.
///
/// # Const generics
/// See [crate-level documentation](crate) for a description of each parameter.
pub struct ShellCtx<
    const NAC: usize, // max autocomplete candidates per letter
    const FNL: usize, // function name length
    const IML: usize, // input max length
    const HTC: usize, // history total capacity
    const E:   usize, // error buffer size
> {
    parser: InputParser<
        'static,
        CallbackWriter<fn(&[u8]), fn()>,
        NAC,
        FNL,
        IML,
        HTC,
    >,
    key_parser:          AnsiKeyParser,
    pending_key:         Option<Key>,
    is_shortcut:         IsShortcutFn,
    command_dispatcher:  DispatchFn<E>,
    shortcut_dispatcher: DispatchFn<E>,
}

impl<
    const NAC: usize,
    const FNL: usize,
    const IML: usize,
    const HTC: usize,
    const E:   usize,
> ShellCtx<NAC, FNL, IML, HTC, E>
{
    /// Construct a new shell context from the application-supplied config.
    ///
    /// Uses [`uart_hal::write_bytes`] and [`uart_hal::flush_noop`] as the
    /// underlying writer — no UART reference is stored in this struct.
    pub fn new(config: ShellConfig<E>) -> Self {
        let writer = CallbackWriter::new(
            write_bytes as fn(&[u8]),
            flush_noop  as fn(),
        );

        let parser = InputParser::new(
            writer,
            (config.get_commands)(),    // &'static [(&'static str, &'static str)]
            (config.get_datatypes)(),   // &'static str
            (config.get_shortcuts)(),   // &'static str
            config.prompt,              // &'static str
        );

        Self {
            parser,
            key_parser:          AnsiKeyParser::new(),
            pending_key:         None,
            is_shortcut:         config.is_shortcut,
            command_dispatcher:  config.command_dispatcher,
            shortcut_dispatcher: config.shortcut_dispatcher,
        }
    }

    /// Process one byte from `reader` and advance the parser state machine.
    ///
    /// Returns `false` when the shell signals it wants to stop (e.g. the user
    /// typed `#q`). The caller should break its processing loop in that case.
    ///
    /// # Example (inside the RTIC shell task)
    /// ```ignore
    /// ctx.shared.rx_queue.lock(|rx_queue| {
    ///     let mut reader = RxQueueReader::new(rx_queue);
    ///     while !reader.is_empty() {
    ///         if !ctx.local.shell.step(&mut reader) {
    ///             log_info!("Shell exited");
    ///             break;
    ///         }
    ///     }
    /// });
    /// ```
    pub fn step(&mut self, reader: &mut RxQueueReader) -> bool {
        // Decode one raw byte into an ANSI key event (handles multi-byte sequences)
        if let Some(byte) = reader.read_byte() {
            if let Some(key) = self.key_parser.parse_byte(byte) {
                self.pending_key = Some(key);
            }
        }

        // Copy fn-pointers to locals so the closure below can capture them
        // without borrowing `self` (which is already mutably borrowed by `parser`).
        let is_shortcut         = self.is_shortcut;
        let command_dispatcher  = self.command_dispatcher;
        let shortcut_dispatcher = self.shortcut_dispatcher;

        self.parser.parse_input(
            // Key source: take the pending key decoded above
            || self.pending_key.take(),

            // Output sink: write directly via uart_hal
            |s: &str| write_bytes(s.as_bytes()),

            // Command execution: called with the complete, trimmed input line
            |input| {
                let mut error_buf: heapless::String<E> = heapless::String::new();

                let result = if (is_shortcut)(input.as_str()) {
                    (shortcut_dispatcher)(input.as_str(), &mut error_buf)
                } else {
                    (command_dispatcher)(input.as_str(), &mut error_buf)
                };

                match result {
                    Ok(_)  => log_info!("Success"),
                    Err(e) => log_error!("Error: {}", e), // e: &str — Display is fine
                }
            },
        )
    }
}
