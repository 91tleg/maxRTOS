/**
 * @file test_yield.c
 * @brief maxrtos_yield() hands the CPU to the other process of the same
 *        partition, and only that partition.
 *
 * Each partition has two equal-priority processes that count and yield.
 * If yield did nothing, or gave the CPU to the wrong place, one process of
 * a pair would starve while the other ran away.
 */

#include "harness.h"

enum { C_CONTROL_MAIN, C_CONTROL_AUX, C_APPLICATION_MAIN, C_APPLICATION_AUX };

#define MIN_COUNT ( 50U )
#define MAX_SKEW  ( 2U )


static void control_main( void * arg ) { ( void ) arg; for( ;; ) { EMU_RES[ C_CONTROL_MAIN ]++; maxrtos_yield(); } }
static void control_aux( void * arg ) { ( void ) arg; for( ;; ) { EMU_RES[ C_CONTROL_AUX ]++; maxrtos_yield(); } }
static void application_main( void * arg ) { ( void ) arg; for( ;; ) { EMU_RES[ C_APPLICATION_MAIN ]++; maxrtos_yield(); } }
static void application_aux( void * arg ) { ( void ) arg; for( ;; ) { EMU_RES[ C_APPLICATION_AUX ]++; maxrtos_yield(); } }

void emu_test_setup( void )
{
    EMU_CREATE( control_main, PARTITION_ID_control, control_main );
    EMU_CREATE( control_aux, PARTITION_ID_control, control_aux );
    EMU_CREATE( application_main, PARTITION_ID_application, application_main );
    EMU_CREATE( application_aux, PARTITION_ID_application, application_aux );
}

static uint32_t skew( uint32_t a, uint32_t b )
{
    return ( a > b ) ? ( a - b ) : ( b - a );
}

int emu_test_poll( uint32_t tick )
{
    uint32_t c0 = EMU_RES[ C_CONTROL_MAIN ];
    uint32_t c1 = EMU_RES[ C_CONTROL_AUX ];
    uint32_t a0 = EMU_RES[ C_APPLICATION_MAIN ];
    uint32_t a1 = EMU_RES[ C_APPLICATION_AUX ];

    ( void ) tick;

    /* Strict alternation keeps each pair within a step of each other. */
    if( ( skew( c0, c1 ) > MAX_SKEW ) || ( skew( a0, a1 ) > MAX_SKEW ) )
    {
        return EMU_FAILED( "a process ran ahead of its partition peer" );
    }

    if( ( c0 >= MIN_COUNT ) && ( c1 >= MIN_COUNT ) &&
        ( a0 >= MIN_COUNT ) && ( a1 >= MIN_COUNT ) )
    {
        return EMU_PASS;
    }

    return EMU_RUNNING;
}
