/**
 * QMan (Queue Manager) Implementation
 */

#include "QMan.h"

// Global static task counter initialized to 0
uint8_t qman_total_tasks = 0;

// Instantiate the global Queue Manager object.
// Time is initialized with current micros converted to ticks.
QMan qman(micros() >> 5);
