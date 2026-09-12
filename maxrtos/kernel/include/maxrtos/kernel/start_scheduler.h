/**
 * @file start_scheduler.h
 * @brief Interface for starting the MAXRTOS scheduler.
 */

 #ifndef MAXRTOS_KERNEL_START_SCHEDULER_H
#define MAXRTOS_KERNEL_START_SCHEDULER_H

#include "maxrtos/kernel/frame.h"
#include "maxrtos/kernel/partition.h"
#include "maxrtos/kernel/process.h"
#include "maxrtos/status.h"

/**
 * @brief Starts the MAXRTOS scheduler.
 *
 * Initializes scheduler execution at tick zero using the supplied frame
 * schedule and partition table. On successful initialization, the process
 * control block (PCB) corresponding to the first scheduled process is
 * returned through @p out_first_pcb.
 *
 * @param[in]  frame_schedule
 *     Frame schedule used by the scheduler.
 *
 * @param[in]  partition_table
 *     Partition table used to select the active partition.
 *
 * @param[out] out_first_pcb
 *     Address of the pointer that receives the PCB of the
 *     first scheduled process.
 *
 * @return
 *     MAXRTOS_OK if the scheduler was successfully started.
 *     MAXRTOS_ERR_INVALID_ARG if any required argument is NULL or if
 *     the first scheduled process cannot be retrieved.
 */
maxrtos_status_t maxrtos_kernel_start_scheduler(
    maxrtos_frame_schedule_t const * frame_schedule,
    maxrtos_partition_table_t * partition_table,
    maxrtos_process_control_block_t ** out_first_pcb );

#endif /* MAXRTOS_KERNEL_START_SCHEDULER_H */
