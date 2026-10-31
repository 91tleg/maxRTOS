/**
 * @file svc.c
 * @brief C-side SVC dispatch.
 *
 * Runs privileged on behalf of whatever process executed an
 * svc instruction. This is the only entry point through which
 * an unprivileged partition may reach kernel functionality
 * that touches privileged-only state.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "maxrtos/arch/cortex_m7/svc.h"
#include "maxrtos/arch/cortex_m7/context_switch.h"
#include "maxrtos/arch/cortex_m7/fault_handlers.h"
#include "maxrtos/arch/cortex_m7/idle.h"
#include "maxrtos/arch/cortex_m7/mpu_hw.h"
#include "maxrtos/arch/cortex_m7/port.h"
#include "maxrtos/kernel/partition.h"
#include "maxrtos/kernel/process.h"
#include "maxrtos/kernel/yield.h"
#include "maxrtos/kernel/mutex.h"
#include "maxrtos/kernel/queue_port.h"
#include "maxrtos/kernel/sampling_port.h"
#include "maxrtos/kernel/semaphore.h"
#include "maxrtos/kernel/buffer.h"
#include "maxrtos/kernel/tick.h"
#include "maxrtos/kernel/timing.h"

/* Keep the IPC handlers out of line. With link-time optimization the
 * compiler otherwise folds them all into maxrtos_arch_svc_dispatch(), which
 * then saves nine registers and reserves a frame on entry, and every system
 * call, including the short yield, pays for that. */
#define MAXRTOS_SVC_OUT_OF_LINE __attribute__( ( noinline ) )

static void maxrtos_arch_svc_invalid( void );

static void maxrtos_arch_svc_yield( uint32_t * stacked_args );

MAXRTOS_SVC_OUT_OF_LINE static void maxrtos_arch_svc_queue_send(
    uint32_t * stacked_args );

MAXRTOS_SVC_OUT_OF_LINE static void maxrtos_arch_svc_queue_receive(
    uint32_t * stacked_args );

MAXRTOS_SVC_OUT_OF_LINE static void maxrtos_arch_svc_queue_count(
    uint32_t * stacked_args );

MAXRTOS_SVC_OUT_OF_LINE static void maxrtos_arch_svc_periodic_wait(
    uint32_t * stacked_args );

MAXRTOS_SVC_OUT_OF_LINE static void maxrtos_arch_svc_mutex_lock(
    uint32_t * stacked_args );

MAXRTOS_SVC_OUT_OF_LINE static void maxrtos_arch_svc_mutex_unlock(
    uint32_t * stacked_args );

MAXRTOS_SVC_OUT_OF_LINE static void maxrtos_arch_svc_semaphore_wait(
    uint32_t * stacked_args );

MAXRTOS_SVC_OUT_OF_LINE static void maxrtos_arch_svc_semaphore_signal(
    uint32_t * stacked_args );

MAXRTOS_SVC_OUT_OF_LINE static void maxrtos_arch_svc_semaphore_status(
    uint32_t * stacked_args );

MAXRTOS_SVC_OUT_OF_LINE static void maxrtos_arch_svc_buffer_send(
    uint32_t * stacked_args );

MAXRTOS_SVC_OUT_OF_LINE static void maxrtos_arch_svc_buffer_receive(
    uint32_t * stacked_args );

MAXRTOS_SVC_OUT_OF_LINE static void maxrtos_arch_svc_buffer_status(
    uint32_t * stacked_args );

MAXRTOS_SVC_OUT_OF_LINE static void maxrtos_arch_svc_timed_wait(
    uint32_t * stacked_args );

MAXRTOS_SVC_OUT_OF_LINE static void maxrtos_arch_svc_time_get(
    uint32_t * stacked_args );

MAXRTOS_SVC_OUT_OF_LINE static void maxrtos_arch_svc_sampling_write(
    uint32_t * stacked_args );

MAXRTOS_SVC_OUT_OF_LINE static void maxrtos_arch_svc_sampling_read(
    uint32_t * stacked_args );

MAXRTOS_SVC_OUT_OF_LINE static void maxrtos_arch_svc_preempt_check( void );

void maxrtos_arch_svc_dispatch(
    uint32_t * stacked_args,
    uint8_t svc_number )
{
    if( ( stacked_args == NULL ) ||
        ( svc_number >= ( uint8_t ) MAXRTOS_SVC_COUNT ) )
    {
        maxrtos_arch_svc_invalid();
    }

    switch( ( maxrtos_svc_number_t ) svc_number )
    {
        case MAXRTOS_SVC_YIELD:
        {
            maxrtos_arch_svc_yield( stacked_args );
            break;
        }

        case MAXRTOS_SVC_QUEUE_SEND:
        {
            maxrtos_arch_svc_queue_send( stacked_args );
            break;
        }

        case MAXRTOS_SVC_QUEUE_RECEIVE:
        {
            maxrtos_arch_svc_queue_receive( stacked_args );
            break;
        }

        case MAXRTOS_SVC_QUEUE_COUNT:
        {
            maxrtos_arch_svc_queue_count( stacked_args );
            break;
        }

        case MAXRTOS_SVC_PERIODIC_WAIT:
        {
            maxrtos_arch_svc_periodic_wait( stacked_args );
            break;
        }

        case MAXRTOS_SVC_MUTEX_LOCK:
        {
            maxrtos_arch_svc_mutex_lock( stacked_args );
            break;
        }

        case MAXRTOS_SVC_MUTEX_UNLOCK:
        {
            maxrtos_arch_svc_mutex_unlock( stacked_args );
            break;
        }

        case MAXRTOS_SVC_SEMAPHORE_WAIT:
        {
            maxrtos_arch_svc_semaphore_wait( stacked_args );
            break;
        }

        case MAXRTOS_SVC_SEMAPHORE_SIGNAL:
        {
            maxrtos_arch_svc_semaphore_signal( stacked_args );
            break;
        }

        case MAXRTOS_SVC_SEMAPHORE_STATUS:
        {
            maxrtos_arch_svc_semaphore_status( stacked_args );
            break;
        }

        case MAXRTOS_SVC_BUFFER_SEND:
        {
            maxrtos_arch_svc_buffer_send( stacked_args );
            break;
        }

        case MAXRTOS_SVC_BUFFER_RECEIVE:
        {
            maxrtos_arch_svc_buffer_receive( stacked_args );
            break;
        }

        case MAXRTOS_SVC_BUFFER_STATUS:
        {
            maxrtos_arch_svc_buffer_status( stacked_args );
            break;
        }

        case MAXRTOS_SVC_TIMED_WAIT:
        {
            maxrtos_arch_svc_timed_wait( stacked_args );
            break;
        }

        case MAXRTOS_SVC_TIME_GET:
        {
            maxrtos_arch_svc_time_get( stacked_args );
            break;
        }

        case MAXRTOS_SVC_SAMPLING_WRITE:
        {
            maxrtos_arch_svc_sampling_write( stacked_args );
            break;
        }

        case MAXRTOS_SVC_SAMPLING_READ:
        {
            maxrtos_arch_svc_sampling_read( stacked_args );
            break;
        }

        default:
        {
            maxrtos_arch_svc_invalid();
            break;
        }
    }

    /* A call can make a higher-priority process of the caller's partition
     * READY: an IPC wake-up, a mutex release, a priority change. The caller
     * is preempted at once. A yield only gives way, and reading the time or
     * using a sampling port (which never blocks or wakes anyone) changes
     * nothing, so they skip the check. */
    if( ( svc_number != ( uint8_t ) MAXRTOS_SVC_YIELD ) &&
        ( svc_number != ( uint8_t ) MAXRTOS_SVC_TIME_GET ) &&
        ( svc_number != ( uint8_t ) MAXRTOS_SVC_SAMPLING_WRITE ) &&
        ( svc_number != ( uint8_t ) MAXRTOS_SVC_SAMPLING_READ ) )
    {
        maxrtos_arch_svc_preempt_check();
    }
}

/* There is no valid recovery action for a syscall gate that
 * cannot identify what was requested. */
static void maxrtos_arch_svc_invalid( void )
{
    maxrtos_arch_fatal();
}

static void maxrtos_arch_svc_yield( uint32_t * stacked_args )
{
    maxrtos_process_control_block_t const * current_pcb;

    ( void ) stacked_args;

    current_pcb = maxrtos_arch_get_current_pcb();

    if( current_pcb != NULL )
    {
        maxrtos_status_t status;
        maxrtos_process_id_t next_id;
        bool switch_needed;

        status = maxrtos_kernel_yield(
            maxrtos_arch_get_partition_table(),
            current_pcb->partition_id,
            current_pcb->id,
            &next_id,
            &switch_needed );

        if( ( status == MAXRTOS_OK ) && ( switch_needed == true ) )
        {
            maxrtos_process_control_block_t * next_pcb;

            next_pcb = maxrtos_process_get( next_id );

            if( next_pcb != NULL )
            {
                maxrtos_arch_set_next_pcb( next_pcb );
                maxrtos_arch_request_context_switch();
            }
        }
    }
}

/* Finish an IPC service call that may have blocked the caller.
 *
 * The kernel has already made next_id the RUNNING process and recorded
 * it as its partition's current process, so only the hardware switch
 * remains. The blocked caller's own r0 is left untouched: the kernel
 * delivers the final result when it is resumed (see
 * maxrtos_arch_apply_resume_result()). */
static void maxrtos_arch_svc_finish_ipc(
    uint32_t * stacked_args,
    maxrtos_status_t status,
    maxrtos_process_id_t next_id )
{
    if( status == MAXRTOS_PENDING )
    {
        maxrtos_process_control_block_t * next_pcb;

        next_pcb = maxrtos_process_get( next_id );

        if( next_pcb == NULL )
        {
            /* The caller is already BLOCKED and there is nothing valid to
             * switch to. */
            maxrtos_arch_svc_invalid();
        }
        else
        {
            maxrtos_arch_set_next_pcb( next_pcb );
            maxrtos_arch_request_context_switch();
        }
    }
    else
    {
        stacked_args[ 0 ] = ( uint32_t ) status;
    }
}

static void maxrtos_arch_svc_queue_send( uint32_t * stacked_args )
{
    maxrtos_queue_port_id_t port_id;
    uint32_t message_address;
    uint32_t message_size;
    maxrtos_tick_t timeout;
    maxrtos_process_control_block_t const * current_pcb;
    maxrtos_partition_table_t * partition_table;
    maxrtos_process_id_t next_id;
    maxrtos_status_t status;

    port_id = ( maxrtos_queue_port_id_t ) stacked_args[ 0 ];
    message_address = stacked_args[ 1 ];
    message_size = stacked_args[ 2 ];
    timeout = ( maxrtos_tick_t ) stacked_args[ 3 ];

    current_pcb = maxrtos_arch_get_current_pcb();
    partition_table = maxrtos_arch_get_partition_table();
    next_id = MAXRTOS_INVALID_PROCESS_ID;

    if( ( current_pcb == NULL ) || ( partition_table == NULL ) )
    {
        status = MAXRTOS_ERR_INVALID_STATE;
    }
    else if(
        maxrtos_arch_mpu_partition_owns_range(
            current_pcb->partition_id, message_address, message_size ) ==
        false )
    {
        /* The caller is unprivileged; the kernel must not touch memory it
         * could not access itself. The port is named by ID and checked by
         * the kernel against the caller's partition. */
        status = MAXRTOS_ERR_INVALID_ARG;
    }
    else
    {
        status = maxrtos_kernel_queue_port_send(
            port_id,
            ( void const * ) MAXRTOS_PORT_UADDR_TO_PTR( message_address ),
            ( size_t ) message_size,
            timeout,
            partition_table,
            current_pcb->id,
            &next_id );
    }

    maxrtos_arch_svc_finish_ipc( stacked_args, status, next_id );
}

static void maxrtos_arch_svc_queue_receive( uint32_t * stacked_args )
{
    maxrtos_queue_port_id_t port_id;
    uint32_t buffer_address;
    uint32_t buffer_size;
    maxrtos_tick_t timeout;
    maxrtos_process_control_block_t const * current_pcb;
    maxrtos_partition_table_t * partition_table;
    maxrtos_process_id_t next_id;
    maxrtos_status_t status;

    port_id = ( maxrtos_queue_port_id_t ) stacked_args[ 0 ];
    buffer_address = stacked_args[ 1 ];
    buffer_size = stacked_args[ 2 ];
    timeout = ( maxrtos_tick_t ) stacked_args[ 3 ];

    current_pcb = maxrtos_arch_get_current_pcb();
    partition_table = maxrtos_arch_get_partition_table();
    next_id = MAXRTOS_INVALID_PROCESS_ID;

    if( ( current_pcb == NULL ) || ( partition_table == NULL ) )
    {
        status = MAXRTOS_ERR_INVALID_STATE;
    }
    else if(
        maxrtos_arch_mpu_partition_owns_range(
            current_pcb->partition_id, buffer_address, buffer_size ) == false )
    {
        status = MAXRTOS_ERR_INVALID_ARG;
    }
    else
    {
        status = maxrtos_kernel_queue_port_receive(
            port_id,
            ( void * ) MAXRTOS_PORT_UADDR_TO_PTR( buffer_address ),
            ( size_t ) buffer_size,
            timeout,
            partition_table,
            current_pcb->id,
            &next_id );
    }

    maxrtos_arch_svc_finish_ipc( stacked_args, status, next_id );
}

static void maxrtos_arch_svc_queue_count( uint32_t * stacked_args )
{
    maxrtos_process_control_block_t const * current_pcb;

    current_pcb = maxrtos_arch_get_current_pcb();

    /* A port the caller may not use reports no messages. */
    if( current_pcb != NULL )
    {
        stacked_args[ 0 ] = ( uint32_t ) maxrtos_kernel_queue_port_count(
            ( maxrtos_queue_port_id_t ) stacked_args[ 0 ], current_pcb->id );
    }
    else
    {
        stacked_args[ 0 ] = 0U;
    }
}

/* Finish a call that may have blocked the caller until a tick: the caller is
 * BLOCKED if status is MAXRTOS_PENDING, and its final result is delivered when
 * it resumes. No other process may be READY, in which case the CPU idles until
 * the wake-up or the end of the slot. Any other status is the caller's result.
 */
static void maxrtos_arch_svc_finish_wait(
    uint32_t * stacked_args,
    maxrtos_status_t status,
    maxrtos_process_id_t next_id )
{
    if( status == MAXRTOS_PENDING )
    {
        maxrtos_process_control_block_t * next_pcb;

        if( next_id == MAXRTOS_INVALID_PROCESS_ID )
        {
            next_pcb = maxrtos_arch_idle_pcb();
        }
        else
        {
            next_pcb = maxrtos_process_get( next_id );
        }

        if( next_pcb == NULL )
        {
            maxrtos_arch_svc_invalid();
        }

        maxrtos_arch_set_next_pcb( next_pcb );
        maxrtos_arch_request_context_switch();
    }
    else
    {
        stacked_args[ 0 ] = ( uint32_t ) status;
    }
}

static void maxrtos_arch_svc_periodic_wait( uint32_t * stacked_args )
{
    maxrtos_process_control_block_t const * current_pcb;
    maxrtos_partition_table_t * partition_table;
    maxrtos_process_id_t next_id;
    maxrtos_status_t status;

    current_pcb = maxrtos_arch_get_current_pcb();
    partition_table = maxrtos_arch_get_partition_table();
    next_id = MAXRTOS_INVALID_PROCESS_ID;

    if( ( current_pcb == NULL ) || ( partition_table == NULL ) )
    {
        status = MAXRTOS_ERR_INVALID_STATE;
    }
    else
    {
        status = maxrtos_kernel_periodic_wait(
            partition_table, current_pcb->id, &next_id );
    }

    maxrtos_arch_svc_finish_wait( stacked_args, status, next_id );
}

static void maxrtos_arch_svc_timed_wait( uint32_t * stacked_args )
{
    maxrtos_process_control_block_t const * current_pcb;
    maxrtos_partition_table_t * partition_table;
    maxrtos_process_id_t next_id;
    maxrtos_status_t status;

    maxrtos_tick_t delay_ticks;

    delay_ticks = ( maxrtos_tick_t ) stacked_args[ 0 ];
    current_pcb = maxrtos_arch_get_current_pcb();
    partition_table = maxrtos_arch_get_partition_table();
    next_id = MAXRTOS_INVALID_PROCESS_ID;

    if( ( current_pcb == NULL ) || ( partition_table == NULL ) )
    {
        status = MAXRTOS_ERR_INVALID_STATE;
    }
    else
    {
        status = maxrtos_kernel_timed_wait(
            partition_table, current_pcb->id, delay_ticks, &next_id );
    }

    maxrtos_arch_svc_finish_wait( stacked_args, status, next_id );

    if( ( status == MAXRTOS_OK ) && ( delay_ticks == 0U ) )
    {
        /* A zero delay lets processes of the same priority take turns. The
         * caller's result (OK) is already in its frame. */
        maxrtos_arch_svc_yield( stacked_args );
    }
}

static void maxrtos_arch_svc_preempt_check( void )
{
    maxrtos_process_control_block_t const * caller;
    maxrtos_partition_table_t * table;

    caller = maxrtos_arch_get_current_pcb();
    table = maxrtos_arch_get_partition_table();

    /* Only a caller that is still RUNNING can be preempted by what the call
     * made READY. One that blocked was replaced by the best READY process,
     * so nothing in its partition outranks the process now running. */
    if( ( caller != NULL ) && ( table != NULL ) &&
        ( caller->state == MAXRTOS_PROCESS_STATE_RUNNING ) &&
        ( caller->partition_id < MAXRTOS_MAX_PARTITIONS ) &&
        maxrtos_scheduler_has_higher(
            &table->scheduler_ctx[ caller->partition_id ], caller->priority ) )
    {
        maxrtos_process_id_t next_id;

        if( ( maxrtos_partition_dispatch(
                  table, caller->partition_id, &next_id ) == MAXRTOS_OK ) &&
            ( next_id != caller->id ) )
        {
            maxrtos_process_control_block_t * next_pcb;

            next_pcb = maxrtos_process_get( next_id );

            if( next_pcb != NULL )
            {
                maxrtos_arch_set_next_pcb( next_pcb );
                maxrtos_arch_request_context_switch();
            }
        }
    }
}

/* A sampling port call never blocks, wakes nobody and makes nothing READY,
 * so there is no scheduling decision to take after it. The port is named by
 * ID and checked by the kernel against the caller's partition. The message
 * or buffer is memory the kernel touches with privilege, so it must be memory
 * the caller could access itself. */
static void maxrtos_arch_svc_sampling_write( uint32_t * stacked_args )
{
    maxrtos_process_control_block_t const * current_pcb;
    uint32_t message_address;
    uint32_t message_size;
    maxrtos_status_t status;

    message_address = stacked_args[ 1 ];
    message_size = stacked_args[ 2 ];
    current_pcb = maxrtos_arch_get_current_pcb();

    if( current_pcb == NULL )
    {
        status = MAXRTOS_ERR_INVALID_STATE;
    }
    else if(
        maxrtos_arch_mpu_partition_owns_range(
            current_pcb->partition_id, message_address, message_size ) ==
        false )
    {
        status = MAXRTOS_ERR_INVALID_ARG;
    }
    else
    {
        status = maxrtos_kernel_sampling_port_write(
            ( maxrtos_sampling_port_id_t ) stacked_args[ 0 ],
            ( void const * ) MAXRTOS_PORT_UADDR_TO_PTR( message_address ),
            ( size_t ) message_size,
            current_pcb->id );
    }

    stacked_args[ 0 ] = ( uint32_t ) status;
}

static void maxrtos_arch_svc_sampling_read( uint32_t * stacked_args )
{
    maxrtos_process_control_block_t const * current_pcb;
    uint32_t buffer_address;
    uint32_t buffer_size;
    maxrtos_status_t status;

    buffer_address = stacked_args[ 1 ];
    buffer_size = stacked_args[ 2 ];
    current_pcb = maxrtos_arch_get_current_pcb();

    if( current_pcb == NULL )
    {
        status = MAXRTOS_ERR_INVALID_STATE;
    }
    else if(
        maxrtos_arch_mpu_partition_owns_range(
            current_pcb->partition_id, buffer_address, buffer_size ) == false )
    {
        status = MAXRTOS_ERR_INVALID_ARG;
    }
    else
    {
        status = maxrtos_kernel_sampling_port_read(
            ( maxrtos_sampling_port_id_t ) stacked_args[ 0 ],
            ( void * ) MAXRTOS_PORT_UADDR_TO_PTR( buffer_address ),
            ( size_t ) buffer_size,
            current_pcb->id );
    }

    stacked_args[ 0 ] = ( uint32_t ) status;
}

static void maxrtos_arch_svc_time_get( uint32_t * stacked_args )
{
    stacked_args[ 0 ] = ( uint32_t ) maxrtos_kernel_tick_now();
}

static void maxrtos_arch_svc_mutex_lock( uint32_t * stacked_args )
{
    maxrtos_process_control_block_t const * current_pcb;
    maxrtos_partition_table_t * partition_table;
    maxrtos_process_id_t next_id;
    maxrtos_status_t status;

    current_pcb = maxrtos_arch_get_current_pcb();
    partition_table = maxrtos_arch_get_partition_table();
    next_id = MAXRTOS_INVALID_PROCESS_ID;

    if( ( current_pcb == NULL ) || ( partition_table == NULL ) )
    {
        status = MAXRTOS_ERR_INVALID_STATE;
    }
    else
    {
        /* The mutex is named by handle and checked by the kernel against
         * the caller's partition, so no address needs validating. */
        status = maxrtos_kernel_mutex_lock(
            ( maxrtos_mutex_id_t ) stacked_args[ 0 ],
            ( maxrtos_tick_t ) stacked_args[ 1 ],
            partition_table,
            current_pcb->id,
            &next_id );
    }

    maxrtos_arch_svc_finish_ipc( stacked_args, status, next_id );
}

static void maxrtos_arch_svc_mutex_unlock( uint32_t * stacked_args )
{
    maxrtos_process_control_block_t const * current_pcb;
    maxrtos_partition_table_t * partition_table;
    maxrtos_status_t status;

    current_pcb = maxrtos_arch_get_current_pcb();
    partition_table = maxrtos_arch_get_partition_table();

    if( ( current_pcb == NULL ) || ( partition_table == NULL ) )
    {
        status = MAXRTOS_ERR_INVALID_STATE;
    }
    else
    {
        /* Ownership passes to a waiter, if any, which becomes READY. The
         * caller keeps the CPU until the next tick or yield. */
        status = maxrtos_kernel_mutex_unlock(
            ( maxrtos_mutex_id_t ) stacked_args[ 0 ],
            partition_table,
            current_pcb->id );
    }

    stacked_args[ 0 ] = ( uint32_t ) status;
}

static void maxrtos_arch_svc_semaphore_wait( uint32_t * stacked_args )
{
    maxrtos_process_control_block_t const * current_pcb;
    maxrtos_partition_table_t * partition_table;
    maxrtos_process_id_t next_id;
    maxrtos_status_t status;

    current_pcb = maxrtos_arch_get_current_pcb();
    partition_table = maxrtos_arch_get_partition_table();
    next_id = MAXRTOS_INVALID_PROCESS_ID;

    if( ( current_pcb == NULL ) || ( partition_table == NULL ) )
    {
        status = MAXRTOS_ERR_INVALID_STATE;
    }
    else
    {
        /* Named by handle and checked by the kernel against the caller's
         * partition, so no address needs validating. */
        status = maxrtos_kernel_semaphore_wait(
            ( maxrtos_semaphore_id_t ) stacked_args[ 0 ],
            ( maxrtos_tick_t ) stacked_args[ 1 ],
            partition_table,
            current_pcb->id,
            &next_id );
    }

    maxrtos_arch_svc_finish_ipc( stacked_args, status, next_id );
}

static void maxrtos_arch_svc_semaphore_signal( uint32_t * stacked_args )
{
    maxrtos_process_control_block_t const * current_pcb;
    maxrtos_partition_table_t * partition_table;
    maxrtos_status_t status;

    current_pcb = maxrtos_arch_get_current_pcb();
    partition_table = maxrtos_arch_get_partition_table();

    if( ( current_pcb == NULL ) || ( partition_table == NULL ) )
    {
        status = MAXRTOS_ERR_INVALID_STATE;
    }
    else
    {
        status = maxrtos_kernel_semaphore_signal(
            ( maxrtos_semaphore_id_t ) stacked_args[ 0 ],
            partition_table,
            current_pcb->id );
    }

    stacked_args[ 0 ] = ( uint32_t ) status;
}

static void maxrtos_arch_svc_buffer_send( uint32_t * stacked_args )
{
    maxrtos_buffer_id_t id;
    uint32_t message_address;
    uint32_t message_size;
    maxrtos_tick_t timeout;
    maxrtos_process_control_block_t const * current_pcb;
    maxrtos_partition_table_t * partition_table;
    maxrtos_process_id_t next_id;
    maxrtos_status_t status;

    id = ( maxrtos_buffer_id_t ) stacked_args[ 0 ];
    message_address = stacked_args[ 1 ];
    message_size = stacked_args[ 2 ];
    timeout = ( maxrtos_tick_t ) stacked_args[ 3 ];

    current_pcb = maxrtos_arch_get_current_pcb();
    partition_table = maxrtos_arch_get_partition_table();
    next_id = MAXRTOS_INVALID_PROCESS_ID;

    if( ( current_pcb == NULL ) || ( partition_table == NULL ) )
    {
        status = MAXRTOS_ERR_INVALID_STATE;
    }
    else if(
        maxrtos_arch_mpu_partition_owns_range(
            current_pcb->partition_id, message_address, message_size ) ==
        false )
    {
        /* The caller is unprivileged; the kernel must not touch memory it
         * could not access itself. The buffer is named by handle and
         * checked by the kernel against the caller's partition. */
        status = MAXRTOS_ERR_INVALID_ARG;
    }
    else
    {
        status = maxrtos_kernel_buffer_send(
            id,
            ( void const * ) MAXRTOS_PORT_UADDR_TO_PTR( message_address ),
            ( size_t ) message_size,
            timeout,
            partition_table,
            current_pcb->id,
            &next_id );
    }

    maxrtos_arch_svc_finish_ipc( stacked_args, status, next_id );
}

static void maxrtos_arch_svc_buffer_receive( uint32_t * stacked_args )
{
    maxrtos_buffer_id_t id;
    uint32_t buffer_address;
    uint32_t buffer_size;
    maxrtos_tick_t timeout;
    maxrtos_process_control_block_t const * current_pcb;
    maxrtos_partition_table_t * partition_table;
    maxrtos_process_id_t next_id;
    maxrtos_status_t status;

    id = ( maxrtos_buffer_id_t ) stacked_args[ 0 ];
    buffer_address = stacked_args[ 1 ];
    buffer_size = stacked_args[ 2 ];
    timeout = ( maxrtos_tick_t ) stacked_args[ 3 ];

    current_pcb = maxrtos_arch_get_current_pcb();
    partition_table = maxrtos_arch_get_partition_table();
    next_id = MAXRTOS_INVALID_PROCESS_ID;

    if( ( current_pcb == NULL ) || ( partition_table == NULL ) )
    {
        status = MAXRTOS_ERR_INVALID_STATE;
    }
    else if(
        maxrtos_arch_mpu_partition_owns_range(
            current_pcb->partition_id, buffer_address, buffer_size ) == false )
    {
        status = MAXRTOS_ERR_INVALID_ARG;
    }
    else
    {
        status = maxrtos_kernel_buffer_receive(
            id,
            ( void * ) MAXRTOS_PORT_UADDR_TO_PTR( buffer_address ),
            ( size_t ) buffer_size,
            timeout,
            partition_table,
            current_pcb->id,
            &next_id );
    }

    maxrtos_arch_svc_finish_ipc( stacked_args, status, next_id );
}

static void maxrtos_arch_svc_buffer_status( uint32_t * stacked_args )
{
    maxrtos_process_control_block_t const * current_pcb;
    uint32_t out_address;
    maxrtos_status_t status;

    current_pcb = maxrtos_arch_get_current_pcb();
    out_address = stacked_args[ 1 ];

    if( current_pcb == NULL )
    {
        status = MAXRTOS_ERR_INVALID_STATE;
    }
    else if(
        ( out_address % sizeof( uint32_t ) != 0U ) ||
        ( maxrtos_arch_mpu_partition_owns_range(
              current_pcb->partition_id,
              out_address,
              sizeof( maxrtos_buffer_status_t ) ) == false ) )
    {
        /* The kernel writes the result, so the address must be memory the
         * caller could write itself. */
        status = MAXRTOS_ERR_INVALID_ARG;
    }
    else
    {
        status = maxrtos_kernel_buffer_get_status(
            ( maxrtos_buffer_id_t ) stacked_args[ 0 ],
            current_pcb->id,
            ( maxrtos_buffer_status_t * ) MAXRTOS_PORT_UADDR_TO_PTR(
                out_address ) );
    }

    stacked_args[ 0 ] = ( uint32_t ) status;
}

static void maxrtos_arch_svc_semaphore_status( uint32_t * stacked_args )
{
    maxrtos_process_control_block_t const * current_pcb;
    uint32_t out_address;
    maxrtos_status_t status;

    current_pcb = maxrtos_arch_get_current_pcb();
    out_address = stacked_args[ 1 ];

    if( current_pcb == NULL )
    {
        status = MAXRTOS_ERR_INVALID_STATE;
    }
    else if(
        ( out_address % sizeof( uint32_t ) != 0U ) ||
        ( maxrtos_arch_mpu_partition_owns_range(
              current_pcb->partition_id,
              out_address,
              sizeof( maxrtos_semaphore_status_t ) ) == false ) )
    {
        /* The kernel writes the result, so the address must be memory the
         * caller could write itself. */
        status = MAXRTOS_ERR_INVALID_ARG;
    }
    else
    {
        status = maxrtos_kernel_semaphore_get_status(
            ( maxrtos_semaphore_id_t ) stacked_args[ 0 ],
            current_pcb->id,
            ( maxrtos_semaphore_status_t * ) MAXRTOS_PORT_UADDR_TO_PTR(
                out_address ) );
    }

    stacked_args[ 0 ] = ( uint32_t ) status;
}
