/**
 * @file mpu.h
 * @brief Partition MPU region configuration and validation interface.
 *
 * Defines the data types and interface used to configure and
 * validate one MPU data region for each software partition.
 *
 * Each partition is assigned one MPU region covering its RAM and
 * process stacks. The configured region defines the memory access
 * permissions and execute permission applicable to that partition.
 *
 * The current implementation provides spatial isolation of
 * partition data only. Partition code resides in shared executable
 * flash and is not isolated between partitions by the regions
 * represented by this module.
 */

#ifndef MAXRTOS_ARCH_CORTEX_M7_MPU_H
#define MAXRTOS_ARCH_CORTEX_M7_MPU_H

#include <stdint.h>
#include <stdbool.h>

#include "maxrtos/kernel/process.h"
#include "maxrtos/status.h"
#include "maxrtos/config.h"

/**
 * @brief Minimum supported MPU region size in bytes.
 *
 * The ARMv7-M MPU encodes region sizes as powers of two. The
 * architecture-defined minimum region size is 32 bytes.
 */
#define MAXRTOS_MPU_REGION_MIN_SIZE ( 32UL )

/**
 * @brief Access permission assigned to a partition MPU region.
 *
 * The access model currently applies uniformly to the partition
 * region. Privileged and unprivileged access levels are not
 * distinguished by this interface.
 */
typedef enum
{
    MAXRTOS_MPU_ACCESS_NONE = 0U,
    MAXRTOS_MPU_ACCESS_READ_ONLY,
    MAXRTOS_MPU_ACCESS_READ_WRITE
} maxrtos_mpu_access_t;

/**
 * @brief Validated MPU region configuration for one partition.
 *
 * The region describes the memory range assigned to a partition.
 * The base address shall be naturally aligned to size_bytes.
 * size_bytes shall be a power of two and shall be greater than or
 * equal to MAXRTOS_MPU_REGION_MIN_SIZE.
 */
typedef struct
{
    uint32_t base_address;
    uint32_t size_bytes;
    maxrtos_mpu_access_t access;
    bool executable;
} maxrtos_mpu_region_config_t;

/**
 * @brief MPU configuration for partition data regions.
 *
 * One configuration entry is provided for each supported partition.
 * An entry is valid only when the corresponding configured element
 * is true.
 */
typedef struct
{
    maxrtos_mpu_region_config_t regions[ MAXRTOS_MAX_PARTITIONS ];
    bool configured[ MAXRTOS_MAX_PARTITIONS ];
} maxrtos_mpu_config_t;

/**
 * @brief Initialize an MPU configuration.
 *
 * @param[out] config
 *     Pointer to the configuration object to initialize.
 *
 * @return
 *     MAXRTOS_OK if the configuration was initialized.
 *     MAXRTOS_ERR_INVALID_ARG if config is NULL.
 */
maxrtos_status_t maxrtos_mpu_config_init(
    maxrtos_mpu_config_t * config );

/**
 * @brief Configure the MPU data region assigned to a partition.
 *
 * @param[in,out] config
 *     Configuration object to modify.
 *
 * @param[in] partition_id
 *     Identifier of the partition being configured.
 *
 * @param[in] base_address
 *     Base address of the partition region.
 *
 * @param[in] size_bytes
 *     Size of the partition region in bytes.
 *
 * @param[in] access
 *     Access permission assigned to the region.
 *
 * @param[in] executable
 *     Execution permission assigned to the region.
 *
 * @return 
 *     MAXRTOS_OK if the region configuration was accepted.
 *     MAXRTOS_ERR_INVALID_ARG if:
 *         - config is NULL
 *         - partition_id is outside the supported partition range
 *         - size_bytes is less than MAXRTOS_MPU_REGION_MIN_SIZE
 *         - size_bytes is not a power of two
 *         - base_address is not aligned to size_bytes
 */
maxrtos_status_t maxrtos_mpu_set_partition_region(
    maxrtos_mpu_config_t * config,
    maxrtos_partition_id_t partition_id,
    uint32_t base_address,
    uint32_t size_bytes,
    maxrtos_mpu_access_t access,
    bool executable );

/**
 * @brief Retrieve the configured MPU data region for a partition.
 *
 * @param[in] config 
 *     Configuration object to query.
 *
 * @param[in] partition_id
 *     Identifier of the partition being queried.
 *
 * @param[out] out_region
 *     Pointer receiving the configured region.
 *
 * @return 
 *     MAXRTOS_OK if the partition has a configured region.
 *     MAXRTOS_ERR_INVALID_ARG if config or out_region is NULL,
 *     or if partition_id is outside the supported partition range.
 *     MAXRTOS_ERR_NOT_FOUND if the partition has not been
 *     configured since the last initialization.
 *
 * @post If the return value is not MAXRTOS_OK, out_region is not
 *       modified.
 */
maxrtos_status_t maxrtos_mpu_get_partition_region(
    maxrtos_mpu_config_t const * config,
    maxrtos_partition_id_t partition_id,
    maxrtos_mpu_region_config_t * out_region );

#endif /* MAXRTOS_ARCH_CORTEX_M7_MPU_H */
