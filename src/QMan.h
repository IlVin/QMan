/**
 * QMan (Queue Manager) - Ultra-lightweight cooperative task queue manager for Arduino.
 * Author: IlVin
 * Version: 1.0.7-beta
 * License: MIT
 *
 * This library provides a way to run multiple tasks seemingly at the same time
 * without using a complex Operating System. It's perfect for small microcontrollers.
 */

#ifndef QMAN_H
#define QMAN_H

#include <Arduino.h>

// --- Universal Atomic Guard ---
// RAII-style: disable interrupts on creation, enable on destruction.
// Safe for STM32, AVR, and others. Works even if 'return' is used.
struct QManGuard {
    QManGuard()  { noInterrupts(); }
    ~QManGuard() { interrupts();   }
};

// --- Includes ---
#if defined(__AVR__)
  #include <avr/io.h>
  #include <avr/wdt.h>
  #include <avr/interrupt.h>
#endif

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
    QMAN_WARN_STATIC_LIMIT = 2,
    QMAN_WARN_TICK_LIMIT   = 3,
    QMAN_WARN_DUTY_LATE    = 4
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
    uint32_t nextRun;   // When to run it (in system ticks)
};

// --- Hardware Setup ---
#if defined(ARDUINO_ARCH_STM32)
  inline void qman_setup_hw() {
    // Enable DWT cycle counter on STM32
    if (!(DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk)) {
      CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
      DWT->CYCCNT = 0;
      DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    }
  }
#else
  // Dummy function for AVR and other platforms
  inline void qman_setup_hw() {}
#endif

// --- Time Conversion ---

// Maximum allowed duration of one tick in microseconds.
// Smaller = more precision (SoftPWM), Larger = longer max sleep (Pumps).
#ifndef QMAN_TICK_MAX_US
#define QMAN_TICK_MAX_US 32
#endif

// Helper macros to convert time to internal Ticks (system "steps")
#if defined(__AVR__)
  // AVR raw unit is (64e6 / F_CPU) us. Usually 4us or 8us.
  #define QMAN_RAW_US (64000000UL / F_CPU)
  #define QMAN_TICK_SHIFT (QMAN_TICK_MAX_US >= (QMAN_RAW_US * 16) ? 4 : \
                           QMAN_TICK_MAX_US >= (QMAN_RAW_US * 8) ? 3 : 2)
  #define Q_US(us) ((uint32_t)(us) / (QMAN_RAW_US << QMAN_TICK_SHIFT))
  #define Q_TICKS_TO_US(t) ((uint32_t)(t) * (QMAN_RAW_US << QMAN_TICK_SHIFT))

#elif defined(ARDUINO_ARCH_STM32)
  // STM32: Tick = 2^SHIFT cycles.
  #define QMAN_CPU_MHZ (F_CPU / 1000000UL)
  #define QMAN_TICK_SHIFT ( (QMAN_TICK_MAX_US * QMAN_CPU_MHZ >= 4096) ? 12 : \
                            (QMAN_TICK_MAX_US * QMAN_CPU_MHZ >= 2048) ? 11 : 10)
  #define Q_US(us) (((uint32_t)(us) * (F_CPU / 1000000UL)) >> QMAN_TICK_SHIFT)
  #define Q_TICKS_TO_US(t) (((uint32_t)(t) << QMAN_TICK_SHIFT) / (F_CPU / 1000000UL))

#else
  // Generic fallback for other platforms
  #define QMAN_TICK_SHIFT (QMAN_TICK_MAX_US >= 64 ? 6 : 5)
  #define Q_US(us) ((uint32_t)(us) >> QMAN_TICK_SHIFT)
  #define Q_TICKS_TO_US(t) ((uint32_t)(t) << QMAN_TICK_SHIFT)
#endif

// 1 millisecond is just 1000 microseconds. Easy!
#define Q_MS(ms) Q_US((uint32_t)(ms) * 1000UL)
#define Q_TICKS_TO_MS(t) (Q_TICKS_TO_US(t) / 1000UL)

#define Q_NOW_MS() Q_TICKS_TO_MS(qman.Now())
#define Q_NOW_US() Q_TICKS_TO_US(qman.Now())

// Strong type for time to prevent passing raw numbers
struct QTime {
    uint32_t ticks;
    // Explicit constructor: only our literals can create this easily
    explicit constexpr QTime(uint32_t t) : ticks(t) {}
};

// User-defined literals: the only "legal" way to create QTime
constexpr QTime operator"" _ms(unsigned long long ms) { return QTime(Q_MS(ms)); }
constexpr QTime operator"" _us(unsigned long long us) { return QTime(Q_US(us)); }
constexpr QTime operator"" _tick(unsigned long long t) { return QTime((uint32_t)t); }

// --- Internal Time Functions ---
inline uint32_t qman_get_delta_ticks() {
    static uint32_t lastRaw = 0;
    uint32_t currRaw;

#if defined(__AVR__)
    uint32_t m;
    uint8_t t;
    {
        QManGuard guard; // Safe RAII-style interrupt disable
        extern volatile unsigned long timer0_overflow_count;
        m = timer0_overflow_count;
        t = TCNT0;
        // Check if Timer0 overflowed but interrupt hasn't fired yet
        if ((TIFR0 & _BV(TOV0)) && (t < 255)) m++;
    }
    currRaw = (m << 8) | t;
#elif defined(ARDUINO_ARCH_STM32)
    currRaw = DWT->CYCCNT;
#else
    currRaw = micros();
#endif

    uint32_t deltaTicks = (currRaw - lastRaw) >> QMAN_TICK_SHIFT;
    lastRaw += (deltaTicks << QMAN_TICK_SHIFT);
    return deltaTicks;
}

// --- DSL (Domain Specific Language) Macros ---
#define QMAN_PASTE(a, b) a ## b
#define QMAN_JOIN(a, b) QMAN_PASTE(a, b)

/**
 * Define a new task. 
 * Use it like: QMAN_TASK(myBlinkTask) { ... }
 */
#define QMAN_TASK(name) \
    static QManCounter QMAN_JOIN(qmc_, __COUNTER__); \
    void name()

/**
 * Initialization block inside a task. 
 * Code here runs only ONCE when the task starts for the first time.
 * On subsequent restarts (after STOP + GO), jumps directly to QMAN_LOOP.
 */
#define QMAN_INIT \
    static void* __resumeAddr = nullptr; \
    if (__resumeAddr) { \
        void* __tmp = __resumeAddr; \
        __resumeAddr = &&__loop_entry; \
        goto *__tmp; \
    } \
    __resumeAddr = &&__loop_entry; \
    if (true)

/**
 * Main loop of the task. 
 * The task will keep repeating this block until it is stopped.
 */
#define QMAN_LOOP \
    __loop_entry: \
    for (; (__resumeAddr = nullptr, true); )

/**
 * Pause the task for a specific delay. 
 * Other tasks will run while this one is "sleeping".
 */
#define QMAN_SLEEP_1()      QMAN_SLEEP_2(0_tick)
#define QMAN_SLEEP_2(qtime) \
    QMAN_SLEEP_RAW(qtime, __COUNTER__)

#define QMAN_SLEEP_RAW(qtime, tag) \
    do { \
        __resumeAddr = (void*)&&QMAN_JOIN(L_, tag); \
        qman.Sync(qman_get_delta_ticks()); \
        qman.Sleep(qtime); \
        return; \
        QMAN_JOIN(L_, tag): ; \
    } while (0)

// Magic dispatch for 0 or 1 argument
#define QMAN_GET_SLEEP_MACRO(_0, _1, NAME, ...) NAME
#define QMAN_SLEEP(...) QMAN_GET_SLEEP_MACRO(0, ##__VA_ARGS__, QMAN_SLEEP_2, QMAN_SLEEP_1)(__VA_ARGS__)

/**
 * Precise timing: ensures the task runs exactly every 'period' ticks, 
 * accounting for the time the task itself took to execute.
 */
#define QMAN_DUTY(qtime) \
    QMAN_DUTY_RAW(qtime, __COUNTER__)

#define QMAN_DUTY_RAW(qtime, tag) \
    do { \
        __resumeAddr = (void*)&&QMAN_JOIN(L_, tag); \
        qman.Sync(qman_get_delta_ticks()); \
        uint32_t elapsed = qman.Now() - qman.Started(); \
        uint32_t wait = (elapsed < qtime.ticks) ? (qtime.ticks - elapsed) : 0; \
        qman.Duty(QTime(wait)); \
        return; \
        QMAN_JOIN(L_, tag): ; \
    } while (0)

// --- Smart GO Macros ---
// Launch with time: QMAN_GO(task, 100_ms)
#define QMAN_GO_2(task, qtime) qman.Go(task, qtime)
// Launch immediately: QMAN_GO(task)
#define QMAN_GO_1(task)        qman.Go(task)

// Magic dispatch for 1 or 2 arguments
#define QMAN_GET_GO_MACRO(_1, _2, NAME, ...) NAME
#define QMAN_GO(...) QMAN_GET_GO_MACRO(__VA_ARGS__, QMAN_GO_2, QMAN_GO_1)(__VA_ARGS__)

// --- The Core Manager Class ---

template <uint8_t POOL_SIZE>
class QMan_Generic {
private:
    Task pool[POOL_SIZE];       // Array of tasks waiting to run
    uint8_t count;              // Number of tasks currently in the pool
    uint32_t now;               // Current internal time (in system ticks)
    uint32_t lastTaskStart;     // When the current task was started
    TaskFunc currentTask;       // Pointer to the task being executed right now
    ErrorCallback onError;      // Function to call if something goes wrong
    WarningCallback onWarning;  // Function to call for system warnings

#ifdef QMAN_PROF
    // Performance Metrics (Double Buffered)
    uint32_t lastStatsReset;    // Time of last metrics reset
    uint16_t curLateCount, curMaxLatence;  // Accumulators for current window
    uint16_t pubLateCount;                 // Published count for previous second
    uint32_t pubMaxLatence;                // Published max jitter in ticks
#endif

    // Callback type for place-check functions used by schedule_task
    typedef bool (*PlaceCheckFunc)(int32_t diff, uint32_t delay, uint32_t nextRun, uint32_t now);

    // Single helper for go/sleep/duty. The compiler will inline this with constant callbacks.
    static inline uint8_t schedule_task(TaskFunc f, uint32_t delay, PlaceCheckFunc isRightPlace,
                                     uint32_t& now, uint8_t& count, Task* pool) {
        if (f == nullptr) return 0;
        QManGuard guard;
        uint32_t nextRun = now + delay;

        if (count >= POOL_SIZE) return QMAN_ERR_POOL_OVERFLOW;

        int8_t target = count;
        int8_t dupIdx = -1;

        while (target > 0) {
            int32_t diff = (int32_t)(pool[target - 1].nextRun - nextRun);

            if (pool[target - 1].func == f) {
                dupIdx = target - 1;
                if (isRightPlace(diff, delay, nextRun, now)) {
                    pool[dupIdx].nextRun = nextRun;
                    return 0;
                }
            }

            if (isRightPlace(diff, delay, nextRun, now)) break;
            target--;
        }

        if (dupIdx != -1) {
            if (dupIdx < target) {
                memmove(&pool[dupIdx], &pool[dupIdx + 1], sizeof(Task) * (target - dupIdx - 1));
                target--;
            } else if (dupIdx > target) {
                memmove(&pool[target + 1], &pool[target], sizeof(Task) * (dupIdx - target));
            }
            pool[target] = {f, nextRun};
        } else {
            if (target < count) {
                memmove(&pool[target + 1], &pool[target], sizeof(Task) * (count - target));
            }
            pool[target] = {f, nextRun};
            count++;
        }
        return 0;
    }

    // Place-check callbacks. Static inline so the compiler knows they are constant.
    static inline bool go_place_timer(int32_t diff, uint32_t delay, uint32_t nextRun, uint32_t now) {
        (void)delay; (void)nextRun; (void)now;
        return diff > 0;
    }

    static inline bool go_place_event(int32_t diff, uint32_t delay, uint32_t nextRun, uint32_t now) {
        (void)delay; (void)nextRun; (void)now;
        return diff >= 0;
    }

    static inline bool sleep_place(int32_t diff, uint32_t delay, uint32_t nextRun, uint32_t now) {
        (void)delay; (void)nextRun; (void)now;
        return diff > 0;
    }

    static inline bool duty_place(int32_t diff, uint32_t delay, uint32_t nextRun, uint32_t now) {
        (void)delay;
        return (diff > 0) || (diff == 0 && (int32_t)(nextRun - now) > 0);
    }


#ifdef QMAN_PROF
    void update_metrics(int32_t diff) {
        if ((uint32_t)(now - lastStatsReset) >= Q_MS(1000)) {
            // Publish accumulated data for the previous second
            pubLateCount = curLateCount;
            pubMaxLatence = curMaxLatence;
            // Reset accumulators
            curLateCount = 0;
            curMaxLatence = 0;
            lastStatsReset = now;
        }
        if (diff <= 0) {
            curLateCount++;
            uint32_t latence = (uint32_t)(-diff);
            if (latence > curMaxLatence) curMaxLatence = latence;
        }
    }
#endif

    void check_warnings() {
        static bool warningSent = false;
        if (warningSent || !onWarning) return;
        warningSent = true;
        if (qman_total_tasks > POOL_SIZE) {
            onWarning(QMAN_WARN_STATIC_LIMIT);
        }
        if (Q_TICKS_TO_US(1) > QMAN_TICK_MAX_US) {
            onWarning(QMAN_WARN_TICK_LIMIT);
        }
    }

public:
    QMan_Generic() : now(0), lastTaskStart(0), count(0), currentTask(nullptr), 
                     onError(nullptr), onWarning(nullptr) 
#ifdef QMAN_PROF
                     , lastStatsReset(0), curLateCount(0), curMaxLatence(0), 
                     pubLateCount(0), pubMaxLatence(0)
#endif
    {
    }

    /**
     * Advance internal time by delta ticks. Called before Tick().
     */
    void Sync(uint32_t delta) {
        QManGuard guard;
        now += delta;
    }

    /**
     * Call this at the end of setup() to sync internal clock
     * with hardware timers before the first loop.
     */
    void Setup() {
        // We don't care about the first delta, we just use it
        // to set the baseline for 'now' and 'lastRaw'.
        qman_setup_hw();
        Sync(qman_get_delta_ticks());
    }

    /**
     * Start a task. If it's already in the pool, it will be rescheduled.
     */
    void Go(TaskFunc f, QTime time) {
        check_warnings();
        uint8_t err = schedule_task(f, time.ticks, (time.ticks > 0) ? go_place_timer : go_place_event, now, count, pool);
        if (err && onError) onError(err);
    }
    void Go(TaskFunc f) {
        check_warnings();
        uint8_t err = schedule_task(f, 0, go_place_event, now, count, pool);
        if (err && onError) onError(err);
    }

    /**
     * Used by the system to reschedule a task after QMAN_SLEEP.
     */
    void Sleep(QTime time) {
        uint8_t err = schedule_task(currentTask, time.ticks, sleep_place, now, count, pool);
        if (err && onError) onError(err);
    }

    /**
     * Used by the system to reschedule a task after QMAN_DUTY.
     */
    void Duty(QTime time) {
#ifdef QMAN_PROF
        int32_t diff_now = (int32_t)((now + time.ticks) - now);
        update_metrics(diff_now);
        static bool warningSent = false;
        if (diff_now <= 0 && onWarning && !warningSent) {
            warningSent = true;
            onWarning(QMAN_WARN_DUTY_LATE);
        }
#endif
        uint8_t err = schedule_task(currentTask, time.ticks, duty_place, now, count, pool);
        if (err && onError) onError(err);
    }

    /**
     * The Heartbeat: Checks the queue and runs one ready task.
     * Call this in loop(). Only ONE task runs per call.
     */
    void Tick() {
        static bool insideTick = false;

        if (insideTick || count == 0) return;

        TaskFunc taskToRun = nullptr;
        {
            QManGuard guard;

            // Check the most urgent task (rightmost in the array)
            if ((int32_t)(now - pool[count - 1].nextRun) >= 0) {
                taskToRun = pool[count - 1].func;
                count--;  // Remove task from queue before execution
            }
        }

        if (taskToRun) {
            insideTick = true;
            currentTask = taskToRun;
            lastTaskStart = now;
            taskToRun();
            insideTick = false;
            currentTask = nullptr;
        }
    }

    uint32_t Started() { QManGuard g; return lastTaskStart; }
    uint32_t Now()     { QManGuard g; return now; }
    uint8_t Len()      { QManGuard g; return count; }
    const Task* Queue() const { return pool; }
    void Clear() { QManGuard g; count = 0; now = 0; currentTask = nullptr; }

    void OnError(ErrorCallback cb)     { onError = cb; }
    void OnWarning(WarningCallback cb) { onWarning = cb; }

#ifdef QMAN_PROF
    uint16_t GetLateCount() { QManGuard g; return pubLateCount; }
    uint32_t GetMaxLatenceMS() { QManGuard g; return Q_TICKS_TO_MS(pubMaxLatence); }
    uint32_t GetMaxLatenceUS() { QManGuard g; return Q_TICKS_TO_US(pubMaxLatence); }
#endif
};

// --- Global Instance and Auto-Timing ---

typedef QMan_Generic<QMAN_POOL_SIZE> QMan;
extern QMan qman;

// --- Tick Dispatcher Macros ---

// TICK with explicit delta in ticks
#define QMAN_TICK_1(delta) do { qman.Sync(delta); qman.Tick(); } while(0)
// Auto TICK using hardware timers
#define QMAN_TICK_0()      do { qman.Sync(qman_get_delta_ticks()); qman.Tick(); } while(0)

// Magic dispatch to select between QMAN_TICK() and QMAN_TICK(delta)
#define QMAN_GET_TICK_MACRO(_0, _1, NAME, ...) NAME
#define QMAN_TICK(...) QMAN_GET_TICK_MACRO(0, ##__VA_ARGS__, QMAN_TICK_1, QMAN_TICK_0)(__VA_ARGS__)

#endif // QMAN_H
