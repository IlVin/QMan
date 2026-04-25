# Changelog

All notable changes to the **QMan** library will be documented in this file.

## [1.0.7-beta] - 2026-04-25
### Changed
- Task function signature changed from bool (*)() to void (*)().
  QMAN_STOP is now a plain return without a value. Tick() no longer
  inspects the return value of task functions.

- Task is now removed from the queue BEFORE execution (via count-- in
  Tick()) instead of after. This eliminates the need to search for the
  task by pointer after it returns, and avoids index invalidation issues
  caused by ISR-triggered queue modifications during task execution.

- __resumeAddr = &&__loop_entry moved from QMAN_STOP to QMAN_INIT.
  QMAN_STOP is now semantically equivalent to a plain return. On the
  first run, QMAN_INIT saves the restart address automatically, so a
  stopped task will skip initialization when restarted via QMAN_GO.

### Added
- schedule_task() — a single internal helper shared by go(), sleep(),
  and duty(). It uses a compile-time constant place-check callback to
  combine duplicate search and insertion point search into one pass
  through the queue array.

- Place-check callbacks (go_place_timer, go_place_event, sleep_place,
  duty_place) — static inline functions that encode the insertion
  semantics of each scheduling operation. The compiler inlines them,
  eliminating indirect call overhead.

### Optimized
- Duplicate removal and insertion are now done in a single pass through
  the queue array. In the best case (duplicate already at the correct
  position), only nextRun is updated — no memmove required.

- When a duplicate is found at a different position, a single memmove
  shifts the elements between the old and new positions, instead of
  doing two separate memmove calls (one for removal, one for insertion).

- For a 16-element queue, worst-case critical section time reduced by
  approximately 2x compared to the previous implementation.

### Removed
- remove_at() — replaced by inline logic in schedule_task().
- move_task() — replaced by single memmove in schedule_task().
- remove_task() — merged into schedule_task().

## [1.0.6-beta] - 2026-04-24
### Added
- llms.txt

## [1.0.5-beta] - 2026-04-24
### Fixed
- Fixed rare but annoying bug where `QMAN_DUTY` could schedule a task in the past under heavy load, causing instant re-execution.
- Fixed memory corruption issue when `QMAN_POOL_SIZE` was changed after including the library (ODR violation). Now the size is locked at compile time.
- Fixed race condition in `qman_get_delta_ticks()` for AVR that could cause incorrect timing after 70 minutes of operation.

### Changed
- Improved stability of the internal queue when many tasks are added and removed at the same time.
- Enhanced error recovery: if a task returns `true` (STOP), it is now removed immediately and cannot be accidentally rescheduled in the same tick.

### Added
- Better compiler warnings for users who forget to call `QMAN_TICK()` in `loop()`.
- Example sketches now include comments for absolute beginners.

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
