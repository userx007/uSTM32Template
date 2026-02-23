#[cfg(feature = "history-persistence")]
extern crate std;

#[cfg(feature = "history-persistence")]
const HISTORY_FILENAME: &str = ".hist";

#[cfg(feature = "history-persistence")]
use std::fmt::Write;

use crate::heapless::String;
use core::default::Default;

const METADATA_SIZE: usize = 4; // 2 bytes leading + 2 bytes trailing length

/// A fixed-size, circular history buffer for storing strings.
///
/// Uses embedded metadata design
/// - Each entry: [len_hi][len_lo][data...][len_hi][len_lo]
/// - METADATA_SIZE = 4 bytes (2 for leading length, 2 for trailing length)
/// - Enables bidirectional traversal
/// - Single circular buffer, no separate metadata array
///
/// Generic parameters:
/// - `HTC`: History Total Capacity (bytes in buffer)
///
pub struct History<const HTC: usize> {
    /// Circular buffer containing all history entries with embedded metadata
    data: [u8; HTC],
    /// Next write position (head)
    data_head: usize,
    /// Oldest entry position (tail)
    entry_oldest: usize,
    /// Number of entries currently stored
    entry_size: usize,
    /// Current navigation index (for up/down arrow keys)
    current_index: usize,
}

/// Default
///
impl<const HTC: usize> Default for History<HTC> {
    /// Returns a new, empty history buffer.
    fn default() -> Self {
        Self::new()
    }
}

/// Implement History
///
impl<const HTC: usize> History<HTC> {
    /// Creates a new, empty history buffer.
    pub fn new() -> Self {
        let instance = Self {
            data: [0; HTC],
            data_head: 0,
            entry_oldest: 0,
            entry_size: 0,
            current_index: 0,
        };
        #[cfg(feature = "history-persistence")]
        let instance = {
            let mut inst = instance;
            inst.load_from_file(HISTORY_FILENAME);
            inst
        };
        instance
    }

    /// Pushes a new string into the history.
    /// - Trims whitespace.
    /// - Rejects if entry is too large or a duplicate of any existing entry.
    /// - Removes oldest entries if needed to make space.
    /// - Returns `true` if the entry was added, `false` otherwise.
    ///
    pub fn push(&mut self, s: &str) -> bool {
        let trimmed = s.trim();
        let bytes = trimmed.as_bytes();
        let len = bytes.len();

        // Reject if empty or too large for u16 length field
        if len == 0 || len > 65535 {
            return false;
        }

        let needed = Self::entry_total_size(len as u16);

        // Check if entry can possibly fit in buffer
        if needed > HTC {
            return false;
        }

        // Check for duplicates in ENTIRE history
        // If found anywhere, reject the new entry
        if self.entry_size > 0 && self.is_duplicate(bytes, len) {
            return false;
        }

        // Remove oldest entries until we have enough space
        let mut used = self.calculate_used_space();
        while self.entry_size > 0 && (HTC - used) < needed {
            let oldest_len = self.read_length_at(self.entry_oldest);
            let oldest_size = Self::entry_total_size(oldest_len);

            self.remove_oldest_entry();
            used -= oldest_size;
        }

        // Double-check we have space
        if (HTC - used) < needed {
            return false;
        }

        // Write entry with embedded metadata: [len_hi][len_lo][data...][len_hi][len_lo]
        let mut write_pos = self.data_head;

        // Write leading length (2 bytes, big-endian)
        self.write_length_at(write_pos, len as u16);
        write_pos = (write_pos + 2) % HTC;

        // Write data
        for &byte in bytes {
            self.data[write_pos] = byte;
            write_pos = (write_pos + 1) % HTC;
        }

        // Write trailing length (2 bytes) - enables backward traversal
        self.write_length_at(write_pos, len as u16);
        write_pos = (write_pos + 2) % HTC;

        // Update head position and counts
        self.data_head = write_pos;
        self.entry_size += 1;
        self.current_index = self.entry_size - 1;

        #[cfg(feature = "history-persistence")]
        self.append_to_file(HISTORY_FILENAME, trimmed);

        true
    }

    /// Checks if the given bytes match any existing entry
    #[inline]
    fn is_duplicate(&self, bytes: &[u8], len: usize) -> bool {
        let mut pos = self.entry_oldest;

        for _ in 0..self.entry_size {
            let entry_len = self.read_length_at(pos);

            if entry_len as usize == len {
                // Lengths match, compare data
                let data_pos = (pos + 2) % HTC;

                let is_match = bytes
                    .iter()
                    .enumerate()
                    .all(|(j, &ch)| self.data[(data_pos + j) % HTC] == ch);

                if is_match {
                    return true; // Duplicate found
                }
            }

            pos = self.find_next_entry_pos(pos);
        }

        false
    }

    /// Moves to the previous entry position and calls the provided function with its data.
    /// Returns true if an entry was found, false if history is empty.
    ///
    /// # Parameters
    /// - `f`: Callback function that receives each byte of the entry. Return false to stop early.
    ///
    /// # Example
    /// ```
    /// let mut buffer = [0u8; 256];
    /// let mut len = 0;
    /// history.get_prev_entry(|byte| {
    ///     if len < buffer.len() {
    ///         buffer[len] = byte;
    ///         len += 1;
    ///         true
    ///     } else {
    ///         false // buffer full
    ///     }
    /// });
    /// ```
    pub fn get_prev_entry<F>(&mut self, f: F) -> bool
    where
        F: FnMut(u8) -> bool,
    {
        if self.entry_size == 0 {
            return false;
        }
        if self.current_index == 0 {
            self.current_index = self.entry_size - 1;
        } else {
            self.current_index -= 1;
        }
        self.for_each_byte(self.current_index, f).is_some()
    }

    /// Moves to the next entry position and calls the provided function with its data.
    /// Returns true if an entry was found, false if history is empty.
    ///
    /// # Parameters
    /// - `f`: Callback function that receives each byte of the entry. Return false to stop early.
    ///
    pub fn get_next_entry<F>(&mut self, f: F) -> bool
    where
        F: FnMut(u8) -> bool,
    {
        if self.entry_size == 0 {
            return false;
        }
        self.current_index = (self.current_index + 1) % self.entry_size;
        self.for_each_byte(self.current_index, f).is_some()
    }

    /// Sets the current index to the given value, if valid.
    ///
    pub fn set_index(&mut self, index: usize) {
        if index < self.entry_size {
            self.current_index = index;
        }
    }

    /// Returns `true` if the history is empty.
    ///
    #[inline]
    pub fn is_empty(&self) -> bool {
        self.entry_size == 0
    }

    /// Returns the number of entries currently stored.
    ///
    #[inline]
    pub fn len(&self) -> usize {
        self.entry_size
    }

    /// Returns the number of free bytes remaining in the buffer.
    ///
    pub fn get_free_space(&self) -> usize {
        HTC - self.calculate_used_space()
    }

    /// Gets an entry by index and writes it into the provided buffer.
    /// This is a zero-allocation alternative to `get()`.
    ///
    /// # Parameters
    /// - `index`: The entry index (0 = oldest, entry_size - 1 = newest)
    /// - `buffer`: Output buffer to write the entry into
    ///
    /// # Returns
    /// - `Some(actual_len)` if the entry exists, where actual_len is the true length of the entry.
    ///   If actual_len > buffer.len(), the data was truncated to fit.
    /// - `None` if the index is out of bounds
    ///
    /// # Example
    /// ```
    /// let mut buf = [0u8; 256];
    /// if let Some(len) = history.get_into_buffer(0, &mut buf) {
    ///     // Use buf[..len.min(buf.len())]
    /// }
    /// ```
    pub fn get_into_buffer(&self, index: usize, buffer: &mut [u8]) -> Option<usize> {
        if index >= self.entry_size {
            return None;
        }

        // Find the position of the entry at the given index
        let mut pos = self.entry_oldest;
        for _ in 0..index {
            pos = self.find_next_entry_pos(pos);
        }

        let actual_len = self.get_entry_at_pos_into_buffer(pos, buffer, buffer.len());
        Some(actual_len)
    }

    /// Calls a function with each byte of an entry without allocating.
    /// This is useful for streaming or character-by-character processing.
    ///
    /// # Parameters
    /// - `index`: The entry index
    /// - `f`: Closure called for each byte of the entry. Return `false` to stop iteration early.
    ///
    /// # Returns
    /// - `Some(actual_len)` if the entry exists, where actual_len is the total length
    /// - `None` if the index is out of bounds
    ///
    /// # Example
    /// ```
    /// // Count characters in an entry without allocating
    /// let mut count = 0;
    /// history.for_each_byte(0, |_byte| {
    ///     count += 1;
    ///     true // continue
    /// });
    /// ```
    pub fn for_each_byte<F>(&self, index: usize, mut f: F) -> Option<usize>
    where
        F: FnMut(u8) -> bool,
    {
        if index >= self.entry_size {
            return None;
        }

        // Find the position of the entry at the given index
        let mut pos = self.entry_oldest;
        for _ in 0..index {
            pos = self.find_next_entry_pos(pos);
        }

        let len = self.read_length_at(pos) as usize;
        let data_pos = (pos + 2) % HTC;

        for i in 0..len {
            let byte = self.data[(data_pos + i) % HTC];
            if !f(byte) {
                break; // User requested early termination
            }
        }

        Some(len)
    }

    /// Prints all entries and free space info using the provided writer.
    /// This is a zero-allocation method that writes directly to the output.
    ///
    /// # Parameters
    /// - `write_fn`: A closure that accepts formatted output using core::fmt::Arguments.
    ///   This allows zero-allocation formatting directly to the output.
    ///
    /// # Example
    /// ```
    /// // Direct byte-by-byte output to UART or similar
    /// history.show(|args| {
    ///     // Write formatted output directly
    ///     writer.write_fmt(args);
    /// });
    /// ```
    pub fn show<F>(&self, mut write_fn: F)
    where
        F: FnMut(core::fmt::Arguments),
    {
        if self.entry_size == 0 {
            write_fn(format_args!("History is empty.\n"));
            return;
        }

        // Iterate through entries manually without allocation
        let mut pos = self.entry_oldest;
        for idx in 0..self.entry_size {
            write_fn(format_args!("[{}] ", idx));

            // Stream the entry byte-by-byte
            self.for_each_byte_at_pos(pos, |byte| {
                write_fn(format_args!("{}", byte as char));
                true
            });

            write_fn(format_args!("\n"));
            pos = self.find_next_entry_pos(pos);
        }

        let free_bytes = self.get_free_space();
        write_fn(format_args!("Free: {} bytes\n", free_bytes));
    }

    /// Clears all entries from history.
    ///
    pub fn clear(&mut self) {
        self.data_head = 0;
        self.entry_oldest = 0;
        self.entry_size = 0;
        self.current_index = 0;
    }

    // ==================== PRIVATE HELPERS ====================

    /// Gets the entry at a specific position in the buffer by writing into the provided buffer.
    ///
    /// # Parameters
    /// - `pos`: Position in the circular buffer where the entry starts
    /// - `buffer`: Output buffer to write the entry data into
    /// - `buffer_len`: Maximum length to write (typically buffer.len())
    ///
    /// # Returns
    /// The actual length of the entry (before truncation). If return value > buffer_len,
    /// the data was truncated to fit in the buffer.
    ///
    fn get_entry_at_pos_into_buffer(
        &self,
        pos: usize,
        buffer: &mut [u8],
        buffer_len: usize,
    ) -> usize {
        let len = self.read_length_at(pos) as usize;
        let data_pos = (pos + 2) % HTC;

        let bytes_to_copy = len.min(buffer_len);
        for (i, byte) in buffer.iter_mut().enumerate().take(bytes_to_copy) {
            *byte = self.data[(data_pos + i) % HTC];
        }

        len // Return actual length (may be > bytes_to_copy if truncated)
    }

    /// Calls a function with each byte at a specific position in the buffer.
    /// Returns the total length of the entry.
    ///
    #[inline]
    fn for_each_byte_at_pos<F>(&self, pos: usize, mut f: F) -> usize
    where
        F: FnMut(u8) -> bool,
    {
        let len = self.read_length_at(pos) as usize;
        let data_pos = (pos + 2) % HTC;

        for i in 0..len {
            let byte = self.data[(data_pos + i) % HTC];
            if !f(byte) {
                break; // User requested early termination
            }
        }

        len
    }

    /// Reads a u16 length value (big-endian) at the given position.
    ///    
    #[inline]
    fn read_length_at(&self, pos: usize) -> u16 {
        let hi = self.data[pos] as u16;
        let lo = self.data[(pos + 1) % HTC] as u16;
        (hi << 8) | lo
    }

    /// Writes a u16 length value (big-endian) at the given position.
    ///    
    #[inline]
    fn write_length_at(&mut self, pos: usize, len: u16) {
        self.data[pos] = (len >> 8) as u8;
        self.data[(pos + 1) % HTC] = (len & 0xFF) as u8;
    }

    /// Returns the total size of an entry (data + metadata).
    ///    
    #[inline]
    const fn entry_total_size(data_len: u16) -> usize {
        data_len as usize + METADATA_SIZE
    }

    /// Removes the oldest entry from the buffer.
    ///    
    fn remove_oldest_entry(&mut self) {
        if self.entry_size == 0 {
            return;
        }

        let len = self.read_length_at(self.entry_oldest);
        let size = Self::entry_total_size(len);

        // Move oldest pointer forward
        self.entry_oldest = (self.entry_oldest + size) % HTC;
        self.entry_size -= 1;

        // If buffer is now empty, reset pointers
        if self.entry_size == 0 {
            self.data_head = 0;
            self.entry_oldest = 0;
            self.current_index = 0;
        } else if self.current_index >= self.entry_size {
            self.current_index = self.entry_size - 1;
        }
    }

    /// Calculates the total used space in the buffer.
    ///    
    fn calculate_used_space(&self) -> usize {
        if self.entry_size == 0 {
            return 0;
        }

        let mut total = 0;
        let mut pos = self.entry_oldest;

        for _ in 0..self.entry_size {
            let len = self.read_length_at(pos);
            total += Self::entry_total_size(len);
            pos = self.find_next_entry_pos(pos);
        }

        total
    }

    /// Finds the position of the next entry after the given position.
    ///    
    #[inline]
    fn find_next_entry_pos(&self, pos: usize) -> usize {
        let len = self.read_length_at(pos);
        let size = Self::entry_total_size(len);
        (pos + size) % HTC
    }

    #[cfg(feature = "history-persistence")]
    fn load_from_file(&mut self, filename: &str) {
        use std::fs::File;
        use std::io::{BufRead, BufReader};

        if let Ok(file) = File::open(filename) {
            let reader = BufReader::new(file);
            for line in reader.lines().flatten() {
                let trimmed = line.trim();
                if !trimmed.is_empty() {
                    self.push(trimmed);
                }
            }
        }
    }

    #[cfg(feature = "history-persistence")]
    fn append_to_file(&self, filename: &str, entry: &str) {
        use std::fs::OpenOptions;
        use std::io::Write as IoWrite;

        if let Ok(mut file) = OpenOptions::new().create(true).append(true).open(filename) {
            let _ = writeln!(file, "{}", entry);
        }
    }
}
