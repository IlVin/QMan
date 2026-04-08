/**
 * QMan (Queue Manager) - Ultra-lightweight cooperative task queue manager for Arduino.
 * Author: IlVin
 * Version: 1.0.2-beta
 * License: MIT
 * 
 * This library provides a way to run multiple tasks seemingly at the same time
 * without using a complex Operating System. It's perfect for small microcontrollers.
 */

#ifndef QMAN_H
#define QMAN_H

#include <Arduino.h>
#include <avr/wdt.h>
#include <util/atomic.h>

// --- Basic Types and Enums ---

typedef void (*TaskFunc)();
typedef void (*ErrorCallback)(uint8_t errorCode);
typedef void (*WarningCallback)(uint8_t errorCode);

/**
 * Codes for system notifications
 * 1: Pool Overflow - Too many tasks running at once!
 * 2: Static Limit  - You defined more tasks in code than the pool can hold.
 */
enum QManError {
    QMAN_ERR_POOL_OVERFLOW = 1,
    QMAN_WARN_STATIC_LIMIT = 2
};

// Default number of tasks the manager can hold (can be changed in your sketch)
#ifndef QMAN_POOL_SIZE
#define QMAN_POOL_SIZE 16
#endif

// Global counter to track how many QMAN_TASKs are created in the code
extern uint8_t qman_total_tasks;
struct QManCounter { QManCounter() { qman_total_tasks++; } };

// Internal structure to store task data
struct Task {
    TaskFunc func;      // What function to run
    uint32_t nextRun;   // When to run it
};

#define QMAN_PASTE(a, b) a ## b
#define QMAN_JOIN(a, b) QMAN_PASTE(a, b)

// --- DSL (Domain Specific Language) Macros ---

/**
 * Define a new task. 
 * Use it like: QMAN_TASK(myBlinkTask) { ... }
 */
#define QMAN_TASK(name) \
    static QManCounter QMAN_JOIN(cnt_, name); \
    void name()

/**
 * Initialization block inside a task. 
 * Code here runs only ONCE when the task starts for the first time.
 */
#define QMAN_INIT \
    static void* __resumeAddr = nullptr; \
    if (__resumeAddr) goto *__resumeAddr; \
    if (true)

/**
 * Main loop of the task. 
 * The task will keep repeating this block until it is stopped.
 */
#define QMAN_LOOP \
    __loop_entry: \
    for (; (__resumeAddr = nullptr, true); )

/**
 * Stop the task and reset it. 
 * Next time it starts, it will begin from the top of QMAN_LOOP.
 */
#define QMAN_STOP \
    do { \
        __resumeAddr = &&__loop_entry; \
        return; \
    } while (0)

// Helper macros to convert time to internal Ticks (1 Tick = 32 microseconds)
#define Q_US(us) ((uint32_t)(us) >> 5)
#define Q_MS(ms) ((uint32_t)(ms) * 31250UL / 1000UL)

// Task launch and control commands
#define QMAN_GO_MS(task, ms) qman.Go(task, Q_MS(ms))
#define QMAN_GO_US(task, us) qman.Go(task, Q_US(us))
#define QMAN_SLEEP_MS(ms)    QMAN_SLEEP(Q_MS(ms))
#define QMAN_SLEEP_US(us)    QMAN_SLEEP(Q_US(us))
#define QMAN_DUTY_MS(ms)     QMAN_DUTY(Q_MS(ms))
#define QMAN_DUTY_US(us)     QMAN_DUTY(Q_US(us))

/**
 * Pause the task for a specific delay. 
 * Other tasks will run while this one is "sleeping".
 */
#define QMAN_SLEEP(delay) \
    do { \
        __resumeAddr = (void*)&&QMAN_JOIN(L_, __LINE__); \
        qman.Sleep(delay); \
        return; \
        QMAN_JOIN(L_, __LINE__): ; \
    } while (0)

/**
 * Precise timing: ensures the task runs exactly every 'period' ticks, 
 * accounting for the time the task itself took to execute.
 */
#define QMAN_DUTY(period) \
    do { \
        __resumeAddr = (void*)&&QMAN_JOIN(L_, __LINE__); \
        uint32_t current = qman.Now(); \
        uint32_t started = qman.Started(); \
        uint32_t elapsed = current - started; \
        qman.Sleep((elapsed < (uint32_t)period) ? ((uint32_t)period - elapsed) : 0); \
        return; \
        QMAN_JOIN(L_, __LINE__): ; \
    } while (0)

#define QMAN_GO(task, delay) qman.Go(task, delay)

// --- The Core Manager Class ---

template <uint8_t POOL_SIZE>
class QMan_Generic {
private:
    Task pool[POOL_SIZE];       // Array of tasks waiting to run
    uint8_t count;              // Number of tasks currently in the pool
    uint32_t now;               // Current internal time
    uint32_t lastTaskStart;     // When the current task was started
    TaskFunc currentTask;       // Pointer to the task being executed right now
    ErrorCallback onError;      // Function to call if something goes wrong
    WarningCallback onWarning;  // Function to call for system warnings

    /**
     * Internal: adds or moves a task in the queue.
     * It uses a single-pass shift to be very fast.
     */
    void insert(TaskFunc f, uint32_t delay, bool pushDeep) {
        if (f == nullptr) return;
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            uint32_t nextRun = now + delay;
            int8_t oldIdx = -1;
            int8_t targetIdx = 0;
            for (uint8_t i = 0; i < count; i++) {
                if (pool[i].func == f) oldIdx = i;
                bool condition = pushDeep ? 
                    ((int32_t)(pool[i].nextRun - nextRun) > 0) : 
                    ((int32_t)(pool[i].nextRun - nextRun) >= 0);
                if (condition) targetIdx++;
            }
            if (oldIdx != -1 && oldIdx < targetIdx) targetIdx--;
            
            // Check if we have space for a new task
            if (oldIdx == -1) {
                if (count >= POOL_SIZE) {
                    if (onError) onError(QMAN_ERR_POOL_OVERFLOW);
                    wdt_enable(WDTO_120MS);
                    cli(); while(1); // Stop and wait for Watchdog reset
                }
                count++;
            }
            
            // Move other tasks to make room for the new one
            if (oldIdx != -1) {
                if (oldIdx > targetIdx) { 
                    for (uint8_t j = oldIdx; j > targetIdx; j--) pool[j] = pool[j - 1];
                } else if (oldIdx < targetIdx) {
                    for (uint8_t j = oldIdx; j < targetIdx; j++) pool[j] = pool[j + 1];
                }
            } else {
                for (uint8_t j = count - 1; j > targetIdx; j--) pool[j] = pool[j - 1];
            }
            pool[targetIdx] = {f, nextRun};
        }
    }

public:
    QMan_Generic(uint32_t ts) : now(ts), lastTaskStart(ts), count(0), 
                               currentTask(nullptr), onError(nullptr), onWarning(nullptr) {}

    /**
     * Start a task. If it's already in the pool, it will be rescheduled.
     */
    void Go(TaskFunc f, uint32_t delay = 0) {
        static bool warningSent = false;
        if (!warningSent && onWarning) {
            warningSent = true;
            if (qman_total_tasks > POOL_SIZE) onWarning(QMAN_WARN_STATIC_LIMIT);
        }
        insert(f, delay, false);
    }

    /**
     * Used by the system to reschedule a task after QMAN_SLEEP.
     */
    void Sleep(uint32_t delay) { insert(currentTask, delay, true); }

    /**
     * The Heartbeat: Checks the queue and runs any task that is ready.
     */
    void Tick(uint32_t ts) {
        static bool insideTick = false;
        if (insideTick) return; // Protect against recursive calls
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            now = ts;
            if (count == 0) return;
            if ((int32_t)(now - pool[count - 1].nextRun) >= 0) {
                 currentTask = pool[--count].func;
                 lastTaskStart = now; 
            }
        }
        if (currentTask) {
            insideTick = true;
            currentTask();      // Run the task function
            insideTick = false;
            currentTask = nullptr;
        }
    }

    uint32_t Started() { ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { return lastTaskStart; } }
    uint32_t Now()     { ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { return now; } }
    uint8_t Len()      { ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { return count; } }
    
    void OnError(ErrorCallback cb)     { onError = cb; }
    void OnWarning(WarningCallback cb) { onWarning = cb; }
};

// --- Global Instance and Auto-Timing ---

typedef QMan_Generic<QMAN_POOL_SIZE> QMan;
extern QMan qman;

extern uint32_t qman_last_micros;
extern uint32_t qman_task_ticks;

/**
 * Internal: tracks time using the Arduino micros() clock.
 * It prevents errors when the clock rolls over (every 70 mins).
 */
inline void qman_auto_tick() {
    uint32_t currentMicros = micros();
    if (qman_last_micros == 0) {
        qman_last_micros = currentMicros;
        return;
    }
    uint32_t delta = currentMicros - qman_last_micros;
    if (delta >= 32) {
        uint32_t t = delta >> 5;
        qman_task_ticks += t;
        qman_last_micros += (t << 5);
    }
    qman.Tick(qman_task_ticks);
}

// Helpers for the QMAN_TICK macro
inline void QMAN_TICK_DISPATCHER() { qman_auto_tick(); }
inline void QMAN_TICK_DISPATCHER(uint32_t ts) { qman.Tick(ts); }

/**
 * Call this in your loop() to keep the manager running.
 * QMAN_TICK()     -> uses automatic timing.
 * QMAN_TICK(time) -> uses your own time value (useful for tests).
 */
#define QMAN_SELECT_TICK(_0, _1, NAME, ...) NAME
#define QMAN_TICK(...) QMAN_SELECT_TICK(0, ##__VA_ARGS__, QMAN_TICK_DISPATCHER(__VA_ARGS__), QMAN_TICK_DISPATCHER())

#endif // QMAN_H
