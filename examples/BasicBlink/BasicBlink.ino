#include <QMan.h>

/**
 * BasicBlink Example
 * Demonstrates complex blink sequences using QMan DSL macros.
 * This task blinks 3 times fast, then 3 times slow, without blocking other tasks.
 */

QMAN_TASK(taskBlinkSequence) {
    // Static variables persist between task re-entries
    static uint8_t i;

    QMAN_INIT {
        // Initial configuration runs only once
        pinMode(LED_BUILTIN, OUTPUT);
    }

    QMAN_LOOP {
        // --- 3 Fast Blinks ---
        for (i = 0; i < 3; i++) {
            digitalWrite(LED_BUILTIN, HIGH);
            QMAN_SLEEP_MS(100); 
            
            digitalWrite(LED_BUILTIN, LOW);
            QMAN_SLEEP_MS(100);
        }

        QMAN_SLEEP_MS(1000); // Wait 1 second

        // --- 3 Slow Blinks ---
        for (i = 0; i < 3; i++) {
            digitalWrite(LED_BUILTIN, HIGH);
            QMAN_SLEEP_MS(500);
            
            digitalWrite(LED_BUILTIN, LOW);
            QMAN_SLEEP_MS(500);
        }

        QMAN_SLEEP_MS(1000); // Wait 1 second
    }
}

void setup() {
    // Initialize serial for debugging if needed
    Serial.begin(115200);
    
    // Start the sequence task immediately
    QMAN_GO(taskBlinkSequence, 0);
}

void loop() {
    // Drive the QMan engine
    QMAN_TICK();
}
