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

## Comparison with other Schedulers


| Feature | QMan | AceRoutine | GyverOS | TaskScheduler | Protothreads | FreeRTOS |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Programming Style** | **DSL (Async-like)** | C++ Classes | Polling | Callbacks | Macros/Switch | Task Functions |
| **RAM Usage** | **Extremely Low** | Medium | Low | Medium | Low | Very High |
| **Context Saving** | **Yes (Zero-stack)** | Yes | No | No | Yes | Yes (Full stack) |
| **Safety Guards** | **WDT / OnError** | None | None | None | None | HardFault |
| **Time Rollover** | **Immune (38h)** | millis() based | millis() | millis() drift | Manual check | Tick-based |

---

## Code Style Examples (Blink Task)

### 1. QMan (Efficient & Linear)
Linear code, no global flags needed. Best for 8-bit MCUs.
```cpp
QMAN_TASK(blink) {
  QMAN_INIT { pinMode(13, OUTPUT); }
  QMAN_LOOP {
    digitalWrite(13, !digitalRead(13));
    QMAN_SLEEP_MS(500);
  }
}
```

### 2. AceRoutine (Modern OO)
Uses C++ classes. Good, but higher RAM overhead due to object-oriented design.
```cpp
COROUTINE(blink) {
  COROUTINE_LOOP() {
    digitalWrite(13, !digitalRead(13));
    COROUTINE_DELAY(500);
  }
}
```

### 3. GyverOS (Polling Style)
Lightweight but requires manual state management (global flags) for complex logic.
```cpp
void task() {
  static bool state;
  digitalWrite(13, state = !state);
}
// Logic: OS.attach(0, task, 500);
```

### 4. TaskScheduler (The Popular)
No context saving. Tasks must be broken down into many separate callback functions.
```cpp
void blinkCb() { digitalWrite(13, !digitalRead(13)); }
Task t1(500, TASK_FOREVER, &blinkCb);
```

### 5. Protothreads (The Ancestor)
Low-level macros. Lacks a built-in scheduler; you must run each thread manually in loop().
```cpp
PT_THREAD(blink(struct pt *p)) {
  PT_BEGIN(p);
  while(1) {
    digitalWrite(13, !digitalRead(13));
    PT_WAIT_UNTIL(p, millis() - last > 500);
    last = millis();
  }
  PT_END(p);
}
```

### 6. FreeRTOS (The Professional)
Preemptive multitasking. Powerful, but consumes ~150 bytes of RAM per task.
```cpp
void vBlink(void *pv) {
  for(;;) {
    digitalWrite(13, !digitalRead(13));
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}
```


---
Created by developers for the Arduino community.
