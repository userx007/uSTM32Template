//! # Shortcut Dispatcher Macro
//!
//! This procedural macro generates a `no_std`-compatible command dispatcher module
//! based on a compact shortcut mapping file. It is designed for embedded or constrained
//! environments where heap allocation is limited or unavailable.
//!
//! ## Purpose
//! - Parses a shortcut mapping file at compile time.
//! - Registers shortcut keys mapped to function paths.
//! - Provides a dispatcher function that matches input strings to registered shortcuts
//!   and invokes the corresponding function.
//! - Includes helper functions to list all available shortcuts and check if a shortcut is supported.
//!
//! ## Macro Input Format
//!
//! ```rust
//! mod <module_name>;
//! error_buffer_size = <expression>;
//! path = "<file_path>"
//! ```
//!
//! - `mod <module_name>`: Name of the generated module.
//! - `error_buffer_size`: Maximum size of the error buffer (const expression).
//! - `path`: Path to the file containing shortcut mappings (relative to CARGO_MANIFEST_DIR).
//! - **Note**: No trailing semicolon after the path parameter.
//!
//! ## Generated API
//! - `dispatch<'a>(input: &'a str, error_buffer: &'a mut heapless::String<ERROR_BUFFER_SIZE>) -> Result<(), &'a str>`
//! - `is_supported_shortcut(input: &str) -> bool`
//! - `get_shortcuts() -> &'static str`

use proc_macro::TokenStream;
use quote::quote;
use syn::{
    Expr, Ident, LitStr, Token,
    parse::{Parse, ParseStream},
    parse_macro_input,
};

/// Struct to parse macro input in the format:
/// `mod <n>; error_buffer_size = <expr>; path = "<file_path>"`
struct ShortcutMacroInput {
    _mod_token: Token![mod],         // Token for the `mod` keyword
    mod_name: Ident,                 // Identifier for the module name
    _semi1: Token![;],               // Semicolon after module declaration
    _error_buffer_size_token: Ident, // Identifier for `error_buffer_size` keyword
    _eq_token: Token![=],            // Equals sign for error_buffer_size assignment
    error_buffer_size: Expr,         // Expression representing the error buffer size
    _semi2: Token![;],               // Semicolon after error_buffer_size declaration
    _path_token: Ident,              // Identifier for `path` keyword
    _eq_token2: Token![=],           // Equals sign for path assignment
    path: LitStr,                    // Literal string representing the file path
}

impl Parse for ShortcutMacroInput {
    fn parse(input: ParseStream) -> syn::Result<Self> {
        Ok(ShortcutMacroInput {
            _mod_token: input.parse()?,
            mod_name: input.parse()?,
            _semi1: input.parse()?,
            _error_buffer_size_token: input.parse()?,
            _eq_token: input.parse()?,
            error_buffer_size: input.parse()?,
            _semi2: input.parse()?,
            _path_token: input.parse()?,
            _eq_token2: input.parse()?,
            path: input.parse()?,
        })
    }
}

pub fn generate_shortcuts_dispatcher_from_file(input: TokenStream) -> TokenStream {
    let ShortcutMacroInput {
        mod_name,
        error_buffer_size,
        path,
        ..
    } = parse_macro_input!(input as ShortcutMacroInput);

    // Resolve path relative to the crate invoking the macro
    let manifest_dir = std::env::var("CARGO_MANIFEST_DIR").unwrap();
    let full_path = std::path::Path::new(&manifest_dir).join(path.value());

    let raw = std::fs::read_to_string(&full_path)
        .unwrap_or_else(|_| panic!("Failed to read shortcut file: {:?}", full_path));

    let mut match_arms = vec![];
    let mut prefixes = std::collections::HashSet::new();
    let mut shortcut_keys = vec![];
    let mut buffer = String::new();

    for line in raw.lines() {
        let line = line.trim();
        if line.is_empty() {
            continue;
        }
        buffer.push_str(line);
        if line.ends_with("},") {
            if let Some((prefix, rest)) = buffer.split_once(':') {
                let prefix = prefix.trim();
                prefixes.insert(prefix.to_string());

                for entry in rest.split(',') {
                    let entry = entry.trim().trim_matches('{').trim_matches('}').trim();
                    if entry.is_empty() {
                        continue;
                    }
                    if let Some((key, func)) = entry.split_once(':') {
                        let key = key.trim();
                        let func = func.trim();
                        if let Ok(path) = syn::parse_str::<syn::Path>(func) {
                            let full_key = format!("{}{}", prefix, key);
                            shortcut_keys.push(full_key.clone());
                            match_arms.push(quote! {
                                #full_key => {
                                    #path(param);
                                    Ok(())
                                },
                            });
                        } else {
                            panic!("Invalid function path: {}", func);
                        }
                    }
                }
            }
            buffer.clear();
        }
    }

    let supported_checks = prefixes.iter().map(|p| {
        quote! { c == #p }
    });

    let shortcut_list = shortcut_keys.join(" | ");
    let list_fn = quote! {
        pub fn get_shortcuts() -> &'static str {
            #shortcut_list
        }
    };

    let support_fn = quote! {
        pub fn is_supported_shortcut(input: &str) -> bool {
            let trimmed = input.trim();
            if trimmed.is_empty() {
                return false;
            }
            let c = &trimmed[0..1];
            #( #supported_checks )||*
        }
    };

    let dispatch_fn = quote! {
        pub fn dispatch<'a>(input: &'a str, error_buffer: &'a mut heapless::String<{ #error_buffer_size }>) -> Result<(), &'a str> {
            let trimmed = input.trim();
            let (key, param) = if trimmed.len() >= 2 {
                let key = &trimmed[..2];
                let param = trimmed[2..].trim();
                (key, param)
            } else {
                (trimmed, "")
            };
            match key {
                #( #match_arms )*
                _ => {
                    error_buffer.clear();
                    use core::fmt::Write;
                    let _ = write!(error_buffer, "Unknown shortcut: {}", key);
                    Err(error_buffer.as_str())
                },
            }
        }
    };

    let expanded = quote! {
        pub mod #mod_name {
            #dispatch_fn
            #support_fn
            #list_fn
        }
    };

    TokenStream::from(expanded)
}

// ==================================================
// ================= TESTS ==========================
// ==================================================

#[cfg(test)]
mod tests {
    use std::collections::HashMap;
    use std::sync::Mutex;

    // Error buffer size for tests - configurable
    const ERROR_BUFFER_SIZE: usize = 32;

    // Global state to track function calls in tests
    static CALL_LOG: Mutex<Option<HashMap<String, Vec<String>>>> = Mutex::new(None);

    fn ensure_log_initialized() {
        let mut log = CALL_LOG.lock().unwrap();
        if log.is_none() {
            *log = Some(HashMap::new());
        }
    }

    fn record_call(func_name: &str, param: &str) {
        ensure_log_initialized();
        let mut log = CALL_LOG.lock().unwrap();
        log.as_mut()
            .unwrap()
            .entry(func_name.to_string())
            .or_insert_with(Vec::new)
            .push(param.to_string());
    }

    fn get_calls(func_name: &str) -> Vec<String> {
        ensure_log_initialized();
        let log = CALL_LOG.lock().unwrap();
        log.as_ref()
            .unwrap()
            .get(func_name)
            .cloned()
            .unwrap_or_default()
    }

    fn clear_log() {
        ensure_log_initialized();
        let mut log = CALL_LOG.lock().unwrap();
        log.as_mut().unwrap().clear();
    }

    // Test handler functions
    mod test_handlers {
        use super::record_call;

        pub fn bang_plus(param: &str) {
            record_call("bang_plus", param);
        }
        pub fn bang_minus(param: &str) {
            record_call("bang_minus", param);
        }
        pub fn bang_hash(param: &str) {
            record_call("bang_hash", param);
        }

        pub fn plus_plus(param: &str) {
            record_call("plus_plus", param);
        }
        pub fn plus_minus(param: &str) {
            record_call("plus_minus", param);
        }
        pub fn plus_hash(param: &str) {
            record_call("plus_hash", param);
        }

        pub fn minus_plus(param: &str) {
            record_call("minus_plus", param);
        }
        pub fn minus_minus(param: &str) {
            record_call("minus_minus", param);
        }
        pub fn minus_hash(param: &str) {
            record_call("minus_hash", param);
        }

        pub fn hash_bang(param: &str) {
            record_call("hash_bang", param);
        }
        pub fn hash_plus(param: &str) {
            record_call("hash_plus", param);
        }
        pub fn hash_question(param: &str) {
            record_call("hash_question", param);
        }

        pub fn question_bang(param: &str) {
            record_call("question_bang", param);
        }
        pub fn question_plus(param: &str) {
            record_call("question_plus", param);
        }
        pub fn question_question(param: &str) {
            record_call("question_question", param);
        }
    }

    // Create a test shortcuts.txt file in the test directory
    const TEST_SHORTCUTS: &str = r#"!: { +: test_handlers::bang_plus, -: test_handlers::bang_minus, #: test_handlers::bang_hash },
+: { +: test_handlers::plus_plus, -: test_handlers::plus_minus, #: test_handlers::plus_hash },
-: { +: test_handlers::minus_plus, -: test_handlers::minus_minus, #: test_handlers::minus_hash },
#: { !: test_handlers::hash_bang, +: test_handlers::hash_plus, ?: test_handlers::hash_question },
?: { !: test_handlers::question_bang, +: test_handlers::question_plus, ?: test_handlers::question_question },
"#;

    // Write test shortcuts to a file before tests run
    fn setup_test_shortcuts() {
        use std::fs;
        use std::path::Path;

        let test_dir = Path::new(env!("CARGO_MANIFEST_DIR")).join("tests");
        fs::create_dir_all(&test_dir).unwrap();
        fs::write(test_dir.join("test_shortcuts.txt"), TEST_SHORTCUTS).unwrap();
    }

    // Use the macro to generate the dispatcher
    use crate::generate_shortcuts_dispatcher_from_file;

    macro_rules! define_shortcuts {
        (mod $mod_name:ident; error_buffer_size = $size:expr; path = $path:expr) => {
            generate_shortcuts_dispatcher_from_file(
                quote::quote! {
                    mod $mod_name;
                    error_buffer_size = $size;
                    path = $path
                }
                .into(),
            )
        };
    }

    // Generate the shortcuts module
    setup_test_shortcuts!();
    define_shortcuts! {
        mod shortcuts;
        error_buffer_size = ERROR_BUFFER_SIZE;
        path = "tests/test_shortcuts.txt"
    }

    #[test]
    fn test_is_supported_shortcut() {
        assert!(shortcuts::is_supported_shortcut("!"));
        assert!(shortcuts::is_supported_shortcut("+"));
        assert!(shortcuts::is_supported_shortcut("-"));
        assert!(shortcuts::is_supported_shortcut("#"));
        assert!(shortcuts::is_supported_shortcut("?"));
        assert!(!shortcuts::is_supported_shortcut("x"));
        assert!(!shortcuts::is_supported_shortcut(""));
    }

    #[test]
    fn test_get_shortcuts() {
        let shortcuts_str = shortcuts::get_shortcuts();
        assert!(shortcuts_str.contains("!+"));
        assert!(shortcuts_str.contains("!-"));
        assert!(shortcuts_str.contains("!#"));
        assert!(shortcuts_str.contains("++"));
        assert!(shortcuts_str.contains("+-"));
        assert!(shortcuts_str.contains("+#"));
        assert!(shortcuts_str.contains("-+"));
        assert!(shortcuts_str.contains("--"));
        assert!(shortcuts_str.contains("-#"));
        assert!(shortcuts_str.contains("#!"));
        assert!(shortcuts_str.contains("#+"));
        assert!(shortcuts_str.contains("#?"));
        assert!(shortcuts_str.contains("?!"));
        assert!(shortcuts_str.contains("?+"));
        assert!(shortcuts_str.contains("??"));
    }

    #[test]
    fn test_invalid_shortcut() {
        let mut error_buffer = heapless::String::<ERROR_BUFFER_SIZE>::new();
        let result = shortcuts::dispatch("xx", &mut error_buffer);
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("Unknown shortcut"));
    }

    #[test]
    fn test_all_bang_shortcuts() {
        clear_log();
        let mut error_buffer = heapless::String::<ERROR_BUFFER_SIZE>::new();

        assert!(shortcuts::dispatch("!+", &mut error_buffer).is_ok());
        assert_eq!(get_calls("bang_plus").len(), 1);

        clear_log();
        assert!(shortcuts::dispatch("!-", &mut error_buffer).is_ok());
        assert_eq!(get_calls("bang_minus").len(), 1);

        clear_log();
        assert!(shortcuts::dispatch("!#", &mut error_buffer).is_ok());
        assert_eq!(get_calls("bang_hash").len(), 1);
    }

    #[test]
    fn test_all_plus_shortcuts() {
        clear_log();
        let mut error_buffer = heapless::String::<ERROR_BUFFER_SIZE>::new();

        assert!(shortcuts::dispatch("++", &mut error_buffer).is_ok());
        assert_eq!(get_calls("plus_plus").len(), 1);

        clear_log();
        assert!(shortcuts::dispatch("+-", &mut error_buffer).is_ok());
        assert_eq!(get_calls("plus_minus").len(), 1);

        clear_log();
        assert!(shortcuts::dispatch("+#", &mut error_buffer).is_ok());
        assert_eq!(get_calls("plus_hash").len(), 1);
    }

    #[test]
    fn test_all_minus_shortcuts() {
        clear_log();
        let mut error_buffer = heapless::String::<ERROR_BUFFER_SIZE>::new();

        assert!(shortcuts::dispatch("-+", &mut error_buffer).is_ok());
        assert_eq!(get_calls("minus_plus").len(), 1);

        clear_log();
        assert!(shortcuts::dispatch("--", &mut error_buffer).is_ok());
        assert_eq!(get_calls("minus_minus").len(), 1);

        clear_log();
        assert!(shortcuts::dispatch("-#", &mut error_buffer).is_ok());
        assert_eq!(get_calls("minus_hash").len(), 1);
    }

    #[test]
    fn test_all_hash_shortcuts() {
        clear_log();
        let mut error_buffer = heapless::String::<ERROR_BUFFER_SIZE>::new();

        assert!(shortcuts::dispatch("#!", &mut error_buffer).is_ok());
        assert_eq!(get_calls("hash_bang").len(), 1);

        clear_log();
        assert!(shortcuts::dispatch("#+", &mut error_buffer).is_ok());
        assert_eq!(get_calls("hash_plus").len(), 1);

        clear_log();
        assert!(shortcuts::dispatch("#?", &mut error_buffer).is_ok());
        assert_eq!(get_calls("hash_question").len(), 1);
    }

    #[test]
    fn test_all_question_shortcuts() {
        clear_log();
        let mut error_buffer = heapless::String::<ERROR_BUFFER_SIZE>::new();

        assert!(shortcuts::dispatch("?!", &mut error_buffer).is_ok());
        assert_eq!(get_calls("question_bang").len(), 1);

        clear_log();
        assert!(shortcuts::dispatch("?+", &mut error_buffer).is_ok());
        assert_eq!(get_calls("question_plus").len(), 1);

        clear_log();
        assert!(shortcuts::dispatch("??", &mut error_buffer).is_ok());
        assert_eq!(get_calls("question_question").len(), 1);
    }

    #[test]
    fn test_parameter_passing() {
        clear_log();
        let mut error_buffer = heapless::String::<ERROR_BUFFER_SIZE>::new();

        shortcuts::dispatch("!+ first", &mut error_buffer).unwrap();
        shortcuts::dispatch("!+ second", &mut error_buffer).unwrap();
        shortcuts::dispatch("!+ third", &mut error_buffer).unwrap();

        let calls = get_calls("bang_plus");
        assert_eq!(calls.len(), 3);
        assert_eq!(calls[0], "first");
        assert_eq!(calls[1], "second");
        assert_eq!(calls[2], "third");
    }

    #[test]
    fn test_hash_question_debug() {
        clear_log();
        let mut error_buffer = heapless::String::<ERROR_BUFFER_SIZE>::new();

        // Test the exact failing case
        let input = "#? /path/to/file";
        let result = shortcuts::dispatch(input, &mut error_buffer);

        // Check if dispatch succeeded
        assert!(result.is_ok(), "Dispatch failed with: {:?}", result);

        // Check if function was called
        let calls = get_calls("hash_question");
        assert!(!calls.is_empty(), "hash_question was not called at all");
        assert_eq!(
            calls.len(),
            1,
            "hash_question called {} times instead of 1",
            calls.len()
        );
        assert_eq!(
            calls[0], "/path/to/file",
            "Wrong parameter: got '{}' expected '/path/to/file'",
            calls[0]
        );
    }

    #[test]
    fn test_complex_parameters() {
        clear_log();
        let mut error_buffer = heapless::String::<ERROR_BUFFER_SIZE>::new();

        shortcuts::dispatch("++ key=value", &mut error_buffer).unwrap();
        assert_eq!(get_calls("plus_plus"), vec!["key=value"]);

        clear_log();
        shortcuts::dispatch("-- --flag", &mut error_buffer).unwrap();
        assert_eq!(get_calls("minus_minus"), vec!["--flag"]);

        clear_log();
        shortcuts::dispatch("#+ /path/to/file", &mut error_buffer).unwrap();
        assert_eq!(get_calls("hash_plus"), vec!["/path/to/file"]);

        clear_log();
        shortcuts::dispatch("?! 123 456 789", &mut error_buffer).unwrap();
        assert_eq!(get_calls("question_bang"), vec!["123 456 789"]);
    }

    #[test]
    fn test_special_characters_in_parameters() {
        clear_log();
        let mut error_buffer = heapless::String::<ERROR_BUFFER_SIZE>::new();

        shortcuts::dispatch("!+ @#$%", &mut error_buffer).unwrap();
        assert_eq!(get_calls("bang_plus"), vec!["@#$%"]);

        clear_log();
        shortcuts::dispatch("?? !@#$%^&*()", &mut error_buffer).unwrap();
        assert_eq!(get_calls("question_question"), vec!["!@#$%^&*()"]);

        clear_log();
        shortcuts::dispatch("+- hello!world?", &mut error_buffer).unwrap();
        assert_eq!(get_calls("plus_minus"), vec!["hello!world?"]);
    }

    #[test]
    fn test_error_message_format() {
        let mut error_buffer = heapless::String::<ERROR_BUFFER_SIZE>::new();
        let result = shortcuts::dispatch("xx", &mut error_buffer);
        assert!(result.is_err());
        let err = result.unwrap_err();
        assert!(err.contains("Unknown shortcut"));
        assert!(err.contains("xx"));
    }

    #[test]
    fn test_sequential_dispatch() {
        clear_log();
        let mut error_buffer = heapless::String::<ERROR_BUFFER_SIZE>::new();

        assert!(shortcuts::dispatch("!+ one", &mut error_buffer).is_ok());
        assert!(shortcuts::dispatch("++ two", &mut error_buffer).is_ok());
        assert!(shortcuts::dispatch("-- three", &mut error_buffer).is_ok());
        assert!(shortcuts::dispatch("#? four", &mut error_buffer).is_ok());

        assert_eq!(get_calls("bang_plus"), vec!["one"]);
        assert_eq!(get_calls("plus_plus"), vec!["two"]);
        assert_eq!(get_calls("minus_minus"), vec!["three"]);
        assert_eq!(get_calls("hash_question"), vec!["four"]);
    }

    #[test]
    fn test_unicode_parameters() {
        clear_log();
        let mut error_buffer = heapless::String::<ERROR_BUFFER_SIZE>::new();

        shortcuts::dispatch("!+ 你好", &mut error_buffer).unwrap();
        assert_eq!(get_calls("bang_plus"), vec!["你好"]);

        clear_log();
        shortcuts::dispatch("?? 🚀💻", &mut error_buffer).unwrap();
        assert_eq!(get_calls("question_question"), vec!["🚀💻"]);
    }

    #[test]
    fn test_empty_vs_no_parameter() {
        clear_log();
        let mut error_buffer = heapless::String::<ERROR_BUFFER_SIZE>::new();

        shortcuts::dispatch("!+", &mut error_buffer).unwrap();
        assert_eq!(get_calls("bang_plus"), vec![""]);

        clear_log();
        shortcuts::dispatch("!+   ", &mut error_buffer).unwrap();
        assert_eq!(get_calls("bang_plus"), vec![""]);
    }

    #[test]
    fn test_shortcut_boundary_cases() {
        let mut error_buffer = heapless::String::<ERROR_BUFFER_SIZE>::new();
        // Test exactly 2 characters
        assert!(shortcuts::dispatch("!+", &mut error_buffer).is_ok());

        // Test more than 2 characters (valid with param)
        assert!(shortcuts::dispatch("!+x", &mut error_buffer).is_ok());

        // Test 1 character (invalid)
        assert!(shortcuts::dispatch("!", &mut error_buffer).is_err());
    }
}
