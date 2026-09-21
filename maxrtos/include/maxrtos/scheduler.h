/**
 * @file scheduler.h
 * @brief Public application-facing scheduler startup interface.
 */

#ifndef MAXRTOS_SCHEDULER_H
#define MAXRTOS_SCHEDULER_H

/**
 * @brief Start the partition frame scheduler.
 *
 * Registers every READY process created with maxrtos_process_create()
 * with its partition, prepares its initial context, and enters the
 * first scheduled process. Suspended processes are not registered.
 *
 * Call once after the generated configuration has been initialized
 * (maxrtos_config_init()) and all static processes have been created.
 * Process identifiers are not needed: the process pool is the source
 * of truth.
 *
 * Does not return. A configuration or startup error (no configuration
 * initialized, no runnable process, or a partition rejecting a
 * process) halts execution, because it indicates invalid static
 * configuration rather than a recoverable runtime condition.
 */
void maxrtos_scheduler_start( void );

#endif /* MAXRTOS_SCHEDULER_H */
