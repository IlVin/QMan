/**
 * QMan (Queue Manager) Implementation
 * 
 * This file contains the global variables and the main instance of the 
 * manager. These variables are placed here to ensure they exist only once 
 * in the final firmware.
 */

#include "QMan.h"

/**
 * Global task counter.
 * Incremented by QManCounter objects before main() starts.
 * Used to check if the number of tasks exceeds the POOL_SIZE.
 */
uint8_t qman_total_tasks = 0;

/**
 * Time accumulation variables.
 * Used by the QMAN_TICK() macro to track micros() and convert them 
 * into stable 32-bit ticks, handling clock rollover automatically.
 */
uint32_t qman_last_micros = 0;
uint32_t qman_task_ticks = 0;

/**
 * The main Queue Manager instance.
 * Initialized with 0 ticks. The first call to QMAN_TICK() will synchronize
 * the internal clock with the hardware timer.
 */
QMan qman(0);
