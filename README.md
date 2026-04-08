# QMan
QMan (Queue Manager) — Ultra-lightweight cooperative task queue manager for Arduino. Features async-style DSL macros, zero-stack coroutines, and LIFO/FIFO priority management. Includes static safety checks, overflow protection via WDT/Error callbacks, and deterministic tick-based timing.

# Quick glimpse🚀

```cpp
// Quick glimpse
QMAN_TASK(myTask) {
    QMAN_INIT { /* Setup */ }
    QMAN_LOOP {
        doSomething();
        QMAN_SLEEP_MS(500); 
    }
}
```

**QMan** is an ultra-lightweight, high-performance cooperative task queue manager for Arduino and AVR-based microcontrollers. It allows you to write non-blocking, asynchronous-style code using simple DSL macros without the overhead of a full RTOS.

> **Status:** v1.0.0-beta  
> **License:** MIT

## Why QMan?
Unlike simple timers, QMan is a disciplined **Queue Manager**. It doesn't just run functions; it manages their lifecycle, execution order, and system safety.

- **Async-style Syntax:** Write complex logic with `QMAN_SLEEP` and `QMAN_LOOP` that looks like `async/await`.
- **Zero-Stack Coroutines:** Tasks don't need dedicated stack space, saving precious RAM.
- **Smart Queue:** Optimized LIFO/FIFO priority management.
- **Static Safety:** Built-in counter warns you if the number of tasks in code exceeds the pool size.
- **Deterministic:** Time is injected from outside, making it perfect for unit testing and stress testing.

## Installation
1. Download this repository as a `.zip` file.
2. In Arduino IDE, go to **Sketch -> Include Library -> Add .ZIP Library**.

## Quick Start

```cpp
#include <QMan.h>

// 1. Define a task using DSL macros
QMAN_TASK(blinkTask) {
    QMAN_INIT {
        pinMode(LED_BUILTIN, OUTPUT);
    }
    
    QMAN_LOOP {
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        QMAN_SLEEP_MS(500); // Non-blocking sleep
    }
}

void setup() {
    // 2. Start the task immediately
    QMAN_GO(blinkTask, 0);
}

void loop() {
    // 3. Drive the manager with system ticks (32us per tick)
    QMAN_TICK(); 
}
```

## Safety Features
QMan is built for reliability in embedded systems:
* **OnError Callback:** Triggered on dynamic pool overflow before a hard Watchdog reset.
* **OnWarning Callback:** Triggered if your code defines more tasks than the allocated memory pool.
* **Recursion Protection:** Prevents stack crashes if a task recursively calls the dispatcher.

## Error Codes

| Code | Enum | Description |
| :--- | :--- | :--- |
| 1 | QMAN_ERR_POOL_OVERFLOW | Dynamic queue is full. System will reboot. |
| 2 | QMAN_WARN_STATIC_LIMIT | Total defined tasks exceed pool size. Check your memory! |

---
Created by developers for the Arduino community.
