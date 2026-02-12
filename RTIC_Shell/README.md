# ASCII diagram showing the execution flow

1. **Complete Flow**: From key press through UART hardware → ISR → queue → shell task → parser → command execution → output back to terminal

2. **Example Walkthrough**: Shows what happens when user types "help" and presses Enter

3. **Key Components**:
   - Hardware interrupts (usart2_isr)
   - Async software task (shell_task)
   - Data queues (rx_queue, tx_buffer)
   - Shell processing (ShellCtx, InputParser)
   - Command dispatcher

4. **Data Flow**: Both RX (input) and TX (output) paths clearly shown

5. **Task Priorities**: How RTIC schedules different priority levels

6. **Critical Sections**: How RTIC prevents race conditions with resource locking

The diagram shows the complete journey of a keystroke from terminal all the way through parsing, command execution, and echo back!

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    KEY PRESS TO COMMAND EXECUTION FLOW                      │
└─────────────────────────────────────────────────────────────────────────────┘

  User Terminal                 STM32F411 Hardware              RTIC Tasks
  ─────────────                 ──────────────────              ──────────

       │
       │ User presses 'h'
       │ key on keyboard
       ├──────────────────────┐
       │                      │
       │                      ▼
       │                 ┌─────────┐
       │                 │ UART RX │  Hardware receives byte
       │                 │  0x68   │  (ASCII 'h' = 0x68)
       │                 └────┬────┘
       │                      │
       │                      │ UART RX interrupt fires
       │                      │
       │                      ▼
       │              ╔═══════════════════╗
       │              ║   usart2_isr      ║  Priority 3 (highest)
       │              ║   (Hardware ISR)  ║  Interrupt Handler
       │              ╚═══════════════════╝
       │                      │
       │                      ├─► uart_rx.read() → byte = 0x68
       │                      │
       │                      ├─► rx_queue.enqueue(0x68)
       │                      │   [Queue: 0x68]
       │                      │
       │                      ├─► shell_task::spawn().ok()
       │                      │   (Schedule async task)
       │                      │
       │                      └─► Return from ISR
       │                                          │
       │                                          ▼
       │                                  ╔═══════════════════╗
       │                                  ║   shell_task      ║  Priority 1
       │                                  ║   (Software Task) ║  Async Task
       │                                  ╚═══════════════════╝
       │                                          │
       │                                          ├─► Lock rx_queue
       │                                          │
       │                                          ├─► reader = RticQueueReader { queue }
       │                                          │
       │                                          ├─► while !reader.is_empty() {
       │                                          │
       │                                          ├───► shell.step(&mut reader)
       │                                          │
       │              ┌───────────────────────────┘
       │              │
       │              ▼
       │      ╔═══════════════════╗
       │      ║  ShellCtx::step   ║
       │      ╚═══════════════════╝
       │              │
       │              ├─► reader.read_byte() → Some(0x68)
       │              │   [Queue now empty]
       │              │
       │              ├─► key_parser.parse_byte(0x68)
       │              │   → Some(Key::Char('h'))
       │              │
       │              ├─► pending_key = Some(Key::Char('h'))
       │              │
       │              ├─► parser.parse_input(
       │              │       key_provider = || pending_key.take(),
       │              │       echo_fn = |s| uart_write_bytes(s.as_bytes()),
       │              │       executor = |input| { ... }
       │              │   )
       │              │
       │              ▼
       │      ╔═══════════════════════╗
       │      ║  InputParser          ║  (from ushell_input crate)
       │      ║  ::parse_input        ║
       │      ╚═══════════════════════╝
       │              │
       │              ├─► Get key: key_provider() → Key::Char('h')
       │              │
       │              ├─► Process key:
       │              │   - Add 'h' to input buffer
       │              │   - Update cursor position
       │              │   - Handle editing operations
       │              │
       │              ├─► Echo character back:
       │              │   echo_fn("h")
       │              │     │
       ├─────────────┘     └──► uart_write_bytes("h".as_bytes())
       │                            │
       ◄────────────────────────────┼──► tx_buffer.push_back(0x68)
       │  'h' echoed back           │
       │  to terminal               └──► uart_tx.listen()
       │                                  (Enable TX interrupt)
       │
       │                                          │
       │                                          │ TX interrupt fires
       │                                          │
       │                                          ▼
       │                                  ╔═══════════════════╗
       │                                  ║   usart2_isr      ║
       │                                  ║   (TX handling)   ║
       │                                  ╚═══════════════════╝
       │                                          │
       ◄────────────────────────────────────────┬─┘
       │  Byte transmitted                      │
       │  to terminal                           ├─► tx_buffer.pop_front() → 0x68
       │                                        │
       │                                        └─► uart_tx.write(0x68)
       │
       │
       │ User continues typing...
       │ "help" + Enter
       │
       │ [Same flow repeats for 'e', 'l', 'p', '\r']
       │
       │ When '\r' (Enter) is pressed:
       │
       │                      ╔═══════════════════════╗
       │                      ║  InputParser          ║
       │                      ║  ::parse_input        ║
       │                      ╚═══════════════════════╝
       │                              │
       │                              ├─► Key::Enter detected
       │                              │
       │                              ├─► Input complete: "help"
       │                              │
       │                              ├─► Call executor("help")
       │                              │
       │                              ▼
       │                      ╔═══════════════════════╗
       │                      ║  Command Executor     ║
       │                      ║  (closure in step)    ║
       │                      ╚═══════════════════════╝
       │                              │
       │                              ├─► Create error_buffer
       │                              │
       │                              ├─► Check: shortcuts::is_supported_shortcut("help")?
       │                              │   → false
       │                              │
       │                              ├─► commands::dispatch("help", &mut error_buffer)
       │                              │
       │                              ▼
       │                      ╔═══════════════════════╗
       │                      ║  commands::dispatch   ║  (Generated by macro)
       │                      ╚═══════════════════════╝
       │                              │
       │                              ├─► Parse command: "help"
       │                              │
       │                              ├─► Match command name
       │                              │
       │                              ├─► Call: uc::help_cmd()
       │                              │          (from ushell_usercode)
       │                              │
       │                              ├─► Returns: Ok(())
       │                              │
       │                              ▼
       │                      ╔═══════════════════════╗
       │                      ║  Back to executor     ║
       │                      ╚═══════════════════════╝
       │                              │
       │                              ├─► match result {
       │                              │       Ok(_) => log_info!("Success"),
       │                              │       Err(e) => log_error!("Error: {}", e)
       │                              │   }
       │                              │
       │                              ├─► log_info!("Success")
       ◄──────────────────────────────┘       │
       │  "Success" message                   ├──► uart_write_bytes(...)
       │  printed to terminal                 │
       │                                      └──► [Back through TX buffer/interrupt]
       │
       ▼


═══════════════════════════════════════════════════════════════════════════════
                              KEY COMPONENTS
═══════════════════════════════════════════════════════════════════════════════

┌─────────────────────┐
│   RX Flow (Input)   │
└─────────────────────┘
  UART RX → usart2_isr → rx_queue → shell_task → ShellCtx → InputParser
                                                                   │
                                                                   ▼
                                                            Command Executor

┌─────────────────────┐
│  TX Flow (Output)   │
└─────────────────────┘
  uart_write_bytes → tx_buffer → usart2_isr (TX) → UART TX → Terminal


═══════════════════════════════════════════════════════════════════════════════
                            DATA STRUCTURES
═══════════════════════════════════════════════════════════════════════════════

┌────────────────────────────────────────────────────────────────┐
│  rx_queue: Queue<u8, 128>                                      │
│  ┌──────┬──────┬──────┬──────┬─────────┐                       │
│  │ 0x68 │ 0x65 │ 0x6C │ 0x70 │   ...   │  Stores raw bytes     │
│  │ 'h'  │ 'e'  │ 'l'  │ 'p'  │         │  from UART RX         │
│  └──────┴──────┴──────┴──────┴─────────┘                       │
└────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────┐
│  tx_buffer: Deque<u8, 512>                                     │
│  ┌──────┬──────┬──────┬─────────┐                              │
│  │ 0x68 │ 0x0A │ ...  │         │  Stores bytes to send        │
│  │ 'h'  │ '\n' │      │         │  to UART TX                  │
│  └──────┴──────┴──────┴─────────┘                              │
└────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────┐
│  InputParser state:                                            │
│  ┌──────────────────────────────────────────────┐              │
│  │  input_buffer: "help"                        │              │
│  │  cursor_pos: 4                               │              │
│  │  history: ["ls", "status", ...]              │              │
│  └──────────────────────────────────────────────┘              │
└────────────────────────────────────────────────────────────────┘


═══════════════════════════════════════════════════════════════════════════════
                              TASK PRIORITIES
═══════════════════════════════════════════════════════════════════════════════

Priority 3 (Highest):  usart2_isr  - Hardware interrupt (fast!)
Priority 2:            led_blink   - Timer interrupt  
Priority 1 (Lowest):   shell_task  - Software task (can be preempted)


═══════════════════════════════════════════════════════════════════════════════
                           CRITICAL SECTIONS
═══════════════════════════════════════════════════════════════════════════════

When shell_task accesses shared resources:

    ctx.shared.rx_queue.lock(|rx_queue| {
        // ← Critical section: rx_queue locked
        // ← usart2_isr cannot access rx_queue here
        
        let mut reader = RticQueueReader { queue: rx_queue };
        while !reader.is_empty() {
            shell.step(&mut reader);
        }
        
        // ← Lock released when closure exits
    });

RTIC ensures no race conditions by:
- Preventing priority inversion
- Automatic resource locking
- Static analysis at compile time
```