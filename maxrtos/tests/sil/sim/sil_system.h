/**
 * @file sil_system.h
 * @brief Builds a complete partitioned RTOS system on the virtual target.
 *
 * Performs the same steps, in the same order and through the same production
 * APIs, as the generated maxrtos_config_init() plus the application's process
 * creation: partition table, health monitor, per-partition MPU regions,
 * static MPU regions, IPC ports, frame schedule, SysTick wiring. Only the
 * memory layout is decided here instead of by a linker script.
 */

#ifndef MAXRTOS_SIL_SYSTEM_H
#define MAXRTOS_SIL_SYSTEM_H

#include <stdint.h>

#include "maxrtos/arch/cortex_m7/mpu.h"
#include "maxrtos/kernel/frame.h"
#include "maxrtos/kernel/health_monitor.h"
#include "maxrtos/kernel/partition.h"
#include "maxrtos/kernel/queue_port.h"

#include "sim_target.h"

#define SIL_MAX_PARTITIONS ( 4U )
#define SIL_MAX_PROCESSES_PER_PARTITION ( 4U )
#define SIL_MAX_PORTS ( 4U )

/* Virtual memory map (same addresses as the Nucleo-H753ZI demo). */
#define SIL_DTCM_BASE ( 0x20000000UL )
#define SIL_DTCM_SIZE ( 0x00020000UL )
#define SIL_SRAM_BASE ( 0x24000000UL )
#define SIL_SRAM_SIZE ( 0x00100000UL )
#define SIL_KERNEL_GUARD ( SIL_DTCM_BASE + 0x100UL ) /* a word of kernel data */

#define SIL_STACK_BYTES ( 32U * 1024U )

typedef struct
{
    void ( * entry )( void * );
    void * arg;
    uint8_t priority;
} sil_process_spec_t;

typedef struct
{
    sil_process_spec_t process[ SIL_MAX_PROCESSES_PER_PARTITION ];
    uint32_t process_count;

    /* Health monitor action per fault class; MAXRTOS_HM_ACTION_COUNT means
     * "use the generator default" (restart_process). */
    maxrtos_hm_action_t action[ MAXRTOS_FAULT_COUNT ];
} sil_partition_spec_t;

typedef struct
{
    uint32_t message_size;
    uint32_t capacity;
    uint32_t member_mask; /* bit n set: partition n is a member */
} sil_port_spec_t;

typedef struct
{
    sil_partition_spec_t partition[ SIL_MAX_PARTITIONS ];
    uint32_t partition_count;

    maxrtos_frame_slot_t slot[ 8 ];
    uint32_t slot_count;

    sil_port_spec_t port[ SIL_MAX_PORTS ];
    uint32_t port_count;
} sil_system_spec_t;

typedef struct
{
    maxrtos_partition_table_t table;
    maxrtos_frame_schedule_t frame;
    maxrtos_mpu_config_t mpu;
    maxrtos_health_monitor_t hm;
    maxrtos_frame_slot_t slots[ 8 ];

    uint32_t domain_base[ SIL_MAX_PARTITIONS ];
    uint32_t domain_size[ SIL_MAX_PARTITIONS ];
    maxrtos_process_id_t process_id[ SIL_MAX_PARTITIONS ][ SIL_MAX_PROCESSES_PER_PARTITION ];

    uint32_t port_address[ SIL_MAX_PORTS ];
    maxrtos_queue_port_t * port[ SIL_MAX_PORTS ];
} sil_system_t;

/** Fill a spec with the default health-monitor policy for every partition. */
void sil_spec_defaults( sil_system_spec_t * spec );

/** The system built by the last sil_build() (valid until the next). */
sil_system_t * sil_system( void );

/**
 * Reset the virtual target and build the system described by spec.
 * Aborts the current test case (SIL_REQUIRE) if any production API rejects
 * the configuration.
 */
void sil_build( sil_system_spec_t const * spec );

/** Two-partition system used by most scenarios: 5 and 3 tick slots. */
void sil_spec_two_partitions(
    sil_system_spec_t * spec,
    void ( * p0_entry )( void * ),
    void ( * p1_entry )( void * ) );

#endif /* MAXRTOS_SIL_SYSTEM_H */
