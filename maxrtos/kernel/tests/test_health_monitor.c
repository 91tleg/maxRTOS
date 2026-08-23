/**
 * @file test_health_monitor.c
 * @brief Unit tests for the health-monitor policy.
 */

#include <assert.h>
#include <stdio.h>

#include "maxrtos/kernel/health_monitor.h"

static void test_init_rejects_null( void )
{
    assert(
        maxrtos_hm_init( NULL ) ==
        MAXRTOS_ERR_INVALID_ARG );

    printf( "test_init_rejects_null: PASS\n" );
}

static void test_init_defaults_every_entry_to_halt_partition( void )
{
    maxrtos_health_monitor_t hm;
    maxrtos_partition_id_t partition_id;
    maxrtos_fault_type_t fault_type;
    maxrtos_hm_action_t action;

    assert( maxrtos_hm_init( &hm ) == MAXRTOS_OK );

    for( partition_id = 0U;
         partition_id < MAXRTOS_MAX_PARTITIONS;
         partition_id++ )
    {
        for( fault_type = 0U;
             fault_type < MAXRTOS_FAULT_COUNT;
             fault_type++ )
        {
            assert(
                maxrtos_hm_get_policy(
                    &hm,
                    partition_id,
                    fault_type,
                    &action ) ==
                MAXRTOS_OK );

            assert(
                action ==
                MAXRTOS_HM_ACTION_HALT_PARTITION );
        }
    }

    printf(
        "test_init_defaults_every_entry_to_halt_partition: PASS\n" );
}

static void test_set_policy_rejects_bad_args( void )
{
    maxrtos_health_monitor_t hm;

    assert( maxrtos_hm_init( &hm ) == MAXRTOS_OK );

    assert(
        maxrtos_hm_set_policy(
            NULL,
            0U,
            MAXRTOS_FAULT_MEMORY_ACCESS,
            MAXRTOS_HM_ACTION_IGNORE ) ==
        MAXRTOS_ERR_INVALID_ARG );

    assert(
        maxrtos_hm_set_policy(
            &hm,
            MAXRTOS_MAX_PARTITIONS,
            MAXRTOS_FAULT_MEMORY_ACCESS,
            MAXRTOS_HM_ACTION_IGNORE ) ==
        MAXRTOS_ERR_INVALID_ARG );

    assert(
        maxrtos_hm_set_policy(
            &hm,
            0U,
            MAXRTOS_FAULT_COUNT,
            MAXRTOS_HM_ACTION_IGNORE ) ==
        MAXRTOS_ERR_INVALID_ARG );

    assert(
        maxrtos_hm_set_policy(
            &hm,
            0U,
            MAXRTOS_FAULT_MEMORY_ACCESS,
            MAXRTOS_HM_ACTION_COUNT ) ==
        MAXRTOS_ERR_INVALID_ARG );

    printf( "test_set_policy_rejects_bad_args: PASS\n" );
}

static void test_get_policy_rejects_bad_args( void )
{
    maxrtos_health_monitor_t hm;
    maxrtos_hm_action_t action;

    assert( maxrtos_hm_init( &hm ) == MAXRTOS_OK );

    assert(
        maxrtos_hm_get_policy(
            NULL,
            0U,
            MAXRTOS_FAULT_MEMORY_ACCESS,
            &action ) ==
        MAXRTOS_ERR_INVALID_ARG );

    assert(
        maxrtos_hm_get_policy(
            &hm,
            0U,
            MAXRTOS_FAULT_MEMORY_ACCESS,
            NULL ) ==
        MAXRTOS_ERR_INVALID_ARG );

    assert(
        maxrtos_hm_get_policy(
            &hm,
            MAXRTOS_MAX_PARTITIONS,
            MAXRTOS_FAULT_MEMORY_ACCESS,
            &action ) ==
        MAXRTOS_ERR_INVALID_ARG );

    assert(
        maxrtos_hm_get_policy(
            &hm,
            0U,
            MAXRTOS_FAULT_COUNT,
            &action ) ==
        MAXRTOS_ERR_INVALID_ARG );

    printf( "test_get_policy_rejects_bad_args: PASS\n" );
}

static void test_set_then_get_roundtrips_correctly( void )
{
    maxrtos_health_monitor_t hm;
    maxrtos_hm_action_t action;

    assert( maxrtos_hm_init( &hm ) == MAXRTOS_OK );

    assert(
        maxrtos_hm_set_policy(
            &hm,
            2U,
            MAXRTOS_FAULT_DIVIDE_BY_ZERO,
            MAXRTOS_HM_ACTION_RESTART_PROCESS ) ==
        MAXRTOS_OK );

    assert(
        maxrtos_hm_get_policy(
            &hm,
            2U,
            MAXRTOS_FAULT_DIVIDE_BY_ZERO,
            &action ) ==
        MAXRTOS_OK );

    assert(
        action ==
        MAXRTOS_HM_ACTION_RESTART_PROCESS );

    printf( "test_set_then_get_roundtrips_correctly: PASS\n" );
}

static void test_set_policy_does_not_affect_other_entries( void )
{
    maxrtos_health_monitor_t hm;
    maxrtos_hm_action_t action;

    assert( maxrtos_hm_init( &hm ) == MAXRTOS_OK );

    assert(
        maxrtos_hm_set_policy(
            &hm,
            1U,
            MAXRTOS_FAULT_BUS_ERROR,
            MAXRTOS_HM_ACTION_IGNORE ) ==
        MAXRTOS_OK );

    /* Same partition, different fault type. */
    assert(
        maxrtos_hm_get_policy(
            &hm,
            1U,
            MAXRTOS_FAULT_MEMORY_ACCESS,
            &action ) ==
        MAXRTOS_OK );

    assert(
        action ==
        MAXRTOS_HM_ACTION_HALT_PARTITION );

    /* Different partition, same fault type. */
    assert(
        maxrtos_hm_get_policy(
            &hm,
            0U,
            MAXRTOS_FAULT_BUS_ERROR,
            &action ) ==
        MAXRTOS_OK );

    assert(
        action ==
        MAXRTOS_HM_ACTION_HALT_PARTITION );

    /* Configured entry. */
    assert(
        maxrtos_hm_get_policy(
            &hm,
            1U,
            MAXRTOS_FAULT_BUS_ERROR,
            &action ) ==
        MAXRTOS_OK );

    assert(
        action ==
        MAXRTOS_HM_ACTION_IGNORE );

    printf(
        "test_set_policy_does_not_affect_other_entries: PASS\n" );
}

int main( void )
{
    test_init_rejects_null();
    test_init_defaults_every_entry_to_halt_partition();
    test_set_policy_rejects_bad_args();
    test_get_policy_rejects_bad_args();
    test_set_then_get_roundtrips_correctly();
    test_set_policy_does_not_affect_other_entries();

    printf( "all health monitor tests passed\n" );

    return 0;
}
