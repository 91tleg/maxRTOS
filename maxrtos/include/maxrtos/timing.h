/**
 * @file timing.h
 * @brief Public application-facing periodic process interface.
 *
 * Configure a process's period and time capacity with
 * maxrtos_process_set_timing() (maxrtos/kernel/timing.h) after creating it;
 * the generated header supplies PROCESS_PERIOD_TICKS_<name> and
 * PROCESS_TIME_CAPACITY_TICKS_<name>. Then call maxrtos_periodic_wait() at the
 * end of every release. maxrtos_timed_wait() waits a number of ticks and
 * maxrtos_get_time() reads the tick counter.
 */

#ifndef MAXRTOS_TIMING_H
#define MAXRTOS_TIMING_H

#include "maxrtos/status.h"
#include "maxrtos/types.h"

/**
 * @brief Complete the current release and wait for the next.
 *
 * Meets the current release's deadline and blocks the calling process until
 * its next release. If that release is already due (an overrun) the call
 * returns at once. While the process waits, the CPU runs the partition's other
 * processes, or idles.
 *
 * @return
 *     MAXRTOS_OK once the next release has begun.
 *     MAXRTOS_ERR_INVALID_STATE if the caller is not periodic.
 */
maxrtos_status_t maxrtos_periodic_wait( void );

/**
 * @brief Wait for a number of ticks (ARINC 653 TIMED_WAIT).
 *
 * Blocks the calling process until the kernel tick counter has advanced by
 * delay_ticks, counted the way an IPC timeout is: a wait of N ticks begun when
 * maxrtos_get_time() reads T ends when it reads T + N. The call falls
 * somewhere inside a tick, so the real time waited is between N - 1 and N
 * ticks. While the process waits the CPU runs the partition's other
 * processes, or idles. The process runs again once it is READY and its
 * partition's slot is on.
 *
 * A delay of 0 returns at once and does not give up the CPU; call
 * maxrtos_yield() for that.
 *
 * @param[in] delay_ticks
 *     Ticks to wait. MAXRTOS_TIMEOUT_INFINITE is rejected.
 *
 * @return
 *     MAXRTOS_OK once the delay has passed (or at once for a delay of 0).
 *     MAXRTOS_ERR_INVALID_ARG if delay_ticks is MAXRTOS_TIMEOUT_INFINITE.
 *     MAXRTOS_ERR_OVERFLOW if the wake-up tick does not fit the counter.
 */
maxrtos_status_t maxrtos_timed_wait( maxrtos_tick_t delay_ticks );

/**
 * @brief Read the kernel tick counter (ARINC 653 GET_TIME).
 *
 * The counter starts at 0 when the scheduler starts and advances by one on
 * every tick.
 *
 * @return The current tick count.
 */
maxrtos_tick_t maxrtos_get_time( void );

#endif /* MAXRTOS_TIMING_H */
