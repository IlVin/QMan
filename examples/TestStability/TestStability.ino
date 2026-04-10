#include <QMan.h>

// Определяем пин для диода (универсально)
#if defined(ARDUINO_ARCH_STM32)
  #define LED_PIN PC13
#else
  #define LED_PIN LED_BUILTIN
#endif

// Определяем допустимый джиттер micros() в зависимости от платформы
#if defined(__AVR__)
  #define MICROS_JITTER 5 // У AVR дискретность micros() = 4мкс
#else
  #define MICROS_JITTER 2 // У STM32 и прочих точность выше
#endif

// Вычисляем длительность одного тика в микросекундах
uint32_t oneTickUs = Q_TICKS_TO_US(1);

QMAN_TASK(taskUniversalTest) {
    static uint32_t startUs;
    static uint32_t targetUs = 1000000UL; // 1 секунда
    
    QMAN_INIT {
        pinMode(LED_PIN, OUTPUT);
        Serial.begin(115200);
        delay(1000);
        Serial.println(F("\n--- QMan Universal Timing Test ---"));
        
        // Выводим параметры системы
        Serial.print(F("Tick Shift: ")); Serial.println(QMAN_TICK_SHIFT);
        Serial.print(F("Single Tick (theoretical): ")); 
        Serial.print(Q_TICKS_TO_US(1)); Serial.println(F(" us"));
        Serial.print(F("Max allowed jitter (QMAN_TICK_MAX_US): ")); 
        Serial.print(QMAN_TICK_MAX_US); Serial.println(F(" us"));
        Serial.println(F("----------------------------------"));
    }

    QMAN_LOOP {
        // --- Тестируем SLEEP ---
        startUs = micros();
        
        QMAN_SLEEP(1000_ms); 
        
        uint32_t actualUs = micros() - startUs;
        int32_t drift = (int32_t)actualUs - (int32_t)targetUs;
        uint32_t oneTick = Q_TICKS_TO_US(1);

        Serial.print(F("[RESULT] Delay 1000ms. Drift: "));
        Serial.print(drift);
        Serial.print(F(" us. Status: "));

        // Проверка: дрейф не должен быть отрицательным (раннее пробуждение)
        // и не должен превышать длительность одного тика + время выполнения Tick()
        if (drift >= -(int32_t)MICROS_JITTER && drift <= (int32_t)(oneTickUs + MICROS_JITTER + 10)) {
            // 10us - микрозапас на выполнение кода самой проверки
            Serial.println(F("PASS (Perfect)"));
        } else if (drift < -(int32_t)MICROS_JITTER) {
            Serial.print(F("FAIL (Too early: ")); 
            Serial.print(drift); 
            Serial.println(F(" us)"));
        } else {
            Serial.print(F("WARN (Jitter > 1 Tick: "));
            Serial.print(drift);
            Serial.println(F(" us)"));
        }

        digitalWrite(LED_PIN, !digitalRead(LED_PIN)); // Моргнуть по завершении цикла
        QMAN_SLEEP(2000_ms); // Пауза между тестами
    }
}

void setup() {
    qman.Setup();
    QMAN_GO(taskUniversalTest);
}

void loop() {
    QMAN_TICK();
}
