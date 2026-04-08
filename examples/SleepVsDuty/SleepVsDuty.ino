#include <QMan.h>

/**
 * SleepVsDuty Example
 * Demonstrates the difference between relative sleep and fixed-frequency duty.
 * 
 * taskSleep: The interval "drifts" because it adds 1s AFTER the work is done.
 * taskDuty: Maintains a strict 1s period regardless of how long the work takes.
 */

// --- Task 1: Relative Timing (Drifting) ---
QMAN_TASK(taskSleep) {
    QMAN_INIT { Serial.println(F("Sleep Task: Started")); }

    QMAN_LOOP {
        Serial.print(F("Sleep Start: ")); Serial.println(micros());
        
        delay(100); // Simulate some heavy work (100ms)
        
        // SLEEP will wait 1000ms STARTING FROM NOW.
        // Total period will be ~1100ms.
        QMAN_SLEEP_MS(1000); 
    }
}

// --- Task 2: Fixed Timing (Compensating) ---
QMAN_TASK(taskDuty) {
    QMAN_INIT { Serial.println(F("Duty Task: Started")); }

    QMAN_LOOP {
        Serial.print(F("Duty  Start: ")); Serial.println(micros());
        
        delay(100); // Simulate the same heavy work (100ms)
        
        // DUTY will wait 900ms, because 100ms was already spent on work.
        // Total period will be EXACTLY 1000ms.
        QMAN_DUTY_MS(1000); 
    }
}

void setup() {
    Serial.begin(115200);
    while(!Serial);
    
    // Start both tasks to compare their timestamps in Serial
    QMAN_GO(taskSleep, 0);
    QMAN_GO(taskDuty, 0);
}

void loop() {
    QMAN_TICK();
}
