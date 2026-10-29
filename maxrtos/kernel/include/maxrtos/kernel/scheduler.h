/**
 * @file scheduler.h
 * @brief Fixed-priority process scheduler interface.
 *
 * The ready queues order processes for priority-preemptive scheduling. Lower
 * numeric priority values represent higher priority. Processes with equal
 * priority are selected in FIFO order based on ready-queue insertion order. The
 * scheduler only orders the READY processes: when the running one is replaced
 * is decided by the dispatch layer (dispatch.h): it is preempted by a strictly
 * higher priority, and gives way to an equal one only when it yields.
 */

#ifndef MAXRTOS_KERNEL_SCHEDULER_H
#define MAXRTOS_KERNEL_SCHEDULER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "maxrtos/status.h"
#include "maxrtos/config.h"
#include "maxrtos/types.h"
#include "maxrtos/kernel/waitlist.h"

/**
 * @brief FIFO ready queue for a single priority level.
 *
 * Stores process IDs currently eligible for execution at the
 * associated priority.
 */
typedef struct
{
    maxrtos_process_id_t items[ MAXRTOS_MAX_READY_PER_PRIORITY ];
    size_t head;
    size_t count;
} maxrtos_ready_queue_t;

/**
 * @brief Scheduler state for one scheduling domain.
 *
 * Contains one FIFO ready queue for each supported process priority.
 *
 * The scheduler does not own process control blocks. Process storage
 * is maintained by the process-management subsystem.
 */
typedef struct maxrtos_scheduler_context_s
{
    maxrtos_ready_queue_t queues[ MAXRTOS_MAX_PRIORITY + 1U ];

    /* Bit p is set while queues[ p ] is not empty. */
    uint32_t ready_mask;
} maxrtos_scheduler_context_t;

/**
 * @brief Initialize a scheduler context.
 *
 * Clears all ready queues in the specified scheduler context.
 * Process control blocks are not modified.
 *
 * @param[out] ctx
 *     Scheduler context to initialize.
 *
 * @return
 *     MAXRTOS_OK if the context was initialized successfully.
 *     MAXRTOS_ERR_INVALID_ARG if ctx is NULL.
 *
 * @pre The process pool shall have been initialized.
 */
maxrtos_status_t maxrtos_scheduler_init( maxrtos_scheduler_context_t * ctx );

/**
 * @brief Add a READY process to a scheduler context.
 *
 * Adds the specified process ID to the ready queue corresponding to
 * the process priority.
 *
 * @param[in,out] ctx
 *     Scheduler context.
 *
 * @param[in] id
 *     Process identifier.
 *
 * @return
 *     MAXRTOS_OK if the process was added successfully.
 *     MAXRTOS_ERR_INVALID_ARG if ctx is NULL.
 *     MAXRTOS_ERR_INVALID_ID if id does not identify an allocated
 *     process.
 *     MAXRTOS_ERR_INVALID_STATE if the process is not READY.
 *     MAXRTOS_ERR_QUEUE_FULL if the ready queue is full.
 *
 * The process shall be in the READY state. This function does not
 * modify the process state.
 */
maxrtos_status_t maxrtos_scheduler_add_process(
    maxrtos_scheduler_context_t * ctx,
    maxrtos_process_id_t id );

/**
 * @brief Remove a process from a scheduler context.
 *
 * Removes the specified process ID from its priority ready queue.
 *
 * @param[in,out] ctx
 *     Scheduler context.
 *
 * @param[in] id
 *     Process identifier.
 *
 * @return
 *     MAXRTOS_OK if the process was removed successfully.
 *     MAXRTOS_ERR_INVALID_ARG if ctx is NULL.
 *     MAXRTOS_ERR_INVALID_ID if id does not identify an allocated
 *     process.
 *     MAXRTOS_ERR_INVALID_STATE if the process is not READY.
 *     MAXRTOS_ERR_NOT_FOUND if the process is not present in the
 *     ready queue.
 *
 * The process shall be in the READY state when this function is
 * called. This function does not modify the process state.
 */
maxrtos_status_t maxrtos_scheduler_remove_process(
    maxrtos_scheduler_context_t * ctx,
    maxrtos_process_id_t id );

/* The ready-queue operations below are inline: the dispatcher calls them on
 * every context switch, and a call across translation units costs more than
 * the operation. */

_Static_assert(
    MAXRTOS_MAX_PRIORITY < 32U,
    "the ready mask holds one bit per priority in 32 bits" );

/* Index of the lowest set bit of a non-zero mask, by de Bruijn
 * multiplication: the product's top five bits identify the isolated bit. */
static inline uint32_t maxrtos_scheduler_lowest_set_bit( uint32_t mask )
{
    static const uint8_t s_bit_index[ 32 ] = {
        0U,  1U,  28U, 2U,  29U, 14U, 24U, 3U,  30U, 22U, 20U,
        15U, 25U, 17U, 4U,  8U,  31U, 27U, 13U, 23U, 21U, 19U,
        16U, 7U,  26U, 12U, 18U, 6U,  11U, 5U,  10U, 9U };

    uint32_t isolated;

    isolated = mask & ( ~mask + 1U );

    return s_bit_index[ ( isolated * UINT32_C( 0x077CB531 ) ) >> 27U ];
}

static inline size_t maxrtos_scheduler_queue_tail(
    maxrtos_ready_queue_t const * queue )
{
    size_t tail;

    tail = ( queue->head + queue->count ) % MAXRTOS_MAX_READY_PER_PRIORITY;

    return tail;
}

static inline maxrtos_status_t maxrtos_scheduler_queue_push(
    maxrtos_ready_queue_t * queue,
    maxrtos_process_id_t id )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_QUEUE_FULL;

    if( queue != NULL )
    {
        if( queue->count < MAXRTOS_MAX_READY_PER_PRIORITY )
        {
            size_t tail;

            tail = maxrtos_scheduler_queue_tail( queue );

            queue->items[ tail ] = id;
            queue->count++;
            status = MAXRTOS_OK;
        }
    }

    return status;
}

/**
 * @brief Select the highest-priority READY process.
 *
 * Selects the READY process with the highest priority. If multiple
 * READY processes have the same priority, the process at the front
 * of the corresponding ready queue is selected.
 *
 * @param[in] ctx
 *     Scheduler context.
 *
 * @param[out] out_id
 *     Output location for the selected process identifier.
 *
 * @return
 *     MAXRTOS_OK if a READY process was selected.
 *     MAXRTOS_ERR_INVALID_ARG if ctx or out_id is NULL.
 *     MAXRTOS_ERR_QUEUE_EMPTY if no READY process is available.
 *
 * This function does not modify ready-queue membership, process
 * state, or processor execution state.
 */
static inline maxrtos_status_t maxrtos_scheduler_next(
    maxrtos_scheduler_context_t const * ctx,
    maxrtos_process_id_t * out_id )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( ctx != NULL ) && ( out_id != NULL ) )
    {
        if( ctx->ready_mask == 0U )
        {
            status = MAXRTOS_ERR_QUEUE_EMPTY;
        }
        else
        {
            /* Priority 0 is the highest, so the lowest set bit wins.
             * Peek only. Selection does not remove the process from the
             * ready queue or modify queue state. */
            uint32_t priority;

            priority = maxrtos_scheduler_lowest_set_bit( ctx->ready_mask );

            *out_id =
                ctx->queues[ priority ].items[ ctx->queues[ priority ].head ];

            status = MAXRTOS_OK;
        }
    }

    return status;
}

/* Remove the head of the non-empty queue for @p priority. */
static inline void maxrtos_scheduler_pop(
    maxrtos_scheduler_context_t * ctx,
    uint32_t priority )
{
    maxrtos_ready_queue_t * queue;

    queue = &ctx->queues[ priority ];
    queue->head = ( queue->head + 1U ) % MAXRTOS_MAX_READY_PER_PRIORITY;
    queue->count--;

    if( queue->count == 0U )
    {
        ctx->ready_mask &= ~( UINT32_C( 1 ) << priority );
    }
}

/**
 * @brief Remove and return the process maxrtos_scheduler_next() would pick.
 *
 * Unlike maxrtos_scheduler_add_process() and _remove_process() this does
 * not look the process up or check its state: the caller (the dispatcher)
 * has already done both.
 *
 * @return MAXRTOS_OK, MAXRTOS_ERR_QUEUE_EMPTY if nothing is queued, or
 *         MAXRTOS_ERR_INVALID_ARG.
 */
static inline maxrtos_status_t maxrtos_scheduler_take_next(
    maxrtos_scheduler_context_t * ctx,
    maxrtos_process_id_t * out_id )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( ctx != NULL ) && ( out_id != NULL ) )
    {
        if( ctx->ready_mask == 0U )
        {
            status = MAXRTOS_ERR_QUEUE_EMPTY;
        }
        else
        {
            uint32_t priority;

            priority = maxrtos_scheduler_lowest_set_bit( ctx->ready_mask );

            *out_id =
                ctx->queues[ priority ].items[ ctx->queues[ priority ].head ];
            maxrtos_scheduler_pop( ctx, priority );

            status = MAXRTOS_OK;
        }
    }

    return status;
}

/**
 * @brief Queue a process at the tail of the queue for @p priority without
 *        looking it up or checking its state.
 *
 * @return MAXRTOS_OK, MAXRTOS_ERR_QUEUE_FULL, or MAXRTOS_ERR_INVALID_ARG.
 */
static inline maxrtos_status_t maxrtos_scheduler_enqueue(
    maxrtos_scheduler_context_t * ctx,
    maxrtos_process_id_t id,
    uint8_t priority )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( ctx != NULL ) && ( priority <= MAXRTOS_MAX_PRIORITY ) )
    {
        status = maxrtos_scheduler_queue_push( &ctx->queues[ priority ], id );

        if( status == MAXRTOS_OK )
        {
            ctx->ready_mask |= UINT32_C( 1 ) << priority;
        }
    }

    return status;
}

/**
 * @brief Queue a process at the head of the queue for @p priority without
 *        looking it up or checking its state.
 *
 * Used for a process that was preempted: it stays the oldest READY process
 * of its priority and runs again before its equal-priority peers.
 *
 * @return MAXRTOS_OK, MAXRTOS_ERR_QUEUE_FULL, or MAXRTOS_ERR_INVALID_ARG.
 */
static inline maxrtos_status_t maxrtos_scheduler_enqueue_front(
    maxrtos_scheduler_context_t * ctx,
    maxrtos_process_id_t id,
    uint8_t priority )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( ctx != NULL ) && ( priority <= MAXRTOS_MAX_PRIORITY ) )
    {
        maxrtos_ready_queue_t * queue;

        queue = &ctx->queues[ priority ];
        status = MAXRTOS_ERR_QUEUE_FULL;

        if( queue->count < MAXRTOS_MAX_READY_PER_PRIORITY )
        {
            queue->head =
                ( queue->head + MAXRTOS_MAX_READY_PER_PRIORITY - 1U ) %
                MAXRTOS_MAX_READY_PER_PRIORITY;
            queue->items[ queue->head ] = id;
            queue->count++;
            ctx->ready_mask |= UINT32_C( 1 ) << priority;
            status = MAXRTOS_OK;
        }
    }

    return status;
}

/**
 * @brief True if a READY process has a strictly higher priority than
 *        @p priority (a numerically lower value).
 *
 * This is the test for preemption: a running process of @p priority keeps the
 * CPU until this is true.
 */
static inline bool maxrtos_scheduler_has_higher(
    maxrtos_scheduler_context_t const * ctx,
    uint8_t priority )
{
    return ( ctx != NULL ) &&
           ( ( ctx->ready_mask & ( ( UINT32_C( 1 ) << priority ) - 1U ) ) !=
             0U );
}

/**
 * @brief Return the number of schedulable processes.
 *
 * @param[in] ctx
 *     Scheduler context.
 *
 * @return
 *     Number of process IDs currently present in the scheduler
 *     ready queues.
 *
 * This function does not modify scheduler state or process state.
 */
size_t maxrtos_scheduler_process_count(
    maxrtos_scheduler_context_t const * ctx );

#endif /* MAXRTOS_KERNEL_SCHEDULER_H */
