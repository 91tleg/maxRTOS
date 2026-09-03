/**
 * @file mpu_hw.h
 * @brief Cortex-M7 MPU hardware configuration interface.
 *
 * Provides the architecture-specific interface for programming the
 * ARMv7-M MPU. The corresponding mpu.c module contains the
 * platform-independent configuration and validation logic and does
 * not access MPU hardware registers.
 *
 * Partition-owned MPU regions use region numbers corresponding to
 * partition IDs. Reserved MPU regions are available for system-wide
 * memory such as code, peripherals, and kernel memory.
 *
 * MPU configuration is applied during the PendSV context-switch path
 * before the incoming process context is restored.
 */

#ifndef MAXRTOS_ARCH_CORTEX_M7_MPU_HW_H
#define MAXRTOS_ARCH_CORTEX_M7_MPU_HW_H

#include <stdint.h>
#include <stdbool.h>

#include "maxrtos/arch/cortex_m7/mpu.h"
#include "maxrtos/kernel/process.h"

/**
 * @brief Register the MPU configuration used by this module.
 *
 * The configuration shall remain valid for the lifetime of the MPU
 * hardware configuration.
 *
 * @param[in] config
 *     Configuration containing the partition MPU regions.
 *     Must not be NULL.
 */
void maxrtos_arch_mpu_set_config(
    maxrtos_mpu_config_t const * config );

/**
 * @brief Enable the Cortex-M7 MPU with deny-by-default semantics.
 *
 * Enables MPU protection and leaves PRIVDEFENA cleared. Reserved
 * system regions shall be configured before enabling the MPU.
 *
 * @pre Required reserved MPU regions have been configured.
 * @pre A valid MPU configuration has been registered.
 */
void maxrtos_arch_mpu_init( void );

/**
 * @brief Configure the MPU for the process about to become current.
 *
 * Extracts the partition ID from the supplied process control block
 * and configures the corresponding MPU region.
 *
 * This function is called by PendSV_Handler after the incoming PCB
 * has been selected and before its processor context is restored.
 *
 * @param[in] next_pcb
 *     Process control block about to become current.
 *     If NULL, no MPU configuration is performed.
 * 
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if next_pcb is NULL.
 */
maxrtos_status_t maxrtos_arch_mpu_configure_for_next_pcb(
    maxrtos_process_control_block_t const * next_pcb );

/**
 * @brief Configure a reserved MPU region.
 *
 * Programs an MPU region outside the partition-owned region range.
 * This function is intended for one-time system initialization rather
 * than the context-switch path and therefore performs full argument
 * validation.
 *
 * Reserved regions may be used for system-wide memory such as program
 * code, vector tables, peripherals, and kernel memory.
 *
 * @param[in] region_number
 *     MPU region number. Must satisfy
 *     MAXRTOS_MAX_PARTITIONS <= region_number < 16.
 *
 * @param[in] base_address
 *     Region base address. Must be aligned to size_bytes.
 *
 * @param[in] size_bytes
 *     Region size. Must be a power of two and at least MAXRTOS_MPU_REGION_MIN_SIZE.
 *
 * @param[in] access
 *     Access permissions for the region.
 *
 * @param[in] executable
 *     true if the region may contain executable memory; false otherwise.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if any argument violates the
 *     required region constraints.
 */
maxrtos_status_t maxrtos_arch_mpu_configure_region(
    uint32_t region_number,
    uint32_t base_address,
    uint32_t size_bytes,
    maxrtos_mpu_access_t access,
    bool executable );

#endif /* MAXRTOS_ARCH_CORTEX_M7_MPU_HW_H */
