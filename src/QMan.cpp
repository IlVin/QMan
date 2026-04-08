/**
 * QMan (Queue Manager) Implementation
 * Correctly handles time accumulation and provides global state variables.
 */

#include "QMan.h"

// Static task registration counter
uint8_t qman_total_tasks = 0;

// Global time accumulators used by QMAN_TICK() macro
uint32_t qman_last_micros = 0;
uint32_t qman_task_ticks = 0;

// Global instance of the Queue Manager.
// Starts with 0 ticks to ensure full 32-bit rollover compatibility.
QMan qman(0);
