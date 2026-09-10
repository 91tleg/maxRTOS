/**
 * @file deadline_monitor.h
 * @brief Per-process period and time-capacity deadline monitoring.
 *
 * Each monitored process declares two attributes: PERIOD (how often its
 * work repeats, in ticks) and TIME_CAPACITY (how many ticks of
 * actual CPU time it is allowed to consume within each period). If a
 * process consumes more than its declared TIME_CAPACITY before its
 * PERIOD elapses, it is a deadline violation.
 */

#ifndef MAXRTOS_KERNEL_DEADLINE_MONITOR_H
#define MAXRTOS_KERNEL_DEADLINE_MONITOR_H

#include <stdint.h>
#include <stdbool.h>

#include "maxrtos/kernel/process.h"
#include "maxrtos/status.h"
#include "maxrtos/config.h"

/**
 * @brief Per-process deadline monitoring state, for every process in
 * the system.
 *
 * @field period_ticks
 *     period_ticks[p] is process p's declared PERIOD, in ticks.
 *     Only meaningful if configured[p] is true.
 *
 * @field time_capacity_ticks
 *     time_capacity_ticks[p] is process p's declared TIME_CAPACITY
 *     (WCET budget per period), in ticks.
 *
 * @field accumulated_ticks
 *     Ticks actually consumed so far within process p's CURRENT period.
 *
 * @field next_period_tick
 *     Absolute tick value at which process p's current period ends and
 *     the next begins.
 *
 * @field configured
 *     Whether process p has had a budget configured at all.
 */
typedef struct
{
    uint32_t period_ticks[ MAXRTOS_MAX_PROCESSES ];
    uint32_t time_capacity_ticks[ MAXRTOS_MAX_PROCESSES ];
    uint32_t accumulated_ticks[ MAXRTOS_MAX_PROCESSES ];
    uint32_t next_period_tick[ MAXRTOS_MAX_PROCESSES ];
    bool configured[ MAXRTOS_MAX_PROCESSES ];
} maxrtos_deadline_monitor_t;

/**
 * @brief Initialize a deadline monitor, every process marked
 * unconfigured (unmonitored).
 *
 * @param[out] monitor
 *     Monitor to initialize. Must not be NULL.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if monitor is NULL.
 */
maxrtos_status_t maxrtos_deadline_monitor_init(
    maxrtos_deadline_monitor_t * monitor );

/**
 * @brief Declare a process's PERIOD and TIME_CAPACITY, beginning
 * monitoring for it.
 *
 * @param[in,out] monitor
 *     Monitor to configure. Must not be NULL.
 *
 * @param[in] process_id
 *     Which process to configure. Must refer to a valid, allocated
 *     process.
 *
 * @param[in] period_ticks
 *     Declared period in ticks.
 *
 * @param[in] time_capacity_ticks
 *     Declared budget per period, in ticks.
 *
 * @param[in] start_tick
 *     The current absolute tick value at the moment this is called.
 *
 * @return
 *     MAXRTOS_OK on success.
 *     MAXRTOS_ERR_INVALID_ARG if:
 *         - monitor is NULL
 *         - process_id is invalid
 *         - period_ticks is 0
 *         - time_capacity_ticks is 0 or exceeds period_ticks
 */
maxrtos_status_t maxrtos_deadline_monitor_set_budget(
    maxrtos_deadline_monitor_t * monitor,
    maxrtos_process_id_t process_id,
    uint32_t period_ticks,
    uint32_t time_capacity_ticks,
    uint32_t start_tick );

/**
 * @brief Record that a process was the one actually dispatched
 * (running) for one tick, advance its period if the boundary has
 * been reached, and report whether it has now exceeded its own
 * declared TIME_CAPACITY.
 *
 * @param[in,out] monitor
 *     Monitor to update. Must not be NULL.
 *
 * @param[in] process_id
 *     Which process ran this tick. If this process has no budget configured,
 *     this call is a no-op. *out_violated is set to false and MAXRTOS_OK
 *     is returned.
 *
 * @param[in] current_tick
 *     The current absolute tick value. Shall be called once per tick for
 *     whichever process was dispatched that tick.
 *
 * @param[out] out_violated
 *     True if process_id has now exceeded its declared TIME_CAPACITY within
 *     its current period; False otherwise. Must not be NULL.
 *
 * @return
 *     MAXRTOS_OK on success (even if a violation was detected).
 *     MAXRTOS_ERR_INVALID_ARG if monitor or out_violated is NULL,
 *     or if process_id is invalid.
 */
maxrtos_status_t maxrtos_deadline_monitor_record_running(
    maxrtos_deadline_monitor_t * monitor,
    maxrtos_process_id_t process_id,
    uint32_t current_tick,
    bool * out_violated );

#endif /* MAXRTOS_KERNEL_DEADLINE_MONITOR_H */
