/**
 * @file mpu.c
 * @brief Cortex-M7 MPU region configuration and validation.
 *
 * Provides configuration and validation for partition memory regions
 * and static inter-partition port regions.
 */

#include <stddef.h>
#include <stdbool.h>

#include "maxrtos/arch/cortex_m7/mpu.h"

static bool maxrtos_mpu_is_power_of_two( uint32_t value )
{
    return ( value > 0U ) &&
           ( ( value & ( value - 1U ) ) == 0U );
}

static bool maxrtos_mpu_region_geometry_valid(
    uint32_t base_address,
    uint32_t size_bytes )
{
    return ( size_bytes >= MAXRTOS_MPU_REGION_MIN_SIZE ) &&
           ( maxrtos_mpu_is_power_of_two( size_bytes ) == true ) &&
           ( ( base_address % size_bytes ) == 0U );
}

maxrtos_status_t maxrtos_mpu_config_init(
    maxrtos_mpu_config_t * config )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( config != NULL )
    {
        size_t partition_id;

        for( partition_id = 0U;
             partition_id < MAXRTOS_MAX_PARTITIONS;
             partition_id++ )
        {
            config->configured[ partition_id ] = false;
        }

        config->port_region_count = 0U;

        status = MAXRTOS_OK;
    }

    return status;
}

maxrtos_status_t maxrtos_mpu_set_partition_region(
    maxrtos_mpu_config_t * config,
    maxrtos_partition_id_t partition_id,
    uint32_t base_address,
    uint32_t size_bytes,
    maxrtos_mpu_access_t access,
    bool executable )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( config != NULL ) &&
        ( partition_id < MAXRTOS_MAX_PARTITIONS ) &&
        ( maxrtos_mpu_region_geometry_valid( base_address, size_bytes ) == true ) &&
        ( ( access == MAXRTOS_MPU_ACCESS_NONE ) ||
          ( access == MAXRTOS_MPU_ACCESS_READ_ONLY ) ||
          ( access == MAXRTOS_MPU_ACCESS_READ_WRITE ) ||
          ( access == MAXRTOS_MPU_ACCESS_PRIV_READ_ONLY ) ||
          ( access == MAXRTOS_MPU_ACCESS_PRIV_READ_WRITE ) ) )
    {
        config->regions[ partition_id ].base_address = base_address;
        config->regions[ partition_id ].size_bytes = size_bytes;
        config->regions[ partition_id ].access = access;
        config->regions[ partition_id ].executable = executable;
        config->configured[ partition_id ] = true;

        status = MAXRTOS_OK;
    }

    return status;
}

maxrtos_status_t maxrtos_mpu_get_partition_region(
    maxrtos_mpu_config_t const * config,
    maxrtos_partition_id_t partition_id,
    maxrtos_mpu_region_config_t * out_region )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( config != NULL ) &&
        ( out_region != NULL ) &&
        ( partition_id < MAXRTOS_MAX_PARTITIONS ) )
    {
        if( config->configured[ partition_id ] == false )
        {
            status = MAXRTOS_ERR_NOT_FOUND;
        }
        else
        {
            *out_region = config->regions[ partition_id ];
            status = MAXRTOS_OK;
        }
    }

    return status;
}

maxrtos_status_t maxrtos_mpu_add_port_region(
    maxrtos_mpu_config_t * config,
    uint32_t base_address,
    uint32_t size_bytes,
    uint32_t member_partition_mask )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( config != NULL ) &&
        ( maxrtos_mpu_region_geometry_valid( base_address, size_bytes ) == true ) &&
        ( member_partition_mask != 0U ) &&
        ( ( member_partition_mask &
            ~( ( MAXRTOS_MAX_PARTITIONS >= 32U ) ?
                 0xFFFFFFFFUL :
                 ( ( 1UL << MAXRTOS_MAX_PARTITIONS ) - 1UL ) ) ) == 0U ) )
    {
        if( config->port_region_count >= MAXRTOS_MAX_PORT_REGIONS )
        {
            status = MAXRTOS_ERR_POOL_FULL;
        }
        else
        {
            size_t index;

            index = config->port_region_count;

            config->port_regions[ index ].base_address = base_address;
            config->port_regions[ index ].size_bytes = size_bytes;
            config->port_regions[ index ].member_partition_mask =
                member_partition_mask;

            config->port_region_count = index + 1U;

            status = MAXRTOS_OK;
        }
    }

    return status;
}

size_t maxrtos_mpu_get_port_region_count(
    maxrtos_mpu_config_t const * config )
{
    size_t count;

    count = 0U;

    if( config != NULL )
    {
        count = config->port_region_count;
    }

    return count;
}

maxrtos_status_t maxrtos_mpu_get_port_region(
    maxrtos_mpu_config_t const * config,
    size_t index,
    maxrtos_mpu_port_region_config_t * out_region )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( config != NULL ) &&
        ( out_region != NULL ) &&
        ( index < config->port_region_count ) )
    {
        *out_region = config->port_regions[ index ];
        status = MAXRTOS_OK;
    }

    return status;
}
