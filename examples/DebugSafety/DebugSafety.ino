#include <QMan.h>

/**
 * DebugSafety Example
 * Demonstrates how to use OnWarning and OnError callbacks to catch 
 * design flaws (too many tasks) and runtime overflows.
 */

// 1. Define a pool size of 5 for this test
#define QMAN_POOL_SIZE 5
#include <QMan.h>

// Initialize counter (required in main sketch)
uint8_t qman_total_tasks = 0;

// --- Safety Callbacks ---

void myWarningHandler(uint8_t code) {
    if (code == QMAN_WARN_STATIC_LIMIT) {
        Serial.println(F("--- WARNING ---"));
        Serial.print(F("Static tasks defined: "));
        Serial.println(qman_total_tasks);
        Serial.println(F("Exceeds POOL_SIZE! System may crash if all run at once."));
    }
}

void myErrorHandler(uint8_t code) {
    if (code == QMAN_ERR_POOL_OVERFLOW) {
        Serial.println(F("--- CRITICAL ERROR ---"));
        Serial.println(F("Queue Pool Overflow! Rebooting system..."));
        Serial.flush(); // Ensure message is sent before WDT reset
    }
}

// --- Tasks ---

// Generate 7 tasks to trigger OnWarning (since POOL_SIZE is 5)
#define DECLARE_DUMMY(n) QMAN_TASK(task_##n) { QMAN_INIT{} QMAN_LOOP{ QMAN_SLEEP_MS(1000); } }

DECLARE_DUMMY(1) DECLARE_DUMMY(2) DECLARE_DUMMY(3) DECLARE_DUMMY(4)
DECLARE_DUMMY(5) DECLARE_DUMMY(6) DECLARE_DUMMY(7)

TaskFunc taskList[] = { task_1, task_2, task_3, task_4, task_5, task_6, task_7 };

void setup() {
    Serial.begin(115200);
    while(!Serial);
    delay(1000);
    Serial.println(F("QMan Safety Debug Test Started"));

    // Register callbacks
    qman.OnWarning(myWarningHandler);
    qman.OnError(myErrorHandler);

    // Attempt to start more tasks than POOL_SIZE (5)
    for (uint8_t i = 0; i < 7; i++) {
        Serial.print(F("Starting task #")); Serial.println(i + 1);
        QMAN_GO(taskList[i], 0);
        
        Serial.print(F("Tasks in queue: ")); Serial.println(qman.Len());
        delay(500);
    }
}

void loop() {
    QMAN_TICK();
}
