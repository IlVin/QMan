#include <LiquidCrystal.h>
#include <QMan.h>

/**
 * AsyncLCD Example
 * Demonstrates how to handle slow LCD hardware without blocking other tasks.
 * The LED blinks smoothly even while the text is being printed character by character.
 */

// LCD pins configuration (adjust for your wiring)
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// --- Task 1: Non-blocking LCD Output ---
QMAN_TASK(taskLcdOutput) {
    static const char* message = "QMan is Running!";
    static uint8_t i;

    QMAN_INIT {
        lcd.begin(16, 2);
        lcd.clear();
    }

    QMAN_LOOP {
        lcd.setCursor(0, 0);
        
        for (i = 0; message[i] != '\0'; i++) {
            lcd.write(message[i]); // Print one character
            
            // CRITICAL: Yield control to the scheduler.
            // This allows taskBlink to run even during the string output.
            QMAN_SLEEP(0); 
        }

        QMAN_SLEEP_MS(2000); // Wait before clearing
        lcd.clear();
        QMAN_SLEEP_MS(500);
    }
}

// --- Task 2: Smooth Heartbeat LED ---
QMAN_TASK(taskBlink) {
    QMAN_INIT {
        pinMode(LED_BUILTIN, OUTPUT);
    }

    QMAN_LOOP {
        digitalWrite(LED_BUILTIN, HIGH);
        QMAN_SLEEP_MS(100);
        digitalWrite(LED_BUILTIN, LOW);
        QMAN_SLEEP_MS(900);
    }
}

void setup() {
    // Start both tasks
    QMAN_GO(taskLcdOutput, 0);
    QMAN_GO(taskBlink, 0);
}

void loop() {
    // Drive the engine
    QMAN_TICK();
}
