/**
 * @file mpu_hw.c
 * @brief Cortex-M7 MPU hardware configuration.
 *
 * This module is the hardware-specific counterpart to mpu.c.
 * mpu.c owns MPU configuration representation and validation, while
 * this module translates validated configuration into MPU register
 * values and performs the required synchronization barriers.
 *
 * Reserved MPU regions are configured directly through
 * maxrtos_arch_mpu_configure_region() and are intended for
 * system-wide memory such as program code, peripherals, and kernel
 * memory.
 *
 * The MPU is configured with deny-by-default semantics by clearing
 * MPU_CTRL.PRIVDEFENA. Consequently, memory required by the system
 * must be covered by an enabled MPU region.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "maxrtos/arch/cortex_m7/mpu_hw.h"

#define MAXRTOS_MPU_TYPE ( *( volatile uint32_t * ) 0xE000ED90UL )
#define MAXRTOS_MPU_CTRL ( *( volatile uint32_t * ) 0xE000ED94UL )
#define MAXRTOS_MPU_RNR  ( *( volatile uint32_t * ) 0xE000ED98UL )
#define MAXRTOS_MPU_RBAR ( *( volatile uint32_t * ) 0xE000ED9CUL )
#define MAXRTOS_MPU_RASR ( *( volatile uint32_t * ) 0xE000EDA0UL )

#define MAXRTOS_MPU_CTRL_ENABLE_BIT     ( 1UL << 0U )
#define MAXRTOS_MPU_CTRL_PRIVDEFENA_BIT ( 1UL << 2U )

/* RASR bit-field positions. */
#define MAXRTOS_RASR_ENABLE_POS ( 0U )
#define MAXRTOS_RASR_SIZE_POS   ( 1U )   /* 5 bits: bits[5:1]   */
#define MAXRTOS_RASR_SRD_POS    ( 8U )   /* 8 bits: bits[15:8]  */
#define MAXRTOS_RASR_B_POS      ( 16U )
#define MAXRTOS_RASR_C_POS      ( 17U )
#define MAXRTOS_RASR_S_POS      ( 18U )
#define MAXRTOS_RASR_TEX_POS    ( 19U )  /* 3 bits: bits[21:19] */
#define MAXRTOS_RASR_AP_POS     ( 24U )  /* 3 bits: bits[26:24] */
#define MAXRTOS_RASR_XN_POS     ( 28U )

/* RASR access-permission encodings. */
#define MAXRTOS_RASR_AP_NO_ACCESS   ( 0x0UL )
#define MAXRTOS_RASR_AP_READ_ONLY   ( 0x6UL )
#define MAXRTOS_RASR_AP_READ_WRITE  ( 0x3UL )

/* Default memory attributes for configured partition regions:
 * Normal memory, inner/outer write-back cacheable, and non-shareable. */
#define MAXRTOS_RASR_TEX_DEFAULT ( 0x0UL )
#define MAXRTOS_RASR_C_DEFAULT   ( 1UL )
#define MAXRTOS_RASR_B_DEFAULT   ( 1UL )
#define MAXRTOS_RASR_S_DEFAULT   ( 0UL )

static maxrtos_mpu_config_t const * s_config = NULL;

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

void maxrtos_arch_mpu_set_config( maxrtos_mpu_config_t const * config )
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

void maxrtos_arch_mpu_configure_for_partition(
    maxrtos_partition_id_t partition_id )
{
    maxrtos_mpu_region_config_t region;
    uint32_t size_field;
    uint32_t ap_field;
    uint32_t xn_field;
    uint32_t rasr;

    /* Configuration was validated during boot by
     * maxrtos_mpu_set_partition_region(). */
    ( void ) maxrtos_mpu_get_partition_region(
        s_config,
        partition_id,
        &region );

    size_field = maxrtos_mpu_encode_size_field( region.size_bytes );

    switch( region.access )
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

    xn_field = region.executable ? 0UL : 1UL;

    MAXRTOS_MPU_RNR = ( uint32_t ) partition_id;

    /* The base address was validated to be naturally aligned to
     * the region size before reaching this function. */
    MAXRTOS_MPU_RBAR = region.base_address;

    rasr = ( 1UL << MAXRTOS_RASR_ENABLE_POS ) |
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

void maxrtos_arch_mpu_configure_for_next_pcb(
    maxrtos_process_control_block_t const * next_pcb )
{
    /* Called from the context-switch path with the next process's
     * PCB. The NULL check protects the C/assembly interface. */
    if ( next_pcb != NULL )
    {
        maxrtos_arch_mpu_configure_for_partition( next_pcb->partition_id );
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

    if( ( region_number >= MAXRTOS_MAX_PARTITIONS ) &&
        ( region_number < 16U ) &&
        ( size_bytes >= MAXRTOS_MPU_REGION_MIN_SIZE ) &&
        ( maxrtos_mpu_hw_is_power_of_two( size_bytes ) == true ) &&
        ( ( base_address % size_bytes ) == 0U ) &&
        ( ( access == MAXRTOS_MPU_ACCESS_NONE ) ||
          ( access == MAXRTOS_MPU_ACCESS_READ_ONLY ) ||
          ( access == MAXRTOS_MPU_ACCESS_READ_WRITE ) ) )
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

        xn_field = executable ? 0UL : 1UL;

        MAXRTOS_MPU_RNR = region_number;
        MAXRTOS_MPU_RBAR = base_address;

        rasr = ( 1UL << MAXRTOS_RASR_ENABLE_POS ) |
               ( size_field << MAXRTOS_RASR_SIZE_POS ) |
               ( ap_field << MAXRTOS_RASR_AP_POS ) |
               ( MAXRTOS_RASR_TEX_DEFAULT << MAXRTOS_RASR_TEX_POS ) |
               ( MAXRTOS_RASR_C_DEFAULT << MAXRTOS_RASR_C_POS ) |
               ( MAXRTOS_RASR_B_DEFAULT << MAXRTOS_RASR_B_POS ) |
               ( MAXRTOS_RASR_S_DEFAULT << MAXRTOS_RASR_S_POS ) |
               ( xn_field << MAXRTOS_RASR_XN_POS );

        MAXRTOS_MPU_RASR = rasr;

        __asm volatile ( "dsb" );
        __asm volatile ( "isb" );

        status = MAXRTOS_OK;
    }

    return status;
}
