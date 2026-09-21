/**
 * @file sim_target.h
 * @brief Virtual Cortex-M7 target for software-in-the-loop (SIL) tests.
 *
 * The SIL build runs the production kernel and the hardware-independent
 * part of the architecture layer (SVC dispatch, MPU driver, SysTick tick
 * handling, scheduler start-up, fault recovery enactment) unmodified on the
 * host. This module replaces only what is genuinely hardware:
 *
 *  - CPU and exceptions: every process runs as a host thread; exactly one
 *    "CPU" runs at a time and a context switch is a hand-over of that
 *    baton. SysTick, SVC and fault exceptions are taken on the running
 *    context and end with the pending PendSV, as on the target.
 *  - Time: virtual. Simulated code advances it with sim_cpu_work(); a
 *    SysTick exception fires every SIM_CYCLES_PER_TICK cycles. Runs are
 *    fully deterministic.
 *  - Memory: a 32-bit virtual address space backed by host memory. The
 *    MPU programmed by the production driver (through the port hooks) is
 *    modelled at register level and gates every simulated access.
 *  - Faults: an MPU violation, or an injected fault, raises the exception
 *    into the production fault recovery path.
 *
 * Process code must reach the simulated hardware only through the
 * services here (sim_cpu_work, sim_write32, ..., and the public RTOS API).
 * Code that neither yields nor calls sim_cpu_work() never lets time pass.
 */

#ifndef MAXRTOS_SIL_SIM_TARGET_H
#define MAXRTOS_SIL_SIM_TARGET_H

#include <stdbool.h>
#include <stdint.h>

#include "sim_mpu_regs.h"

#include "maxrtos/kernel/health_monitor.h"
#include "maxrtos/kernel/process.h"
#include "maxrtos/status.h"

#define SIM_CYCLES_PER_TICK ( 1000U )
#define SIM_SVC_CYCLES      ( 20U )
#define SIM_MAX_TICKS       ( 200000U )

/** Value reported by sim_partition_at_tick() while the idle context ran. */
#define SIM_IDLE ( -1 )

/* --- Lifecycle --------------------------------------------------------- */

/** Tear down any previous run and return kernel/arch/sim state to reset. */
void sim_reset( void );

/** Release all simulator resources. Safe to call repeatedly. */
void sim_shutdown( void );

/**
 * Start the scheduler through the production maxrtos_scheduler_start()
 * and run for the given number of SysTick ticks.
 *
 * @return false if the system halted (or start-up failed) before the ticks
 *         elapsed; see sim_halt_reason().
 */
bool sim_start( uint32_t ticks );

/** Continue a started system for more ticks. */
bool sim_run( uint32_t ticks );

/** Whether the CPU halted (unrecoverable error / halt loop). */
bool sim_halted( void );

/** Description of why the CPU halted, or NULL. */
char const * sim_halt_reason( void );

/** Number of simulator model violations (a driver programmed the model
 *  illegally); a correct driver never causes any. */
uint32_t sim_model_violations( void );
void sim_report_model_violation( char const * message );

/* --- Virtual memory ---------------------------------------------------- */

/** Map [virtual_base, virtual_base + size) to fresh zeroed host memory. */
void * sim_mem_map( char const * name, uint32_t virtual_base, uint32_t size );

/** Host pointer for a virtual range, or NULL if not fully mapped. */
void * sim_mem_ptr( uint32_t address, uint32_t length );

/** Virtual address of a host pointer into mapped memory (0 if unmapped). */
uint32_t sim_ptr_to_uaddr( void const * pointer );

/* --- Services for simulated process code -------------------------------- */

/** Consume CPU cycles; may take SysTick and switch context. */
void sim_cpu_work( uint32_t cycles );

/** A process entry that burns CPU forever without ever yielding. */
void sim_entry_spin( void * arg );

/**
 * Store/load a word as the running context would. Unprivileged contexts
 * are checked against the MPU. A denied access raises a MemManage fault
 * and returns false (only if the fault action lets the context continue).
 */
bool sim_write32( uint32_t address, uint32_t value );
bool sim_read32( uint32_t address, uint32_t * out_value );

/** Raise a fault of the given class on the running context. */
void sim_fault( maxrtos_fault_type_t type );

/** Raw register-level system calls: what a process could pass in r0-r3. */
maxrtos_status_t sim_queue_send_raw(
    uint32_t port, uint32_t message, uint32_t size, uint32_t timeout );
maxrtos_status_t sim_queue_receive_raw(
    uint32_t port, uint32_t buffer, uint32_t size, uint32_t timeout );
uint32_t sim_queue_count_raw( uint32_t port );

/* --- Environment control and observation ---------------------------------- */

/** Raise `type` on whichever context is running when tick `tick` is taken. */
void sim_inject_fault_at_tick( uint32_t tick, maxrtos_fault_type_t type );

/** SysTick exceptions taken so far. */
uint32_t sim_tick( void );

/**
 * Partition of the context that ran for the period after tick `tick`
 * (tick 0 is scheduler start), or SIM_IDLE if idle ran.
 */
int32_t sim_partition_at_tick( uint32_t tick );

/** Process running after tick `tick`, or MAXRTOS_INVALID_PROCESS_ID for idle. */
maxrtos_process_id_t sim_process_at_tick( uint32_t tick );

/** Number of context switches performed. */
uint32_t sim_context_switches( void );

/** The process currently running, NULL if none has started. */
maxrtos_process_control_block_t * sim_current_process( void );

/* --- MPU model (sim_mpu.c) ----------------------------------------------- */

void sim_mpu_reset( void );
bool sim_mpu_enabled( void );
uint32_t sim_mpu_write_count( void );

bool sim_mpu_access_ok(
    uint32_t address, uint32_t length, bool write, bool privileged );

bool sim_mpu_region_info(
    uint32_t region, uint32_t * base, uint32_t * size, uint32_t * access );

#endif /* MAXRTOS_SIL_SIM_TARGET_H */
