/**
 * @file sil_system.c
 * @brief Builds a partitioned RTOS system on the virtual target.
 */

#include <string.h>

#include "sil_system.h"
#include "sil_test.h"

#include "maxrtos/arch/cortex_m7/fault_handlers.h"
#include "maxrtos/arch/cortex_m7/mpu_hw.h"
#include "maxrtos/arch/cortex_m7/systick.h"
#include "maxrtos/arch/cortex_m7/context_switch.h"
#include "maxrtos/kernel/process.h"
#include "maxrtos/kernel/timing.h"

#define PORT_BYTES ( 2048U )

static sil_system_t s_system;

sil_system_t * sil_system( void )
{
    return &s_system;
}

static uint32_t round_up_pow2( uint32_t value )
{
    uint32_t p = 1U;

    while( p < value )
    {
        p <<= 1U;
    }

    return p;
}

void sil_spec_defaults( sil_system_spec_t * spec )
{
    uint32_t p;
    uint32_t f;

    ( void ) memset( spec, 0, sizeof( *spec ) );

    for( p = 0U; p < SIL_MAX_PARTITIONS; p++ )
    {
        for( f = 0U; f < ( uint32_t ) MAXRTOS_FAULT_COUNT; f++ )
        {
            spec->partition[ p ].action[ f ] = MAXRTOS_HM_ACTION_COUNT;
        }
    }
}

void sil_spec_two_partitions(
    sil_system_spec_t * spec,
    void ( * p0_entry )( void * ),
    void ( * p1_entry )( void * ) )
{
    sil_spec_defaults( spec );

    spec->partition_count = 2U;
    spec->partition[ 0 ].process_count = 1U;
    spec->partition[ 0 ].process[ 0 ].entry = p0_entry;
    spec->partition[ 0 ].process[ 0 ].priority = 5U;
    spec->partition[ 1 ].process_count = 1U;
    spec->partition[ 1 ].process[ 0 ].entry = p1_entry;
    spec->partition[ 1 ].process[ 0 ].priority = 5U;

    spec->slot_count = 2U;
    spec->slot[ 0 ].partition_id = 0U;
    spec->slot[ 0 ].duration_ticks = 5U;
    spec->slot[ 1 ].partition_id = 1U;
    spec->slot[ 1 ].duration_ticks = 3U;
}

void sil_build( sil_system_spec_t const * spec )
{
    sil_system_t * sys = &s_system;
    uint32_t sram_cursor;
    uint32_t p;
    uint32_t i;
    uint8_t * sram;

    SIL_REQUIRE( spec->partition_count > 0U );
    SIL_REQUIRE( spec->partition_count <= SIL_SRAM_SIZE / ( 128U * 1024U ) );

    sim_reset();
    ( void ) memset( sys, 0, sizeof( *sys ) );

    ( void ) sim_mem_map( "DTCM", SIL_DTCM_BASE, SIL_DTCM_SIZE );
    sram = sim_mem_map( "SRAM", SIL_SRAM_BASE, SIL_SRAM_SIZE );
    SIL_REQUIRE( sram != NULL );

    /* Partition table and the health monitor (generated config order). */
    SIL_REQUIRE_EQ( maxrtos_partition_table_init( &sys->table ), MAXRTOS_OK );
    maxrtos_arch_set_partition_table( &sys->table );

    SIL_REQUIRE_EQ( maxrtos_hm_init( &sys->hm ), MAXRTOS_OK );

    for( p = 0U; p < spec->partition_count; p++ )
    {
        uint32_t f;

        for( f = 0U; f < ( uint32_t ) MAXRTOS_FAULT_COUNT; f++ )
        {
            maxrtos_hm_action_t action = spec->partition[ p ].action[ f ];

            if( action == MAXRTOS_HM_ACTION_COUNT )
            {
                action = MAXRTOS_HM_ACTION_RESTART_PROCESS;
            }

            SIL_REQUIRE_EQ( maxrtos_hm_set_policy( &sys->hm, ( maxrtos_partition_id_t ) p,
                                                   ( maxrtos_fault_type_t ) f, action ),
                            MAXRTOS_OK );
        }
    }

    maxrtos_arch_set_health_monitor( &sys->hm );
    maxrtos_arch_fault_handlers_init();

    /* Privilege is production state that outlives one build: start from
     * "everything unprivileged", then apply the spec, as a reset does. */
    for( p = 0U; p < MAXRTOS_MAX_PARTITIONS; p++ )
    {
        maxrtos_arch_set_partition_privileged( ( maxrtos_partition_id_t ) p, false );
    }

    for( p = 0U; p < spec->partition_count; p++ )
    {
        if( spec->partition[ p ].system )
        {
            maxrtos_arch_set_partition_privileged( ( maxrtos_partition_id_t ) p, true );
        }
    }

    /* One power-of-two, size-aligned MPU domain per partition. */
    SIL_REQUIRE_EQ( maxrtos_mpu_config_init( &sys->mpu ), MAXRTOS_OK );
    sram_cursor = 0U;

    for( p = 0U; p < spec->partition_count; p++ )
    {
        uint32_t stacks = ( spec->partition[ p ].process_count > 0U )
                              ? spec->partition[ p ].process_count : 1U;
        uint32_t size = round_up_pow2( stacks * SIL_STACK_BYTES );

        sram_cursor = ( sram_cursor + size - 1U ) / size * size;
        sys->domain_base[ p ] = SIL_SRAM_BASE + sram_cursor;
        sys->domain_size[ p ] = size;
        sram_cursor += size;

        SIL_REQUIRE_EQ( maxrtos_mpu_set_partition_region(
                            &sys->mpu, ( maxrtos_partition_id_t ) p,
                            sys->domain_base[ p ], size,
                            MAXRTOS_MPU_ACCESS_READ_WRITE, false ),
                        MAXRTOS_OK );
    }

    maxrtos_arch_mpu_set_config( &sys->mpu );

    /* Static regions, as in the demo's module.json. */
    ( void ) maxrtos_arch_mpu_configure_region( 0U, 0x08000000UL, 0x200000UL, MAXRTOS_MPU_ACCESS_READ_ONLY, true );
    ( void ) maxrtos_arch_mpu_configure_region( 1U, 0x58000000UL, 0x80000UL, MAXRTOS_MPU_ACCESS_READ_WRITE, false );
    ( void ) maxrtos_arch_mpu_configure_region( 2U, SIL_DTCM_BASE, SIL_DTCM_SIZE, MAXRTOS_MPU_ACCESS_PRIV_READ_WRITE, false );
    ( void ) maxrtos_arch_mpu_configure_region( 3U, SIL_SRAM_BASE, SIL_SRAM_SIZE, MAXRTOS_MPU_ACCESS_PRIV_READ_WRITE, false );

    /* IPC ports, after the domains, each aligned to its own size. */
    for( i = 0U; i < spec->port_count; i++ )
    {
        uint32_t mask = spec->port[ i ].member_mask;

        sram_cursor = ( sram_cursor + PORT_BYTES - 1U ) / PORT_BYTES * PORT_BYTES;
        sys->port_address[ i ] = SIL_SRAM_BASE + sram_cursor;
        sys->port[ i ] = ( maxrtos_queue_port_t * ) ( void * ) ( sram + sram_cursor );
        sram_cursor += PORT_BYTES;

        SIL_REQUIRE( sizeof( maxrtos_queue_port_t ) <= PORT_BYTES );
        SIL_REQUIRE_EQ( maxrtos_mpu_add_port_region( &sys->mpu, sys->port_address[ i ], PORT_BYTES, mask ),
                        MAXRTOS_OK );
        SIL_REQUIRE_EQ( maxrtos_queue_port_init( sys->port[ i ], spec->port[ i ].message_size,
                                                 spec->port[ i ].capacity ),
                        MAXRTOS_OK );
    }

    maxrtos_arch_mpu_init( false );

    /* Frame schedule, context switching, SysTick wiring. */
    for( i = 0U; i < spec->slot_count; i++ )
    {
        sys->slots[ i ] = spec->slot[ i ];
    }

    SIL_REQUIRE_EQ( maxrtos_frame_init( &sys->frame, sys->slots, spec->slot_count ), MAXRTOS_OK );
    maxrtos_arch_context_switch_init();
    maxrtos_arch_systick_init( &sys->frame, &sys->table );

    /* Application: create the processes, each on its own stack inside its
     * partition's domain. */
    for( p = 0U; p < spec->partition_count; p++ )
    {
        for( i = 0U; i < spec->partition[ p ].process_count; i++ )
        {
            sil_process_spec_t const * proc = &spec->partition[ p ].process[ i ];
            uint8_t * stack = sram + ( sys->domain_base[ p ] - SIL_SRAM_BASE ) + ( i * SIL_STACK_BYTES );

            SIL_REQUIRE_EQ( maxrtos_process_create(
                                stack, SIL_STACK_BYTES, ( maxrtos_partition_id_t ) p,
                                proc->priority, proc->entry, proc->arg,
                                &sys->process_id[ p ][ i ] ),
                            MAXRTOS_OK );

            if( ( proc->period != 0U ) || ( proc->time_capacity != 0U ) )
            {
                SIL_REQUIRE_EQ( maxrtos_process_set_timing( sys->process_id[ p ][ i ],
                                                            proc->period, proc->time_capacity ),
                                MAXRTOS_OK );
            }
        }
    }
}
