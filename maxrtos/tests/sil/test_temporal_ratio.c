/**
 * @file test_sil_temporal_ratio.c
 * @brief SIL test of temporal partitioning over many
 *        simulated major frames.
 */

#include <assert.h>
#include <stdio.h>

#include "maxrtos/kernel/process.h"
#include "maxrtos/kernel/partition.h"
#include "maxrtos/kernel/frame.h"
#include "maxrtos/kernel/tick.h"

#define TEST_STACK_SIZE      ( 128U )
#define SIMULATED_FRAMES     ( 200U )

#define PARTITION_0_TICKS    ( 5U )
#define PARTITION_1_TICKS    ( 3U )
#define TICKS_PER_FRAME      \
    ( PARTITION_0_TICKS + PARTITION_1_TICKS )

#define RATIO_TOLERANCE_PERCENT ( 2U )

static uint8_t s_stack_a[ TEST_STACK_SIZE ];
static uint8_t s_stack_b[ TEST_STACK_SIZE ];

static void dummy_entry( void * arg )
{
    ( void ) arg;

    for( ;; )
    {

    }
}

static void test_5_3_ratio_holds_over_many_frames( void )
{
    maxrtos_partition_table_t table;
    maxrtos_frame_schedule_t schedule;
    maxrtos_frame_slot_t slots[ 2 ];
    maxrtos_process_id_t id_p0;
    maxrtos_process_id_t id_p1;
    maxrtos_process_control_block_t * pcb_p0;
    maxrtos_process_control_block_t * pcb_p1;

    uint32_t partition_0_dispatches;
    uint32_t partition_1_dispatches;
    uint32_t total_ticks;
    uint32_t tick;

    double expected_ratio;
    double actual_ratio;
    double lower_bound;
    double upper_bound;

    maxrtos_process_pool_init();
    assert(
        maxrtos_partition_table_init(
            &table ) == MAXRTOS_OK );

    assert(
        maxrtos_process_create(
            s_stack_a,
            TEST_STACK_SIZE,
            ( maxrtos_partition_id_t ) 0U,
            5U,
            true,
            dummy_entry,
            NULL,
            &id_p0 ) == MAXRTOS_OK );

    assert(
        maxrtos_process_create(
            s_stack_b,
            TEST_STACK_SIZE,
            ( maxrtos_partition_id_t ) 1U,
            5U,
            true,
            dummy_entry,
            NULL,
            &id_p1 ) == MAXRTOS_OK );

    assert(
        maxrtos_partition_add_process(
            &table, 
            id_p0 ) == MAXRTOS_OK );

    assert(
        maxrtos_partition_add_process(
            &table,
            id_p1 ) == MAXRTOS_OK );

    pcb_p0 = maxrtos_process_get( id_p0 );
    pcb_p1 = maxrtos_process_get( id_p1 );
    assert( pcb_p0 != NULL );
    assert( pcb_p1 != NULL );

    slots[ 0 ].partition_id = 0U;
    slots[ 0 ].duration_ticks = PARTITION_0_TICKS;

    slots[ 1 ].partition_id = 1U;
    slots[ 1 ].duration_ticks = PARTITION_1_TICKS;

    assert(
        maxrtos_frame_init(
            &schedule,
            slots,
            2U ) == MAXRTOS_OK );

    partition_0_dispatches = 0U;
    partition_1_dispatches = 0U;
    total_ticks = SIMULATED_FRAMES * TICKS_PER_FRAME;

    for( tick = 0U; tick < total_ticks; tick++ )
    {
        maxrtos_process_id_t next_id;
        maxrtos_process_control_block_t * next_pcb;

        assert(
            maxrtos_kernel_on_tick(
                &schedule,
                &table,
                tick,
                &next_id ) == MAXRTOS_OK );

        next_pcb = maxrtos_process_get( next_id );
        assert( next_pcb != NULL );

        if( next_pcb->partition_id == 0U )
        {
            partition_0_dispatches++;
        }
        else if( next_pcb->partition_id == 1U )
        {
            partition_1_dispatches++;
        }
        else
        {
            assert( 0 && "dispatch returned unexpected partition" );
        }
    }

    /* Sanity: every tick must have been attributed to exactly one
     * of the two partitions. */
    assert(
        ( partition_0_dispatches + partition_1_dispatches ) ==
        total_ticks );

    expected_ratio =
        ( double ) PARTITION_0_TICKS / ( double ) PARTITION_1_TICKS;

    actual_ratio =
        ( double ) partition_0_dispatches /
        ( double ) partition_1_dispatches;

    lower_bound =
        expected_ratio *
        ( 1.0 - ( ( double ) RATIO_TOLERANCE_PERCENT / 100.0 ) );

    upper_bound =
        expected_ratio *
        ( 1.0 + ( ( double ) RATIO_TOLERANCE_PERCENT / 100.0 ) );

    printf(
        "  partition 0 dispatches: %u\n"
        "  partition 1 dispatches: %u\n"
        "  expected ratio: %.4f, actual ratio: %.4f "
        "(tolerance [%.4f, %.4f])\n",
        partition_0_dispatches,
        partition_1_dispatches,
        expected_ratio,
        actual_ratio,
        lower_bound,
        upper_bound );

    assert( actual_ratio >= lower_bound );
    assert( actual_ratio <= upper_bound );

    printf( "test_5_3_ratio_holds_over_many_frames: PASS\n" );
}

int main( void )
{
    test_5_3_ratio_holds_over_many_frames();

    printf( "all SIL temporal ratio tests passed\n" );

    return 0;
}
