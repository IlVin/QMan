# QMan

**QMan (Queue Manager)** is a very small and simple tool for Arduino. It helps you run many tasks at the same time without waiting for each other. It is perfect for small computers like Arduino Uno, Nano, Mega, STM32, ESP8266, and ESP32.

## Project Status

- **Version:** 1.0.7-beta
- **License:** MIT (free to use)
- **Works on:** AVR (Uno, Nano, Mega), STM32, ESP8266, ESP32, LGT8F

## Three Ways to Write Tasks

### Way 1: Simple One-Shot Task (No QMAN_LOOP)

Use this when you want a task to run once and then stop, or start another task:

```
cpp
#include <QMan.h>

QMAN_TASK(turnOn) {
    digitalWrite(LED_BUILTIN, HIGH);
    QMAN_GO(turnOff, 1000_ms);
}

QMAN_TASK(turnOff) {
    digitalWrite(LED_BUILTIN, LOW);
    QMAN_GO(turnOn, 1000_ms);
}

void setup() {
    QMAN_GO(turnOn);
}

void loop() {
    QMAN_TICK();
}
```

Each task runs once, then starts the other task. Simple and clean.

### Way 2: Standard Infinite Loop (With QMAN_LOOP)

Use this when a task needs to repeat forever:

```
cpp
#include <QMan.h>

QMAN_TASK(blinkTask) {
    QMAN_INIT { pinMode(LED_BUILTIN, OUTPUT); }
    QMAN_LOOP {
        digitalWrite(LED_BUILTIN, HIGH);
        QMAN_SLEEP(500_ms);
        digitalWrite(LED_BUILTIN, LOW);
        QMAN_SLEEP(500_ms);
    }
}

void setup() {
    QMAN_GO(blinkTask);
}

void loop() {
    QMAN_TICK();
}
```

The task runs forever. Use QMAN_SLEEP to pause without blocking other tasks.

### Way 3: Hybrid — One Loop Iteration Then Exit

You can also exit a QMAN_LOOP early using `return`. The task will run once, then stop:

```
cpp
#include <QMan.h>

QMAN_TASK(blinkTask) {
    QMAN_INIT { pinMode(LED_BUILTIN, OUTPUT); }
    QMAN_LOOP {
        digitalWrite(LED_BUILTIN, HIGH);
        QMAN_SLEEP(500_ms);
        digitalWrite(LED_BUILTIN, LOW);
        QMAN_GO(otherTask, 500_ms);
        return;   // exit the task after one loop
    }
}

void setup() {
    QMAN_GO(blinkTask);
}

void loop() {
    QMAN_TICK();
}
```

This runs the loop body once, then the task stops.

## Why QMan is Great

- **Easy to use** — write code that looks like normal steps, but it does not block
- **Saves memory** — tasks do not need their own stack space (only 8 bytes per task on AVR!)
- **Smart queue** — automatically decides which task to run next
- **Safe** — warns you if you create too many tasks
- **Fixed timing** — QMAN_DUTY keeps exact timing even if your task takes time to run
- **Testable** — you can fake time with QMAN_TICK(N_tick) to test your code on PC

## How Time Works

QMan measures time in "ticks". One tick is 32 microseconds by default. You never work with ticks directly. Instead, use special numbers:

- `1000_ms` — one thousand milliseconds (1 second)
- `500_us` — five hundred microseconds
- `10_tick` — ten ticks (rarely needed)

The system automatically converts these to ticks at compile time. This makes your code fast and safe.

## Main Commands (Macros)

| Macro | What it does |
|-------|---------------|
| `QMAN_TASK(name)` | Create a new task |
| `QMAN_INIT` | Code that runs ONCE, the first time the task starts (needed for SLEEP/DUTY) |
| `QMAN_LOOP` | The main repeating part of the task (needed for SLEEP/DUTY) |
| `QMAN_SLEEP(time)` | Pause the task (use `1000_ms`, `500_us`, etc.) |
| `QMAN_DUTY(time)` | Run task every `time` milliseconds (adjusts for task duration) |
| `QMAN_GO(task)` | Start a task right now |
| `QMAN_GO(task, delay)` | Start a task after a delay (use `_ms`, `_us`, or `_tick`) |
| `QMAN_TICK()` | Call this in `loop()` to drive the manager |

**Important:** `QMAN_INIT` runs only once — the first time you call `QMAN_GO` for that task. If you stop a task and start it again with `QMAN_GO`, `QMAN_INIT` will NOT run again. Use `QMAN_INIT` for one-time setup like `pinMode()`.

**Note:** `QMAN_SLEEP` and `QMAN_DUTY` need both `QMAN_INIT` and `QMAN_LOOP`. Simple tasks without SLEEP do not need them.

## SLEEP vs DUTY

**SLEEP** (simple pause):
- Waits a fixed time AFTER your task finishes
- Example: task takes 100ms, SLEEP(1000ms) → total 1100ms (drifts)

**DUTY** (exact timing):
- Keeps exact period regardless of task duration
- Example: task takes 100ms, DUTY(1000ms) → waits 900ms → total 1000ms (exact)

Use DUTY for things that need perfect timing, like reading a sensor every second.

## Change the Task Limit

By default, you can have up to 16 tasks. Change it before including the library:

```
cpp
#define QMAN_POOL_SIZE 32
#include <QMan.h>
```

## Change Timing Accuracy

Default tick is 32 microseconds. Smaller = more accurate but more CPU work:

```
cpp
#define QMAN_TICK_MAX_US 16
#include <QMan.h>
```

## Error Handling

```
cpp
void onWarning(uint8_t code) {
    Serial.print("Warning: ");
    Serial.println(code);
}

void onError(uint8_t code) {
    Serial.print("Error: ");
    Serial.println(code);
}

void setup() {
    qman.OnWarning(onWarning);
    qman.OnError(onError);
    QMAN_GO(myTask);
}
```

### Error Codes

| Code | Name | Meaning |
|------|------|---------|
| 1 | `QMAN_ERR_POOL_OVERFLOW` | Too many tasks running at once |
| 2 | `QMAN_WARN_STATIC_LIMIT` | You created more tasks than the pool can hold |
| 3 | `QMAN_WARN_TICK_LIMIT` | Requested tick accuracy is not possible |
| 4 | `QMAN_WARN_DUTY_LATE` | Task took too long to finish |

## Important Rule

If you use `QMAN_SLEEP` or `QMAN_DUTY`, ALL variables inside your task must be `static`:

```
cpp
QMAN_TASK(myTask) {
    QMAN_INIT {
        static int counter = 0;  // correct
    }
    QMAN_LOOP {
        static int value = 0;    // correct
        int wrong = 0;           // WRONG! Will break after SLEEP
        QMAN_SLEEP(100_ms);
    }
}
```

This is because QMan does not save the stack. The task remembers where to resume, but local variables are lost.

If you do not use `QMAN_SLEEP` or `QMAN_DUTY`, you can use normal variables freely.

## How QMan Compares to Others

| Feature | QMan | AceRoutine | GyverOS | TaskScheduler | FreeRTOS |
|---------|------|------------|---------|---------------|----------|
| Easy to learn | Yes | Medium | Medium | Hard | Hard |
| RAM per task (AVR) | **<8 bytes** | ~16-28 bytes | Low | Medium | Very High |
| Saves task state | Yes | Yes | No | No | Yes |
| Auto time adjustment (DUTY) | **Yes** | No | No | No | Yes |
| Safety features | Yes | No | No | No | Yes |
| Works on 8-bit Arduino | **Yes** | Yes | Yes | Yes | No |

## Examples

The library comes with these examples:

| Example | What it shows |
|---------|----------------|
| `AsyncLCD.ino` | Print to LCD without blocking the LED blink |
| `SleepVsDuty.ino` | Difference between SLEEP and DUTY |
| `TestStability.ino` | Check timing accuracy |

Look in the `examples/` folder to try them.

## License

MIT License — you can use it for free, even in commercial projects.

## Author

Created by Ilia Vinokurov for the Arduino community. Questions? Open an Issue on GitHub: https://github.com/IlVin/QMan