/**
 * @file mpu_hw.c
 * @brief Cortex-M7 MPU hardware configuration.
 *
 * This module translates validated configuration into MPU register
 * values and performs the required synchronization barriers.
 *
 * MPU regions 0 through 7 are reserved for system-wide memory
 * mappings such as program code, peripherals, and kernel memory.
 *
 * MPU regions 8 through 15 are reserved for partition-owned
 * memory mappings. At most one partition-owned region is enabled
 * at a time.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "maxrtos/arch/cortex_m7/mpu_hw.h"

#define MAXRTOS_MPU_CTRL ( *( volatile uint32_t * ) 0xE000ED94UL )
#define MAXRTOS_MPU_RNR  ( *( volatile uint32_t * ) 0xE000ED98UL )
#define MAXRTOS_MPU_RBAR ( *( volatile uint32_t * ) 0xE000ED9CUL )
#define MAXRTOS_MPU_RASR ( *( volatile uint32_t * ) 0xE000EDA0UL )

#define MAXRTOS_MPU_CTRL_ENABLE_BIT ( 1UL << 0U )

#define MAXRTOS_MPU_SYSTEM_REGION_COUNT ( 8U )
#define MAXRTOS_MPU_REGION_COUNT        ( 16U )

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

/* Default memory attributes for configured partition regions:
 * Normal memory, inner/outer write-back cacheable, and non-shareable. */
#define MAXRTOS_RASR_TEX_DEFAULT       ( UINT32_C( 0 ) )
#define MAXRTOS_RASR_C_DEFAULT         ( UINT32_C( 1 ) )
#define MAXRTOS_RASR_B_DEFAULT         ( UINT32_C( 1 ) )
#define MAXRTOS_RASR_S_DEFAULT         ( UINT32_C( 0 ) )

#define MAXRTOS_RASR_XN_EXECUTABLE     ( UINT32_C( 0 ) )
#define MAXRTOS_RASR_XN_NON_EXECUTABLE ( UINT32_C( 1 ) )

static maxrtos_mpu_config_t const * s_config = NULL;

static bool s_active_region_valid = false;
static maxrtos_partition_id_t s_active_partition_id = 0U;

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

        case MAXRTOS_MPU_ACCESS_NONE:
        default:
            ap_field = MAXRTOS_RASR_AP_NO_ACCESS;
            break;
    }

    xn_field = executable ?
        MAXRTOS_RASR_XN_EXECUTABLE :
        MAXRTOS_RASR_XN_NON_EXECUTABLE;

    MAXRTOS_MPU_RNR = region_number;
    MAXRTOS_MPU_RBAR = base_address;

    rasr = ( UINT32_C( 1 ) << MAXRTOS_RASR_ENABLE_POS ) |
           ( size_field << MAXRTOS_RASR_SIZE_POS ) |
           ( ap_field << MAXRTOS_RASR_AP_POS ) |
           ( MAXRTOS_RASR_TEX_DEFAULT << MAXRTOS_RASR_TEX_POS ) |
           ( MAXRTOS_RASR_C_DEFAULT << MAXRTOS_RASR_C_POS ) |
           ( MAXRTOS_RASR_B_DEFAULT << MAXRTOS_RASR_B_POS ) |
           ( MAXRTOS_RASR_S_DEFAULT << MAXRTOS_RASR_S_POS ) |
           ( xn_field << MAXRTOS_RASR_XN_POS );
           /* SRD remains zero, enabling all subregions. */

    MAXRTOS_MPU_RASR = rasr;

    __asm volatile ( "dsb" );
    __asm volatile ( "isb" );
}

/* Map a partition ID to its reserved MPU region.
 * Regions 0 through 7 are reserved for system-wide regions.
 * Partition 0 therefore starts at region 8. */
static uint32_t maxrtos_mpu_partition_region(
    maxrtos_partition_id_t partition_id )
{
    return MAXRTOS_MPU_SYSTEM_REGION_COUNT +
           ( uint32_t ) partition_id;
}

static void maxrtos_arch_mpu_disable_region(
    uint32_t region_number )
{
    MAXRTOS_MPU_RNR = region_number;
    MAXRTOS_MPU_RASR &= ~( UINT32_C( 1 ) << MAXRTOS_RASR_ENABLE_POS );

    __asm volatile ( "dsb" );
    __asm volatile ( "isb" );
}

void maxrtos_arch_mpu_set_config(
    maxrtos_mpu_config_t const * config )
{
    s_config = config;
}

void maxrtos_arch_mpu_init( void )
{
    /* Enable the MPU with PRIVDEFENA clear so accesses not
     * covered by an enabled region are denied. */
    MAXRTOS_MPU_CTRL = MAXRTOS_MPU_CTRL_ENABLE_BIT;

    __asm volatile ( "dsb" );
    __asm volatile ( "isb" );
}

static void maxrtos_arch_mpu_configure_for_partition(
    maxrtos_partition_id_t partition_id )
{
    maxrtos_mpu_region_config_t region;
    uint32_t partition_region;

    partition_region = maxrtos_mpu_partition_region( partition_id );

    /* Disable current partition's data region before
     * enabling the incoming one. */
    if( s_active_region_valid &&
        ( s_active_partition_id != partition_id ) )
    {
        uint32_t active_region;

        active_region = maxrtos_mpu_partition_region(
            s_active_partition_id );

        maxrtos_arch_mpu_disable_region( active_region );
    }

    /* Configuration was validated during boot by
     * maxrtos_mpu_set_partition_region(). */
    ( void ) maxrtos_mpu_get_partition_region(
        s_config,
        partition_id,
        &region );

    maxrtos_arch_mpu_program_region(
        partition_region,
        region.base_address,
        region.size_bytes,
        region.access,
        region.executable );
    
    s_active_partition_id = partition_id;
    s_active_region_valid = true;
}

void maxrtos_arch_mpu_configure_for_next_pcb(
    maxrtos_process_control_block_t const * next_pcb )
{
    /* Called from the context-switch path with the next process's
     * PCB. The NULL check protects the C/assembly interface. */
    if( next_pcb != NULL )
    {
        maxrtos_arch_mpu_configure_for_partition(
            next_pcb->partition_id );
    }
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

    if( ( region_number >= MAXRTOS_MPU_SYSTEM_REGION_COUNT ) &&
        ( region_number < MAXRTOS_MPU_REGION_COUNT ) &&
        ( size_bytes >= MAXRTOS_MPU_REGION_MIN_SIZE ) &&
        ( ( base_address % size_bytes ) == 0U ) &&
        ( ( access == MAXRTOS_MPU_ACCESS_NONE ) ||
          ( access == MAXRTOS_MPU_ACCESS_READ_ONLY ) ||
          ( access == MAXRTOS_MPU_ACCESS_READ_WRITE ) ) )
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
