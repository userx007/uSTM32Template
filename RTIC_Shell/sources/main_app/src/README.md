# RTIC Shell — Architecture Reference

---

## 1. Crate Dependency Graph

```
                        ┌─────────────────────────────────────────────┐
                        │              WORKSPACE ROOT                 │
                        │         (shared dep versions)               │
                        │  heapless · stm32f4xx-hal · rtic · ushell2  │
                        └─────────────────────────────────────────────┘
                                           │ (workspace deps)
              ┌────────────────────────────┼──────────────────────────┐
              ▼                            ▼                          ▼
   ┌─────────────────┐         ┌──────────────────┐       ┌──────────────────┐
   │    uart_hal     │         │   ushell_ctx     │       │   ushell2        │
   │─────────────────│         │──────────────────│       │──────────────────│
   │ write_bytes()   │◄────────│ ShellCtx         │       │ log_info!        │
   │ flush_noop()    │         │ ShellConfig      │       │ log_simple!      │
   │ handle_tx_ready │         │ DispatchFn       │       │ log_error!       │
   │ init_uart_glbls │         │ step()           │       │ init_logger()    │
   │ RxQueueReader   │         └────────┬─────────┘       │ LoggerConfig     │
   │ UartWriter      │                  │                 └──────────────────┘
   │ LOGGER_WRITER   │                  │ uses                      ▲
   │ RX_QUEUE_SIZE   │                  ▼                           │
   │ TX_BUFFER_SIZE  │       ┌──────────────────┐                   │
   │ UartTx / UartRx │       │  ushell_input    │                   │
   └────────┬────────┘       │──────────────────│                   │
            │                │ InputParser      │                   │
            │                │ AnsiKeyParser    │                   │
            │                │ CallbackWriter   │                   │
            │                │ Key              │                   │
            │                └──────────────────┘                   │
            │                                                       │
            └──────────────────────────┐                            │
                                       ▼                            │
                        ┌──────────────────────────────────────────┐│
                        │              main_app                    ││
                        │──────────────────────────────────────────││
                        │ #[app] RTIC application                  ││
                        │                                          ││
                        │ Shared: uart_tx, tx_buffer,              ││
                        │         rx_queue, shell_pending          ││
                        │                                          ││
                        │ Local:  uart_rx, led,                    ││
                        │         blink_timer, shell               ││
                        │                                          ││
                        │ Tasks:  init, usart2_isr,                ││
                        │         led_blink, shell_task, idle      ││
                        └──────────────────────────────────────────┘│
                                       ▲                            │
                                       │ uses macros                │
                        ┌───────────────────────────────┐           │
                        │   ushell_dispatcher           │           │
                        │───────────────────────────────│           │
                        │ generate_commands_dispatcher! │           │
                        │ generate_shortcuts_dispatcher!│           │
                        │              │                │           │                   └───────────────────────────────┘           │
                                       ▲                            │
                                       │ generates                  │
                        ┌───────────────────────────┐               │
                        │   ushell_usercode         │───────────────┘
                        │───────────────────────────│  (uses log macros)
                        │ commands.cfg              │
                        │ shortcuts.cfg             │
                        │ command implementations   │
                        └───────────────────────────┘
```

---

## 2. RTIC Task Priority & Interrupt Map

```
  Priority 3 (highest)    Priority 2              Priority 1          Priority 0
  ┌──────────────────┐   ┌──────────────────┐   ┌──────────────┐   ┌──────────┐
  │   usart2_isr     │   │   led_blink      │   │  shell_task  │   │   idle   │
  │  (binds=USART2)  │   │  (binds=TIM2)    │   │  (async sw)  │   │          │
  └──────────────────┘   └──────────────────┘   └──────────────┘   └──────────┘
   Can preempt P1,P0      Can preempt P1,P0       Can preempt P0     Never
                                                                     preempted
  Shared access:          Shared access:          Shared access:
  uart_tx (lock)          — (local only) —        uart_tx (lock)
  tx_buffer (lock)                                tx_buffer (lock)
  rx_queue (lock)                                 rx_queue (lock)
  shell_pending (lock)                            shell_pending (lock)
```

---

## 3. Initialisation Call Flow

```
  main_app boots
       │
       ▼
  ┌─────────────────────────────────────────────────────┐
  │  init()                                             │
  │                                                     │
  │  1. RCC constrain → freeze clocks (100 MHz)         │
  │  2. GPIOC split  → PC13 push-pull output (LED)      │
  │  3. GPIOA split  → PA2/PA3 alternate (UART pins)    │
  │  4. Serial::new(USART2, ...)  → serial              │
  │  5. serial.split() → (uart_tx, uart_rx)             │
  │  6. uart_rx.listen()          → arm RX interrupt    │
  │  7. Timer::new(TIM2).counter_hz() → blink_timer     │
  │  8. blink_timer.start(1 Hz) + listen(Update)        │
  │  9. Deque::new()  → tx_buffer                       │
  │  10. Queue::new() → rx_queue                        │
  │  11. init_logger(LoggerConfig, &mut LOGGER_WRITER)  │
  │       └─► ushell2: stores writer ptr for macros     │
  │           (write_bytes is no-op here — not wired)   │
  │  12. MyShell::new(ShellConfig { ... })              │
  │       └─► ushell_ctx::ShellCtx::new()               │
  │            ├─ CallbackWriter::new(write_bytes,      │
  │            │                      flush_noop)       │
  │            ├─ InputParser::new(writer,              │
  │            │    get_commands(), get_datatypes(),    │
  │            │    get_shortcuts(), PROMPT)            │
  │            └─ AnsiKeyParser::new()                  │
  │  13. shell_task::spawn().ok()                       │
  │  14. return (Shared { ... }, Local { ... })         │
  └─────────────────────────────────────────────────────┘
```

---

## 4. Runtime Call Flow

### 4a. UART RX byte received  →  shell executes command

```
  [USART2 hardware interrupt fires]
           │
           ▼
  ┌──────────────────────────────────────────────┐
  │  usart2_isr()                  priority = 3  │
  │                                              │
  │  uart_rx.is_rx_not_empty() → true            │
  │  uart_rx.read()  → Ok(byte)                  │
  │  rx_queue.lock().enqueue(byte)               │
  │  shell_pending.lock():                       │
  │    if !pending:                              │
  │      pending = true                          │
  │      shell_task::spawn()  ───────────────────┼──┐
  │                                              │  │
  │  uart_tx.lock():                             │  │
  │    tx_buffer.lock():                         │  │
  │      handle_tx_ready(uart_tx, tx_buf)        │  │
  │       └─► uart_hal:                          │  │
  │            if is_tx_empty:                   │  │
  │              pop_front → Some(b): write(b)   │  │
  │                          listen()            │  │
  │              pop_front → None:  unlisten()   │  │
  └──────────────────────────────────────────────┘  │
                                                    │ (spawned)
           ┌────────────────────────────────────────┘
           ▼
  ┌──────────────────────────────────────────────┐
  │  shell_task()                  priority = 1  │
  │                                              │
  │  if !initialized:                            │
  │    tx_buffer.lock() + uart_tx.lock():        │
  │      init_uart_globals(                      │
  │        transmute(tx_buf),   ← 'static        │
  │        transmute(uart_tx))  ← 'static        │
  │      └─► uart_hal: stores ptrs in            │
  │           GLOBAL_UART.{tx_buffer, uart_tx}   │
  │    log_simple!("System initialized")         │
  │      └─► ushell2 macro → write_bytes()       │
  │           └─► uart_hal: push to tx_buffer    │
  │                         uart_tx.listen()     │
  │    initialized = true                        │
  │                                              │
  │  rx_queue.lock():                            │
  │    RxQueueReader::new(rx_queue)              │
  │    loop while !reader.is_empty():            │
  │      shell.step(&mut reader)                 │
  │       └─► ushell_ctx::ShellCtx::step()       │
  │            ├─ reader.read_byte() → Some(b)   │
  │            ├─ key_parser.parse_byte(b)       │
  │            │   → Some(Key::*) → pending_key  │
  │            └─ parser.parse_input(            │
  │                 || pending_key.take(),       │
  │                 |s| write_bytes(s),          │
  │                 |input| {                    │
  │                   if is_shortcut(input):     │
  │                     shortcut_dispatcher(...) │
  │                   else:                      │
  │                     command_dispatcher(...)  │
  │                     └─► ushell_usercode:     │
  │                          user command fn()   │
  │                   log_info!("Success")       │
  │                     └─► write_bytes()        │
  │                 })                           │
  │                                              │
  │  shell_pending.lock(): pending = false       │
  └──────────────────────────────────────────────┘
```

### 4b. TX interrupt draining the buffer

```
  [USART2 TX data register empty — interrupt fires]
           │
           ▼
  ┌──────────────────────────────────────────────┐
  │  usart2_isr()                  priority = 3  │
  │                                              │
  │  (RX path — skipped if no byte)              │
  │                                              │
  │  uart_tx.lock():                             │
  │    tx_buffer.lock():                         │
  │      handle_tx_ready(uart_tx, tx_buf)        │
  │        if is_tx_empty():                     │
  │          pop_front():                        │
  │            Some(b) → write(b), listen()   ←──┼─ keeps firing
  │            None    → unlisten()           ←──┼─ ISR stops
  └──────────────────────────────────────────────┘
```

### 4c. LED blink  (independent of shell / UART)

```
  [TIM2 Update event fires at 1 Hz]
           │
           ▼
  ┌──────────────────────────────────────────────┐
  │  led_blink()                   priority = 2  │
  │                                              │
  │  blink_timer.clear_flags(Update)             │
  │  LED_TOGGLE_COUNT.fetch_add(1, Relaxed)      │
  │  if state:  led.set_high()                   │
  │  else:      led.set_low()                    │
  │  log_info!("LED ON/OFF")                     │
  │    └─► write_bytes() → tx_buffer → USART2    │
  │  state = !state                              │
  └──────────────────────────────────────────────┘
```

---

## 5. uart_hal Global State Wiring

```
  ┌──────────────────────────────────────────────────────────┐
  │  uart_hal::GLOBAL_UART  (static mut, UnsafeCell)         │
  │                                                          │
  │  ┌──────────────────────────────────────────────────┐    │
  │  │  tx_buffer: UnsafeCell<Option<&'static mut       │    │
  │  │                         Deque<u8, 512>>>         │    │
  │  │                              ▲                   │    │
  │  │                              │ transmuted ref    │    │
  │  │                              │ from RTIC Shared  │    │
  │  │  uart_tx:   UnsafeCell<Option<&'static mut       │    │
  │  │                         UartTx>>                 │    │
  │  │                              ▲                   │    │
  │  │                              │ transmuted ref    │    │
  │  │                              │ from RTIC Shared  │    │
  │  └──────────────────────────────────────────────────┘    │
  │                                                          │
  │  Written once by init_uart_globals() in shell_task       │
  │  Read by write_bytes() from any task / ISR context       │
  │  Pop'd by handle_tx_ready() in usart2_isr (ISR only)     │
  └──────────────────────────────────────────────────────────┘

  Access pattern (no data races):
  ┌────────────────┬─────────────────┬─────────────────────┐
  │  Accessor      │ Operation       │ Concurrent safe?    │
  ├────────────────┼─────────────────┼─────────────────────┤
  │ write_bytes()  │ push_back       │ Yes — push & pop    │
  │ handle_tx_rdy()│ pop_front       │ are opposite ends   │
  │                │                 │ of the Deque        │
  ├────────────────┼─────────────────┼─────────────────────┤
  │ init_uart_glbl │ write ptr once  │ Yes — called once   │
  │                │                 │ before ISR can fire │
  └────────────────┴─────────────────┴─────────────────────┘
```

---

## 6. Layer Summary

```
  ┌─────────────────────────────────────────────────────────────┐
  │  APPLICATION LAYER  (main_app)                              │
  │  • RTIC task wiring, LED blink business logic               │
  │  • ShellConfig population from generated dispatchers        │
  │  • Hardware peripheral init (clocks, pins, serial, timer)   │
  ├─────────────────────────────────────────────────────────────┤
  │  SHELL LAYER  (ushell_ctx)                                  │
  │  • InputParser + AnsiKeyParser ownership                    │
  │  • ANSI key decoding + parse_input() orchestration          │
  │  • Command/shortcut dispatch via fn-pointer table           │
  ├─────────────────────────────────────────────────────────────┤
  │  UART LAYER  (uart_hal)                                     │
  │  • Global TX ring-buffer + UART Tx pointer                  │
  │  • write_bytes() / flush_noop() fn-pointer sinks            │
  │  • handle_tx_ready() ISR helper                             │
  │  • RxQueueReader lock-scoped wrapper                        │
  │  • UartWriter fmt::Write for logger                         │
  ├─────────────────────────────────────────────────────────────┤
  │  HARDWARE LAYER  (stm32f4xx-hal / RTIC / cortex-m)          │
  │  • USART2 peripheral, DMA-less interrupt-driven I/O         │
  │  • TIM2 periodic update interrupt                           │
  │  • NVIC priority-based preemption model                     │
  └─────────────────────────────────────────────────────────────┘
```