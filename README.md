# QMan

**QMan (Queue Manager)** is a very small and simple tool for Arduino. It helps you run many tasks at the same time without waiting for each other. It is perfect for small computers like Arduino Uno, Nano, Mega, STM32, ESP8266, and ESP32.

## Project Status

- **Version:** 1.0.4-beta
- **License:** MIT (free to use)
- **Works on:** AVR (Uno, Nano, Mega), STM32, ESP8266, ESP32, LGT8F

## Quick Example

```
cpp
#include <QMan.h>

QMAN_TASK(blinkTask) {
    QMAN_INIT { pinMode(LED_BUILTIN, OUTPUT); }
    QMAN_LOOP {
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        QMAN_SLEEP_MS(500); // Wait 500ms without blocking other tasks
    }
}

void setup() {
    QMAN_GO(blinkTask);
}

void loop() {
    QMAN_TICK(); // Run the manager
}
```

## Why QMan is Great

- **Easy to use** — write code that looks like normal steps, but it does not block
- **Saves memory** — tasks do not need their own stack space
- **Smart queue** — automatically decides which task to run next
- **Safe** — warns you if you create too many tasks
- **Fixed timing** — you can make tasks run at exact times, even if they take different amounts of time to finish

## How to Install

1. Download this library as a `.zip` file
2. In Arduino IDE, go to **Sketch → Include Library → Add .ZIP Library**
3. Or search for "QMan" in the Library Manager

## Main Commands (Macros)

| Macro | What it does |
|-------|---------------|
| `QMAN_TASK(name)` | Create a new task |
| `QMAN_INIT` | Code that runs once when the task starts |
| `QMAN_LOOP` | The main repeating part of the task |
| `QMAN_SLEEP_MS(ms)` | Pause the task for `ms` milliseconds |
| `QMAN_DUTY(ms)` | Run the task every `ms` milliseconds (adjusts for task time) |
| `QMAN_STOP` | Stop the task completely |
| `QMAN_GO(task)` | Start a task right now |
| `QMAN_GO(task, delay)` | Start a task after some delay |
| `QMAN_TICK()` | Call this in `loop()` to drive the manager |

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

## How QMan Compares to Others

| Feature | QMan | AceRoutine | GyverOS | TaskScheduler | FreeRTOS |
|---------|------|------------|---------|---------------|----------|
| Easy to learn | Yes | Medium | Medium | Hard | Hard |
| RAM per task | **Very low (<8 bytes)** | Medium | Low | Medium | High |
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