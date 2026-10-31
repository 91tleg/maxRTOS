/**
 * @file test_mpu_regs.c
 * @brief What the MPU hardware holds matches the configuration, per partition.
 *
 * The host and QEMU tests check the MPU region table the kernel computes. This
 * reads the regions back from the real Cortex-M7 MPU, which is where the
 * silicon's own rules show up: the number of regions, the size encoding, how
 * the base address and region number read back, and that regions nothing
 * configured are off.
 *
 * Every tick, before the context switch it requested has run, the poll
 * function reads all 16 regions and compares them with what the running
 * partition should have: the background regions of retarget_module.py, the
 * partition domain in region 8, and nothing from region 9 on. IPC ports are
 * kernel memory and have no region, so those must stay off whichever
 * partition runs.
 */

#include "hil.h"
#include "maxrtos/arch/cortex_m7/context_switch.h"
#include "maxrtos/arch/cortex_m7/mpu.h"

#define MPU_TYPE ( *( volatile uint32_t * ) 0xE000ED90UL )
#define MPU_CTRL ( *( volatile uint32_t * ) 0xE000ED94UL )
#define MPU_RNR  ( *( volatile uint32_t * ) 0xE000ED98UL )
#define MPU_RBAR ( *( volatile uint32_t * ) 0xE000ED9CUL )
#define MPU_RASR ( *( volatile uint32_t * ) 0xE000EDA0UL )

/* RASR fields. */
#define RASR_ENABLE ( 1UL << 0U )
#define RASR_XN     ( 1UL << 28U )

/* Access permissions (RASR.AP) used by the regions below. */
#define AP_PRIV_RW ( 1UL )
#define AP_RW      ( 3UL )
#define AP_RO      ( 6UL )

/* Memory attributes mpu_hw.c gives every region: normal, write-back, TEX 0,
 * C 1, B 1, not shareable. */
#define RASR_ATTRIBUTES ( ( 1UL << 17U ) | ( 1UL << 16U ) )

#define SAMPLES_PER_PARTITION ( 30U )

extern uint8_t __partition_control_domain_start[];
extern uint8_t __partition_application_domain_start[];

typedef struct
{
    uint32_t number;
    uint32_t base;
    uint32_t size;
    uint32_t ap;
    bool executable;
} background_t;

/* Keep in step with REGIONS in retarget_module.py. */
static background_t const k_background[] = {
    { 0U, HIL_FLASH_BASE, HIL_FLASH_SIZE, AP_RO, true },
    { 1U, HIL_AHB4_BASE, HIL_AHB4_SIZE, AP_PRIV_RW, false },
    { 2U, HIL_DTCM_BASE, HIL_DTCM_SIZE, AP_PRIV_RW, false },
    { 3U, HIL_SRAM_BASE, HIL_SRAM_SIZE, AP_PRIV_RW, false },
    { 5U, EMU_RESULTS_BASE, EMU_RESULTS_SIZE, AP_RW, false },
};

#define BACKGROUND_COUNT                                                       \
    ( sizeof( k_background ) / sizeof( k_background[ 0 ] ) )

static uint32_t s_samples[ 2 ];

static void idle_control( void * arg )
{
    ( void ) arg;
    for( ;; )
    {
        maxrtos_yield();
    }
}
static void idle_application( void * arg )
{
    ( void ) arg;
    for( ;; )
    {
        maxrtos_yield();
    }
}

void emu_test_setup( void )
{
    EMU_CREATE( control_main, PARTITION_ID_control, idle_control );
    EMU_CREATE( control_aux, PARTITION_ID_control, idle_control );
    EMU_CREATE( application_main, PARTITION_ID_application, idle_application );
    EMU_CREATE( application_aux, PARTITION_ID_application, idle_application );
}

/* RASR.SIZE encodes a region of 2^(SIZE + 1) bytes. */
static uint32_t size_field( uint32_t size )
{
    uint32_t field = 0U;

    while( ( 2UL << field ) < size )
    {
        field++;
    }

    return field;
}

static uint32_t rasr_for(
    uint32_t size,
    uint32_t ap,
    bool executable )
{
    return RASR_ENABLE | ( size_field( size ) << 1U ) | RASR_ATTRIBUTES |
           ( ap << 24U ) | ( executable ? 0UL : RASR_XN );
}

/* Check one region against what it should hold. A base of 0 with enabled
 * false means "must be off". */
static int check_region(
    uint32_t number,
    bool enabled,
    uint32_t base,
    uint32_t want_rasr )
{
    uint32_t rbar;
    uint32_t rasr;
    int result = EMU_RUNNING;

    MPU_RNR = number;
    rbar = MPU_RBAR;
    rasr = MPU_RASR;

    if( !enabled )
    {
        if( ( rasr & RASR_ENABLE ) != 0U )
        {
            result = hil_fail_values( hil_label( "rasr r", number ), rasr, 0U );
        }
    }
    else if( rasr != want_rasr )
    {
        result =
            hil_fail_values( hil_label( "rasr r", number ), rasr, want_rasr );
    }
    else if( ( rbar & ~0x1FUL ) != base )
    {
        result = hil_fail_values(
            hil_label( "rbar r", number ), rbar & ~0x1FUL, base );
    }
    else if( ( rbar & 0xFUL ) != number )
    {
        result = hil_fail_values(
            hil_label( "rbar region r", number ), rbar & 0xFUL, number );
    }
    else
    {
        /* matches */
    }

    return result;
}

static int check_partition( uint32_t partition )
{
    uint32_t i;
    uint32_t number;
    uint32_t domain_base;
    uint32_t domain_size;
    int result = EMU_RUNNING;

    domain_base = ( partition == 0U )
                      ? ( uint32_t ) __partition_control_domain_start
                      : ( uint32_t ) __partition_application_domain_start;
    domain_size = ( partition == 0U ) ? PARTITION_DOMAIN_SIZE_control
                                      : PARTITION_DOMAIN_SIZE_application;

    /* Background regions, then the unused system regions. */
    for( number = 0U;
         ( number < MAXRTOS_MPU_PARTITION_REGION ) && ( result == EMU_RUNNING );
         number++ )
    {
        bool found = false;

        for( i = 0U; i < BACKGROUND_COUNT; i++ )
        {
            if( k_background[ i ].number == number )
            {
                found = true;
                result = check_region(
                    number,
                    true,
                    k_background[ i ].base,
                    rasr_for(
                        k_background[ i ].size,
                        k_background[ i ].ap,
                        k_background[ i ].executable ) );
            }
        }

        if( !found )
        {
            result = check_region( number, false, 0U, 0U );
        }
    }

    /* The partition domain: read-write, never executable. */
    if( result == EMU_RUNNING )
    {
        result = check_region(
            MAXRTOS_MPU_PARTITION_REGION,
            true,
            domain_base,
            rasr_for( domain_size, AP_RW, false ) );
    }

    /* Nothing above the partition region: a port is not memory a partition
     * can map, so these stay off whichever partition runs. */
    for( number = MAXRTOS_MPU_PARTITION_REGION + 1U;
         ( number < MAXRTOS_MPU_HW_REGION_COUNT ) && ( result == EMU_RUNNING );
         number++ )
    {
        result = check_region( number, false, 0U, 0U );
    }

    return result;
}

int emu_test_poll( uint32_t tick )
{
    maxrtos_process_control_block_t const * pcb;
    uint32_t saved_rnr;
    int result = EMU_RUNNING;

    if( tick == 1U )
    {
        uint32_t const type = MPU_TYPE;

        if( ( ( type >> 8U ) & 0xFFU ) != MAXRTOS_MPU_HW_REGION_COUNT )
        {
            result = hil_fail_values(
                "MPU_TYPE.DREGION",
                ( type >> 8U ) & 0xFFU,
                MAXRTOS_MPU_HW_REGION_COUNT );
        }
        else if( ( type & 1UL ) != 0U )
        {
            result = hil_fail_values( "MPU_TYPE.SEPARATE", type & 1UL, 0U );
        }
        else if( MPU_CTRL != 1UL ) /* ENABLE only: no PRIVDEFENA, no HFNMIENA */
        {
            result = hil_fail_values( "MPU_CTRL", MPU_CTRL, 1U );
        }
        else
        {
            /* fine */
        }
    }

    pcb = maxrtos_arch_get_current_pcb();

    /* The kernel loads the MPU when it switches in a process; between ticks
     * that is the process now running. The idle context belongs to no
     * partition and leaves the previous partition's regions in place. */
    if( ( result == EMU_RUNNING ) && ( pcb != NULL ) &&
        ( pcb->partition_id < 2U ) )
    {
        saved_rnr = MPU_RNR;
        result = check_partition( pcb->partition_id );
        MPU_RNR = saved_rnr;

        if( result == EMU_RUNNING )
        {
            s_samples[ pcb->partition_id ]++;

            if( ( s_samples[ 0 ] >= SAMPLES_PER_PARTITION ) &&
                ( s_samples[ 1 ] >= SAMPLES_PER_PARTITION ) )
            {
                result = EMU_PASS;
            }
        }
    }

    return result;
}
