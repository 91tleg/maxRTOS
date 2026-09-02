/**
 * @file mpu.c
 * @brief Cortex-M7 MPU partition-region configuration.
 *
 * Provides representation, initialization, validation, and
 * access of partition MPU region configuration.
 *
 * Configuration is statically stored for each partition. A valid
 * partition region requires a power-of-two region size and a base
 * address aligned to that region size.
 */

#include <stddef.h>
#include <stdbool.h>

#include "maxrtos/arch/cortex_m7/mpu.h"

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
        status = MAXRTOS_OK;
    }

    return status;
}

static bool maxrtos_mpu_is_power_of_two( uint32_t value )
{
    return ( value > 0U ) &&
           ( ( value & ( value - 1U ) ) == 0U );
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
        ( size_bytes >= MAXRTOS_MPU_REGION_MIN_SIZE ) &&
        ( maxrtos_mpu_is_power_of_two( size_bytes ) == true ) &&
        ( ( base_address % size_bytes ) == 0U ) )
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
