#include <QMan.h>

String results = "";

// Одноразовые задачи для теста хронологии
bool oneA() { results += "A"; return true; }
bool oneB() { results += "B"; return true; }
bool oneC() { results += "C"; return true; }

// Задача A, которая один раз проспит 3 тика и завершится
bool taskA_Sleep3() {
    QMAN_INIT {
        results += "A";
        // Мы НЕ МОЖЕМ просто написать qman.Sleep и идти дальше.
        // Мы должны использовать макрос, который сделает 'return false'.
        QMAN_SLEEP(3_tick); 
    }
    QMAN_LOOP {
        results += "a"; 
        QMAN_STOP; // Вернет 'true', и Tick удалит задачу
    }
}

// Задачи-заглушки (теперь возвращают bool!)
bool taskA() {
    QMAN_INIT {}
    QMAN_LOOP {
        results += "A";
        QMAN_SLEEP();
    }
}
bool taskB() {
    QMAN_INIT {}
    QMAN_LOOP {
        results += "B";
        QMAN_SLEEP();
    }
}
bool taskC() {
    QMAN_INIT {}
    QMAN_LOOP {
        results += "C";
        QMAN_SLEEP();
    }
}
bool taskU() {
    QMAN_INIT {}
    QMAN_LOOP {
        results += "!";
        QMAN_STOP;
    }
} // Urgent - удаляется сразу

void printStep(const char* msg) {
    qman.Clear(); 
    results = "";
    Serial.print(F("\nTEST: ")); Serial.println(msg);
}

void check(String expected) {
    if (results == expected) Serial.println(F("RESULT: PASS"));
    else {
        Serial.print(F("RESULT: FAIL! Expected ")); Serial.print(expected);
        Serial.print(F(", got ")); Serial.println(results);
    }
}

void setup() {
    Serial.begin(115200);
    while(!Serial);
    delay(1000);
    qman.Setup();

    Serial.println(F("--- QMan Deterministic Unit Tests ---"));

// --- CASE 1: Порядок вставки 0_tick (LIFO для событий) ---
    printStep("LIFO Order for Events (A,B,C)");
    qman.Go(taskA);
    qman.Go(taskB);
    qman.Go(taskC);
    // Теперь в голове C, потом B, потом A
    for(int i=0; i<6; i++) qman.Tick(0_tick);
    check("CBACBA"); // Исправили ожидание

    // --- CASE 2: Хронология (Старые долги) ---
    printStep("Chronology (Backlog first)");
    // Этот тест проходит, так как время важнее порядка вставки
    qman.Go(taskA, QTime(10)); 
    qman.Go(taskB, QTime(0));  
    qman.Tick(10_tick);
    qman.Tick(0_tick);
    check("BA");

    // --- CASE 3: Вклинивание GO (Приоритет) ---
    printStep("GO Priority (In-place)");
    qman.Go(taskA);
    qman.Go(taskB);
    qman.Tick(0_tick); // Выполнили B (так как он был HEAD). В пуле [B, A], HEAD=A.
    qman.Go(taskU);    // Вклиниваем Urgent. Он встает ВЫШЕ A.
    qman.Tick(0_tick); // Выполнит !
    qman.Tick(0_tick); // Выполнит A
    qman.Tick(0_tick); // Выполнит B
    check("B!AB"); // Исправили ожидание под LIFO-приоритет

    // --- CASE 4: Sleep Yield (Вежливость) ---
    printStep("Yielding (B sleeps, A goes)");
    qman.Go(taskA);
    qman.Go(taskB); // Пул [A, B], HEAD=B
    qman.Tick(0_tick); // Выполнили B. Он ушел в хвост за A.
    qman.Tick(0_tick); // Выполнили A.
    check("BA"); // Исправили ожидание

   // --- CASE 5: Sleep на будущее (3 тика) ---
    printStep("Sleep 3 ticks (Middle Insertion)");
    qman.Go(oneC, QTime(5));
    qman.Go(oneB, QTime(1));
    qman.Go(taskA_Sleep3);   
    
    qman.Tick(QTime(0)); // Выполнит A(0), она уснет до T=3. В пуле [C(5), A(3), B(1)]
    
    qman.Tick(QTime(1)); // Now=1. Должна выйти B(1). В пуле [C(5), A(3)]
    qman.Tick(QTime(1)); // Now=2. Никто не готов.
    qman.Tick(QTime(1)); // Now=3. Должна выйти a (вторая часть A). В пуле [C(5)]
    qman.Tick(QTime(2)); // Now=5. Должна выйти C(5).
    
    check("ABaC"); 

    Serial.println(F("\n--- ALL TESTS FINISHED ---"));
}

void loop() {}
