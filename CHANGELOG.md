# Changelog

All notable changes to the **QMan** library will be documented in this file.

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
