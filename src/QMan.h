/**
 * QMan (Queue Manager) - Ultra-lightweight cooperative task queue manager for Arduino.
 * Author: IlVin
 * Version: 1.0.1-beta
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

enum QManError {
    QMAN_ERR_POOL_OVERFLOW = 1,
    QMAN_WARN_STATIC_LIMIT = 2
};

#ifndef QMAN_POOL_SIZE
#define QMAN_POOL_SIZE 16
#endif

extern uint8_t qman_total_tasks;
struct QManCounter { QManCounter() { qman_total_tasks++; } };

struct Task {
    TaskFunc func;
    uint32_t nextRun;
};

#define QMAN_PASTE(a, b) a ## b
#define QMAN_JOIN(a, b) QMAN_PASTE(a, b)

// --- DSL Macros ---
#define QMAN_TASK(name) \
    static QManCounter QMAN_JOIN(cnt_, name); \
    void name()

#define QMAN_INIT \
    static void* __resumeAddr = nullptr; \
    if (__resumeAddr) goto *__resumeAddr; \
    if (true)

#define QMAN_LOOP \
    __loop_entry: \
    for (; (__resumeAddr = nullptr, true); )

#define QMAN_STOP \
    do { \
        __resumeAddr = &&__loop_entry; \
        return; \
    } while (0)

// Time quantum: 32 microseconds
#define Q_US(us) ((uint32_t)(us) >> 5)
#define Q_MS(ms) ((uint32_t)(ms) * 31250UL / 1000UL)

#define QMAN_GO_MS(task, ms) qman.Go(task, Q_MS(ms))
#define QMAN_GO_US(task, us) qman.Go(task, Q_US(us))
#define QMAN_SLEEP_MS(ms)    QMAN_SLEEP(Q_MS(ms))
#define QMAN_SLEEP_US(us)    QMAN_SLEEP(Q_US(us))
#define QMAN_DUTY_MS(ms)     QMAN_DUTY(Q_MS(ms))
#define QMAN_DUTY_US(us)     QMAN_DUTY(Q_US(us))

#define QMAN_SLEEP(delay) \
    do { \
        __resumeAddr = (void*)&&QMAN_JOIN(L_, __LINE__); \
        qman.Sleep(delay); \
        return; \
        QMAN_JOIN(L_, __LINE__): ; \
    } while (0)

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

// --- Advanced Timing Engine ---
extern uint32_t qman_last_micros;
extern uint32_t qman_task_ticks;

/**
 * Automatically calculates elapsed time since last call and drives the manager.
 * Uses 32-bit arithmetic to handle micros() rollover seamlessly.
 */
inline void qman_auto_tick() {
    uint32_t currentMicros = micros();
    // First call initialization
    if (qman_last_micros == 0) {
        qman_last_micros = currentMicros;
        return;
    }
    // Calculate delta and accumulate ticks
    uint32_t delta = currentMicros - qman_last_micros;
    if (delta >= 32) {
        uint32_t t = delta >> 5;
        qman_task_ticks += t;           // Linearly growing 32-bit counter
        qman_last_micros += (t << 5);   // Preserve remaining microseconds
    }
    qman.Tick(qman_task_ticks);
}

// Macro overloading: QMAN_TICK() for auto-mode, QMAN_TICK(ts) for manual/test mode
#define QMAN_GET_TICK_MACRO(_1, NAME, ...) NAME
#define QMAN_TICK(...) QMAN_GET_TICK_MACRO(__VA_ARGS__, qman.Tick, qman_auto_tick)(__VA_ARGS__)

template <uint8_t POOL_SIZE>
class QMan_Generic {
private:
    Task pool[POOL_SIZE];
    uint8_t count;
    uint32_t now;
    uint32_t lastTaskStart;
    TaskFunc currentTask;
    ErrorCallback onError;
    WarningCallback onWarning;

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
            if (oldIdx == -1) {
                if (count >= POOL_SIZE) {
                    if (onError) onError(QMAN_ERR_POOL_OVERFLOW);
                    wdt_enable(WDTO_120MS);
                    cli(); while(1);
                }
                count++;
            }
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

    void Go(TaskFunc f, uint32_t delay = 0) {
        static bool warningSent = false;
        if (!warningSent && onWarning) {
            warningSent = true;
            if (qman_total_tasks > POOL_SIZE) onWarning(QMAN_WARN_STATIC_LIMIT);
        }
        insert(f, delay, false);
    }

    void Sleep(uint32_t delay) { insert(currentTask, delay, true); }

    void Tick(uint32_t ts) {
        static bool insideTick = false;
        if (insideTick) return;
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
            currentTask();
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

typedef QMan_Generic<QMAN_POOL_SIZE> QMan;
extern QMan qman;

#endif
