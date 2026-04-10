#include <QMan.h>

/**
 * TaskChaining Example
 * Demonstrates task interaction and proper termination using QMAN_STOP.
 */

// --- Task 2: The Worker (Blinks 5 times and resets) ---
QMAN_TASK(taskWorker) {
    static uint8_t count;
    
    QMAN_INIT {
        // This runs only ONCE when the firmware starts
        pinMode(LED_BUILTIN, OUTPUT);
    }

    QMAN_LOOP {
        Serial.println(F("Worker: Execution started/restarted!"));
        count = 0;

        for (count = 0; count < 5; count++) {
            digitalWrite(LED_BUILTIN, HIGH);
            QMAN_SLEEP(100_ms);
            digitalWrite(LED_BUILTIN, LOW);
            QMAN_SLEEP(100_ms);
        }

        Serial.println(F("Worker: Sequence complete. Self-terminating..."));
        
        // QMAN_STOP saves the LOOP entry address to __resumeAddr and returns.
        // When QMAN_GO triggers this task again, it will start from the top of QMAN_LOOP.
        QMAN_STOP; 
    }
}

// --- Task 1: The Boss (Monitors condition and triggers Worker) ---
QMAN_TASK(taskBoss) {
    static uint8_t secondsElapsed;

    QMAN_INIT {
        Serial.println(F("Boss: Monitoring started..."));
        secondsElapsed = 0;
    }

    QMAN_LOOP {
        Serial.print(F("Boss: Monitoring second "));
        Serial.println(++secondsElapsed);

        if (secondsElapsed >= 5) {
            Serial.println(F("Boss: Condition met! Starting Worker..."));
            
            // Trigger the worker task. It will always start from the top thanks to QMAN_STOP.
            QMAN_GO(taskWorker);
            
            secondsElapsed = 0;
        }

        QMAN_SLEEP(1000_ms);
    }
}

void setup() {
    Serial.begin(115200);
    while(!Serial);
    
    // Start only the Boss
    QMAN_GO(taskBoss);
}

void loop() {
    QMAN_TICK();
}
