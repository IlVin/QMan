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
typedef bool (*TaskFunc)(); // true = закончить и удалить, false = спать/уступить
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
    QMAN_WARN_TICK_LIMIT   = 3
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

// --- Setups ---
#if defined(ARDUINO_ARCH_STM32)
  inline void qman_setup_hw() {
    // Correct CMSIS names for STM32
    if (!(DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk)) {
      CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
      DWT->CYCCNT = 0;
      DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    }
  }
#else
  // Dummy function for AVR and others
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

// Literals: the only "legal" way to create QTime
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
    static QManCounter QMAN_JOIN(cnt_, name); \
    bool name()

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
    do { __resumeAddr = &&__loop_entry; return true; } while (0)

/**
 * Pause the task for a specific delay. 
 * Other tasks will run while this one is "sleeping".
 */
#define QMAN_SLEEP_1()      QMAN_SLEEP_2(0_tick)
#define QMAN_SLEEP_2(qtime) \
    do { \
        __resumeAddr = (void*)&&QMAN_JOIN(L_, __LINE__); \
        qman.Sleep(qtime); \
        return false; \
        QMAN_JOIN(L_, __LINE__): ; \
    } while (0)

// Исправленный "магический" выбор для 0 или 1 аргумента
#define QMAN_GET_SLEEP_MACRO(_0, _1, NAME, ...) NAME
#define QMAN_SLEEP(...) QMAN_GET_SLEEP_MACRO(0, ##__VA_ARGS__, QMAN_SLEEP_2, QMAN_SLEEP_1)(__VA_ARGS__)



/**
 * Precise timing: ensures the task runs exactly every 'period' ticks, 
 * accounting for the time the task itself took to execute.
 */
#define QMAN_DUTY(qtime) \
    do { \
        __resumeAddr = (void*)&&QMAN_JOIN(L_, __LINE__); \
        uint32_t current = qman.Now(); \
        uint32_t started = qman.Started(); \
        uint32_t elapsed = current - started; \
        uint32_t wait = (elapsed < qtime.ticks) ? (qtime.ticks - elapsed) : 0; \
        qman.Sleep(QTime(wait)); \
        return false; \
        QMAN_JOIN(L_, __LINE__): ; \
    } while (0)

    // --- Smart GO Macros ---
// Launch with time: QMAN_GO(task, 100_ms)
#define QMAN_GO_2(task, qtime) qman.Go(task, qtime)
// Launch immediately: QMAN_GO(task)
#define QMAN_GO_1(task)        qman.Go(task)

// Magic dispatcher for 1 or 2 arguments
#define QMAN_GET_GO_MACRO(_1, _2, NAME, ...) NAME
#define QMAN_GO(...) QMAN_GET_GO_MACRO(__VA_ARGS__, QMAN_GO_2, QMAN_GO_1)(__VA_ARGS__)


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
    // Вспомогательная функция для физического удаления по индексу
    void remove_at(uint8_t idx) {
        if (idx < count - 1) {
            memmove(&pool[idx], &pool[idx + 1], sizeof(Task) * (count - idx - 1));
        }
        count--;
    }

    void go(TaskFunc f, uint32_t delay) {
        if (f == nullptr) return;
        QManGuard guard;
        uint32_t nextRun = now + delay;

        for (uint8_t i = 0; i < count; i++) {
            if (pool[i].func == f) { remove_at(i); break; }
        }

        if (count >= POOL_SIZE) return;

        // Ищем с ПРАВОГО края (HEAD)
        int8_t target = count;
        while (target > 0) {
            int32_t diff = (int32_t)(pool[target - 1].nextRun - nextRun);
            if (delay > 0) {
                if (diff > 0) break; // Таймер: встаем ПРАВЕЕ тех, кто в будущем
            } else {
                if (diff >= 0) break; // Событие: встаем ПРАВЕЕ тех, кто в будущем или сейчас
            }
            target--;
        }

        if (target < count) {
            memmove(&pool[target + 1], &pool[target], sizeof(Task) * (count - target));
        }
        pool[target] = {f, nextRun};
        count++;
    }

    void sleep(TaskFunc f, uint32_t delay) {
        if (f == nullptr) return;
        QManGuard guard;
        uint32_t nextRun = now + delay;

        int8_t oldIdx = -1;
        for (uint8_t i = 0; i < count; i++) {
            if (pool[i].func == f) { oldIdx = i; break; }
        }
        if (oldIdx == -1) return;

        // Поиск нового места (всегда вежливый FIFO)
        int8_t target = count;
        while (target > 0) {
            if ((int32_t)(pool[target - 1].nextRun - nextRun) > 0) break;
            target--;
        }

        if (oldIdx < target) target--;

        if (oldIdx != target) {
            Task temp = pool[oldIdx];
            if (oldIdx > target) {
                memmove(&pool[target + 1], &pool[target], sizeof(Task) * (oldIdx - target));
            } else {
                memmove(&pool[oldIdx], &pool[oldIdx + 1], sizeof(Task) * (target - oldIdx));
            }
            pool[target] = temp;
        } else {
            pool[oldIdx].nextRun = nextRun;
        }
    }

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
    QMan_Generic() : now(0), lastTaskStart(0), count(0),
                               currentTask(nullptr), onError(nullptr), onWarning(nullptr) {
    }

    /**
     * Call this at the end of setup() to sync internal clock
     * with hardware timers before the first loop.
     */
    void Setup() {
        // We don't care about the first delta, we just use it
        // to set the baseline for 'now' and 'lastRaw'.
        qman_setup_hw();
        Tick(QTime(qman_get_delta_ticks()));
    }

    /**
     * Start a task. If it's already in the pool, it will be rescheduled.
     */
    void Go(TaskFunc f, QTime time) { check_warnings(); go(f, time.ticks); }
    void Go(TaskFunc f) { check_warnings(); go(f, 0); }

    /**
     * Used by the system to reschedule a task after QMAN_SLEEP.
     */
    void Sleep(QTime time) { sleep(currentTask, time.ticks); }


    /**
     * The Heartbeat: Checks the queue and runs any task that is ready.
     */
    void Tick(QTime time) {
        static bool insideTick = false;

        {
            QManGuard guard;
            now += time.ticks;
        }

        if (insideTick || count == 0) return;

        TaskFunc taskToRun = nullptr;
        {
            QManGuard guard;

            // Проверяем самую срочную (последнюю в массиве)
            if ((int32_t)(now - pool[count - 1].nextRun) >= 0) {
                currentTask = pool[count - 1].func; // Просто смотрим
                taskToRun = currentTask;
                lastTaskStart = now;
            }
        }

        if (taskToRun) {
            insideTick = true;
            bool shouldRemove = taskToRun(); // Выполняем
            insideTick = false;

            if (shouldRemove) {
                QManGuard guard;
                // Ищем и удаляем, так как за время работы
                // индекс мог измениться из-за GO в прерываниях
                for (int8_t i = count - 1; i >= 0; i--) { // Поиск с конца чуть быстрее
                    if (pool[i].func == taskToRun) {
                        remove_at(i);
                        break;
                    }
                }
            }
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
};

// --- Global Instance and Auto-Timing ---

typedef QMan_Generic<QMAN_POOL_SIZE> QMan;
extern QMan qman;

// --- Tick Dispatcher Macros ---

// Base TICK with delta ticks
#define QMAN_TICK_1(delta) qman.Tick(QTime(delta))
// Auto TICK using hardware timers
#define QMAN_TICK_0()      qman.Tick(QTime(qman_get_delta_ticks()))

// Magic to select between QMAN_TICK() and QMAN_TICK(delta)
#define QMAN_GET_TICK_MACRO(_0, _1, NAME, ...) NAME
#define QMAN_TICK(...) QMAN_GET_TICK_MACRO(0, ##__VA_ARGS__, QMAN_TICK_1, QMAN_TICK_0)(__VA_ARGS__)

#endif // QMAN_H
