/**
 * @file test_sil_fault_recovery_soak.c
 * @brief SIL integration/soak test of fault recovery, health monitoring,
 *        and partition/process interaction over simulated faults.
 *
 * PROVES:
 *   - RESTART_PROCESS repeatedly recovers the same process without
 *     changing its process ID or partition assignment.
 *   - Repeated recovery does not invalidate an unrelated process in
 *     another partition.
 *   - IGNORE leaves the process identity unchanged over repeated faults.
 *   - HALT_PARTITION policy selects the expected recovery action.
 *   - Invalid process IDs are rejected after repeated recovery activity.
 *
 * DOES NOT PROVE:
 *   - That a real target fault exception is correctly classified.
 *   - That architecture-specific fault handlers correctly capture
 *     exception context.
 *   - That target-specific stack/context recovery works correctly.
 */

#include <assert.h>
#include <stdio.h>

#include "maxrtos/kernel/process.h"
#include "maxrtos/kernel/partition.h"
#include "maxrtos/kernel/health_monitor.h"
#include "maxrtos/kernel/fault_recovery.h"

#define TEST_STACK_SIZE   ( 128U )
#define SOAK_ITERATIONS   ( 5000U )

static uint8_t s_stack_faulting[ TEST_STACK_SIZE ];
static uint8_t s_stack_bystander[ TEST_STACK_SIZE ];

static void dummy_entry( void * arg )
{
    ( void ) arg;

    for( ;; )
    {

    }
}

/* Repeatedly recover the same process, checking that its identity
 * and partition assignment remain valid throughout the soak. */
static void test_restart_process_soak_preserves_identity( void )
{
    maxrtos_health_monitor_t hm;
    maxrtos_partition_table_t table;

    maxrtos_process_id_t faulting_id;
    maxrtos_process_id_t bystander_id;

    maxrtos_process_control_block_t * bystander_pcb_before;
    maxrtos_process_control_block_t * bystander_pcb_after;

    uint32_t iteration;

    maxrtos_process_pool_init();
    assert( maxrtos_partition_table_init( &table ) == MAXRTOS_OK );
    assert( maxrtos_hm_init( &hm ) == MAXRTOS_OK );

    assert(
        maxrtos_hm_set_policy(
            &hm,
            0U,
            MAXRTOS_FAULT_MEMORY_ACCESS,
            MAXRTOS_HM_ACTION_RESTART_PROCESS ) == MAXRTOS_OK );

    /* Faulting process: partition 0. */
    assert(
        maxrtos_process_create(
            s_stack_faulting,
            TEST_STACK_SIZE,
            ( maxrtos_partition_id_t ) 0U,
            5U,
            true,
            dummy_entry,
            NULL,
            &faulting_id ) ==
        MAXRTOS_OK );

    /* Partition 1, never faults. */
    assert(
        maxrtos_process_create(
            s_stack_bystander,
            TEST_STACK_SIZE,
            ( maxrtos_partition_id_t ) 1U,
            5U,
            true,
            dummy_entry,
            NULL,
            &bystander_id ) == MAXRTOS_OK );

    assert(
        maxrtos_partition_add_process(
            &table,
            faulting_id ) == MAXRTOS_OK );

    assert(
        maxrtos_partition_add_process(
            &table,
            bystander_id ) == MAXRTOS_OK );

    bystander_pcb_before = maxrtos_process_get( bystander_id );
    assert( bystander_pcb_before != NULL );

    for( iteration = 0U; iteration < SOAK_ITERATIONS; iteration++ )
    {
        maxrtos_hm_action_t action;
        maxrtos_process_control_block_t * faulting_pcb;
        maxrtos_process_id_t dispatched_id;

        assert(
            maxrtos_partition_dispatch(
                &table,
                0U,
                &dispatched_id ) == MAXRTOS_OK );

        assert( dispatched_id == faulting_id );

        assert(
            maxrtos_fault_recovery_handle(
                &hm,
                &table,
                faulting_id,
                MAXRTOS_FAULT_MEMORY_ACCESS,
                &action ) == MAXRTOS_OK );

        assert( action == MAXRTOS_HM_ACTION_RESTART_PROCESS );

        faulting_pcb = maxrtos_process_get( faulting_id );
        assert( faulting_pcb != NULL );
        assert( faulting_pcb->partition_id == 0U );

        /* Recovery of partition 0 must not replace or alter the
         * partition 1 process. Pointer identity verifies that
         * the same PCB remains allocated; partition_id verifies
         * that its partition assignment is unchanged. */
        bystander_pcb_after = maxrtos_process_get( bystander_id );
        assert( bystander_pcb_after == bystander_pcb_before );
        assert( bystander_pcb_after->partition_id == 1U );
    }

    printf(
        "test_restart_process_soak_preserves_identity: PASS "
        "(%u iterations)\n",
        SOAK_ITERATIONS );
}

static void test_ignore_action_does_not_modify_state( void )
{
    maxrtos_health_monitor_t hm;
    maxrtos_partition_table_t table;
    maxrtos_process_id_t id;
    maxrtos_process_control_block_t * pcb_before;
    maxrtos_process_control_block_t * pcb_after;
    maxrtos_hm_action_t action;
    uint32_t iteration;

    maxrtos_process_pool_init();
    assert( maxrtos_partition_table_init( &table ) == MAXRTOS_OK );
    assert( maxrtos_hm_init( &hm ) == MAXRTOS_OK );

    assert(
        maxrtos_hm_set_policy(
            &hm,
            0U,
            MAXRTOS_FAULT_BUS_ERROR,
            MAXRTOS_HM_ACTION_IGNORE ) == MAXRTOS_OK );

    assert(
        maxrtos_process_create(
            s_stack_faulting,
            TEST_STACK_SIZE,
            (maxrtos_partition_id_t) 0U,
            5U,
            true,
            dummy_entry,
            NULL,
            &id ) == MAXRTOS_OK );

    assert(
        maxrtos_partition_add_process(
            &table,
            id ) == MAXRTOS_OK );

    pcb_before = maxrtos_process_get( id );
    assert( pcb_before != NULL );

    {
        maxrtos_process_id_t dispatched_id;

        assert(
            maxrtos_partition_dispatch(
                &table,
                0U,
                &dispatched_id ) == MAXRTOS_OK );

        assert( dispatched_id == id );
    }

    for( iteration = 0U; iteration < SOAK_ITERATIONS; iteration++ )
    {
        assert(
            maxrtos_fault_recovery_handle(
                &hm,
                &table,
                id,
                MAXRTOS_FAULT_BUS_ERROR,
                &action ) == MAXRTOS_OK );

        assert( action == MAXRTOS_HM_ACTION_IGNORE );

        pcb_after = maxrtos_process_get( id );
        assert( pcb_after == pcb_before );
    }

    printf(
        "test_ignore_action_does_not_modify_state: PASS "
        "(%u iterations)\n",
        SOAK_ITERATIONS );
}

static void test_halt_partition_action_reported_correctly( void )
{
    maxrtos_health_monitor_t hm;
    maxrtos_partition_table_t table;
    maxrtos_process_id_t id;
    maxrtos_hm_action_t action;

    maxrtos_process_pool_init();
    assert( maxrtos_partition_table_init( &table ) == MAXRTOS_OK );

    assert( maxrtos_hm_init( &hm ) == MAXRTOS_OK );

    assert(
        maxrtos_process_create(
            s_stack_faulting,
            TEST_STACK_SIZE,
            ( maxrtos_partition_id_t ) 0U,
            5U,
            true,
            dummy_entry,
            NULL,
            &id ) == MAXRTOS_OK );

    assert(
        maxrtos_partition_add_process(
            &table,
            id ) == MAXRTOS_OK );

    {
        maxrtos_process_id_t dispatched_id;

        assert(
            maxrtos_partition_dispatch(
                &table,
                0U,
                &dispatched_id ) == MAXRTOS_OK );

        assert( dispatched_id == id );
    }

    assert(
        maxrtos_fault_recovery_handle(
            &hm,
            &table,
            id,
            MAXRTOS_FAULT_ILLEGAL_INSTRUCTION,
            &action ) == MAXRTOS_OK );

    assert( action == MAXRTOS_HM_ACTION_HALT_PARTITION );

    printf(
        "test_halt_partition_action_reported_correctly: PASS\n" );
}

static void test_invalid_process_id_rejected_after_soak( void )
{
    maxrtos_health_monitor_t hm;
    maxrtos_partition_table_t table;
    maxrtos_process_id_t valid_id;
    maxrtos_hm_action_t action;
    uint32_t iteration;

    maxrtos_process_pool_init();
    assert( maxrtos_partition_table_init( &table ) == MAXRTOS_OK );
    assert( maxrtos_hm_init( &hm ) == MAXRTOS_OK );

    assert(
        maxrtos_hm_set_policy(
            &hm,
            0U,
            MAXRTOS_FAULT_MEMORY_ACCESS,
            MAXRTOS_HM_ACTION_RESTART_PROCESS ) == MAXRTOS_OK );

    assert(
        maxrtos_process_create(
            s_stack_faulting,
            TEST_STACK_SIZE,
            ( maxrtos_partition_id_t ) 0U,
            5U,
            true,
            dummy_entry,
            NULL,
            &valid_id ) == MAXRTOS_OK );

    assert(
        maxrtos_partition_add_process(
            &table,
            valid_id ) == MAXRTOS_OK );

    for( iteration = 0U; iteration < 100U; iteration++ )
    {
        maxrtos_process_id_t dispatched_id;

        assert(
            maxrtos_partition_dispatch(
                &table,
                0U,
                &dispatched_id ) == MAXRTOS_OK );

        assert( dispatched_id == valid_id );

        assert(
            maxrtos_fault_recovery_handle(
                &hm,
                &table,
                valid_id,
                MAXRTOS_FAULT_MEMORY_ACCESS,
                &action ) == MAXRTOS_OK );
    }

    assert(
        maxrtos_fault_recovery_handle(
            &hm,
            &table,
            ( maxrtos_process_id_t ) 0xFFFFU, /* not an allocated ID */
            MAXRTOS_FAULT_MEMORY_ACCESS,
            &action ) == MAXRTOS_ERR_INVALID_ID );

    printf(
        "test_invalid_process_id_rejected_after_soak: PASS\n" );
}

int main( void )
{
    test_restart_process_soak_preserves_identity();
    test_ignore_action_does_not_modify_state();
    test_halt_partition_action_reported_correctly();
    test_invalid_process_id_rejected_after_soak();

    printf( "all SIL fault recovery tests passed\n" );

    return 0;
}
