/**
 * @file process.h
 * @brief Process control block and process lifecycle management.
 *
 * Defines the process control block, process states, and
 * process-management interface.
 */

#ifndef MAXRTOS_KERNEL_PROCESS_H
#define MAXRTOS_KERNEL_PROCESS_H

#include <stddef.h>
#include <stdint.h>

#include "maxrtos/status.h"

/* Maximum number of processes supported by the kernel.
 * Process storage is statically allocated; kernel code does not use
 * dynamic memory allocation. */
#define MAXRTOS_MAX_PROCESSES ( 16U )

/* Maximum supported process priority.
 * Priority 0 is the highest priority. */
#define MAXRTOS_MAX_PRIORITY ( 31U )

typedef uint32_t maxrtos_process_id_t;

/* Sentinel value indicating that no valid process ID is available. */
#define MAXRTOS_INVALID_PROCESS_ID \
    ( ( maxrtos_process_id_t ) 0xFFFFFFFFU )

typedef enum
{
    MAXRTOS_PROCESS_STATE_UNUSED = 0U, /* Process slot is available. */
    MAXRTOS_PROCESS_STATE_READY,       /* Process is eligible for execution. */
    MAXRTOS_PROCESS_STATE_RUNNING,     /* Process is currently executing. */
    MAXRTOS_PROCESS_STATE_BLOCKED,     /* Process is waiting for an event or resource. */
    MAXRTOS_PROCESS_STATE_SUSPENDED    /* Process is administratively stopped. */
} maxrtos_process_state_t;

typedef void ( * maxrtos_process_entry_t ) ( void * arg );

/**
 * @brief Process control block.
 *
 * Stores the execution context, scheduling attributes, and entry
 * information associated with a process.
 *
 * stack_pointer shall remain the first member of this structure.
 * The architecture-specific context-switch implementation accesses
 * this member at offset zero.
 */
typedef struct
{
    void * stack_pointer;
    uint8_t * stack_base;
    size_t stack_size;

    maxrtos_process_id_t id;
    uint8_t priority;
    maxrtos_process_state_t state;

    maxrtos_process_entry_t entry;
    void * entry_arg;
} maxrtos_process_control_block_t;

/**
 * @brief Reset the static process table.
 *
 * Marks all process slots as UNUSED.
 *
 * This function shall be called before any other process-management
 * operation.
 */
void maxrtos_process_pool_init( void );

/**
 * @brief Create and initialize a process.
 *
 * @param[in] stack_base
 *     Caller-owned, statically allocated stack memory.
 *
 * @param[in] stack_size
 *     Size of the stack in bytes.
 *
 * @param[in] priority
 *     Process priority. Valid range is 0 through
 *     MAXRTOS_MAX_PRIORITY, where 0 is the highest priority.
 *
 * @param[in] entry
 *     Process entry function.
 *
 * @param[in] entry_arg
 *     Argument passed to the process entry function.
 *
 * @param[out] out_id
 *     Output location for the newly allocated process ID.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if a required argument is invalid.
 *     MAXRTOS_ERR_POOL_FULL if no process slot is available.
 *
 * The process is created in the READY state. This function does not
 * add the process to the scheduler ready queue.
 */
maxrtos_status_t maxrtos_process_create(
    uint8_t * stack_base,
    size_t stack_size,
    uint8_t priority,
    maxrtos_process_entry_t entry,
    void * entry_arg,
    maxrtos_process_id_t * out_id );

/**
 * @brief Look up a process by ID.
 *
 * @param[in] id
 *     Process ID to look up.
 *
 * @return
 *     Pointer to the process control block if the ID is valid and the
 *     process is allocated; otherwise NULL.
 *
 * The returned pointer refers to storage owned by the process pool.
 * The caller shall not free the returned pointer.
 */
maxrtos_process_control_block_t * maxrtos_process_get(
    maxrtos_process_id_t id );

/**
 * @brief Set the state of a process.
 *
 * @param[in] id
 *     Process ID of the process to update.
 *
 * @param[in] new_state
 *     New process state.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ID if the process ID is invalid or refers
 *     to an unused process slot.
 *
 * This function updates process state only. It does not modify
 * scheduler membership, request a reschedule, or perform a context
 * switch.
 */
maxrtos_status_t maxrtos_process_set_state(
    maxrtos_process_id_t id,
    maxrtos_process_state_t new_state );

#endif /* MAXRTOS_KERNEL_PROCESS_H */
