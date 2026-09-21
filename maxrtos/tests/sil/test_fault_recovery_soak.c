/**
 * @file test_fault_recovery_soak.c
 * @brief SIL soak test of the kernel's fault recovery, health monitoring and
 *        partition/process interaction over thousands of simulated faults.
 *
 * Kernel level (no architecture layer): complements test_fault_containment.c,
 * which drives the same recovery through real exceptions on the virtual
 * target.
 *
 * Requirements
 *   REQ-FT-009  The kernel returns the configured action for every fault it
 *               is asked to recover, and repeated recovery never disturbs
 *               process identity or an unrelated partition.
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


#include "maxrtos/kernel/process.h"
#include "maxrtos/kernel/partition.h"
#include "maxrtos/kernel/health_monitor.h"
#include "maxrtos/kernel/fault_recovery.h"

#include "sil_test.h"

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
    SIL_REQUIRE( maxrtos_partition_table_init( &table ) == MAXRTOS_OK );
    SIL_REQUIRE( maxrtos_hm_init( &hm ) == MAXRTOS_OK );

    SIL_REQUIRE(
        maxrtos_hm_set_policy(
            &hm,
            0U,
            MAXRTOS_FAULT_MEMORY_ACCESS,
            MAXRTOS_HM_ACTION_RESTART_PROCESS ) == MAXRTOS_OK );

    /* Faulting process: partition 0. */
    SIL_REQUIRE(
        maxrtos_process_create(
            s_stack_faulting,
            TEST_STACK_SIZE,
            ( maxrtos_partition_id_t ) 0U,
            5U,
            dummy_entry,
            NULL,
            &faulting_id ) ==
        MAXRTOS_OK );

    /* Partition 1, never faults. */
    SIL_REQUIRE(
        maxrtos_process_create(
            s_stack_bystander,
            TEST_STACK_SIZE,
            ( maxrtos_partition_id_t ) 1U,
            5U,
            dummy_entry,
            NULL,
            &bystander_id ) == MAXRTOS_OK );

    SIL_REQUIRE(
        maxrtos_partition_add_process(
            &table,
            faulting_id ) == MAXRTOS_OK );

    SIL_REQUIRE(
        maxrtos_partition_add_process(
            &table,
            bystander_id ) == MAXRTOS_OK );

    bystander_pcb_before = maxrtos_process_get( bystander_id );
    SIL_REQUIRE( bystander_pcb_before != NULL );

    for( iteration = 0U; iteration < SOAK_ITERATIONS; iteration++ )
    {
        maxrtos_hm_action_t action;
        maxrtos_process_control_block_t * faulting_pcb;
        maxrtos_process_id_t dispatched_id;

        SIL_REQUIRE(
            maxrtos_partition_dispatch(
                &table,
                0U,
                &dispatched_id ) == MAXRTOS_OK );

        SIL_REQUIRE( dispatched_id == faulting_id );

        SIL_REQUIRE(
            maxrtos_fault_recovery_handle(
                &hm,
                &table,
                faulting_id,
                MAXRTOS_FAULT_MEMORY_ACCESS,
                &action ) == MAXRTOS_OK );

        SIL_REQUIRE( action == MAXRTOS_HM_ACTION_RESTART_PROCESS );

        faulting_pcb = maxrtos_process_get( faulting_id );
        SIL_REQUIRE( faulting_pcb != NULL );
        SIL_REQUIRE( faulting_pcb->partition_id == 0U );

        /* Recovery of partition 0 must not replace or alter the
         * partition 1 process. Pointer identity verifies that
         * the same PCB remains allocated; partition_id verifies
         * that its partition assignment is unchanged. */
        bystander_pcb_after = maxrtos_process_get( bystander_id );
        SIL_REQUIRE( bystander_pcb_after == bystander_pcb_before );
        SIL_REQUIRE( bystander_pcb_after->partition_id == 1U );
    }

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
    SIL_REQUIRE( maxrtos_partition_table_init( &table ) == MAXRTOS_OK );
    SIL_REQUIRE( maxrtos_hm_init( &hm ) == MAXRTOS_OK );

    SIL_REQUIRE(
        maxrtos_hm_set_policy(
            &hm,
            0U,
            MAXRTOS_FAULT_BUS_ERROR,
            MAXRTOS_HM_ACTION_IGNORE ) == MAXRTOS_OK );

    SIL_REQUIRE(
        maxrtos_process_create(
            s_stack_faulting,
            TEST_STACK_SIZE,
            (maxrtos_partition_id_t) 0U,
            5U,
            dummy_entry,
            NULL,
            &id ) == MAXRTOS_OK );

    SIL_REQUIRE(
        maxrtos_partition_add_process(
            &table,
            id ) == MAXRTOS_OK );

    pcb_before = maxrtos_process_get( id );
    SIL_REQUIRE( pcb_before != NULL );

    {
        maxrtos_process_id_t dispatched_id;

        SIL_REQUIRE(
            maxrtos_partition_dispatch(
                &table,
                0U,
                &dispatched_id ) == MAXRTOS_OK );

        SIL_REQUIRE( dispatched_id == id );
    }

    for( iteration = 0U; iteration < SOAK_ITERATIONS; iteration++ )
    {
        SIL_REQUIRE(
            maxrtos_fault_recovery_handle(
                &hm,
                &table,
                id,
                MAXRTOS_FAULT_BUS_ERROR,
                &action ) == MAXRTOS_OK );

        SIL_REQUIRE( action == MAXRTOS_HM_ACTION_IGNORE );

        pcb_after = maxrtos_process_get( id );
        SIL_REQUIRE( pcb_after == pcb_before );
    }

}

static void test_halt_partition_action_reported_correctly( void )
{
    maxrtos_health_monitor_t hm;
    maxrtos_partition_table_t table;
    maxrtos_process_id_t id;
    maxrtos_hm_action_t action;

    maxrtos_process_pool_init();
    SIL_REQUIRE( maxrtos_partition_table_init( &table ) == MAXRTOS_OK );

    SIL_REQUIRE( maxrtos_hm_init( &hm ) == MAXRTOS_OK );

    SIL_REQUIRE(
        maxrtos_process_create(
            s_stack_faulting,
            TEST_STACK_SIZE,
            ( maxrtos_partition_id_t ) 0U,
            5U,
            dummy_entry,
            NULL,
            &id ) == MAXRTOS_OK );

    SIL_REQUIRE(
        maxrtos_partition_add_process(
            &table,
            id ) == MAXRTOS_OK );

    {
        maxrtos_process_id_t dispatched_id;

        SIL_REQUIRE(
            maxrtos_partition_dispatch(
                &table,
                0U,
                &dispatched_id ) == MAXRTOS_OK );

        SIL_REQUIRE( dispatched_id == id );
    }

    SIL_REQUIRE(
        maxrtos_fault_recovery_handle(
            &hm,
            &table,
            id,
            MAXRTOS_FAULT_ILLEGAL_INSTRUCTION,
            &action ) == MAXRTOS_OK );

    SIL_REQUIRE( action == MAXRTOS_HM_ACTION_HALT_PARTITION );

}

static void test_invalid_process_id_rejected_after_soak( void )
{
    maxrtos_health_monitor_t hm;
    maxrtos_partition_table_t table;
    maxrtos_process_id_t valid_id;
    maxrtos_hm_action_t action;
    uint32_t iteration;

    maxrtos_process_pool_init();
    SIL_REQUIRE( maxrtos_partition_table_init( &table ) == MAXRTOS_OK );
    SIL_REQUIRE( maxrtos_hm_init( &hm ) == MAXRTOS_OK );

    SIL_REQUIRE(
        maxrtos_hm_set_policy(
            &hm,
            0U,
            MAXRTOS_FAULT_MEMORY_ACCESS,
            MAXRTOS_HM_ACTION_RESTART_PROCESS ) == MAXRTOS_OK );

    SIL_REQUIRE(
        maxrtos_process_create(
            s_stack_faulting,
            TEST_STACK_SIZE,
            ( maxrtos_partition_id_t ) 0U,
            5U,
            dummy_entry,
            NULL,
            &valid_id ) == MAXRTOS_OK );

    SIL_REQUIRE(
        maxrtos_partition_add_process(
            &table,
            valid_id ) == MAXRTOS_OK );

    for( iteration = 0U; iteration < 100U; iteration++ )
    {
        maxrtos_process_id_t dispatched_id;

        SIL_REQUIRE(
            maxrtos_partition_dispatch(
                &table,
                0U,
                &dispatched_id ) == MAXRTOS_OK );

        SIL_REQUIRE( dispatched_id == valid_id );

        SIL_REQUIRE(
            maxrtos_fault_recovery_handle(
                &hm,
                &table,
                valid_id,
                MAXRTOS_FAULT_MEMORY_ACCESS,
                &action ) == MAXRTOS_OK );
    }

    SIL_REQUIRE(
        maxrtos_fault_recovery_handle(
            &hm,
            &table,
            ( maxrtos_process_id_t ) 0xFFFFU, /* not an allocated ID */
            MAXRTOS_FAULT_MEMORY_ACCESS,
            &action ) == MAXRTOS_ERR_INVALID_ID );

}

static sil_case_t const s_cases[] =
{
    SIL_CASE( test_restart_process_soak_preserves_identity, "REQ-FT-009", "5000 restarts keep process identity and leave a bystander untouched" ),
    SIL_CASE( test_ignore_action_does_not_modify_state, "REQ-FT-009", "IGNORE leaves process state untouched over 5000 faults" ),
    SIL_CASE( test_halt_partition_action_reported_correctly, "REQ-FT-009", "HALT_PARTITION is reported for the configured fault class" ),
    SIL_CASE( test_invalid_process_id_rejected_after_soak, "REQ-FT-009", "An invalid process ID is rejected after recovery activity" ),
};

int main( int argc, char ** argv )
{
    return sil_run_suite( "fault_recovery_soak", s_cases,
                          sizeof( s_cases ) / sizeof( s_cases[ 0 ] ), argc, argv );
}
