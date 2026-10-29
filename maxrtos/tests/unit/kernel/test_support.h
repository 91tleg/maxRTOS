/**
 * @file test_support.h
 * @brief Scaffolding shared by the kernel unit tests.
 */

#ifndef MAXRTOS_TEST_SUPPORT_H
#define MAXRTOS_TEST_SUPPORT_H

#include <assert.h>

#include "maxrtos/kernel/partition.h"
#include "maxrtos/kernel/process.h"
#include "maxrtos/kernel/scheduler.h"

/**
 * Make @p id the running process of its partition, whatever the priorities.
 *
 * A test that exercises an IPC primitive needs particular processes to be the
 * caller, one after the other. The kernel itself only hands the CPU to a
 * process that is not outranked by the running one (priority preemption), so
 * it cannot be used to walk through processes of mixed priority. This puts the
 * running process back in its ready queue and takes
 * @p id out of its own, the way a dispatch would, without applying the rule.
 */
static inline void test_run_as(
    maxrtos_partition_table_t * table,
    maxrtos_process_id_t id )
{
    maxrtos_process_control_block_t * pcb;
    maxrtos_process_control_block_t * running;
    maxrtos_scheduler_context_t * ctx;
    maxrtos_partition_id_t partition;

    pcb = maxrtos_process_get( id );
    assert( pcb != NULL );

    partition = pcb->partition_id;
    ctx = &table->scheduler_ctx[ partition ];
    running = maxrtos_process_get( table->current_id[ partition ] );

    if( pcb->state != MAXRTOS_PROCESS_STATE_RUNNING )
    {
        if( ( running != NULL ) &&
            ( running->state == MAXRTOS_PROCESS_STATE_RUNNING ) )
        {
            running->state = MAXRTOS_PROCESS_STATE_READY;
            assert(
                maxrtos_scheduler_enqueue(
                    ctx, running->id, running->priority ) == MAXRTOS_OK );
        }

        assert( pcb->state == MAXRTOS_PROCESS_STATE_READY );
        assert( maxrtos_scheduler_remove_process( ctx, id ) == MAXRTOS_OK );
        pcb->state = MAXRTOS_PROCESS_STATE_RUNNING;
        table->current_id[ partition ] = id;
    }
}

#endif /* MAXRTOS_TEST_SUPPORT_H */
