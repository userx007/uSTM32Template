#![allow(clippy::unbuffered_bytes)]

use crate::heapless::{String, Vec};
/// InputParser is a generic, configurable command-line input handler designed for embedded or constrained environments. It supports:
/// - Autocompletion
/// - Input history
/// - Command parsing
/// - Special key handling (arrows, backspace, tab, etc.)
/// - Inline command help and shortcuts
///
/// It integrates with:
/// - Autocomplete
/// - History
/// - InputBuffer
/// - DisplayRenderer generic over Writer trait
///
///
use core::fmt::Write;
use core::iter::Iterator;
use core::option::Option::{self, None, Some};

use crate::autocomplete::Autocomplete;
use crate::history::History;
use crate::input::buffer::InputBuffer;
use crate::input::key_reader::Key;
use crate::input::renderer::{DisplayRenderer, UnifiedWriter};

// Import StdWriter for hosted builds
#[cfg(feature = "hosted")]
use crate::input::renderer::StdWriter;

/// # Type Parameters
/// - `W`: UnifiedWriter type for output (StdWriter for hosted, CallbackWriter for embedded)
/// - `NAC`: Number of Autocomplete Candidates.
/// - `FNL`: Function Name Length (for autocomplete)
/// - `IML`: Input Maximum Length (input buffer maximum length).
/// - `HTC`: History Total Capacity (number of entries).
///
/// # Fields
/// - `renderer`: DisplayRenderer instance for terminal output
/// - `shell_commands`: Static list of available shell commands and their descriptions.
/// - `shell_datatypes`: Description of supported argument types.
/// - `shell_shortcuts`: Description of available keyboard shortcuts.
/// - `autocomplete`: Autocomplete engine for input suggestions.
/// - `history`: Command history manager (heap-allocated or stack-based depending on feature flags).
/// - `buffer`: Input buffer for editing and cursor movement (heap-allocated or stack-based depending on feature flags).
/// - `prompt`: Static prompt string displayed to the user.
///
pub struct InputParser<
    'a,
    W: UnifiedWriter,
    const NAC: usize,
    const FNL: usize,
    const IML: usize,
    const HTC: usize,
> {
    renderer: DisplayRenderer<W>,
    shell_commands: &'static [(&'static str, &'static str)],
    shell_datatypes: &'static str,
    shell_shortcuts: &'static str,
    autocomplete: Autocomplete<'a, NAC, FNL>,

    #[cfg(feature = "heap-history")]
    history: Box<History<HTC>>,
    #[cfg(not(feature = "heap-history"))]
    history: History<HTC>,

    #[cfg(feature = "heap-input-buffer")]
    buffer: Box<InputBuffer<IML>>,
    #[cfg(not(feature = "heap-input-buffer"))]
    buffer: InputBuffer<IML>,

    prompt: &'static str,
}

impl<
        'a,
        W: UnifiedWriter,
        const NAC: usize,
        const FNL: usize,
        const IML: usize,
        const HTC: usize,
    > InputParser<'a, W, NAC, FNL, IML, HTC>
{
    /// Creates a new instance of `InputParser` with the provided shell configuration, writer, and prompt.
    ///
    /// # Parameters
    /// - `writer`: UnifiedWriter implementation for output (StdWriter for hosted, CallbackWriter for embedded)
    /// - `shell_commands`: A static list of command names and their descriptions.
    /// - `shell_datatypes`: A static string describing supported argument types.
    /// - `shell_shortcuts`: A static string listing available keyboard shortcuts.
    /// - `prompt`: The prompt string displayed to the user during input.
    ///
    /// # Behavior
    /// - Initializes autocomplete candidates from the command names.
    /// - Constructs the history and input buffer, using heap or stack allocation depending on feature flags.
    /// - Creates a DisplayRenderer with the provided writer.
    ///
    pub fn new(
        writer: W,
        shell_commands: &'static [(&'static str, &'static str)],
        shell_datatypes: &'static str,
        shell_shortcuts: &'static str,
        prompt: &'static str,
    ) -> Self {
        let mut candidates = Vec::<&'a str, NAC>::new();
        for &(first, _) in shell_commands {
            candidates.push(first).unwrap();
        }

        #[cfg(feature = "heap-history")]
        let history = Box::new(History::<HTC>::new());
        #[cfg(not(feature = "heap-history"))]
        let history = History::<HTC>::new();

        #[cfg(feature = "heap-input-buffer")]
        let buffer = Box::new(InputBuffer::<IML>::new());
        #[cfg(not(feature = "heap-input-buffer"))]
        let buffer = InputBuffer::<IML>::new();
        let mut renderer = DisplayRenderer::new(writer);

        let log_writer = renderer.writer_mut();
        log_writer.write_str("Shell started (try ###)\n\r");
        log_writer.write_str(prompt);

        Self {
            renderer,
            shell_commands,
            shell_datatypes,
            shell_shortcuts,
            autocomplete: Autocomplete::<'a, NAC, FNL>::new(candidates),
            history,
            buffer,
            prompt,
        }
    }

    /// Helper function: write a number directly to the writer without allocation
    fn write_number(writer: &mut W, mut num: usize) {
        let mut digits = [0u8; 20];
        let mut digit_count = 0;

        if num == 0 {
            writer.write_bytes(b"0");
            return;
        }

        while num > 0 {
            digits[digit_count] = (num % 10) as u8 + b'0';
            num /= 10;
            digit_count += 1;
        }

        // Write digits in reverse order
        for i in 0..digit_count {
            writer.write_bytes(&[digits[digit_count - 1 - i]]);
        }
    }

    fn buffer_to_autocomplete_input(&self) -> String<FNL> {
        let buf_str = self.buffer.to_string();
        buf_str.chars().take(FNL).collect()
    }

    fn render_buffer(&mut self) {
        let buf_str = self.buffer.to_string();
        let cursor_pos = self.buffer.cursor().min(self.buffer.len());
        self.renderer.render(self.prompt, &buf_str, cursor_pos);
    }

    /// Handles a single character input from the user.
    ///
    /// If the character is successfully inserted into the input buffer:
    /// - Updates the autocomplete engine with the first FNL characters.
    /// - Retrieves the current autocomplete suggestion.
    /// - If the suggestion differs from the input prefix, overwrites the buffer with the suggestion.
    ///
    /// If the character cannot be inserted (e.g., buffer full):
    /// - Displays a boundary marker.
    ///
    /// Finally, renders the updated buffer and prompt to the display.
    ///
    pub fn handle_char(&mut self, ch: char) {
        if self.buffer.insert(ch) {
            let input_full = self.buffer.to_string();
            let autocomplete_input: String<FNL> = input_full.chars().take(FNL).collect();

            // Clone for comparison before moving into update_input
            let input_prefix_clone = autocomplete_input.clone();

            self.autocomplete.update_input(autocomplete_input);
            let suggestion = self.autocomplete.current_input();

            if suggestion != input_prefix_clone.as_str() {
                let mut new_buf = String::<IML>::new();
                let _ = new_buf.push_str(suggestion);

                for c in input_full.chars().skip(FNL) {
                    let _ = new_buf.push(c);
                }
                self.buffer.overwrite(&new_buf);
            }
        } else {
            self.renderer.boundary_marker();
        }

        self.render_buffer();
    }

    /// Handles the backspace key event within the input buffer.
    ///
    /// If a character is successfully removed from the buffer:
    /// - Converts the buffer to a string.
    /// - Extracts up to `FNL` characters from the input.
    /// - Updates the autocomplete system with the truncated input.
    ///
    /// If no character can be removed (e.g., buffer is empty), triggers a bell sound.
    ///
    /// Finally, re-renders the prompt and buffer display to reflect the current state.
    ///
    pub fn handle_backspace(&mut self) {
        if self.buffer.backspace() {
            let autocomplete_input = self.buffer_to_autocomplete_input();
            self.autocomplete.update_input(autocomplete_input);
        } else {
            self.renderer.bell();
        }

        self.render_buffer();
    }

    /// Handles the tab key event to cycle through autocomplete suggestions.
    ///
    /// If `reverse` is `true`, triggers reverse cycling (Shift+Tab); otherwise, cycles forward.
    ///
    /// Updates the input buffer with the current autocomplete suggestion:
    /// - Takes up to `FNL` characters from the suggestion.
    /// - Appends the remainder of the original input (after `FNL`).
    ///
    /// Overwrites the buffer with the new input and re-renders the prompt and buffer display.
    ///
    pub fn handle_tab(&mut self, reverse: bool) {
        if reverse {
            self.autocomplete.cycle_backward();
        } else {
            self.autocomplete.cycle_forward();
        }

        let suggestion = self.autocomplete.current_input();
        let input_full = self.buffer.to_string();
        let mut new_buf = String::<IML>::new();
        let _ = new_buf.push_str(suggestion);

        for c in input_full.chars().skip(FNL) {
            let _ = new_buf.push(c);
        }

        self.buffer.overwrite(&new_buf);
        self.render_buffer();
    }

    /// Handles the up arrow key event to navigate backward through command history.
    ///
    /// - Retrieves the previous command from history.
    /// - Overwrites the input buffer with the retrieved command.
    /// - Re-renders the prompt and buffer display to reflect the new input.
    ///
    pub fn handle_up(&mut self) {
        self.buffer.clear();
        let found = self
            .history
            .get_prev_entry(|byte| self.buffer.insert(byte as char));
        if !found {
            self.renderer.bell();
        }
        self.render_buffer();
    }

    /// Handles the down arrow key event to navigate forward through command history.
    ///
    /// - Retrieves the next command from history.
    /// - Overwrites the input buffer with the retrieved command (or clears it if at the end).
    /// - Re-renders the prompt and buffer display to reflect the new input.
    ///
    pub fn handle_down(&mut self) {
        self.buffer.clear();
        let found = self
            .history
            .get_next_entry(|byte| self.buffer.insert(byte as char));
        if !found {
            self.renderer.bell();
        }
        self.render_buffer();
    }

    /// Handles the left arrow key event to move the cursor one position to the left.
    ///
    /// - Moves the cursor left in the input buffer.
    /// - Re-renders the prompt and buffer display to reflect the new cursor position.
    ///
    pub fn handle_left(&mut self) {
        self.buffer.move_left();
        self.render_buffer();
    }

    /// Handles the right arrow key event to move the cursor one position to the right.
    ///
    /// - Moves the cursor right in the input buffer.
    /// - Re-renders the prompt and buffer display to reflect the new cursor position.
    ///
    pub fn handle_right(&mut self) {
        self.buffer.move_right();
        self.render_buffer();
    }

    /// Handles the home key event to move the cursor to the beginning of the line.
    ///
    /// - Moves the cursor to the start of the buffer.
    /// - Re-renders the prompt and buffer display to reflect the new cursor position.
    ///
    pub fn handle_home(&mut self) {
        self.buffer.move_home();
        self.render_buffer();
    }

    /// Handles the end key event to move the cursor to the end of the line.
    ///
    /// - Moves the cursor to the end of the buffer.
    /// - Re-renders the prompt and buffer display to reflect the new cursor position.
    ///
    pub fn handle_end(&mut self) {
        self.buffer.move_end();
        self.render_buffer();
    }

    /// Handles the delete key event to delete the character at the cursor position.
    ///
    /// - Deletes the character at the cursor (if any).
    /// - Re-renders the prompt and buffer display to reflect the updated buffer.
    ///
    pub fn handle_delete(&mut self) {
        self.buffer.delete();
        self.render_buffer();
    }

    /// Handles hashtag commands (e.g., #q, ##, #l, #c, #N).
    ///
    /// Returns:
    /// - A tuple of `(continue_running, maybe_history_command)`
    ///   - `continue_running`: `false` if the shell should exit (e.g., for "#q"), `true` otherwise.
    ///   - `maybe_history_command`: `Some(command)` if a history command was executed, `None` otherwise.
    ///
    /// Hashtag Commands:
    /// - `#q` - Quit/exit the shell.
    /// - `#` - List available commands.
    /// - `##` - List all (commands + shortcuts + arg types).
    /// - `#l` - Show command history.
    /// - `#c` - Clear command history.
    /// - `#N` - Execute command from history at index N.
    ///
    pub fn handle_hashtag(&mut self, stripped: &str) -> (bool, Option<String<IML>>) {
        let writer = self.renderer.writer_mut();
        match stripped {
            "q" => {
                return (false, None);
            }
            "" => {
                writer.write_str("Available commands:\n\r");
                for &(name, desc) in self.shell_commands {
                    writer.write_str("  ");
                    writer.write_str(name);
                    writer.write_str(": ");
                    writer.write_str(desc);
                    writer.write_str("\n\r");
                }
            }
            "#" => {
                writer.write_str("Available commands:\n\r");
                for &(name, desc) in self.shell_commands {
                    writer.write_str("  ");
                    writer.write_str(name);
                    writer.write_str(": ");
                    writer.write_str(desc);
                    writer.write_str("\n\r");
                }
                writer.write_str("\n\rArgument types:\n\r");
                writer.write_str(self.shell_datatypes);
                writer.write_str("\n\r\n\rShortcuts:\n\r");
                writer.write_str(self.shell_shortcuts);
                writer.write_str("\n\r");
            }
            "l" => {
                if self.history.is_empty() {
                    writer.write_str("History is empty.\n\r");
                } else {
                    // Iterate through history entries
                    for idx in 0..self.history.len() {
                        writer.write_str("[");
                        Self::write_number(writer, idx);
                        writer.write_str("] ");

                        // Stream the entry byte-by-byte
                        self.history.for_each_byte(idx, |byte| {
                            writer.write_bytes(&[byte]);
                            true
                        });

                        writer.write_str("\n\r");
                    }

                    // Write free space info
                    writer.write_str("Free: ");
                    Self::write_number(writer, self.history.get_free_space());
                    writer.write_str(" bytes\n\r");
                }
                writer.flush();
            }
            "c" => {
                self.history.clear();
                writer.write_str("History cleared.\n\r");
            }
            _ => {
                // Try to parse as a number for history command execution
                if let Ok(index) = stripped.parse::<usize>() {
                    self.buffer.clear();
                    if let Some(_len) = self
                        .history
                        .for_each_byte(index, |byte| self.buffer.insert(byte as char))
                    {
                        let cmd = self.buffer.to_string();
                        writer.write_str("Executing: ");
                        writer.write_str(cmd.as_ref());
                        writer.write_str("\n\r");
                        self.buffer.clear();
                        return (true, Some(cmd));
                    } else {
                        writer.write_str("Invalid history index.\n\r");
                    }
                } else {
                    writer.write_str("Unknown hashtag command.\n\r");
                }
            }
        }
        (true, None)
    }

    /// Clears the entire input buffer and resets autocomplete state.
    ///
    /// - Clears the buffer content.
    /// - Resets the autocomplete engine to an empty input.
    /// - Re-renders the prompt and empty buffer.
    ///
    pub fn handle_clear(&mut self) {
        self.buffer.clear();
        self.autocomplete.update_input(String::<FNL>::new());
        self.render_buffer();
    }

    /// Processes the current input when the Enter key is pressed.
    ///
    /// Behavior:
    /// - Commits the current buffer content to history (unless empty or starts with '#').
    /// - Clears the buffer.
    /// - Resets autocomplete state.
    /// - Returns the command string for execution.
    ///
    pub fn handle_enter(&mut self) -> String<IML> {
        let cmd = self.buffer.to_string();
        if !cmd.is_empty() && !cmd.starts_with('#') {
            self.history.push(cmd.as_str());
        }
        self.buffer.clear();
        self.autocomplete.update_input(String::<FNL>::new());
        cmd
    }

    // =============== NEW GENERIC API (works for both hosted and embedded) ===============

    /// Unified input parsing method that works for both hosted and embedded environments.
    ///
    /// This method provides a complete input processing loop with:
    /// - Full line editing capabilities (arrow keys, home/end, delete, etc.)
    /// - Autocompletion (Tab/Shift+Tab)
    /// - Command history (Up/Down arrows)
    /// - Hashtag command support (#q, ##, #h, #c, #N)
    /// - Command execution via the provided callback
    /// - Automatic history management
    ///
    /// # Parameters
    /// - `read_key_fn`: Closure that returns the next key (or None if no key available)
    /// - `write_output`: Closure for writing output strings (e.g., "\r\n")
    /// - `exec_command`: Closure for executing parsed commands
    ///
    /// # Returns
    /// - `true` if the shell should continue running
    /// - `false` if the shell should exit (user typed "#q")
    ///
    /// # Hashtag Commands
    /// Special commands starting with '#':
    /// - `#q` - Quit/exit the shell
    /// - `#` - List available commands
    /// - `##` - List all (commands + shortcuts + arg types)
    /// - `#l` - Show command history
    /// - `#c` - Clear command history
    /// - `#N` - Execute command from history at index N
    ///
    /// # Example (Embedded with UART)
    /// ```no_run
    /// let mut key_parser = AnsiKeyParser::new();
    /// let mut pending_key = None;
    ///
    /// loop {
    ///     let should_continue = parser.parse_input(
    ///         || pending_key.take(),
    ///         |s: &str| { uart_tx.blocking_write(s.as_bytes()); },
    ///         |cmd: &String<256>| {
    ///             // Your command execution logic
    ///             execute_command(cmd);
    ///         }
    ///     );
    ///     
    ///     if !should_continue {
    ///         break;
    ///     }
    ///     
    ///     // Read next byte and parse key
    ///     if let Ok(byte) = uart_rx.read() {
    ///         if let Some(key) = key_parser.parse_byte(byte) {
    ///             pending_key = Some(key);
    ///         }
    ///     }
    /// }
    /// ```
    ///
    /// # Example (Hosted/PC)
    /// ```no_run
    /// use crate::input::key_reader::platform::read_key;
    ///
    /// loop {
    ///     if !parser.parse_input(
    ///         || read_key().ok(),
    ///         |s| print!("{}", s),
    ///         |cmd| {
    ///             println!("Executing: {}", cmd);
    ///         }
    ///     ) {
    ///         println!("Goodbye!");
    ///         break;
    ///     }
    /// }
    /// ```
    ///
    pub fn parse_input<R, O, E>(
        &mut self,
        mut read_key_fn: R,
        mut write_output: O,
        exec_command: E,
    ) -> bool
    where
        R: FnMut() -> Option<Key>,
        O: FnMut(&str),
        E: Fn(&String<IML>),
    {
        if let Some(key) = read_key_fn() {
            match key {
                Key::Char(ch) => {
                    self.handle_char(ch);
                }
                Key::Backspace => {
                    self.handle_backspace();
                }
                Key::Enter => {
                    write_output("\r\n");
                    let cmd = self.handle_enter();

                    if !cmd.is_empty() {
                        // Handle hashtag commands
                        if let Some(stripped) = cmd.strip_prefix('#') {
                            let (continue_running, maybe_history_command) =
                                self.handle_hashtag(stripped);
                            if !continue_running {
                                let writer = self.renderer.writer_mut();
                                writer.write_str("Shell exited...\n\r");
                                return false;
                            }
                            if let Some(history_command) = maybe_history_command {
                                exec_command(&history_command);
                            }
                        } else {
                            // Regular command execution
                            exec_command(&cmd);
                        }
                    }
                    self.render_buffer();
                }
                Key::Tab => {
                    self.handle_tab(false);
                }
                Key::ShiftTab => {
                    self.handle_tab(true);
                }
                Key::ArrowUp => {
                    self.handle_up();
                }
                Key::ArrowDown => {
                    self.handle_down();
                }
                Key::ArrowLeft => {
                    self.handle_left();
                }
                Key::ArrowRight => {
                    self.handle_right();
                }
                Key::Home => {
                    self.handle_home();
                }
                Key::End => {
                    self.handle_end();
                }
                Key::Delete => {
                    self.handle_delete();
                }
                Key::CtrlU => {
                    // Delete from cursor to beginning of line
                    self.buffer.delete_to_start();
                    self.render_buffer();
                }
                Key::CtrlK => {
                    // Delete from cursor to end of line
                    self.buffer.delete_to_end();
                    self.render_buffer();
                }
                Key::CtrlD => {
                    if !self.buffer.is_empty() {
                        self.buffer.clear();
                        self.render_buffer();
                    }
                }
                // Ignore keys we don't handle
                Key::Insert | Key::PageUp | Key::PageDown => {
                    // Ignore these keys
                }
            }
        }
        true
    }
}
