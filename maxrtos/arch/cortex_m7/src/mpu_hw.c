/**
 * @file mpu_hw.c
 * @brief Cortex-M7 MPU hardware configuration.
 *
 * Translates validated MPU configuration into hardware register
 * settings and performs the required synchronization barriers.
 *
 * MPU regions below MAXRTOS_MPU_PARTITION_REGION are reserved for
 * system mappings. MAXRTOS_MPU_PARTITION_REGION is reused for the
 * currently scheduled partition. Remaining regions are statically
 * assigned to communication ports and are enabled only for their
 * declared member partitions.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "maxrtos/types.h"
#include "maxrtos/arch/cortex_m7/mpu_hw.h"
#include "maxrtos/arch/cortex_m7/port.h"

#define MAXRTOS_MPU_CTRL MAXRTOS_PORT_MPU_CTRL
#define MAXRTOS_MPU_RNR  MAXRTOS_PORT_MPU_RNR
#define MAXRTOS_MPU_RBAR MAXRTOS_PORT_MPU_RBAR
#define MAXRTOS_MPU_RASR MAXRTOS_PORT_MPU_RASR

#define MAXRTOS_MPU_CTRL_ENABLE_BIT  ( UINT32_C( 1 ) << 0U )
#define MAXRTOS_MPU_CTRL_PRIVDEFENA_BIT ( UINT32_C( 1 ) << 2U )

/* RASR bit-field positions. */
#define MAXRTOS_RASR_ENABLE_POS ( 0U )
#define MAXRTOS_RASR_SIZE_POS   ( 1U )
#define MAXRTOS_RASR_B_POS      ( 16U )
#define MAXRTOS_RASR_C_POS      ( 17U )
#define MAXRTOS_RASR_S_POS      ( 18U )
#define MAXRTOS_RASR_TEX_POS    ( 19U )
#define MAXRTOS_RASR_AP_POS     ( 24U )
#define MAXRTOS_RASR_XN_POS     ( 28U )

/* RASR access-permission encodings. */
#define MAXRTOS_RASR_AP_NO_ACCESS      ( UINT32_C( 0 ) )
#define MAXRTOS_RASR_AP_READ_ONLY      ( UINT32_C( 6 ) )
#define MAXRTOS_RASR_AP_READ_WRITE     ( UINT32_C( 3 ) )

/* Privileged-only AP encodings. */
#define MAXRTOS_RASR_AP_PRIV_READ_WRITE ( UINT32_C( 1 ) )
#define MAXRTOS_RASR_AP_PRIV_READ_ONLY  ( UINT32_C( 5 ) )

/* Default memory attributes for configured regions: Normal memory,
 * inner/outer write-back cacheable, and non-shareable. */
#define MAXRTOS_RASR_TEX_DEFAULT       ( UINT32_C( 0 ) )
#define MAXRTOS_RASR_C_DEFAULT         ( UINT32_C( 1 ) )
#define MAXRTOS_RASR_B_DEFAULT         ( UINT32_C( 1 ) )
#define MAXRTOS_RASR_S_DEFAULT         ( UINT32_C( 0 ) )

#define MAXRTOS_RASR_XN_EXECUTABLE     ( UINT32_C( 0 ) )
#define MAXRTOS_RASR_XN_NON_EXECUTABLE ( UINT32_C( 1 ) )

static maxrtos_mpu_config_t const * s_config = NULL;

/* Partition whose region is currently programmed into the partition
 * slot. No partition is loaded until the first context switch, and
 * installing a new configuration invalidates whatever was loaded. */
static uint32_t s_active_partition_id = MAXRTOS_INVALID_PARTITION_ID;

/**
 * @brief Convert a power-of-two region size to the ARM MPU SIZE field.
 *
 * The ARM MPU SIZE field encodes:
 *
 *     SIZE = log2(region_size) - 1
 *
 * @param[in] size_bytes
 *     Region size in bytes. Must be a power of two and at least 32 bytes.
 *
 * @return Encoded ARM MPU SIZE field value.
 */
static uint32_t maxrtos_mpu_encode_size_field(
    uint32_t size_bytes )
{
    uint32_t size_field;
    uint32_t size_bytes_copy;

    size_field = 0U;
    size_bytes_copy = size_bytes;

    while( size_bytes_copy != 1U )
    {
        size_bytes_copy >>= 1U;
        size_field++;
    }

    size_field -= 1U;

    return size_field;
}

static void maxrtos_arch_mpu_program_region(
    uint32_t region_number,
    uint32_t base_address,
    uint32_t size_bytes,
    maxrtos_mpu_access_t access,
    bool executable )
{
    uint32_t size_field;
    uint32_t ap_field;
    uint32_t xn_field;
    uint32_t rasr;

    size_field = maxrtos_mpu_encode_size_field( size_bytes );

    switch( access )
    {
        case MAXRTOS_MPU_ACCESS_READ_ONLY:
            ap_field = MAXRTOS_RASR_AP_READ_ONLY;
            break;

        case MAXRTOS_MPU_ACCESS_READ_WRITE:
            ap_field = MAXRTOS_RASR_AP_READ_WRITE;
            break;

        case MAXRTOS_MPU_ACCESS_PRIV_READ_ONLY:
            ap_field = MAXRTOS_RASR_AP_PRIV_READ_ONLY;
            break;

        case MAXRTOS_MPU_ACCESS_PRIV_READ_WRITE:
            ap_field = MAXRTOS_RASR_AP_PRIV_READ_WRITE;
            break;

        case MAXRTOS_MPU_ACCESS_NONE:
        default:
            ap_field = MAXRTOS_RASR_AP_NO_ACCESS;
            break;
    }

    xn_field = executable ?
        MAXRTOS_RASR_XN_EXECUTABLE :
        MAXRTOS_RASR_XN_NON_EXECUTABLE;

    MAXRTOS_PORT_MPU_REG_WRITE( MAXRTOS_MPU_RNR, region_number );
    MAXRTOS_PORT_MPU_REG_WRITE( MAXRTOS_MPU_RBAR, base_address );

    rasr = ( UINT32_C( 1 ) << MAXRTOS_RASR_ENABLE_POS ) |
           ( size_field << MAXRTOS_RASR_SIZE_POS ) |
           ( ap_field << MAXRTOS_RASR_AP_POS ) |
           ( MAXRTOS_RASR_TEX_DEFAULT << MAXRTOS_RASR_TEX_POS ) |
           ( MAXRTOS_RASR_C_DEFAULT << MAXRTOS_RASR_C_POS ) |
           ( MAXRTOS_RASR_B_DEFAULT << MAXRTOS_RASR_B_POS ) |
           ( MAXRTOS_RASR_S_DEFAULT << MAXRTOS_RASR_S_POS ) |
           ( xn_field << MAXRTOS_RASR_XN_POS );
           /* SRD remains zero, enabling all subregions. */

    MAXRTOS_PORT_MPU_REG_WRITE( MAXRTOS_MPU_RASR, rasr );

    MAXRTOS_PORT_BARRIER();
}

static void maxrtos_arch_mpu_disable_region(
    uint32_t region_number )
{
    MAXRTOS_PORT_MPU_REG_WRITE( MAXRTOS_MPU_RNR, region_number );
    MAXRTOS_PORT_MPU_REG_WRITE( MAXRTOS_MPU_RASR, 0UL );

    MAXRTOS_PORT_BARRIER();
}

void maxrtos_arch_mpu_set_config(
    maxrtos_mpu_config_t const * config )
{
    s_config = config;
    s_active_partition_id = MAXRTOS_INVALID_PARTITION_ID;
}

void maxrtos_arch_mpu_init( bool privileged_default_map )
{
    uint32_t ctrl;

    ctrl = MAXRTOS_MPU_CTRL_ENABLE_BIT;

    if( privileged_default_map == true )
    {
        ctrl |= MAXRTOS_MPU_CTRL_PRIVDEFENA_BIT;
    }

    MAXRTOS_PORT_MPU_REG_WRITE( MAXRTOS_MPU_CTRL, ctrl );

    MAXRTOS_PORT_BARRIER();
}

static void maxrtos_arch_mpu_configure_port_regions_for_partition(
    maxrtos_partition_id_t partition_id )
{
    size_t count;
    size_t i;

    count = maxrtos_mpu_get_port_region_count( s_config );

    for( i = 0U; i < count; i++ )
    {
        maxrtos_mpu_port_region_config_t port_region;
        uint32_t region_number;

        region_number = MAXRTOS_MPU_PARTITION_REGION + 1U + ( uint32_t ) i;

        if( maxrtos_mpu_get_port_region(
                s_config,
                i,
                &port_region ) == MAXRTOS_OK )
        {
            bool is_member;

            is_member = ( ( port_region.member_partition_mask &
                            ( 1UL << ( uint32_t ) partition_id ) ) != 0U );

            if( is_member )
            {
                maxrtos_arch_mpu_program_region(
                    region_number,
                    port_region.base_address,
                    port_region.size_bytes,
                    MAXRTOS_MPU_ACCESS_READ_WRITE,
                    false );
            }
            else
            {
                maxrtos_arch_mpu_disable_region( region_number );
            }
        }
    }
}

static maxrtos_status_t maxrtos_arch_mpu_configure_for_partition(
    maxrtos_partition_id_t partition_id )
{
    maxrtos_status_t status;
    maxrtos_mpu_region_config_t region;

    if( ( uint32_t ) partition_id == s_active_partition_id )
    {
        status = MAXRTOS_OK;
    }
    else
    {
        status = maxrtos_mpu_get_partition_region(
            s_config,
            partition_id,
            &region );

        if( status == MAXRTOS_OK )
        {
            maxrtos_arch_mpu_program_region(
                MAXRTOS_MPU_PARTITION_REGION,
                region.base_address,
                region.size_bytes,
                region.access,
                region.executable );

            s_active_partition_id = ( uint32_t ) partition_id;
        }
    }

    if( status == MAXRTOS_OK )
    {
        maxrtos_arch_mpu_configure_port_regions_for_partition(
            partition_id );
    }

    return status;
}

maxrtos_status_t maxrtos_arch_mpu_configure_for_next_pcb(
    maxrtos_process_control_block_t const * next_pcb )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( next_pcb != NULL )
    {
        if( next_pcb->partition_id == MAXRTOS_INVALID_PARTITION_ID )
        {
            /* The architecture idle context: privileged and belongs to
             * no partition, so there is no partition memory to map. The
             * previously loaded partition region is left as it is. */
            status = MAXRTOS_OK;
        }
        else
        {
            status = maxrtos_arch_mpu_configure_for_partition(
                next_pcb->partition_id );
        }
    }

    return status;
}

maxrtos_status_t maxrtos_arch_mpu_configure_region(
    uint32_t region_number,
    uint32_t base_address,
    uint32_t size_bytes,
    maxrtos_mpu_access_t access,
    bool executable )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( region_number < MAXRTOS_MPU_PARTITION_REGION ) &&
        ( size_bytes >= MAXRTOS_MPU_REGION_MIN_SIZE ) &&
        ( ( base_address % size_bytes ) == 0U ) &&
        ( ( access == MAXRTOS_MPU_ACCESS_NONE ) ||
          ( access == MAXRTOS_MPU_ACCESS_READ_ONLY ) ||
          ( access == MAXRTOS_MPU_ACCESS_READ_WRITE ) ||
          ( access == MAXRTOS_MPU_ACCESS_PRIV_READ_ONLY ) ||
          ( access == MAXRTOS_MPU_ACCESS_PRIV_READ_WRITE ) ) )
    {
        maxrtos_arch_mpu_program_region(
            region_number,
            base_address,
            size_bytes,
            access,
            executable );

        status = MAXRTOS_OK;
    }

    return status;
}

bool maxrtos_arch_mpu_partition_owns_range(
    maxrtos_partition_id_t partition_id,
    uint32_t address,
    uint32_t size )
{
    bool owns;
    maxrtos_mpu_region_config_t region;

    owns = false;

    if( ( s_config != NULL ) &&
        ( size > 0U ) &&
        ( maxrtos_mpu_get_partition_region(
              s_config, partition_id, &region ) == MAXRTOS_OK ) &&
        ( region.access == MAXRTOS_MPU_ACCESS_READ_WRITE ) )
    {
        uint32_t end;

        end = address + size;

        /* end < address detects wrap-around. */
        owns = ( end > address ) &&
               ( address >= region.base_address ) &&
               ( end <= ( region.base_address + region.size_bytes ) );
    }

    return owns;
}

bool maxrtos_arch_mpu_is_port_member(
    maxrtos_partition_id_t partition_id,
    uint32_t address )
{
    bool member;

    member = false;

    if( ( s_config != NULL ) && ( partition_id < MAXRTOS_MAX_PARTITIONS ) )
    {
        size_t count;

        count = maxrtos_mpu_get_port_region_count( s_config );

        for( size_t i = 0U; ( i < count ) && ( member == false ); i++ )
        {
            maxrtos_mpu_port_region_config_t port_region;

            if( ( maxrtos_mpu_get_port_region( s_config, i, &port_region ) ==
                  MAXRTOS_OK ) &&
                ( port_region.base_address == address ) &&
                ( ( port_region.member_partition_mask &
                    ( 1UL << ( uint32_t ) partition_id ) ) != 0U ) )
            {
                member = true;
            }
        }
    }

    return member;
}
