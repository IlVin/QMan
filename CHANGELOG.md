# Changelog

All notable changes to the **QMan** library will be documented in this file.

## [1.0.3-beta] - 2026-04-10
### Added
- ```Nitro Memory Engine```: Implemented high-performance task insertion and removal using ```memmove``` instead of iterative swaps.
- ```Process-Oriented Execution```: Tasks now return a ```bool```. ```QMAN_STOP``` explicitly removes a task from the pool, while ```QMAN_SLEEP``` preserves it for the next run.
- ```Deterministic Yield```: Added "polite" rescheduling logic. Tasks with the same execution time now form a fair FIFO (First-In-First-Out) queue.
- ```Priority Dispatcher```: New insertion logic: ```GO(0)``` (event) acts as LIFO among peers for immediate response, while ```GO(delay)``` (scheduled) follows strict FIFO.
- ```Self-Healing Clock```: The ```Tick``` method now guarantees incrementing the ```now``` timestamp on every call, preventing "time slips" even during nested calls or empty queue periods.
- ```Universal Atomic Guard```: Introduced an RAII-style interrupt protection (```QManGuard```) ensuring thread safety across all platforms (AVR, STM32, ESP).

### Changed
- ```Smart DSL```: ```QMAN_SLEEP``` and ```QMAN_GO``` macros are now variadic—they can be called without arguments for immediate "yield" execution in the next cycle.
- ```Strict Typing```: Introduced the ```QTime``` struct and literals (```_ms```, ```_us```, ```_tick```) to eliminate "raw number" errors in scheduling methods.
- ```Refined Tick Logic```: The dispatcher now executes exactly one task per call, providing predictable load distribution within the main ```loop()```.

### Fixed
- Fixed ```QMAN_DUTY``` logic: periods are now calculated with absolute precision from the ```Started()``` point, perfectly compensating for task execution time.
- Fixed the "task looping" bug where a task could re-run instantly during zero-delay rescheduling.
- Fixed "backlog starvation" where overdue tasks could be blocked by newly injected priority events.

## [1.0.2-beta] - 2026-04-08
### Fixed
- Fixed "incomplete type" and "not declared in scope" compiler errors by reordering internal functions.
- Fixed `QMAN_TICK` macro to properly handle both auto-mode and manual timestamp injection using a dispatcher.
- Improved technical documentation and comments inside the source code.

## [1.0.1-beta] - 2026-04-08
### Added
- **Everlasting Time Engine**: Added a 32-bit tick accumulator (`qman_task_ticks`) to prevent the 70-minute `micros()` rollover freeze.
- New `Now()` and `Started()` methods for thread-safe time tracking.
- Jitter compensation in the auto-tick logic to preserve microsecond precision.

### Changed
- `QMAN_TICK` now supports two modes: `QMAN_TICK()` (auto) and `QMAN_TICK(ts)` (manual).
- All 32-bit getters are now wrapped in `ATOMIC_BLOCK` for AVR safety.

### Fixed
- Resolved the "45-70 minute freeze" caused by 27-bit signed arithmetic limitations.

## [1.0.0-beta] - 2026-04-08
### Added
- Initial release of **QMan (Queue Manager)**.
- DSL macros: `QMAN_TASK`, `QMAN_INIT`, `QMAN_LOOP`, `QMAN_SLEEP`, `QMAN_DUTY`.
- Static task counting with `OnWarning` callback.
- Dynamic pool overflow protection with `OnError` and Watchdog reset.
- Single-pass priority queue optimization.

---
```cpp
// "Time is a flat circle, but QMan makes it linear."
```
