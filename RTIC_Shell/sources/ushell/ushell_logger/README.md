
# Flexible logger 

Works in both `no_std` embedded and hosted environments, using a writer trait approach. This is the most optimal solution as it:

1. Works with any output (UART, USB, stdout, etc.)
2. Zero-cost abstraction in `no_std`
3. Thread-safe for hosted environments
4. Maintains your color and formatting features## Summary


### **Key Features:**

1. **Dual Mode Support:**
   - **Hosted mode** (`feature = "hosted"`): Uses global static logger with `println!`
   - **Embedded mode** (default): Uses local logger with any writer that implements `fmt::Write`

2. **Writer Trait Pattern:**
   - Any type implementing `core::fmt::Write` automatically works
   - Supports UART, USB, buffers, or custom output
   - Zero-cost abstraction

3. **Consistent API:**
   - Same macros (`log_info!`, `log_error!`, etc.) in both modes
   - In embedded: pass logger as first argument
   - In hosted: uses global logger

4. **Configuration:**
   - Color entire line or just labels
   - Minimum log level filtering
   - Runtime reconfiguration

### **Usage:**

#### INITIALIZATION:

For no_std environments - buffer_size is the last parameter:

```rust
use my_logger::{init_logger, LoggerConfig, LogLevel};

Initialize with 512-byte buffer
init_logger(&mut my_writer, LoggerConfig::default(), 512);

Initialize with 1024-byte buffer
init_logger(&mut my_writer, LoggerConfig::default(), 1024);
```

For hosted (std) environments:

```rust
use my_logger::{init_logger, LoggerConfig};

Buffer size parameter not needed for hosted environments
init_logger(LoggerConfig::default());
```

#### USAGE:

```rust
use my_logger::{log_info, log_error, log_debug, log_simple};

log_info!("Application started");
log_error!("Error code: {}", 42);
log_debug!("Debug value: {:?}", some_struct);
log_simple!("Simple message without level prefix");
```

#### BUFFER SIZE BEHAVIOR:

- The logger ALWAYS respects the buffer_size parameter from init_logger().
- The value is rounded up to the nearest supported size.
- Supported sizes: 64, 128, 256, 512, 1024, 2048, 4096 bytes

**Examples:**
- init_logger(..., 50)   -> uses 64 bytes
- init_logger(..., 100)  -> uses 128 bytes
- init_logger(..., 256)  -> uses 256 bytes
- init_logger(..., 500)  -> uses 512 bytes
- init_logger(..., 1500) -> uses 2048 bytes
- init_logger(..., 3000) -> uses 4096 bytes
- init_logger(..., 5000) -> uses 4096 bytes (capped at maximum)

#### ADVANCED: EXPLICIT BUFFER SIZE PER LOG CALL

You can also specify buffer size per log call (bypasses global setting):

```rust
use my_logger::{log_with_buffer_size, LogLevel};

Use 512 bytes just for this log call
log_with_buffer_size!(LogLevel::Info, 512, "Long message: {}", data);

Or for simple logs
log_simple_with_buffer_size!(1024, "Very long message: {}", data);
```

#### BENEFITS:
- Set buffer size once at initialization
- All log calls automatically use the configured size
- No need to specify size in every log macro call
- Supports 7 predefined sizes from 64 to 4096 bytes
- Efficient - uses compile-time sized buffers (heapless::String)
- Works for both std and no_std environments

```rust
fn main() {
    // Example initialization for different use cases:
    
    // Embedded system with very limited RAM
    init_logger(&mut writer, config, 64);
    
    // Small embedded application
    init_logger(&mut writer, config, 128);
    
    // Standard embedded application (most common)
    init_logger(&mut writer, config, 256);
    
    // Application with longer log messages
    init_logger(&mut writer, config, 1024);
    
    // Application with very long messages
    init_logger(&mut writer, config, 4096);
    
    // Then use logging macros normally - buffer size is automatically selected!
    log_info!("This is a log message");
    log_error!("Error: {}", error_code);
    log_debug!("Debug data: {:?}", some_struct);
}
```