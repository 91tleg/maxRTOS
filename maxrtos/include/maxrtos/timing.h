/**
 * @file timing.h
 * @brief Public application-facing periodic process interface.
 *
 * Configure a process's period and time capacity with
 * maxrtos_process_set_timing() (maxrtos/kernel/timing.h) after creating it;
 * the generated header supplies PROCESS_PERIOD_TICKS_<name> and
 * PROCESS_TIME_CAPACITY_TICKS_<name>. Then call maxrtos_periodic_wait() at the
 * end of every release.
 */

#ifndef MAXRTOS_TIMING_H
#define MAXRTOS_TIMING_H

#include "maxrtos/status.h"

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

#endif /* MAXRTOS_TIMING_H */
