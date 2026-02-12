# uSTM32Template

[**STM32 + Embassy RTOS + Shell**](Embassy_Shell/README.md)<br>
Step-by-Step Guide STM32F4xx with Embassy-rs

- Embassy is a modern, lightweight, asynchronous runtime and framework for embedded systems written in Rust.
It enables writing highly efficient, concurrent firmware using Rust’s async/await features — without needing a traditional RTOS.

- Embassy is designed specifically for resource-constrained microcontrollers and works without requiring dynamic memory allocation. It uses cooperative multitasking, where tasks yield control at `.await` points, resulting in predictable and efficient execution.

- The framework provides integrated support for timers, peripherals, interrupts, and multiple hardware platforms through HAL integrations. It also focuses on low-power operation by automatically putting the MCU to sleep when idle.

---

[**STM32 + RTIC + Shell**](RTIC_Shell/README.md)<br>
Step-by-Step Guide STM32F4xx with Embassy-rs

- RTIC (Real-Time Interrupt-driven Concurrency) is a lightweight concurrency framework for embedded systems written in Rust.
It enables developers to build real-time applications with deterministic behavior by leveraging hardware interrupts and a priority-based scheduling model.

- RTIC is designed for resource-constrained microcontrollers and does not require dynamic memory allocation. It follows a task-based model where software tasks are bound to interrupts or scheduled with defined priorities, ensuring predictable execution and low latency.

- The framework provides compile-time guarantees for memory safety and data sharing, preventing race conditions without requiring a traditional RTOS. By combining Rust’s safety features with a static scheduling model, RTIC helps developers create reliable, efficient, and real-time embedded applications.

---

[**STM32F + FreeRTOS + Shell**](FreeRTOS_Shell/README.md)<br>
Step-by-Step Guide STM32F4xx with FreeRTOS

- FreeRTOS is a widely used, open-source real-time operating system designed for microcontrollers and small embedded systems.
It enables developers to build deterministic, multitasking applications using a preemptive priority-based scheduler.

- FreeRTOS provides core RTOS features such as tasks (threads), queues, semaphores, mutexes, timers, and event groups for inter-task communication and synchronization. It supports both preemptive and cooperative scheduling modes, allowing flexibility depending on application requirements.

- The kernel is small and highly portable, running on a wide range of architectures and microcontrollers. FreeRTOS can operate with or without dynamic memory allocation and is suitable for resource-constrained devices.

- With its mature ecosystem, extensive documentation, and commercial support options, FreeRTOS is commonly used in IoT devices, industrial control systems, consumer electronics, and safety-critical applications.
