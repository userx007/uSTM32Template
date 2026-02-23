use crate::heapless::{String, Vec};

/// Autocomplete struct for managing and filtering command candidates.
/// Optimized to only load candidates after the first character is entered.
///
/// - `'a`: Lifetime for string slices.
/// - `NAC`: Autocomplete Number of Candidates (can now be MAX_COMMANDS_PER_LETTER)
/// - `FNL`: Function Name Length
///
pub struct Autocomplete<'a, const NAC: usize, const FNL: usize> {
    /// Current candidates (subset based on first character).
    candidates: Vec<&'a str, NAC>,
    /// Filtered candidates matching the current input.
    filtered: Vec<&'a str, NAC>,
    /// Current user input.
    input: String<FNL>,
    /// Index for cycling through filtered candidates with Tab.
    tab_index: usize,
    /// Tracks the first character for which candidates were loaded.
    first_char_loaded: Option<char>,
}

impl<'a, const NAC: usize, const FNL: usize> Default for Autocomplete<'a, NAC, FNL> {
    fn default() -> Self {
        Self {
            candidates: Vec::new(),
            filtered: Vec::new(),
            input: String::new(),
            tab_index: 0,
            first_char_loaded: None,
        }
    }
}

impl<'a, const NAC: usize, const FNL: usize> Autocomplete<'a, NAC, FNL> {
    /// Creates a new empty Autocomplete instance.
    /// Candidates are loaded lazily when the first character is typed.
    ///
    pub fn new() -> Self {
        Self::default()
    }

    /// Updates the input string and filters candidates accordingly.
    ///
    /// The `get_candidates` closure is called with the first character of the input
    /// to retrieve the relevant subset of commands (e.g., all commands starting with 'a').
    ///
    /// - If input is empty, clears all candidates and returns.
    /// - If first character changes, reloads candidates for that character.
    /// - If no matches, keeps the input unchanged.
    /// - If only one match, auto-completes input with a trailing space.
    /// - If multiple matches, fills input with the longest common prefix.
    ///
    /// # Arguments
    /// * `new_input` - The new input string to filter against
    /// * `get_candidates` - Closure that returns all commands starting with a given character
    ///
    pub fn update_input<F>(&mut self, new_input: &str, get_candidates: F)
    where
        F: FnOnce(char) -> &'a [&'a str],
    {
        self.input.clear();
        let _ = self.input.push_str(new_input);
        self.filtered.clear();

        let input_str = self.input.as_str();

        // If input is empty, clear everything and return
        if input_str.is_empty() {
            self.candidates.clear();
            self.first_char_loaded = None;
            self.tab_index = 0;
            return;
        }

        // Get the first character
        let first_char = match input_str.chars().next() {
            Some(c) => c,
            None => {
                self.candidates.clear();
                self.first_char_loaded = None;
                self.tab_index = 0;
                return;
            }
        };

        // Reload candidates if first character changed
        if self.first_char_loaded != Some(first_char) {
            self.candidates.clear();
            let relevant_candidates = get_candidates(first_char);

            // Load candidates starting with this first character
            for &c in relevant_candidates {
                if self.candidates.push(c).is_err() {
                    break; // Stop if we exceed capacity
                }
            }

            self.first_char_loaded = Some(first_char);
        }

        // Filter candidates that match the full input prefix
        for &c in self.candidates.iter() {
            if c.starts_with(input_str) {
                let _ = self.filtered.push(c); // Ignore overflow
            }
        }

        // Apply auto-completion logic
        self.tab_index = 0;
        if self.filtered.len() == 1 {
            // Single match: auto-complete with trailing space
            self.input.clear();
            let _ = self.input.push_str(self.filtered[0]);
            let _ = self.input.push(' ');
        } else if self.filtered.len() > 1 {
            // Multiple matches: use longest common prefix
            self.input = Self::longest_common_prefix(&self.filtered);
        }
    }

    /// Cycles forward through filtered candidates and adds a trailing space.
    ///
    pub fn cycle_forward(&mut self) {
        if self.filtered.is_empty() {
            return;
        }
        self.tab_index = (self.tab_index + 1) % self.filtered.len();
        self.input.clear();
        let _ = self.input.push_str(self.filtered[self.tab_index]);
        let _ = self.input.push(' ');
    }

    /// Cycles backward through filtered candidates and adds a trailing space.
    ///
    pub fn cycle_backward(&mut self) {
        if self.filtered.is_empty() {
            return;
        }
        self.tab_index = if self.tab_index == 0 {
            self.filtered.len() - 1
        } else {
            self.tab_index - 1
        };
        self.input.clear();
        let _ = self.input.push_str(self.filtered[self.tab_index]);
        let _ = self.input.push(' ');
    }

    /// Returns the current input string.
    ///
    pub fn current_input(&self) -> &str {
        &self.input
    }

    /// Returns the filtered candidates (useful for displaying suggestions).
    ///
    pub fn filtered_candidates(&self) -> &[&'a str] {
        &self.filtered
    }

    /// Finds the longest common prefix among the filtered candidates.
    ///
    fn longest_common_prefix(strings: &[&str]) -> String<FNL> {
        if strings.is_empty() {
            return String::new();
        }
        let mut prefix = strings[0];
        for s in strings.iter().skip(1) {
            while !s.starts_with(prefix) {
                if prefix.is_empty() {
                    break;
                }
                prefix = &prefix[..prefix.len() - 1];
            }
        }
        let mut result = String::new();
        let _ = result.push_str(prefix); // Ignore overflow
        result
    }

    /// Resets the input, candidates, filtered list, and tab index.
    ///
    pub fn reset(&mut self) {
        self.input.clear();
        self.candidates.clear();
        self.filtered.clear();
        self.first_char_loaded = None;
        self.tab_index = 0;
    }
}

// ==================== TESTS =======================

#[cfg(test)]
mod tests {
    use super::*;
    use heapless::{String, Vec};

    // Now we can size NAC for the maximum commands per letter, not total commands!
    const NAC: usize = 4; // e.g., if max 4 commands start with any single letter
    const FNL: usize = 32;

    // Mock function registry organized by first character
    fn get_commands_for_char(c: char) -> &'static [&'static str] {
        match c {
            'a' => &["alpha", "alpine"],
            'b' => &["beta"],
            'g' => &["gamma", "gamut", "gambit"],
            'z' => &["zeta"],
            _ => &[],
        }
    }

    //----------------------------
    // Basic construction
    //----------------------------

    #[test]
    fn test_new() {
        let ac: Autocomplete<NAC, FNL> = Autocomplete::new();
        assert_eq!(ac.current_input(), "");
        assert_eq!(ac.filtered.len(), 0);
        assert_eq!(ac.candidates.len(), 0);
        assert_eq!(ac.tab_index, 0);
        assert_eq!(ac.first_char_loaded, None);
    }

    //----------------------------
    // Lazy loading behavior
    //----------------------------

    #[test]
    fn test_no_candidates_until_first_char() {
        let mut ac = Autocomplete::<NAC, FNL>::new();

        // Initially empty
        assert_eq!(ac.candidates.len(), 0);

        // Empty input should not load candidates
        let s: String<FNL> = String::new();
        ac.update_input(&s, get_commands_for_char);
        assert_eq!(ac.candidates.len(), 0);
    }

    #[test]
    fn test_loads_candidates_on_first_char() {
        let mut ac = Autocomplete::<NAC, FNL>::new();

        let mut s = String::<FNL>::new();
        s.push_str("a").unwrap();
        ac.update_input(&s, get_commands_for_char);

        // Should have loaded candidates for 'a'
        assert_eq!(ac.candidates.len(), 2); // "alpha", "alpine"
        assert_eq!(ac.first_char_loaded, Some('a'));
    }

    #[test]
    fn test_reloads_on_first_char_change() {
        let mut ac = Autocomplete::<NAC, FNL>::new();

        // Load 'a' commands
        let mut s = String::<FNL>::new();
        s.push_str("a").unwrap();
        ac.update_input(&s, get_commands_for_char);
        assert_eq!(ac.candidates.len(), 2);

        // Switch to 'g' commands
        s.clear();
        s.push_str("g").unwrap();
        ac.update_input(&s, get_commands_for_char);
        assert_eq!(ac.candidates.len(), 3); // "gamma", "gamut", "gambit"
        assert_eq!(ac.first_char_loaded, Some('g'));
    }

    #[test]
    fn test_doesnt_reload_on_same_first_char() {
        let mut ac = Autocomplete::<NAC, FNL>::new();

        // Load 'a' commands
        let mut s = String::<FNL>::new();
        s.push_str("a").unwrap();
        ac.update_input(&s, get_commands_for_char);
        let initial_count = ac.candidates.len();

        // Extend input but keep 'a' as first char
        s.clear();
        s.push_str("alp").unwrap();
        ac.update_input(&s, get_commands_for_char);

        // Candidates should not be reloaded (same count)
        assert_eq!(ac.candidates.len(), initial_count);
        assert_eq!(ac.first_char_loaded, Some('a'));
    }

    //----------------------------
    // Filtering behavior
    //----------------------------

    #[test]
    fn test_filter_multiple() {
        let mut ac = Autocomplete::<NAC, FNL>::new();
        let mut s: String<FNL> = String::new();
        s.push_str("ga").unwrap();

        ac.update_input(&s, get_commands_for_char);

        assert_eq!(ac.filtered.len(), 3); // gamma, gamut, gambit
        assert!(ac.filtered.contains(&"gamma"));
        assert!(ac.filtered.contains(&"gamut"));
        assert!(ac.filtered.contains(&"gambit"));
    }

    #[test]
    fn test_filter_none() {
        let mut ac = Autocomplete::<NAC, FNL>::new();

        let mut s = String::<FNL>::new();
        s.push_str("xyz").unwrap();
        ac.update_input(&s, get_commands_for_char);

        assert_eq!(ac.filtered.len(), 0);
        assert_eq!(ac.current_input(), "xyz");
    }

    #[test]
    fn test_partial_match_with_lcp() {
        let mut ac = Autocomplete::<NAC, FNL>::new();

        let mut s = String::<FNL>::new();
        s.push_str("alp").unwrap();
        ac.update_input(&s, get_commands_for_char);

        // "alpha" and "alpine" match → multiple, so LCP computed
        assert_eq!(ac.filtered.len(), 2);
        assert_eq!(ac.current_input(), "alp");
    }

    //----------------------------
    // Single-match auto-complete
    //----------------------------

    #[test]
    fn test_single_match_auto_complete() {
        let mut ac = Autocomplete::<NAC, FNL>::new();
        let mut s = String::<FNL>::new();
        s.push_str("bet").unwrap();

        ac.update_input(&s, get_commands_for_char);

        assert_eq!(ac.filtered.len(), 1);
        assert_eq!(ac.current_input(), "beta ");
    }

    //----------------------------
    // Longest Common Prefix Edge Cases
    //----------------------------

    #[test]
    fn test_lcp_no_common_prefix() {
        let strings = ["alpha", "beta", "gamma"];
        let result = Autocomplete::<NAC, FNL>::longest_common_prefix(&strings);
        assert_eq!(result, "");
    }

    #[test]
    fn test_lcp_entire_word_common() {
        let strings = ["test", "testing", "tester"];
        let result = Autocomplete::<NAC, FNL>::longest_common_prefix(&strings);
        assert_eq!(result, "test");
    }

    #[test]
    fn test_lcp_one_string() {
        let strings = ["hello"];
        let result = Autocomplete::<NAC, FNL>::longest_common_prefix(&strings);
        assert_eq!(result, "hello");
    }

    //----------------------------
    // Cycling behavior
    //----------------------------

    #[test]
    fn test_cycle_forward_wrap() {
        let mut ac = Autocomplete::<NAC, FNL>::new();

        let mut s = String::<FNL>::new();
        s.push_str("ga").unwrap();
        ac.update_input(&s, get_commands_for_char);

        ac.cycle_forward(); // index 1
        ac.cycle_forward(); // index 2
        ac.cycle_forward(); // wrap → index 0

        assert_eq!(ac.current_input(), "gamma ");
    }

    #[test]
    fn test_cycle_backward_wrap() {
        let mut ac = Autocomplete::<NAC, FNL>::new();

        let mut s = String::<FNL>::new();
        s.push_str("ga").unwrap();
        ac.update_input(&s, get_commands_for_char);

        ac.cycle_backward(); // wrap to last
        assert_eq!(ac.current_input(), "gambit ");
    }

    #[test]
    fn test_cycle_no_filtered_candidates() {
        let mut ac = Autocomplete::<NAC, FNL>::new();
        ac.cycle_forward(); // should not panic
        ac.cycle_backward(); // should not panic
        assert_eq!(ac.current_input(), "");
    }

    //----------------------------
    // Reset behavior
    //----------------------------

    #[test]
    fn test_reset() {
        let mut ac = Autocomplete::<NAC, FNL>::new();
        let mut s = String::<FNL>::new();

        s.push_str("alp").unwrap();
        ac.update_input(&s, get_commands_for_char);

        ac.reset();
        assert_eq!(ac.current_input(), "");
        assert_eq!(ac.filtered.len(), 0);
        assert_eq!(ac.candidates.len(), 0);
        assert_eq!(ac.first_char_loaded, None);
        assert_eq!(ac.tab_index, 0);
    }

    //----------------------------
    // Memory efficiency verification
    //----------------------------

    #[test]
    fn test_memory_efficiency() {
        // This test demonstrates that NAC can be small (4) instead of total commands (7+)
        let mut ac = Autocomplete::<4, FNL>::new(); // Only need 4, not 7!

        let mut s = String::<FNL>::new();
        s.push_str("g").unwrap();
        ac.update_input(&s, get_commands_for_char);

        // All 'g' commands loaded (3 of them)
        assert_eq!(ac.candidates.len(), 3);
        assert_eq!(ac.filtered.len(), 3);
    }

    //----------------------------
    // Edge cases
    //----------------------------

    #[test]
    fn test_empty_input_clears_state() {
        let mut ac = Autocomplete::<NAC, FNL>::new();

        let mut s = String::<FNL>::new();
        s.push_str("a").unwrap();
        ac.update_input(&s, get_commands_for_char);
        assert!(ac.candidates.len() > 0);

        s.clear();
        ac.update_input(&s, get_commands_for_char);

        assert_eq!(ac.candidates.len(), 0);
        assert_eq!(ac.filtered.len(), 0);
        assert_eq!(ac.first_char_loaded, None);
    }

    #[test]
    fn test_unknown_first_char() {
        let mut ac = Autocomplete::<NAC, FNL>::new();

        let mut s = String::<FNL>::new();
        s.push_str("x").unwrap();
        ac.update_input(&s, get_commands_for_char);

        assert_eq!(ac.candidates.len(), 0);
        assert_eq!(ac.filtered.len(), 0);
        assert_eq!(ac.current_input(), "x");
    }

    //----------------------------
    // Fuzz-like deterministic randomized test
    //----------------------------

    #[test]
    fn test_fuzz_random_sequences() {
        let mut ac = Autocomplete::<NAC, FNL>::new();

        let test_inputs = [
            "a", "al", "alp", "alpi", "g", "ga", "gam", "gamb", "z", "ze", "zet", "zeta",
        ];

        for inp in test_inputs {
            let mut s = String::<FNL>::new();
            s.push_str(inp).unwrap();
            ac.update_input(&s, get_commands_for_char);

            // Strong invariants:
            // 1. filtered only contains candidates that start with input prefix
            let prefix = inp;
            for f in ac.filtered.iter() {
                assert!(f.starts_with(prefix));
            }

            // 2. tab_index always valid
            if ac.filtered.len() > 0 {
                assert!(ac.tab_index < ac.filtered.len());
            } else {
                assert_eq!(ac.tab_index, 0);
            }

            // 3. If candidates loaded, first_char_loaded matches input
            if !inp.is_empty() {
                assert_eq!(ac.first_char_loaded, inp.chars().next());
            }
        }
    }
}
