# ushell2

A lightweight, `no_std` compatible shell runtime component for building interactive command-line interfaces in Rust.

This crate provides the core shell execution functions that orchestrate command parsing, input handling, and terminal management for shell-based applications. It's part of the [uRustShell](https://github.com/userx007/uRustShell) framework.

## Features

- **Zero-allocation design** — uses `heapless` data structures for embedded/constrained environments
- **Generic error handling** — accepts any error type that implements `Debug`
- **Dual dispatch model** — separate handlers for commands and shortcuts
- **Automatic terminal management** — sets up raw mode for interactive input
- **Integration with `shell-input`** — leverages autocomplete, history, and advanced editing features

## Usage

```rust
use ushell::{ShellRunner, ShellConfig};
use heapless::String;

// Define your command and shortcut handlers
fn get_commands() -> &'static [(&'static str, &'static str)] {
    &[("help", "Show help"), ("echo", "Echo text")]
}

fn command_dispatcher<'a>(cmd: &'a str, error_buffer: &'a mut String<32>) -> Result<(), &'a str> {
    // Parse and execute commands
    // On error, write to error_buffer and return Err(error_buffer.as_str())
    Ok(())
}

fn shortcut_dispatcher<'a>(cmd: &'a str, error_buffer: &'a mut String<32>) -> Result<(), &'a str> {
    // Handle shortcuts
    Ok(())
}

fn is_shortcut(input: &str) -> bool {
    input.starts_with("###")
}

// Configure the shell
let config = ShellConfig {
    get_commands,
    get_datatypes: || "String, Int",
    get_shortcuts: || "Ctrl+C: Cancel",
    is_shortcut,
    command_dispatcher,
    shortcut_dispatcher,
    prompt: "> ",
};

// Run the shell (requires UART function pointers)
ShellRunner::run::<
    10,   // NAC: Number of autocomplete candidates
    20,   // FNL: Function name length
    128,  // IML: Input max length
    512,  // HTC: History total capacity
    _     // Read function type (inferred)
>(
    uart_write,  // fn(&[u8])
    uart_flush,  // fn()
    uart_read_byte,  // FnMut() -> Option<u8>
    config,
);
```

## Generic Parameters

The shell functions require several const generic parameters to configure behavior:

- `NAC` — Number of Autocomplete Candidates (max commands for tab completion)
- `FNL` — Maximum length of function names (for autocomplete)
- `IML` — Maximum input line length (input buffer size)
- `HTC` — Total capacity for history storage (in bytes)

Note: The read function type is typically inferred as `_` in the type parameters.

## Configuration Parameters

The `ShellConfig` struct contains the following fields:

- `get_commands` — Returns a static slice of `(name, description)` tuples for available commands
- `get_datatypes` — Returns a help string describing supported parameter types
- `get_shortcuts` — Returns a help string listing available keyboard shortcuts
- `is_shortcut` — Predicate to determine if input should be treated as a shortcut
- `command_dispatcher` — Executes regular commands, writes errors to provided buffer
- `shortcut_dispatcher` — Executes shortcut commands, writes errors to provided buffer
- `prompt` — The prompt string to display

## Execution Model

The shell runs in a continuous loop, processing input byte-by-byte:

1. Reads bytes from UART via the provided `read_byte_fn` callback
2. Parses bytes into key events using `AnsiKeyParser` (handles ANSI escape sequences)
3. Processes keys through `InputParser` which manages:
   - Input buffer editing
   - Command history (up/down arrows)
   - Tab completion
   - Prompt rendering
4. When Enter is pressed:
   - Checks input against `is_shortcut` predicate
   - Calls either `shortcut_dispatcher` or `command_dispatcher`
   - Logs success/error messages
5. Loop continues until parser signals termination (e.g., #q command)

## Error Handling

Both command and shortcut dispatchers use a unified error handling approach:

- Dispatchers receive an `error_buffer: &mut String<32>` parameter
- On error, write the error message to the buffer and return `Err(error_buffer.as_str())`
- Errors are logged with `log_error!` macro: `Error: <message>`
- Success is logged with `log_info!` macro: `Success`

## Integration with uRustShell

This crate is designed to work within the uRustShell framework, which provides:

- Command registration via `.cfg` files
- Automatic parameter validation and type checking
- Autocomplete and command history
- Advanced line editing capabilities
- Shortcut support with major/minor groups

For a complete working example, see the [uRustShell repository](https://github.com/userx007/uRustShell).

## Dependencies

- `heapless` — Stack-allocated data structures
- `ushell-input` — Input parsing and terminal management
- `ushell-logger` — Logging functionality

## Platform Support

This crate is `no_std` compatible and suitable for embedded systems, though it does require terminal access for raw mode control.

## License

See the [uRustShell repository](https://github.com/userx007/uRustShell) for license information.