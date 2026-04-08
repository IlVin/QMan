/**
 * QMan (Queue Manager) - Ultra-lightweight cooperative task queue manager for Arduino.
 * Author: IlVin
 * Version: 1.0.0-beta
 * License: MIT
 */

#ifndef QMAN_H
#define QMAN_H

#include <Arduino.h>
#include <avr/wdt.h>
#include <util/atomic.h>

// --- Type Definitions ---
typedef void (*TaskFunc)();
typedef void (*ErrorCallback)(uint8_t errorCode);
typedef void (*WarningCallback)(uint8_t errorCode);

/**
 * System status and error codes
 */
enum QManError {
    QMAN_ERR_POOL_OVERFLOW = 1, // Dynamic queue is full, system cannot add more tasks
    QMAN_WARN_STATIC_LIMIT = 2  // Total defined QMAN_TASKs exceed POOL_SIZE
};

/**
 * Default pool size if not defined in the main sketch.
 * Each task entry occupies 6 bytes of SRAM.
 */
#ifndef QMAN_POOL_SIZE
#define QMAN_POOL_SIZE 16
#endif

// Global counter for tasks declared via QMAN_TASK macro
extern uint8_t qman_total_tasks;

/**
 * Helper structure to increment task counter during global object construction.
 * Works before main() starts.
 */
struct QManCounter { 
    QManCounter() { qman_total_tasks++; } 
};

/**
 * Task entry structure for the priority queue
 */
struct Task {
    TaskFunc func;      // Pointer to the task function
    uint32_t nextRun;   // Calculated system tick for the next execution
};

#define QMAN_PASTE(a, b) a ## b
#define QMAN_JOIN(a, b) QMAN_PASTE(a, b)

// --- DSL Macros ---

/**
 * Macro to define a task. Automatically creates a counter object to track 
 * total task count in the firmware.
 */
#define QMAN_TASK(name) \
    static QManCounter QMAN_JOIN(cnt_, name); \
    void name()

/**
 * Coroutine entry point. Resumes execution from the last saved address.
 */
#define QMAN_INIT \
    static void* __resumeAddr = nullptr; \
    if (__resumeAddr) goto *__resumeAddr; \
    if (true)

/**
 * Main loop for a task. If task exits via return, it will be terminated.
 */
#define QMAN_LOOP \
    __loop_entry: \
    for (; (__resumeAddr = nullptr, true); )

/**
 * Explicitly stop the task and return it to the loop entry.
 */
#define QMAN_STOP \
    do { \
        __resumeAddr = &&__loop_entry; \
        return; \
    } while (0)

// --- Time Conversion Macros ---
// Tick quantum: 32 microseconds (1,000,000 / 32 = 31,250 ticks per second)
#define Q_US(us) ((uint32_t)(us) >> 5)
#define Q_MS(ms) ((uint32_t)(ms) * 31250UL / 1000UL)

// Alias macros for system calls
#define QMAN_GO_MS(task, ms) qman.Go(task, Q_MS(ms))
#define QMAN_GO_US(task, us) qman.Go(task, Q_US(us))
#define QMAN_SLEEP_MS(ms)    QMAN_SLEEP(Q_MS(ms))
#define QMAN_SLEEP_US(us)    QMAN_SLEEP(Q_US(us))
#define QMAN_DUTY_MS(ms)     QMAN_DUTY(Q_MS(ms))
#define QMAN_DUTY_US(us)     QMAN_DUTY(Q_US(us))

/**
 * Suspends task execution for the specified duration.
 * Saves current address and returns control to the dispatcher.
 */
#define QMAN_SLEEP(delay) \
    do { \
        __resumeAddr = (void*)&&QMAN_JOIN(L_, __LINE__); \
        qman.Sleep(delay); \
        return; \
        QMAN_JOIN(L_, __LINE__): ; \
    } while (0)

/**
 * Calculates duty cycle sleep to maintain fixed frequency execution.
 */
#define QMAN_DUTY(period) \
    do { \
        __resumeAddr = (void*)&&QMAN_JOIN(L_, __LINE__); \
        uint32_t current = (micros() >> 5); \
        uint32_t started = qman.Started(); \
        uint32_t elapsed = current - started; \
        qman.Sleep((elapsed < (uint32_t)period) ? ((uint32_t)period - elapsed) : 0); \
        return; \
        QMAN_JOIN(L_, __LINE__): ; \
    } while (0)

#define QMAN_GO(task, delay) qman.Go(task, delay)
#define QMAN_TICK()          qman.Tick(micros() >> 5)

/**
 * Generic Queue Manager class
 */
template <uint8_t POOL_SIZE>
class QMan_Generic {
private:
    Task pool[POOL_SIZE];
    uint8_t count;
    uint32_t now;               // Current system tick (injected via Tick)
    uint32_t lastTaskStart;     // Timestamp when current task started
    TaskFunc currentTask;       // Pointer to the currently executing task
    ErrorCallback onError;      // Triggered on critical overflow
    WarningCallback onWarning;  // Triggered on static limit violation

    /**
     * Internal method to insert or relocate a task in the priority queue.
     * Implements single-pass shifting for performance optimization.
     */
    void insert(TaskFunc f, uint32_t delay, bool pushDeep) {
        if (f == nullptr) return;

        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            uint32_t nextRun = now + delay;
            int8_t oldIdx = -1;
            int8_t targetIdx = 0;

            // Step 1: Locate existing task index and find new target position
            for (uint8_t i = 0; i < count; i++) {
                if (pool[i].func == f) oldIdx = i;
            
                // Priority sorting: Time first, then LIFO/FIFO logic
                bool condition = pushDeep ? 
                    ((int32_t)(pool[i].nextRun - nextRun) > 0) : 
                    ((int32_t)(pool[i].nextRun - nextRun) >= 0);
            
                if (condition) targetIdx++;
            }

            // Adjust target index if the task is moving deeper after self-removal
            if (oldIdx != -1 && oldIdx < targetIdx) targetIdx--;

            // Step 2: Handle new task insertion and overflow checks
            if (oldIdx == -1) {
                if (count >= POOL_SIZE) {
                    if (onError) onError(QMAN_ERR_POOL_OVERFLOW);
                    wdt_enable(WDTO_120MS);
                    cli(); // Force hang to trigger WDT reset
                    while(1);
                }
                count++;
            }

            // Step 3: Perform a single-pass array shift
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
     * Submits a new task to the queue (High Priority / LIFO at same time quantum)
     */
    void Go(TaskFunc f, uint32_t delay = 0) {
        static bool warningSent = false;
        if (!warningSent && onWarning) {
            warningSent = true;
            if (qman_total_tasks > POOL_SIZE) 
                onWarning(QMAN_WARN_STATIC_LIMIT);
        }
        insert(f, delay, false);
    }

    /**
     * Reschedules the current task (Low Priority / FIFO at same time quantum)
     */
    void Sleep(uint32_t delay) {
        insert(currentTask, delay, true);
    }

    /**
     * Main dispatcher. Processes ready tasks and manages execution context.
     *ts: current system time in ticks.
     */
    void Tick(uint32_t ts) {
        static bool insideTick = false;
        if (insideTick) return; // Prevent recursive calls

        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            now = ts;
            if (count == 0) return;
            
            // Tasks are sorted by time; most urgent is at the end of the array
            if ((int32_t)(now - pool[count - 1].nextRun) >= 0) {
                 currentTask = pool[--count].func;
                 lastTaskStart = now; 
            }
        }
        
        if (currentTask) {
            insideTick = true;
            currentTask();
            insideTick = false;
            currentTask = nullptr;
        }
    }

    /**
     * Returns the timestamp when the current task was popped from the queue.
     */
    uint32_t Started() { 
         ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            return lastTaskStart; 
         }
    }

    /**
     * Returns number of tasks currently in the queue.
     */
    uint8_t Len() { 
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { return count; } 
    }
    
    void OnError(ErrorCallback cb) { onError = cb; }
    void OnWarning(WarningCallback cb) { onWarning = cb; }
};

// Global instance declaration
typedef QMan_Generic<QMAN_POOL_SIZE> QMan;
extern QMan qman;

#endif // QMAN_H
