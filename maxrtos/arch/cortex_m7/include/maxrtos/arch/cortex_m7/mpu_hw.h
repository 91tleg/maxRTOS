/**
 * @file mpu_hw.h
 * @brief Cortex-M7 MPU hardware configuration interface.
 *
 * Provides the architecture-specific interface for programming the
 * ARMv7-M MPU. Configuration and validation are provided by mpu.c.
 *
 * The partition region is reused for the currently scheduled
 * partition. Lower regions are reserved for system mappings and
 * remaining regions are statically assigned to communication ports.
 *
 * MPU configuration is updated by PendSV before restoring the
 * incoming process context.
 */

#ifndef MAXRTOS_ARCH_CORTEX_M7_MPU_HW_H
#define MAXRTOS_ARCH_CORTEX_M7_MPU_HW_H

#include <stdint.h>
#include <stdbool.h>

#include "maxrtos/arch/cortex_m7/mpu.h"
#include "maxrtos/kernel/process.h"

/**
 * @brief Register the MPU configuration.
 *
 * @param[in] config
 *     MPU configuration. Must not be NULL and shall remain valid for
 *     the lifetime of the MPU configuration.
 */
void maxrtos_arch_mpu_set_config(
    maxrtos_mpu_config_t const * config );

/**
 * @brief Initialize the Cortex-M7 MPU.
 *
 * Enables the MPU. Required reserved regions shall be configured before
 * enabling the MPU.
 *
 * @param[in] privileged_default_map
 *     Value of MPU_CTRL.PRIVDEFENA.
 *     false: deny by default. Every access, privileged or not, must be
 *     permitted by an enabled region, so the kernel's own code, data and
 *     stacks need explicit privileged regions (the generator checks the
 *     ones it knows about). This is the isolation model the design
 *     intends.
 *     true: privileged code may also use the default memory map wherever
 *     no region applies, so anything privileged (including a system
 *     partition) can reach all memory the MPU does not cover, and a
 *     missing region goes unnoticed. Use only as a stopgap while a
 *     missing region is being found.
 *
 * @pre Required reserved MPU regions are configured.
 * @pre A valid MPU configuration has been registered.
 */
void maxrtos_arch_mpu_init( bool privileged_default_map );

/**
 * @brief Configure the MPU for the incoming process.
 *
 * Configures the reusable partition region and updates static port
 * region access for the process.
 *
 * @param[in] next_pcb
 *     Process control block about to become current. If NULL, no
 *     configuration is performed.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if next_pcb is NULL.
 */
maxrtos_status_t maxrtos_arch_mpu_configure_for_next_pcb(
    maxrtos_process_control_block_t const * next_pcb );

/**
 * @brief Configure a reserved system MPU region.
 *
 * Intended for system initialization, not the context-switch path.
 * The region must be below MAXRTOS_MPU_PARTITION_REGION.
 *
 * @param[in] region_number
 *     MPU region number. Must be less than
 *     MAXRTOS_MPU_PARTITION_REGION.
 *
 * @param[in] base_address
 *     Region base address, aligned to size_bytes.
 *
 * @param[in] size_bytes
 *     Region size. Must be a power of two and at least
 *     MAXRTOS_MPU_REGION_MIN_SIZE.
 *
 * @param[in] access
 *     Access permissions for the region.
 *
 * @param[in] executable
 *     true if the region may contain executable memory.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if any argument is invalid.
 */
maxrtos_status_t maxrtos_arch_mpu_configure_region(
    uint32_t region_number,
    uint32_t base_address,
    uint32_t size_bytes,
    maxrtos_mpu_access_t access,
    bool executable );

/**
 * @brief Check that a byte range lies entirely inside a partition's
 *        own read-write memory domain.
 *
 * Used to validate buffers that a process hands to the kernel: privileged
 * kernel code must never read or write memory the caller could not
 * access itself.
 *
 * @return
 *     true if [address, address + size) is non-empty, does not wrap, and
 *     lies inside the partition's configured region.
 */
bool maxrtos_arch_mpu_partition_owns_range(
    maxrtos_partition_id_t partition_id,
    uint32_t address,
    uint32_t size );

/**
 * @brief Check that address is the base of a static port region that the
 *        partition is a member of.
 *
 * Used to validate port objects passed to kernel IPC services.
 */
bool maxrtos_arch_mpu_is_port_member(
    maxrtos_partition_id_t partition_id,
    uint32_t address );

#endif /* MAXRTOS_ARCH_CORTEX_M7_MPU_HW_H */
