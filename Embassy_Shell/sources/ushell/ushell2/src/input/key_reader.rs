use core::default::Default;
use core::option::Option::{self, None, Some};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Key {
    // Arrow keys
    ArrowUp,
    ArrowDown,
    ArrowLeft,
    ArrowRight,

    // Navigation keys
    Home,
    End,
    Insert,
    Delete,
    PageUp,
    PageDown,

    // Input / editing keys
    Enter,
    Backspace,
    Tab,
    ShiftTab,

    // Control sequences
    CtrlU,
    CtrlK,
    CtrlD,
    CtrlN,
    CtrlP,

    // Printable character
    Char(char),
}

/// ============= TRAIT-BASED INTERFACE FOR EMBEDDED =============
///
pub trait KeyReader {
    /// Read a single key, returning None if no key is available
    fn try_read_key(&mut self) -> Option<Key>;

    /// Read a single byte from input
    fn read_byte(&mut self) -> Option<u8>;
}

/// ============= HOSTED PLATFORMS (Windows/Unix) =============
///
#[cfg(all(feature = "hosted", windows))]
pub mod platform {
    use super::Key;

    #[cfg(feature = "hosted")]
    extern crate std;

    use std::io;
    use winapi::shared::minwindef::DWORD;
    use winapi::um::consoleapi::ReadConsoleInputW;
    use winapi::um::processenv::GetStdHandle;
    use winapi::um::winbase::STD_INPUT_HANDLE;
    use winapi::um::wincon::{INPUT_RECORD, KEY_EVENT};
    use winapi::um::wincontypes::KEY_EVENT_RECORD;

    const LEFT_CTRL_PRESSED: u32 = 0x0008;
    const RIGHT_CTRL_PRESSED: u32 = 0x0004;
    const SHIFT_PRESSED: u32 = 0x0010;

    pub fn read_key() -> io::Result<Key> {
        unsafe {
            let handle = GetStdHandle(STD_INPUT_HANDLE);
            if handle.is_null() {
                return Err(io::Error::new(io::ErrorKind::Other, "Invalid handle"));
            }

            let mut record: INPUT_RECORD = std::mem::zeroed();
            let mut read: DWORD = 0;

            loop {
                if ReadConsoleInputW(handle, &mut record, 1, &mut read) == 0 {
                    return Err(io::Error::last_os_error());
                }

                if record.EventType == KEY_EVENT {
                    let key_event: KEY_EVENT_RECORD = *record.Event.KeyEvent();
                    if key_event.bKeyDown == 0 {
                        continue;
                    }

                    let vkey = key_event.wVirtualKeyCode;
                    let c = *key_event.uChar.UnicodeChar() as u32;
                    let ctrl = (key_event.dwControlKeyState
                        & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))
                        != 0;
                    let shift = (key_event.dwControlKeyState & SHIFT_PRESSED) != 0;

                    if ctrl {
                        match vkey {
                            0x55 => return Ok(Key::CtrlU),
                            0x4B => return Ok(Key::CtrlK),
                            0x44 => return Ok(Key::CtrlD),
                            0x4E => return Ok(Key::CtrlN),
                            0x50 => return Ok(Key::CtrlP),
                            _ => {}
                        }
                    }

                    match vkey {
                        0x21 => return Ok(Key::PageUp),
                        0x22 => return Ok(Key::PageDown),
                        0x23 => return Ok(Key::End),
                        0x24 => return Ok(Key::Home),
                        0x25 => return Ok(Key::ArrowLeft),
                        0x26 => return Ok(Key::ArrowUp),
                        0x27 => return Ok(Key::ArrowRight),
                        0x28 => return Ok(Key::ArrowDown),
                        0x2E => return Ok(Key::Delete),
                        0x08 => return Ok(Key::Backspace),
                        0x09 => return Ok(if shift { Key::ShiftTab } else { Key::Tab }),
                        0x0D => return Ok(Key::Enter),
                        _ => {}
                    }

                    if c != 0 {
                        return Ok(Key::Char(std::char::from_u32(c).unwrap_or('\0')));
                    }
                }
            }
        }
    }
}

#[cfg(all(feature = "hosted", not(windows)))]
pub mod platform {
    use super::Key;

    #[cfg(feature = "hosted")]
    extern crate std;

    use std::io::{self, Read};

    pub fn read_key() -> io::Result<Key> {
        let stdin = io::stdin();
        let mut bytes = stdin.lock().bytes();

        while let Some(Ok(b)) = bytes.next() {
            match b {
                b'\x1B' => {
                    // Handle escape sequences
                    if let Some(Ok(b2)) = bytes.next() {
                        if b2 == b'[' {
                            if let Some(Ok(b3)) = bytes.next() {
                                return Ok(match b3 {
                                    b'A' => Key::ArrowUp,
                                    b'B' => Key::ArrowDown,
                                    b'C' => Key::ArrowRight,
                                    b'D' => Key::ArrowLeft,
                                    b'H' => Key::Home,
                                    b'F' => Key::End,
                                    b'Z' => Key::ShiftTab,
                                    b'1' | b'2' | b'3' | b'4' | b'5' | b'6' => {
                                        let _ = bytes.next();
                                        match b3 {
                                            b'1' => Key::Home,
                                            b'2' => Key::Insert,
                                            b'3' => Key::Delete,
                                            b'4' => Key::End,
                                            b'5' => Key::PageUp,
                                            b'6' => Key::PageDown,
                                            _ => Key::Char('~'),
                                        }
                                    }
                                    _ => Key::Char(b3 as char),
                                });
                            }
                        }
                    }
                }
                b'\x15' => return Ok(Key::CtrlU),
                b'\x0B' => return Ok(Key::CtrlK),
                b'\x04' => return Ok(Key::CtrlD),
                b'\x0E' => return Ok(Key::CtrlN),
                b'\x10' => return Ok(Key::CtrlP),
                b'\r' | b'\n' => return Ok(Key::Enter),
                b'\t' => return Ok(Key::Tab),
                b'\x7F' | b'\x08' => return Ok(Key::Backspace),
                c => return Ok(Key::Char(c as char)),
            }
        }

        Err(io::Error::new(io::ErrorKind::UnexpectedEof, "No input"))
    }
}

/// ============= EMBEDDED IMPLEMENTATION =============
///
#[cfg(not(feature = "hosted"))]
pub mod embedded {
    use super::Key;
    use heapless::Vec;

    /// Simple VT100/ANSI escape sequence parser for embedded
    pub struct AnsiKeyParser {
        escape_buffer: Vec<u8, 8>,
        in_escape: bool,
    }

    impl Default for AnsiKeyParser {
        fn default() -> Self {
            Self::new()
        }
    }

    impl AnsiKeyParser {
        pub const fn new() -> Self {
            Self {
                escape_buffer: Vec::new(),
                in_escape: false,
            }
        }

        /// Parse a single byte and return a Key if complete
        #[inline]
        pub fn parse_byte(&mut self, byte: u8) -> Option<Key> {
            match byte {
                // Escape sequence start
                0x1B => {
                    self.in_escape = true;
                    self.escape_buffer.clear();
                    let _ = self.escape_buffer.push(byte);
                    None
                }

                // If we're in an escape sequence
                _ if self.in_escape => {
                    let _ = self.escape_buffer.push(byte);
                    self.try_complete_escape()
                }

                // Control characters
                0x15 => Some(Key::CtrlU), // Ctrl+U
                0x0B => Some(Key::CtrlK), // Ctrl+K
                0x04 => Some(Key::CtrlD), // Ctrl+D
                0x0E => Some(Key::CtrlN), // Ctrl+N
                0x10 => Some(Key::CtrlP), // Ctrl+P
                b'\r' | b'\n' => Some(Key::Enter),
                b'\t' => Some(Key::Tab),
                0x7F | 0x08 => Some(Key::Backspace),

                // Printable characters
                c if (0x20..0x7F).contains(&c) => Some(Key::Char(c as char)),

                _ => None,
            }
        }

        #[inline]
        fn try_complete_escape(&mut self) -> Option<Key> {
            let buf = &self.escape_buffer[..];

            // Common VT100 sequences: ESC [ X
            if buf.len() >= 3 && buf[0] == 0x1B && buf[1] == b'[' {
                let result = match buf[2] {
                    b'A' => Some(Key::ArrowUp),
                    b'B' => Some(Key::ArrowDown),
                    b'C' => Some(Key::ArrowRight),
                    b'D' => Some(Key::ArrowLeft),
                    b'H' => Some(Key::Home),
                    b'F' => Some(Key::End),
                    b'Z' => Some(Key::ShiftTab),

                    // Extended sequences: ESC [ N ~
                    b'1' | b'2' | b'3' | b'4' | b'5' | b'6' => {
                        if buf.len() >= 4 && buf[3] == b'~' {
                            match buf[2] {
                                b'1' => Some(Key::Home),
                                b'2' => Some(Key::Insert),
                                b'3' => Some(Key::Delete),
                                b'4' => Some(Key::End),
                                b'5' => Some(Key::PageUp),
                                b'6' => Some(Key::PageDown),
                                _ => None,
                            }
                        } else {
                            None // Wait for more bytes
                        }
                    }
                    _ => Some(Key::Char(buf[2] as char)),
                };

                if result.is_some() {
                    self.in_escape = false;
                    self.escape_buffer.clear();
                }
                result
            } else if buf.len() >= 4 {
                // Escape sequence too long, reset
                self.in_escape = false;
                self.escape_buffer.clear();
                None
            } else {
                None // Wait for more bytes
            }
        }
    }
}

// =================================
// ============= TESTS =============
// =================================

#[cfg(test)]
mod tests {
    use super::*;
    use core::matches;

    #[test]
    fn test_key_enum_debug() {
        let key = Key::ArrowUp;
        assert_eq!(format!("{:?}", key), "ArrowUp");

        let char_key = Key::Char('a');
        assert_eq!(format!("{:?}", char_key), "Char('a')");
    }

    #[test]
    fn test_key_variants_exist() {
        let _keys = [
            Key::ArrowUp,
            Key::ArrowDown,
            Key::ArrowLeft,
            Key::ArrowRight,
            Key::Home,
            Key::End,
            Key::Insert,
            Key::Delete,
            Key::PageUp,
            Key::PageDown,
            Key::Enter,
            Key::Backspace,
            Key::Tab,
            Key::ShiftTab,
            Key::CtrlU,
            Key::CtrlK,
            Key::CtrlD,
            Key::CtrlN,
            Key::CtrlP,
            Key::Char('x'),
        ];
    }

    #[cfg(not(feature = "hosted"))]
    #[test]
    fn test_ansi_parser_simple_chars() {
        let mut parser = embedded::AnsiKeyParser::new();

        assert_eq!(parser.parse_byte(b'a'), Some(Key::Char('a')));
        assert_eq!(parser.parse_byte(b'Z'), Some(Key::Char('Z')));
        assert_eq!(parser.parse_byte(b'5'), Some(Key::Char('5')));
    }

    #[cfg(not(feature = "hosted"))]
    #[test]
    fn test_ansi_parser_control_keys() {
        let mut parser = embedded::AnsiKeyParser::new();

        assert_eq!(parser.parse_byte(0x15), Some(Key::CtrlU));
        assert_eq!(parser.parse_byte(0x0B), Some(Key::CtrlK));
        assert_eq!(parser.parse_byte(0x04), Some(Key::CtrlD));
        assert_eq!(parser.parse_byte(0x0E), Some(Key::CtrlN));
        assert_eq!(parser.parse_byte(0x10), Some(Key::CtrlP));
        assert_eq!(parser.parse_byte(b'\r'), Some(Key::Enter));
        assert_eq!(parser.parse_byte(b'\t'), Some(Key::Tab));
    }

    #[cfg(not(feature = "hosted"))]
    #[test]
    fn test_ansi_parser_arrow_keys() {
        let mut parser = embedded::AnsiKeyParser::new();

        // Arrow Up: ESC [ A
        assert_eq!(parser.parse_byte(0x1B), None);
        assert_eq!(parser.parse_byte(b'['), None);
        assert_eq!(parser.parse_byte(b'A'), Some(Key::ArrowUp));

        // Arrow Down: ESC [ B
        assert_eq!(parser.parse_byte(0x1B), None);
        assert_eq!(parser.parse_byte(b'['), None);
        assert_eq!(parser.parse_byte(b'B'), Some(Key::ArrowDown));
    }

    #[cfg(not(feature = "hosted"))]
    #[test]
    fn test_ansi_parser_delete_key() {
        let mut parser = embedded::AnsiKeyParser::new();

        // Delete: ESC [ 3 ~
        assert_eq!(parser.parse_byte(0x1B), None);
        assert_eq!(parser.parse_byte(b'['), None);
        assert_eq!(parser.parse_byte(b'3'), None);
        assert_eq!(parser.parse_byte(b'~'), Some(Key::Delete));
    }

    #[cfg(not(feature = "hosted"))]
    #[test]
    fn test_ansi_parser_end_key() {
        let mut parser = embedded::AnsiKeyParser::new();

        // End: ESC [ 4 ~
        assert_eq!(parser.parse_byte(0x1B), None);
        assert_eq!(parser.parse_byte(b'['), None);
        assert_eq!(parser.parse_byte(b'4'), None);
        assert_eq!(parser.parse_byte(b'~'), Some(Key::End));

        // End: ESC [ F (alternative sequence)
        assert_eq!(parser.parse_byte(0x1B), None);
        assert_eq!(parser.parse_byte(b'['), None);
        assert_eq!(parser.parse_byte(b'F'), Some(Key::End));
    }

    #[test]
    fn test_key_matching() {
        fn is_arrow_key(key: &Key) -> bool {
            matches!(
                key,
                Key::ArrowUp | Key::ArrowDown | Key::ArrowLeft | Key::ArrowRight
            )
        }

        assert!(is_arrow_key(&Key::ArrowUp));
        assert!(is_arrow_key(&Key::ArrowLeft));
        assert!(!is_arrow_key(&Key::Enter));
        assert!(!is_arrow_key(&Key::Char('a')));
    }
}
