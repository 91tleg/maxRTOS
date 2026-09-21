/**
 * @file sim_mpu.c
 * @brief Register-level model of the ARMv7-M protected memory system
 *        architecture (PMSAv7) MPU.
 *
 * The production MPU driver (arch/cortex_m7/src/mpu_hw.c) is compiled
 * unchanged and programs this model through its register hooks. Access
 * checks then follow the architectural rules, so a test observes what the
 * driver really configured, not what a test double claims.
 *
 * Modelled: 16 regions, RNR/RBAR/RASR/CTRL, power-of-two sizes, base
 * alignment, the AP encodings, region overlap (highest number wins),
 * PRIVDEFENA, and denial of unmapped access for unprivileged code.
 * Not modelled: subregions (the driver never uses them), attributes,
 * execute-never, alignment faults.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "sim_target.h"

#define MPU_REGIONS ( 16U )

#define CTRL_ENABLE     ( 1UL << 0U )
#define CTRL_PRIVDEFENA ( 1UL << 2U )

typedef struct
{
    bool enabled;
    uint32_t base;
    uint32_t size; /* bytes; 0 when disabled */
    uint32_t ap;
    bool xn;
} sim_mpu_region_t;

static struct
{
    uint32_t ctrl;
    uint32_t rnr;
    uint32_t rbar_shadow[ MPU_REGIONS ];
    sim_mpu_region_t region[ MPU_REGIONS ];
    uint32_t writes;
} s_mpu;

void sim_mpu_reset( void )
{
    ( void ) memset( &s_mpu, 0, sizeof( s_mpu ) );
}

uint32_t sim_mpu_write_count( void )
{
    return s_mpu.writes;
}

void sim_mpu_write( uint32_t reg, uint32_t value )
{
    s_mpu.writes++;

    switch( reg )
    {
        case SIM_MPU_REG_CTRL:
            s_mpu.ctrl = value;
            break;

        case SIM_MPU_REG_RNR:
            s_mpu.rnr = value & 0xFFU;
            break;

        case SIM_MPU_REG_RBAR:
            if( s_mpu.rnr < MPU_REGIONS )
            {
                s_mpu.rbar_shadow[ s_mpu.rnr ] = value;
            }
            break;

        case SIM_MPU_REG_RASR:
            if( s_mpu.rnr < MPU_REGIONS )
            {
                sim_mpu_region_t * r = &s_mpu.region[ s_mpu.rnr ];
                uint32_t size_field = ( value >> 1U ) & 0x1FU;

                r->enabled = ( value & 1U ) != 0U;
                r->ap = ( value >> 24U ) & 0x7U;
                r->xn = ( ( value >> 28U ) & 1U ) != 0U;
                r->size = ( size_field >= 4U && size_field < 31U )
                              ? ( 2UL << size_field ) : 0U;
                r->base = s_mpu.rbar_shadow[ s_mpu.rnr ] & 0xFFFFFFE0UL;

                /* The architecture requires base to be size aligned; a
                 * driver bug here would be UNPREDICTABLE on silicon. */
                if( r->enabled && ( ( r->size == 0U ) ||
                                    ( ( r->base % r->size ) != 0U ) ) )
                {
                    sim_report_model_violation(
                        "MPU region programmed with a base not aligned "
                        "to its size, or an invalid size" );
                    r->enabled = false;
                }
            }
            break;

        default:
            sim_report_model_violation( "write to an unknown MPU register" );
            break;
    }
}

bool sim_mpu_enabled( void )
{
    return ( s_mpu.ctrl & CTRL_ENABLE ) != 0U;
}

static bool ap_permits( uint32_t ap, bool write, bool privileged )
{
    bool permitted;

    switch( ap )
    {
        case 1U: permitted = privileged; break;                      /* priv RW */
        case 2U: permitted = privileged || !write; break;            /* priv RW, user RO */
        case 3U: permitted = true; break;                            /* full */
        case 5U: permitted = privileged && !write; break;            /* priv RO */
        case 6U:
        case 7U: permitted = !write; break;                          /* RO */
        default: permitted = false; break;                           /* none */
    }

    return permitted;
}

/* Highest-numbered enabled region containing the address, or -1. */
static int find_region( uint32_t address )
{
    int found = -1;
    int i;

    for( i = 0; i < ( int ) MPU_REGIONS; i++ )
    {
        sim_mpu_region_t const * r = &s_mpu.region[ i ];

        if( r->enabled && ( address >= r->base ) &&
            ( ( address - r->base ) < r->size ) )
        {
            found = i;
        }
    }

    return found;
}

/* Smallest address > a at which some enabled region starts or ends, so the
 * access decision is constant on [a, boundary - 1]. UINT32_MAX + 1 is
 * represented by 0 (no further boundary). */
static uint32_t next_boundary( uint32_t a )
{
    uint32_t best = 0U;
    unsigned i;

    for( i = 0U; i < MPU_REGIONS; i++ )
    {
        sim_mpu_region_t const * r = &s_mpu.region[ i ];

        if( r->enabled )
        {
            uint32_t start = r->base;
            uint32_t end_plus_one = r->base + r->size; /* 0 on wrap */

            if( ( start > a ) && ( ( best == 0U ) || ( start < best ) ) )
            {
                best = start;
            }

            if( ( end_plus_one > a ) &&
                ( ( best == 0U ) || ( end_plus_one < best ) ) )
            {
                best = end_plus_one;
            }
        }
    }

    return best;
}

bool sim_mpu_access_ok(
    uint32_t address,
    uint32_t length,
    bool write,
    bool privileged )
{
    uint32_t last;
    uint32_t a;

    if( length == 0U )
    {
        return true;
    }

    last = address + ( length - 1U );

    if( last < address )
    {
        return false; /* wraps the address space */
    }

    if( !sim_mpu_enabled() )
    {
        return true;
    }

    a = address;

    for( ;; )
    {
        int idx = find_region( a );
        uint32_t boundary = next_boundary( a );
        uint32_t segment_last = ( boundary == 0U ) ? 0xFFFFFFFFUL : ( boundary - 1U );

        if( idx < 0 )
        {
            if( !( privileged && ( ( s_mpu.ctrl & CTRL_PRIVDEFENA ) != 0U ) ) )
            {
                return false;
            }
        }
        else if( !ap_permits( s_mpu.region[ idx ].ap, write, privileged ) )
        {
            return false;
        }

        if( segment_last >= last )
        {
            return true;
        }

        a = segment_last + 1U;
    }
}

bool sim_mpu_region_info(
    uint32_t region,
    uint32_t * base,
    uint32_t * size,
    uint32_t * ap )
{
    if( ( region >= MPU_REGIONS ) || !s_mpu.region[ region ].enabled )
    {
        return false;
    }

    *base = s_mpu.region[ region ].base;
    *size = s_mpu.region[ region ].size;
    *ap = s_mpu.region[ region ].ap;

    return true;
}
