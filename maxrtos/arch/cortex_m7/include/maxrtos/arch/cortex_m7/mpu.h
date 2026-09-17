/**
 * @file mpu.h
 * @brief Cortex-M7 MPU configuration and validation interface.
 *
 * Defines MPU region configuration for partition memory and static
 * inter-partition communication ports.
 *
 * One hardware region is reserved for the active partition and is
 * reconfigured on each context switch. Port regions are statically
 * allocated and enabled only for their declared member partitions.
 *
 * The Cortex-M7 MPU provides 16 regions. Regions 0-7 are reserved for
 * system mappings, region 8 is the reusable partition region, and
 * regions 9-15 are available for static port regions.
 */

#ifndef MAXRTOS_ARCH_CORTEX_M7_MPU_H
#define MAXRTOS_ARCH_CORTEX_M7_MPU_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "maxrtos/kernel/process.h"
#include "maxrtos/status.h"
#include "maxrtos/config.h"

/**
 * @brief Minimum supported MPU region size in bytes.
 *
 * MPU regions must be powers of two and are at least 32 bytes.
 */
#define MAXRTOS_MPU_REGION_MIN_SIZE ( 32UL )

/**
 * @brief Number of Cortex-M7 MPU hardware regions.
 */
#define MAXRTOS_MPU_HW_REGION_COUNT ( 16U )

/**
 * @brief Number of MPU regions reserved for system mappings.
 */
#define MAXRTOS_MPU_SYSTEM_REGION_COUNT ( 8U )

/**
 * @brief MPU region reserved for the active partition.
 *
 * This region is reconfigured on each context switch.
 */
#define MAXRTOS_MPU_PARTITION_REGION \
    ( MAXRTOS_MPU_SYSTEM_REGION_COUNT )

/**
 * @brief Number of MPU regions available for static port mappings.
 */
#define MAXRTOS_MAX_PORT_REGIONS \
    ( MAXRTOS_MPU_HW_REGION_COUNT - MAXRTOS_MPU_PARTITION_REGION - 1U )

/**
 * @brief MPU access permissions.
 */
typedef enum
{
    MAXRTOS_MPU_ACCESS_NONE = 0U,
    MAXRTOS_MPU_ACCESS_READ_ONLY,
    MAXRTOS_MPU_ACCESS_READ_WRITE,
    MAXRTOS_MPU_ACCESS_PRIV_READ_ONLY,
    MAXRTOS_MPU_ACCESS_PRIV_READ_WRITE,
} maxrtos_mpu_access_t;

/**
 * @brief MPU region configuration for a partition.
 *
 * @note
 * base_address shall be aligned to size_bytes. size_bytes shall be
 * a power of two and at least MAXRTOS_MPU_REGION_MIN_SIZE.
 */
typedef struct
{
    uint32_t base_address;
    uint32_t size_bytes;
    maxrtos_mpu_access_t access;
    bool executable;
} maxrtos_mpu_region_config_t;

/**
 * @brief Static MPU region configuration for a communication port.
 *
 * The region is read-write for member partitions and disabled for
 * all other partitions. The region is never executable.
 *
 * member_partition_mask uses bit N to represent partition N.
 */
typedef struct
{
    uint32_t base_address;
    uint32_t size_bytes;
    uint32_t member_partition_mask;
} maxrtos_mpu_port_region_config_t;

/**
 * @brief MPU configuration for partitions and static port regions.
 */
typedef struct
{
    maxrtos_mpu_region_config_t regions[ MAXRTOS_MAX_PARTITIONS ];
    bool configured[ MAXRTOS_MAX_PARTITIONS ];

    maxrtos_mpu_port_region_config_t port_regions[ MAXRTOS_MAX_PORT_REGIONS ];
    size_t port_region_count;
} maxrtos_mpu_config_t;

/**
 * @brief Initialize an MPU configuration.
 *
 * @param[out] config
 *     Configuration object to initialize.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if config is NULL.
 */
maxrtos_status_t maxrtos_mpu_config_init(
    maxrtos_mpu_config_t * config );

/**
 * @brief Configure the MPU region assigned to a partition.
 *
 * @param[in,out] config Configuration object to modify.
 * @param[in] partition_id Partition being configured.
 * @param[in] base_address Region base address.
 * @param[in] size_bytes Region size in bytes.
 * @param[in] access Region access permissions.
 * @param[in] executable Execution permission.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if an argument is invalid or the
 *     region does not satisfy the required alignment and size rules.
 */
maxrtos_status_t maxrtos_mpu_set_partition_region(
    maxrtos_mpu_config_t * config,
    maxrtos_partition_id_t partition_id,
    uint32_t base_address,
    uint32_t size_bytes,
    maxrtos_mpu_access_t access,
    bool executable );

/**
 * @brief Retrieve a partition's configured MPU region.
 *
 * @param[in] config Configuration object to query.
 * @param[in] partition_id Partition being queried.
 * @param[out] out_region Receives the configured region.
 *
 * @return
 *     MAXRTOS_OK if the region is configured.
 *     MAXRTOS_ERR_INVALID_ARG if an argument is invalid.
 *     MAXRTOS_ERR_NOT_FOUND if the partition has not been configured.
 *
 * @post
 *     out_region is not modified unless MAXRTOS_OK is returned.
 */
maxrtos_status_t maxrtos_mpu_get_partition_region(
    maxrtos_mpu_config_t const * config,
    maxrtos_partition_id_t partition_id,
    maxrtos_mpu_region_config_t * out_region );

/**
 * @brief Declare a static MPU region for a communication port.
 *
 * Port regions are allocated in declaration order after the reusable
 * partition region and are enabled only for declared member partitions.
 *
 * @param[in,out] config Configuration object to modify.
 * @param[in] base_address Port shared-memory base address.
 * @param[in] size_bytes Port shared-memory size in bytes.
 * @param[in] member_partition_mask Bitmask of partitions granted access.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if an argument is invalid.
 *     MAXRTOS_ERR_POOL_FULL if all static port regions are allocated.
 */
maxrtos_status_t maxrtos_mpu_add_port_region(
    maxrtos_mpu_config_t * config,
    uint32_t base_address,
    uint32_t size_bytes,
    uint32_t member_partition_mask );

/**
 * @brief Return the number of declared static port regions.
 *
 * @param[in] config Configuration object to query.
 *
 * @return Number of declared port regions, or zero if config is NULL.
 */
size_t maxrtos_mpu_get_port_region_count(
    maxrtos_mpu_config_t const * config );

/**
 * @brief Retrieve a static port region by declaration index.
 *
 * @param[in] config Configuration object to query.
 * @param[in] index Port region declaration index.
 * @param[out] out_region Receives the configured port region.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if an argument is invalid or index is
 *     out of range.
 */
maxrtos_status_t maxrtos_mpu_get_port_region(
    maxrtos_mpu_config_t const * config,
    size_t index,
    maxrtos_mpu_port_region_config_t * out_region );

#endif /* MAXRTOS_ARCH_CORTEX_M7_MPU_H */
