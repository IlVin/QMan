#include <QMan.h>

String results = "";
uint32_t timeA = 0;
uint32_t timeB = 0;

// Тестируем SLEEP: Должен прибавить задержку к текущему моменту (Now)
QMAN_TASK(testSleep) {
    QMAN_INIT {}
    QMAN_LOOP {
        qman.Tick(10_tick); // Имитируем работу 10 тиков
        QMAN_SLEEP(100_tick); 
    }
}

// Тестируем DUTY: Должен прибавить задержку к моменту старта (Started)
QMAN_TASK(testDuty) {
    QMAN_INIT {}
    QMAN_LOOP {
        qman.Tick(10_tick); // Имитируем работу 10 тиков
        QMAN_DUTY(100_tick); 
    }
}

void setup() {
    Serial.begin(115200);
    while(!Serial);
    qman.Setup();
    Serial.println(F("--- QMan SLEEP vs DUTY Logic Test ---"));

    // --- CASE 1: SLEEP ---
    qman.Clear();
    qman.Go(testSleep); // T=0
    qman.Tick(0_tick);  // Старт задачи. Now=0, Started=0.
    // Внутри задачи Now стал 10. Вызван SLEEP(100).
    // Ожидаем: nextRun = 10 + 100 = 110.
    timeA = qman.Queue()[qman.Len()-1].nextRun;
    
    Serial.print(F("SLEEP (Expected 110): ")); Serial.print(timeA);
    if (timeA == 110) Serial.println(F(" -> PASS"));
    else Serial.println(F(" -> FAIL"));

    // --- CASE 2: DUTY ---
    qman.Clear();
    qman.Go(testDuty);  // T=0
    qman.Tick(0_tick);  // Старт задачи. Now=0, Started=0.
    // Внутри задачи Now стал 10. Вызван DUTY(100).
    // Ожидаем: nextRun = 0 + 100 = 100. (Работа 10 тиков проигнорирована)
    timeB = qman.Queue()[qman.Len()-1].nextRun;

    Serial.print(F("DUTY  (Expected 100): ")); Serial.print(timeB);
    if (timeB == 100) Serial.println(F(" -> PASS"));
    else Serial.println(F(" -> FAIL"));

    Serial.println(F("-------------------------------------"));
}

void loop() {}
