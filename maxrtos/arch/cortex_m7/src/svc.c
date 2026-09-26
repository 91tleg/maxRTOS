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
#include "maxrtos/kernel/semaphore.h"
#include "maxrtos/kernel/buffer.h"
#include "maxrtos/kernel/timing.h"

static void maxrtos_arch_svc_invalid( void );

static void maxrtos_arch_svc_yield(
    uint32_t * stacked_args );

static void maxrtos_arch_svc_queue_send(
    uint32_t * stacked_args );

static void maxrtos_arch_svc_queue_receive(
    uint32_t * stacked_args );

static void maxrtos_arch_svc_queue_count(
    uint32_t * stacked_args );

static void maxrtos_arch_svc_periodic_wait(
    uint32_t * stacked_args );

static void maxrtos_arch_svc_mutex_lock(
    uint32_t * stacked_args );

static void maxrtos_arch_svc_mutex_unlock(
    uint32_t * stacked_args );

static void maxrtos_arch_svc_semaphore_wait(
    uint32_t * stacked_args );

static void maxrtos_arch_svc_semaphore_signal(
    uint32_t * stacked_args );

static void maxrtos_arch_svc_semaphore_status(
    uint32_t * stacked_args );

static void maxrtos_arch_svc_buffer_send(
    uint32_t * stacked_args );

static void maxrtos_arch_svc_buffer_receive(
    uint32_t * stacked_args );

static void maxrtos_arch_svc_buffer_status(
    uint32_t * stacked_args );

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

        default:
        {
            maxrtos_arch_svc_invalid();
            break;
        }
    }
}

/* There is no valid recovery action for a syscall gate that
 * cannot identify what was requested. */
static void maxrtos_arch_svc_invalid( void )
{
    MAXRTOS_PORT_HALT();
}

static void maxrtos_arch_svc_yield(
    uint32_t * stacked_args )
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

        if( ( status == MAXRTOS_OK ) &&
            ( switch_needed == true ) )
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

static void maxrtos_arch_svc_queue_send(
    uint32_t * stacked_args )
{
    uint32_t port_address;
    uint32_t message_address;
    uint32_t message_size;
    maxrtos_tick_t timeout;
    maxrtos_process_control_block_t const * current_pcb;
    maxrtos_partition_table_t * partition_table;
    maxrtos_process_id_t next_id;
    maxrtos_status_t status;

    port_address = stacked_args[ 0 ];
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
    else if( ( maxrtos_arch_mpu_is_port_member(
                   current_pcb->partition_id, port_address ) == false ) ||
             ( maxrtos_arch_mpu_partition_owns_range(
                   current_pcb->partition_id,
                   message_address,
                   message_size ) == false ) )
    {
        /* The caller is unprivileged; the kernel must not touch memory it
         * could not access itself. */
        status = MAXRTOS_ERR_INVALID_ARG;
    }
    else
    {
        status = maxrtos_kernel_queue_port_send(
            ( maxrtos_queue_port_t * ) MAXRTOS_PORT_UADDR_TO_PTR( port_address ),
            ( void const * ) MAXRTOS_PORT_UADDR_TO_PTR( message_address ),
            ( size_t ) message_size,
            timeout,
            partition_table,
            current_pcb->id,
            &next_id );
    }

    maxrtos_arch_svc_finish_ipc( stacked_args, status, next_id );
}

static void maxrtos_arch_svc_queue_receive(
    uint32_t * stacked_args )
{
    uint32_t port_address;
    uint32_t buffer_address;
    uint32_t buffer_size;
    maxrtos_tick_t timeout;
    maxrtos_process_control_block_t const * current_pcb;
    maxrtos_partition_table_t * partition_table;
    maxrtos_process_id_t next_id;
    maxrtos_status_t status;

    port_address = stacked_args[ 0 ];
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
    else if( ( maxrtos_arch_mpu_is_port_member(
                   current_pcb->partition_id, port_address ) == false ) ||
             ( maxrtos_arch_mpu_partition_owns_range(
                   current_pcb->partition_id,
                   buffer_address,
                   buffer_size ) == false ) )
    {
        status = MAXRTOS_ERR_INVALID_ARG;
    }
    else
    {
        status = maxrtos_kernel_queue_port_receive(
            ( maxrtos_queue_port_t * ) MAXRTOS_PORT_UADDR_TO_PTR( port_address ),
            ( void * ) MAXRTOS_PORT_UADDR_TO_PTR( buffer_address ),
            ( size_t ) buffer_size,
            timeout,
            partition_table,
            current_pcb->id,
            &next_id );
    }

    maxrtos_arch_svc_finish_ipc( stacked_args, status, next_id );
}

static void maxrtos_arch_svc_queue_count(
    uint32_t * stacked_args )
{
    maxrtos_process_control_block_t const * current_pcb;
    uint32_t port_address;

    port_address = stacked_args[ 0 ];
    current_pcb = maxrtos_arch_get_current_pcb();

    /* A port the caller may not use reports no messages. */
    if( ( current_pcb != NULL ) &&
        ( maxrtos_arch_mpu_is_port_member(
              current_pcb->partition_id, port_address ) == true ) )
    {
        stacked_args[ 0 ] = ( uint32_t ) maxrtos_kernel_queue_port_count(
            ( maxrtos_queue_port_t const * ) MAXRTOS_PORT_UADDR_TO_PTR( port_address ) );
    }
    else
    {
        stacked_args[ 0 ] = 0U;
    }
}

static void maxrtos_arch_svc_periodic_wait(
    uint32_t * stacked_args )
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

    if( status == MAXRTOS_PENDING )
    {
        maxrtos_process_control_block_t * next_pcb;

        /* The caller is BLOCKED until its next release, and its result is
         * delivered when it resumes. No other process may be READY, in which
         * case the CPU idles until the release or the end of the slot. */
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

static void maxrtos_arch_svc_mutex_lock(
    uint32_t * stacked_args )
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

static void maxrtos_arch_svc_mutex_unlock(
    uint32_t * stacked_args )
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

static void maxrtos_arch_svc_semaphore_wait(
    uint32_t * stacked_args )
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

static void maxrtos_arch_svc_semaphore_signal(
    uint32_t * stacked_args )
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

static void maxrtos_arch_svc_buffer_send(
    uint32_t * stacked_args )
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
    else if( maxrtos_arch_mpu_partition_owns_range(
                 current_pcb->partition_id,
                 message_address,
                 message_size ) == false )
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

static void maxrtos_arch_svc_buffer_receive(
    uint32_t * stacked_args )
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
    else if( maxrtos_arch_mpu_partition_owns_range(
                 current_pcb->partition_id,
                 buffer_address,
                 buffer_size ) == false )
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

static void maxrtos_arch_svc_buffer_status(
    uint32_t * stacked_args )
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
    else if( ( out_address % sizeof( uint32_t ) != 0U ) ||
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
            ( maxrtos_buffer_status_t * ) MAXRTOS_PORT_UADDR_TO_PTR( out_address ) );
    }

    stacked_args[ 0 ] = ( uint32_t ) status;
}

static void maxrtos_arch_svc_semaphore_status(
    uint32_t * stacked_args )
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
    else if( ( out_address % sizeof( uint32_t ) != 0U ) ||
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
            ( maxrtos_semaphore_status_t * ) MAXRTOS_PORT_UADDR_TO_PTR( out_address ) );
    }

    stacked_args[ 0 ] = ( uint32_t ) status;
}
