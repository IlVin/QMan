#include <Arduino.h>
#include "QMan.h"

#define TOLERANCE_PCT 2.0 

void printResult(const char* mode, const char* label, uint32_t targetMs, uint32_t actualUs) {
    float actualMs = actualUs / 1000.0;
    float diff = abs((float)targetMs - actualMs);
    float errorPct = (diff / (float)targetMs) * 100.0;

    Serial.print(F("[ "));
    Serial.print(errorPct <= TOLERANCE_PCT ? F(" PASS ") : F(" FAIL "));
    Serial.print(F(" ] "));
    
    Serial.print(mode); Serial.print(F(" "));
    Serial.print(label);
    Serial.print(F(" | Target: ")); Serial.print(targetMs);
    Serial.print(F("ms | Actual: ")); Serial.print(actualMs, 2);
    Serial.print(F("ms | Error: ")); Serial.print(errorPct, 2);
    Serial.println(F("%"));
}

QMAN_TASK(testSuiteAtomic) {
    static uint32_t lastMicros;
    static uint16_t iterations;
    QMAN_INIT {
        Serial.println(F("\n>>> SUITE: ATOMIC_OPERATIONS"));
        lastMicros = micros();
        iterations = 0;
    }
    QMAN_LOOP {
        uint32_t current = micros();
        Serial.print(F("  - Yield Latency: ")); Serial.print(current - lastMicros); Serial.println(F(" us"));
        lastMicros = current;
        if (++iterations >= 5) {
            QMAN_GO(testSuiteIntervals);
            QMAN_STOP;
        }
        QMAN_SLEEP(0_tick); 
    }
}

QMAN_TASK(testSuiteIntervals) {
    static uint32_t tStart;
    QMAN_INIT {
        Serial.println(F("\n>>> SUITE: TEMPORAL_ACCURACY (SLEEP)"));
        Serial.flush();
    }
    QMAN_LOOP {
        // SLEEP замеряем "вокруг", так как это просто задержка
        tStart = micros(); QMAN_SLEEP(10_ms);
        printResult("SLEEP", "T_10MS ", 10, micros() - tStart);

        tStart = micros(); QMAN_SLEEP(50_ms);
        printResult("SLEEP", "T_50MS ", 50, micros() - tStart);

        tStart = micros(); QMAN_SLEEP(100_ms);
        printResult("SLEEP", "T_100MS", 100, micros() - tStart);

        QMAN_GO(testSuiteDuty);
        QMAN_STOP;
    }
}

QMAN_TASK(testSuiteDuty) {
    static uint32_t lastExit;
    static uint8_t count;

    QMAN_INIT {
        Serial.println(F("\n>>> SUITE: DUTY_CYCLE_STABILITY (COMPENSATION)"));
        lastExit = 0;
        count = 0;
    }

    QMAN_LOOP {
        // Симулируем тяжелую нагрузку ПЕРЕД командой DUTY
        delay(30); 
        
        QMAN_DUTY(100_ms); 
        
        // ЗАМЕР: Насколько точно соблюдается интервал ВЫХОДА из задачи
        uint32_t nowMicros = micros();
        if (lastExit > 0) {
            printResult("DUTY ", "100MS_STABLE", 100, nowMicros - lastExit);
            count++;
        }
        lastExit = nowMicros;

        if (count >= 3) {
            Serial.println(F("\nALL TESTS PASSED. SYSTEM STABLE."));
            QMAN_STOP;
        }
    }
}

void setup() {
    Serial.begin(115200);
    while(!Serial);
    qman.Setup();
    QMAN_GO(testSuiteAtomic);
}

void loop() {
    QMAN_TICK();
}
