/**
 * @file test_frame.c
 * @brief Host-side unit tests for major-frame schedule management.
 *
 * Verifies input validation, schedule initialization, major-frame
 * length calculation, partition selection, slot boundaries, and
 * major-frame wraparound behavior.
 *
 * Tests execute against the host build and do not require
 * target hardware.
 */

#include <assert.h>
#include <stdio.h>

#include "maxrtos/kernel/frame.h"

static void test_init_rejects_null_and_bad_args( void )
{
    maxrtos_frame_schedule_t sched;
    maxrtos_frame_slot_t slots[ 1 ] = { { 0U, 5U } };

    assert(
        maxrtos_frame_init(
            NULL,
            slots,
            1U ) == MAXRTOS_ERR_INVALID_ARG );

    assert(
        maxrtos_frame_init(
            &sched,
            NULL,
            1U ) == MAXRTOS_ERR_INVALID_ARG );

    assert(
        maxrtos_frame_init(
            &sched,
            slots,
            0U ) == MAXRTOS_ERR_INVALID_ARG );

    assert(
        maxrtos_frame_init(
            &sched,
            slots,
            MAXRTOS_MAX_FRAME_SLOTS + 1U ) ==
        MAXRTOS_ERR_INVALID_ARG );

    printf( "test_init_rejects_null_and_bad_args: PASS\n" );
}

static void test_init_rejects_bad_slot_partition_id( void )
{
    maxrtos_frame_schedule_t sched;
    maxrtos_frame_slot_t slots[ 1 ] =
    {
        { MAXRTOS_MAX_PARTITIONS, 5U }
    };

    /* MAXRTOS_MAX_PARTITIONS is outside the valid partition range. */
    assert(
        maxrtos_frame_init(
            &sched,
            slots,
            1U ) == MAXRTOS_ERR_INVALID_ARG );

    printf( "test_init_rejects_bad_slot_partition_id: PASS\n" );
}

static void test_init_rejects_zero_duration_slot( void )
{
    maxrtos_frame_schedule_t sched;
    maxrtos_frame_slot_t slots[ 2 ] =
    {
        { 0U, 5U },
        { 1U, 0U }
    };

    assert(
        maxrtos_frame_init(
            &sched,
            slots,
            2U ) == MAXRTOS_ERR_INVALID_ARG );

    printf( "test_init_rejects_zero_duration_slot: PASS\n" );
}

static void test_init_rejects_one_bad_slot_among_valid_ones( void )
{
    maxrtos_frame_schedule_t sched;
    maxrtos_frame_slot_t slots[ 3 ] =
    {
        { 0U, 5U },
        { 1U, 0U },
        { 0U, 3U }
    };

    /* A single invalid slot invalidates the complete configuration.
     * The implementation must not accept a partially valid schedule. */
    assert(
        maxrtos_frame_init(
            &sched,
            slots,
            3U ) == MAXRTOS_ERR_INVALID_ARG );

    printf( "test_init_rejects_one_bad_slot_among_valid_ones: PASS\n" );
}

static void test_init_computes_correct_major_frame_length( void )
{
    maxrtos_frame_schedule_t sched;
    maxrtos_frame_slot_t slots[ 2 ] =
    {
        { 0U, 5U },
        { 1U, 3U }
    };

    assert(
        maxrtos_frame_init(
            &sched,
            slots,
            2U ) == MAXRTOS_OK );

    assert( sched.slot_count == 2U );
    assert( sched.major_frame_length_ticks == 8U );

    printf( "test_init_computes_correct_major_frame_length: PASS\n" );
}

static void test_partition_at_tick_rejects_null_and_uninitialized( void )
{
    maxrtos_frame_schedule_t sched;
    maxrtos_frame_schedule_t zeroed_sched;
    maxrtos_partition_id_t out;

    zeroed_sched.slot_count = 0U;
    zeroed_sched.major_frame_length_ticks = 0U;

    assert(
        maxrtos_frame_partition_at_tick(
            NULL,
            0U,
            &out ) == MAXRTOS_ERR_INVALID_ARG );

    {
        maxrtos_frame_slot_t slots[ 1 ] =
        {
            { 0U, 5U }
        };

        assert(
            maxrtos_frame_init(
                &sched,
                slots,
                1U ) == MAXRTOS_OK );

        assert(
            maxrtos_frame_partition_at_tick(
                &sched,
                0U,
                NULL ) == MAXRTOS_ERR_INVALID_ARG );
    }

    /* An uninitialized schedule must be rejected before the
     * implementation attempts modulo by major_frame_length_ticks. */
    assert(
        maxrtos_frame_partition_at_tick(
            &zeroed_sched,
            0U,
            &out ) == MAXRTOS_ERR_INVALID_ARG );

    printf(
        "test_partition_at_tick_rejects_null_and_uninitialized: PASS\n" );
}

static void test_partition_at_tick_exact_boundaries( void )
{
    maxrtos_frame_schedule_t sched;
    maxrtos_frame_slot_t slots[ 2 ] =
    {
        { 0U, 5U },
        { 1U, 3U }
    };
    maxrtos_partition_id_t out;

    assert(
        maxrtos_frame_init(
            &sched,
            slots,
            2U ) == MAXRTOS_OK );

    /* Slot 0 occupies [0, 5).
     * Slot 1 occupies [5, 8). */
    assert(
        maxrtos_frame_partition_at_tick(
            &sched,
            0U,
            &out ) == MAXRTOS_OK );

    assert( out == 0U );

    assert(
        maxrtos_frame_partition_at_tick(
            &sched,
            4U,
            &out ) == MAXRTOS_OK );

    assert( out == 0U );

    assert(
        maxrtos_frame_partition_at_tick(
            &sched,
            5U,
            &out ) == MAXRTOS_OK );

    assert( out == 1U );

    assert(
        maxrtos_frame_partition_at_tick(
            &sched,
            7U,
            &out ) == MAXRTOS_OK );

    assert( out == 1U );

    /* Tick 8 is the first tick of the next major frame. */
    assert(
        maxrtos_frame_partition_at_tick(
            &sched,
            8U,
            &out ) == MAXRTOS_OK );

    assert( out == 0U );

    /* Verify lookup remains correct for a tick far beyond the first
     * major frame. */
    assert(
        maxrtos_frame_partition_at_tick(
            &sched,
            ( 800U * 100U ) + 6U,
            &out ) == MAXRTOS_OK );

    assert( out == 1U );

    printf( "test_partition_at_tick_exact_boundaries: PASS\n" );
}

static void test_partition_at_tick_single_slot_schedule( void )
{
    maxrtos_frame_schedule_t sched;
    maxrtos_frame_slot_t slots[ 1 ] =
    {
        { 3U, 10U }
    };
    maxrtos_partition_id_t out;

    assert(
        maxrtos_frame_init(
            &sched,
            slots,
            1U ) == MAXRTOS_OK );

    assert(
        maxrtos_frame_partition_at_tick(
            &sched,
            0U,
            &out ) == MAXRTOS_OK );

    assert( out == 3U );

    assert(
        maxrtos_frame_partition_at_tick(
            &sched,
            9U,
            &out ) == MAXRTOS_OK );

    assert( out == 3U );

    /* Tick 10 wraps to the beginning of the major frame. */
    assert(
        maxrtos_frame_partition_at_tick(
            &sched,
            10U,
            &out ) == MAXRTOS_OK );

    assert( out == 3U );

    printf( "test_partition_at_tick_single_slot_schedule: PASS\n" );
}

int main( void )
{
    test_init_rejects_null_and_bad_args();
    test_init_rejects_bad_slot_partition_id();
    test_init_rejects_zero_duration_slot();
    test_init_rejects_one_bad_slot_among_valid_ones();
    test_init_computes_correct_major_frame_length();
    test_partition_at_tick_rejects_null_and_uninitialized();
    test_partition_at_tick_exact_boundaries();
    test_partition_at_tick_single_slot_schedule();

    printf( "all frame tests passed\n" );

    return 0;
}
