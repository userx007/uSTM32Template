use core::fmt::Write;
use core::ops::FnMut;

/// Import and re-export the unified writer from logger
///
pub use ushell_logger::UnifiedWriter;

/// Standard library writer (for hosted platforms)
///
#[cfg(feature = "hosted")]
pub struct StdWriter;

#[cfg(feature = "hosted")]
impl UnifiedWriter for StdWriter {
    fn write_str(&mut self, s: &str) {
        use std::io::Write;
        let _ = std::io::stdout().write_all(s.as_bytes());
    }

    fn write_bytes(&mut self, bytes: &[u8]) {
        use std::io::Write;
        let _ = std::io::stdout().write_all(bytes);
    }

    fn flush(&mut self) {
        use std::io::Write;
        let _ = std::io::stdout().flush();
    }
}

/// Callback-based writer for embedded (no_std) platforms
/// User provides closures for write and flush operations
///
#[cfg(not(feature = "hosted"))]
pub struct CallbackWriter<W, F>
where
    W: FnMut(&[u8]),
    F: FnMut(),
{
    write_fn: W,
    flush_fn: F,
}

#[cfg(not(feature = "hosted"))]
impl<W, F> CallbackWriter<W, F>
where
    W: FnMut(&[u8]),
    F: FnMut(),
{
    pub fn new(write_fn: W, flush_fn: F) -> Self {
        Self { write_fn, flush_fn }
    }
}

#[cfg(not(feature = "hosted"))]
impl<W, F> UnifiedWriter for CallbackWriter<W, F>
where
    W: FnMut(&[u8]),
    F: FnMut(),
{
    fn write_str(&mut self, s: &str) {
        (self.write_fn)(s.as_bytes());
    }

    fn write_bytes(&mut self, bytes: &[u8]) {
        (self.write_fn)(bytes);
    }

    fn flush(&mut self) {
        (self.flush_fn)();
    }
}

/// DisplayRenderer: handles terminal output
/// Generic over the writer type to support both std and no_std environments
///
pub struct DisplayRenderer<W: UnifiedWriter> {
    writer: W,
}

impl<W: UnifiedWriter> DisplayRenderer<W> {
    /// Create a new DisplayRenderer with the given writer
    pub fn new(writer: W) -> Self {
        Self { writer }
    }

    /// Provides mutable access to the underlying writer
    ///
    /// This allows direct writing without intermediate string allocation,
    /// which is more memory efficient especially in embedded environments.
    ///
    /// # Example
    /// ```
    /// let writer = renderer.writer_mut();
    /// writer.write_str("Direct output");
    /// writer.flush();
    /// ```
    pub fn writer_mut(&mut self) -> &mut W {
        &mut self.writer
    }

    /// Renders the prompt and input content to the terminal.
    ///
    /// - Clears the current line.
    /// - Prints the prompt followed by the content.
    /// - Moves the cursor to the correct position based on `cursor_pos`.
    /// - Ensures cursor position does not exceed content length.
    /// - Flushes output to apply changes immediately.
    ///
    pub fn render(&mut self, prompt: &str, content: &str, cursor_pos: usize) {
        let safe_cursor_pos = cursor_pos.min(content.len());

        // Clear line and write prompt + content
        self.writer.write_str("\r\x1B[K");
        self.writer.write_str(prompt);
        self.writer.write_str(content);

        // Position cursor
        let cursor_position = prompt.len() + safe_cursor_pos + 1;
        self.write_cursor_position(cursor_position);

        self.writer.flush();
    }

    /// Helper to write cursor position escape sequence
    ///
    fn write_cursor_position(&mut self, position: usize) {
        use core::fmt::Write as FmtWrite;
        let mut buf = heapless::String::<16>::new();
        let _ = write!(&mut buf, "\x1B[{}G", position);
        self.writer.write_str(buf.as_str());
    }

    /// Emits an audible bell sound in the terminal.
    ///
    /// - Useful for signaling invalid actions (e.g., backspace at start of buffer).
    /// - Flushes output to ensure the bell is triggered immediately.
    ///
    pub fn bell(&mut self) {
        self.writer.write_bytes(b"\x07");
        self.writer.flush();
    }

    /// Prints a red boundary marker in the terminal.
    ///
    /// - Displays a red pipe character.
    /// - Moves the cursor back two positions.
    /// - Flushes output to apply changes immediately.
    /// - Can be used to visually separate sections or indicate limits.
    ///
    pub fn boundary_marker(&mut self) {
        self.writer.write_str("\x1B[31m|\x1B[0m\x1B[1D \x1B[1D");
        self.writer.flush();
    }
}

// Convenience type aliases
#[cfg(feature = "hosted")]
pub type StdDisplayRenderer = DisplayRenderer<StdWriter>;

#[cfg(not(feature = "hosted"))]
pub type CallbackDisplayRenderer<W, F> = DisplayRenderer<CallbackWriter<W, F>>;

// ==================== USAGE EXAMPLES =======================

/// Example usage for embedded systems with Embassy UART
///
/// ```no_run
/// // In your embedded code:
/// let mut uart = /* your embassy UART instance */;
///
/// let mut renderer = DisplayRenderer::new(CallbackWriter::new(
///     |bytes| {
///         // Write to UART
///         let _ = uart.blocking_write(bytes);
///     },
///     || {
///         // Flush UART (if needed)
///         let _ = uart.blocking_flush();
///     }
/// ));
///
/// renderer.render("> ", "Hello embedded!", 5);
/// ```

// ==================================================
// ==================== TESTS =======================
// ==================================================

#[cfg(test)]
mod tests {
    use super::*;
    use core::str;

    // Mock writer for testing
    struct MockWriter {
        buffer: heapless::Vec<u8, 256>,
    }

    impl MockWriter {
        fn new() -> Self {
            Self {
                buffer: heapless::Vec::new(),
            }
        }

        #[allow(dead_code)]
        fn as_str(&self) -> &str {
            core::str::from_utf8(&self.buffer).unwrap_or("")
        }
    }

    impl UnifiedWriter for MockWriter {
        fn write_str(&mut self, s: &str) {
            self.buffer.extend_from_slice(s.as_bytes()).ok();
        }

        fn write_bytes(&mut self, bytes: &[u8]) {
            self.buffer.extend_from_slice(bytes).ok();
        }

        fn flush(&mut self) {
            // No-op for mock
        }
    }

    #[test]
    fn test_render_clears_and_positions() {
        let mut renderer = DisplayRenderer::new(MockWriter::new());
        renderer.render(">", "Hello", 3);

        let output = renderer.writer.as_str();
        assert!(output.contains("\r\x1B[K")); // Clear line
        assert!(output.contains(">")); // Prompt
        assert!(output.contains("Hello")); // Content
    }

    #[test]
    fn test_bell() {
        let mut renderer = DisplayRenderer::new(MockWriter::new());
        renderer.bell();

        let output = renderer.writer.as_str();
        assert_eq!(output, "\x07");
    }

    #[test]
    fn test_boundary_marker() {
        let mut renderer = DisplayRenderer::new(MockWriter::new());
        renderer.boundary_marker();

        let output = renderer.writer.as_str();
        assert!(output.contains("\x1B[31m")); // Red color
        assert!(output.contains("|")); // Pipe character
        assert!(output.contains("\x1B[0m")); // Reset color
    }

    #[test]
    fn test_cursor_position_safety() {
        let mut renderer = DisplayRenderer::new(MockWriter::new());
        // Cursor position beyond content length should be clamped
        renderer.render(">", "Hi", 100);

        let output = renderer.writer.as_str();
        assert!(output.contains("Hi"));
    }

    #[cfg(not(feature = "hosted"))]
    #[test]
    fn test_callback_writer() {
        use heapless::Vec;
        let mut buffer: Vec<u8, 256> = Vec::new();
        let mut flush_called = false;

        {
            let mut renderer = DisplayRenderer::new(CallbackWriter::new(
                |bytes| {
                    buffer.extend_from_slice(bytes).ok();
                },
                || {
                    flush_called = true;
                },
            ));

            renderer.bell();
        }

        assert_eq!(buffer.as_slice(), b"\x07");
        assert!(flush_called);
    }
}
