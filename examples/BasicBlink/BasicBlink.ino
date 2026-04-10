#include <QMan.h>

/**
 * BasicBlink Example
 * Demonstrates complex blink sequences using QMan DSL macros.
 * This task blinks 3 times fast, then 3 times slow, without blocking other tasks.
 */

#if defined(ARDUINO_ARCH_STM32)
#define LED_BUILTIN PC13
#endif

QMAN_TASK(taskBlinkSequence) {
    // Static variables persist between task re-entries
    static uint8_t i;

    QMAN_INIT {
        Serial.println("INIT");
        // Initial configuration runs only once
        pinMode(LED_BUILTIN, OUTPUT);
    }

    QMAN_LOOP {
        Serial.println("LOOP");
        // --- 3 Fast Blinks ---
        for (i = 0; i < 3; i++) {
            Serial.print("i1:");
            Serial.println(i);
            digitalWrite(LED_BUILTIN, HIGH);
            QMAN_SLEEP(100_ms); 
            
            digitalWrite(LED_BUILTIN, LOW);
            QMAN_SLEEP(100_ms);
        }

        QMAN_SLEEP(1000_ms); // Wait 1 second

        // --- 3 Slow Blinks ---
        for (i = 0; i < 3; i++) {
            Serial.print("i2:");
            Serial.println(i);
            digitalWrite(LED_BUILTIN, HIGH);
            QMAN_SLEEP(500_ms);
            
            digitalWrite(LED_BUILTIN, LOW);
            QMAN_SLEEP(500_ms);
        }

        QMAN_SLEEP(1000_ms); // Wait 1 second
    }
}

void setup() {
    // Initialize serial for debugging if needed
    Serial.begin(115200);
    Serial.println("SETUP");
    // Start the sequence task immediately
    qman.Setup();
    qman.Go(taskBlinkSequence);
}

void loop() {
    // Drive the QMan engine
    QMAN_TICK();
}
