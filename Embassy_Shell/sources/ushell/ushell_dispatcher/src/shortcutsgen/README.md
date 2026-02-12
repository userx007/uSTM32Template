# Shortcut Dispatcher Macro

A `no_std`-compatible procedural macro for generating efficient command dispatchers in embedded and constrained environments.

## Overview

This crate provides a compile-time shortcut mapping system that generates a lightweight dispatcher module without heap allocation. Perfect for embedded systems, CLI tools, or any environment where you need fast command routing with minimal overhead.

## Features

- 🚀 **Zero runtime overhead** - All parsing happens at compile time
- 🔒 **`no_std` compatible** - Works in embedded and bare-metal environments
- 💾 **No heap allocation** - Uses `heapless::String` for error messages
- ⚡ **Fast dispatch** - Direct match-based routing to functions
- 📝 **Simple mapping format** - Easy-to-maintain shortcut definition files

## Installation

Add this to your `Cargo.toml`:

```toml
[dependencies]
shortcut-dispatcher = "0.1.0"
heapless = "0.8"
```

## Usage

### 1. Create a Shortcut Mapping File

Create a file (e.g., `shortcuts.txt`) with your command mappings:

```text
!: { +: commands::bang_plus, -: commands::bang_minus },
?: { #: commands::question_hash, .: commands::question_dot },
```

The format is: `prefix: { key: function_path, key: function_path },`

This creates shortcuts like:
- `!+` → calls `commands::bang_plus()`
- `!-` → calls `commands::bang_minus()`
- `?#` → calls `commands::question_hash()`
- `?.` → calls `commands::question_dot()`

### 2. Define Your Command Functions

```rust
mod commands {
    pub fn bang_plus(param: &str) {
        println!("bang_plus: {}", param);
    }
    
    pub fn bang_minus(param: &str) {
        println!("bang_minus: {}", param);
    }
    
    pub fn question_hash(param: &str) {
        println!("question_hash: {}", param);
    }
    
    pub fn question_dot(param: &str) {
        println!("question_dot: {}", param);
    }
}
```

### 3. Generate the Dispatcher

```rust
use shortcut_dispatcher::define_shortcuts;

define_shortcuts! {
    mod shortcuts;
    error_buffer_size = 64;
    path = "shortcuts.txt"
}
```

**Parameters:**
- `mod shortcuts` - Name of the generated module
- `error_buffer_size = 64` - Maximum size for error message buffer
- `path = "shortcuts.txt"` - Path to your mapping file (relative to `CARGO_MANIFEST_DIR`)
- **Note:** No trailing semicolon after the path

### 4. Use the Generated Dispatcher

```rust
fn main() {
    let mut error_buffer = heapless::String::<64>::new();
    
    // Dispatch commands (requires at least 2 characters)
    match shortcuts::dispatch("!+", &mut error_buffer) {
        Ok(()) => println!("Command executed"),
        Err(e) => println!("Error: {}", e),
    }
    
    // Commands can include parameters
    shortcuts::dispatch("?# params", &mut error_buffer).unwrap();
    
    // Check if a shortcut is supported
    if shortcuts::is_supported_shortcut("!") {
        println!("'!' prefix is supported");
    }
    
    // List all available shortcuts
    println!("Available: {}", shortcuts::get_shortcuts());
}
```

## Generated API

The macro generates three public functions in your specified module:

### `dispatch<'a>(input: &'a str, error_buffer: &'a mut heapless::String<N>) -> Result<(), &'a str>`

Parses the input string and invokes the corresponding function. The first two characters are used as the shortcut key, and any remaining text is passed as a parameter to the function.

**Important:** Input must be at least 2 characters long. Single-character inputs will result in an "Unknown shortcut" error.

The error message lifetime is tied to the `error_buffer` parameter.

```rust
let mut error_buffer = heapless::String::<64>::new();
shortcuts::dispatch("!+", &mut error_buffer)?;              // Calls bang_plus("")
shortcuts::dispatch("?# params", &mut error_buffer)?;       // Calls question_hash("params")
shortcuts::dispatch("!", &mut error_buffer)?;               // Error: too short
```

### `is_supported_shortcut(input: &str) -> bool`

Checks if the input starts with a supported shortcut prefix (first character only).

```rust
if shortcuts::is_supported_shortcut("?") {
    // Valid prefix
}
```

### `get_shortcuts() -> &'static str`

Returns a string listing all available shortcuts, separated by " | ".

```rust
println!("Available shortcuts: {}", shortcuts::get_shortcuts());
// Output: !+ | !- | ?# | ?.
```

## Mapping File Format

The mapping file uses a simple line-based format:

```text
prefix: { key: function::path, key: function::path },
prefix: { key: function::path },
```

- **Prefix**: Single character that starts the shortcut
- **Key**: Single character combined with prefix to form the full shortcut
- **Function path**: Full path to the function to invoke (must be in scope)
- Each line must end with `},`
- Empty lines are ignored
- Multi-line entries are supported if they end with `},`

## Example: Embedded CLI

```rust
#![no_std]

use shortcut_dispatcher::define_shortcuts;

mod hardware {
    pub fn led_on(_: &str) { /* ... */ }
    pub fn led_off(_: &str) { /* ... */ }
    pub fn read_sensor(param: &str) { /* ... */ }
}

define_shortcuts! {
    mod cli;
    error_buffer_size = 32;
    path = "embedded_shortcuts.txt"
}

#[no_mangle]
pub extern "C" fn handle_command(cmd: &str) {
    let mut error_buffer = heapless::String::<32>::new();
    if let Err(e) = cli::dispatch(cmd, &mut error_buffer) {
        // Handle error without allocation
    }
}
```

## `no_std` Compatibility

This crate is fully `no_std` compatible. Error messages use `heapless::String` to avoid heap allocation. Make sure to include `heapless` in your dependencies.

The generated dispatcher module works in `no_std` environments. Simply declare `#![no_std]` at your crate root if needed.

## License

Licensed under:

- MIT license ([LICENSE-MIT](LICENSE-MIT) or http://opensource.org/licenses/MIT)

at your option.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.
