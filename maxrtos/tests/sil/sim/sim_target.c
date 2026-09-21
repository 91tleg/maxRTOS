/**
 * @file sim_target.c
 * @brief Virtual Cortex-M7 target. See sim_target.h.
 */

#define _GNU_SOURCE

#include <pthread.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sim_target.h"
#include "sim_port_overrides.h"

#include "maxrtos/arch/cortex_m7/context_switch.h"
#include "maxrtos/arch/cortex_m7/fault_handlers.h"
#include "maxrtos/arch/cortex_m7/idle.h"
#include "maxrtos/arch/cortex_m7/mpu_hw.h"
#include "maxrtos/arch/cortex_m7/svc.h"
#include "maxrtos/arch/cortex_m7/systick.h"
#include "maxrtos/kernel/tick.h"
#include "maxrtos/queue_port.h"
#include "maxrtos/scheduler.h"
#include "maxrtos/yield.h"

#define SIM_MAX_CTX      ( MAXRTOS_MAX_PROCESSES + 2U ) /* + idle + spare */
#define SIM_MAX_MAPS     ( 8U )
#define SIM_MAX_FAULTS   ( 32U )
#define SIM_HOST_STACK   ( 256U * 1024U )
#define SIM_MIN_STACK    ( 32U * 1024U )

/* ------------------------------------------------------------------------ */
/* Contexts and the baton                                                    */
/* ------------------------------------------------------------------------ */

typedef struct sim_ctx
{
    pthread_t thread;
    pthread_cond_t wake;
    bool created;
    volatile bool restart;        /* re-enter the entry function when resumed */
    maxrtos_process_control_block_t * pcb;
    bool result_valid;            /* a blocked call completed while parked */
    uint32_t result;
    void * host_stack;
    jmp_buf entry_jump;
} sim_ctx_t;

typedef struct
{
    char const * name;
    uint32_t base;
    uint32_t size;
    uint8_t * backing;
} sim_map_t;

typedef struct
{
    uint32_t tick;
    maxrtos_fault_type_t type;
    bool used;
} sim_fault_injection_t;

maxrtos_process_control_block_t * g_maxrtos_current_pcb = NULL;
maxrtos_process_control_block_t * g_maxrtos_next_pcb = NULL;

static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
static bool s_lock_held;
static sim_ctx_t s_bench;
static sim_ctx_t s_ctx[ SIM_MAX_CTX ];
static sim_ctx_t * volatile s_running;
static volatile bool s_shutdown;

static bool s_pendsv_pending;
static uint64_t s_cycles;
static uint64_t s_next_tick_cycle;
static uint32_t s_ticks;
static uint32_t s_stop_tick;
static uint32_t s_switches;
static bool s_started;
static bool s_normal_return;
static bool s_halted;
static char const * s_halt_reason;
static uint32_t s_model_violations;

static jmp_buf s_bench_jump;
static bool s_bench_jump_valid;
static pthread_t s_bench_thread;

static sim_map_t s_maps[ SIM_MAX_MAPS ];
static unsigned s_map_count;

static sim_fault_injection_t s_faults[ SIM_MAX_FAULTS ];

static int16_t s_partition_log[ SIM_MAX_TICKS ];
static int16_t s_process_log[ SIM_MAX_TICKS ];

/* ------------------------------------------------------------------------ */
/* Baton passing                                                             */
/* ------------------------------------------------------------------------ */

static sim_ctx_t * ctx_for_pcb( maxrtos_process_control_block_t const * pcb )
{
    unsigned i;

    for( i = 0U; i < SIM_MAX_CTX; i++ )
    {
        if( s_ctx[ i ].pcb == pcb )
        {
            return &s_ctx[ i ];
        }
    }

    return NULL;
}

static sim_ctx_t * ctx_alloc( maxrtos_process_control_block_t * pcb )
{
    sim_ctx_t * ctx = ctx_for_pcb( pcb );
    unsigned i;

    for( i = 0U; ( ctx == NULL ) && ( i < SIM_MAX_CTX ); i++ )
    {
        if( s_ctx[ i ].pcb == NULL )
        {
            ctx = &s_ctx[ i ];
            ctx->pcb = pcb;
            ( void ) pthread_cond_init( &ctx->wake, NULL );
        }
    }

    return ctx;
}

/* Block the calling thread until it holds the baton again. */
static void ctx_wait( sim_ctx_t * self )
{
    while( ( s_running != self ) && !s_shutdown )
    {
        ( void ) pthread_cond_wait( &self->wake, &s_lock );
    }

    if( s_shutdown && ( self != &s_bench ) )
    {
        ( void ) pthread_mutex_unlock( &s_lock );
        pthread_exit( NULL );
    }

    if( self->restart )
    {
        self->restart = false;
        longjmp( self->entry_jump, 1 );
    }
}

static void * ctx_main( void * argument )
{
    sim_ctx_t * ctx = argument;
    maxrtos_process_control_block_t * pcb;

    ( void ) pthread_mutex_lock( &s_lock );

    while( ( s_running != ctx ) && !s_shutdown )
    {
        ( void ) pthread_cond_wait( &ctx->wake, &s_lock );
    }

    if( s_shutdown )
    {
        ( void ) pthread_mutex_unlock( &s_lock );
        return NULL;
    }

    /* A restart re-enters here with a fresh stack frame. */
    ( void ) setjmp( ctx->entry_jump );

    pcb = ctx->pcb;
    pcb->entry( pcb->entry_arg );

    /* Entry functions must not return (the target traps this). */
    sim_report_model_violation( "process entry function returned" );
    sim_port_halt();

    return NULL;
}

static void ctx_start_thread( sim_ctx_t * ctx )
{
    pthread_attr_t attr;
    maxrtos_process_control_block_t const * pcb = ctx->pcb;
    void * stack = NULL;
    size_t stack_size = 0U;

    ( void ) pthread_attr_init( &attr );

    /* Prefer the process's own (mapped) stack, so its locals live inside
     * its partition's memory domain as they do on the target. */
    if( ( pcb->stack_base != NULL ) && ( pcb->stack_size >= SIM_MIN_STACK ) &&
        ( sim_ptr_to_uaddr( pcb->stack_base ) != 0U ) )
    {
        stack = pcb->stack_base;
        stack_size = pcb->stack_size;
    }
    else
    {
        ctx->host_stack = malloc( SIM_HOST_STACK );
        stack = ctx->host_stack;
        stack_size = SIM_HOST_STACK;
    }

    ( void ) pthread_attr_setstack( &attr, stack, stack_size );
    ( void ) pthread_create( &ctx->thread, &attr, ctx_main, ctx );
    ( void ) pthread_attr_destroy( &attr );

    ctx->created = true;
}

/* Hand the baton to `to`; returns when the caller holds it again. */
static void baton_transfer( sim_ctx_t * to, sim_ctx_t * self )
{
    if( ( to->pcb != NULL ) && !to->created )
    {
        ctx_start_thread( to );
    }

    s_running = to;
    ( void ) pthread_cond_signal( &to->wake );
    ctx_wait( self );
}

/* ------------------------------------------------------------------------ */
/* Halt                                                                      */
/* ------------------------------------------------------------------------ */

static void record_halt( char const * reason )
{
    if( !s_halted )
    {
        s_halted = true;
        s_halt_reason = reason;
    }
}

void sim_port_halt( void )
{
    if( pthread_equal( pthread_self(), s_bench_thread ) && s_bench_jump_valid )
    {
        if( !s_normal_return )
        {
            record_halt( "halt before the scheduler reached its stop tick" );
        }

        s_normal_return = false;
        longjmp( s_bench_jump, 1 );
    }

    /* On a process thread: report to the bench and never run again. */
    record_halt( "CPU halted (unrecoverable error)" );

    {
        sim_ctx_t * self = s_running;

        s_running = &s_bench;
        ( void ) pthread_cond_signal( &s_bench.wake );
        ctx_wait( self );
    }

    for( ;; )
    {
    }
}

bool sim_halted( void )
{
    return s_halted;
}

char const * sim_halt_reason( void )
{
    return s_halt_reason;
}

void sim_report_model_violation( char const * message )
{
    s_model_violations++;
    ( void ) fprintf( stderr, "[sim] model violation: %s\n", message );
}

uint32_t sim_model_violations( void )
{
    return s_model_violations;
}

/* ------------------------------------------------------------------------ */
/* Virtual memory                                                            */
/* ------------------------------------------------------------------------ */

void * sim_mem_map( char const * name, uint32_t virtual_base, uint32_t size )
{
    void * backing = NULL;

    if( ( s_map_count < SIM_MAX_MAPS ) && ( size > 0U ) &&
        ( posix_memalign( &backing, 4096U, size ) == 0 ) )
    {
        ( void ) memset( backing, 0, size );

        s_maps[ s_map_count ].name = name;
        s_maps[ s_map_count ].base = virtual_base;
        s_maps[ s_map_count ].size = size;
        s_maps[ s_map_count ].backing = backing;
        s_map_count++;
    }

    return backing;
}

void * sim_mem_ptr( uint32_t address, uint32_t length )
{
    unsigned i;

    for( i = 0U; i < s_map_count; i++ )
    {
        sim_map_t const * m = &s_maps[ i ];

        if( ( address >= m->base ) &&
            ( ( uint64_t ) address + length <= ( uint64_t ) m->base + m->size ) )
        {
            return m->backing + ( address - m->base );
        }
    }

    return NULL;
}

uint32_t sim_ptr_to_uaddr( void const * pointer )
{
    unsigned i;
    uint8_t const * p = pointer;

    for( i = 0U; i < s_map_count; i++ )
    {
        sim_map_t const * m = &s_maps[ i ];

        if( ( p >= m->backing ) && ( p < ( m->backing + m->size ) ) )
        {
            return m->base + ( uint32_t ) ( p - m->backing );
        }
    }

    return 0U;
}

void * sim_uaddr_to_ptr( uint32_t address )
{
    return sim_mem_ptr( address, 1U );
}

/* ------------------------------------------------------------------------ */
/* Architecture interface (what context_switch.c/.S provide on the target)   */
/* ------------------------------------------------------------------------ */

maxrtos_status_t maxrtos_arch_init_stack( maxrtos_process_control_block_t * pcb )
{
    maxrtos_status_t status = MAXRTOS_ERR_INVALID_ARG;

    if( ( pcb != NULL ) && ( pcb->stack_base != NULL ) &&
        ( pcb->stack_size >= 64U ) && ( pcb->entry != NULL ) )
    {
        sim_ctx_t * ctx = ctx_alloc( pcb );

        if( ctx != NULL )
        {
            /* A context that has already run re-enters its entry function
             * with a fresh frame the next time it is restored. */
            if( ctx->created )
            {
                ctx->restart = true;
            }

            ctx->result_valid = false;
            pcb->stack_pointer = &pcb->stack_base[ pcb->stack_size ];
            pcb->ipc_result_pending = false;
            status = MAXRTOS_OK;
        }
    }

    return status;
}

void maxrtos_arch_set_current_pcb( maxrtos_process_control_block_t * pcb )
{
    g_maxrtos_current_pcb = pcb;
}

void maxrtos_arch_set_next_pcb( maxrtos_process_control_block_t * pcb )
{
    g_maxrtos_next_pcb = pcb;
}

maxrtos_process_control_block_t * maxrtos_arch_get_current_pcb( void )
{
    return g_maxrtos_current_pcb;
}

void maxrtos_arch_request_context_switch( void )
{
    s_pendsv_pending = true;
}

void maxrtos_arch_context_switch_init( void )
{
    maxrtos_arch_idle_init();
}

void maxrtos_arch_apply_resume_result( maxrtos_process_control_block_t * pcb )
{
    /* On the target this writes r0 of the saved exception frame; here the
     * value is parked with the context and returned by its system call. */
    if( ( pcb != NULL ) && pcb->ipc_result_pending )
    {
        sim_ctx_t * ctx = ctx_for_pcb( pcb );

        if( ctx != NULL )
        {
            ctx->result = ( uint32_t ) pcb->ipc_result;
            ctx->result_valid = true;
        }

        pcb->ipc_result_pending = false;
    }
}

void maxrtos_arch_fault_handlers_init( void )
{
    /* Nothing to enable: faults are raised by the simulator. */
}

maxrtos_process_control_block_t * sim_current_process( void )
{
    return g_maxrtos_current_pcb;
}

static bool current_is_privileged( void )
{
    maxrtos_process_control_block_t const * pcb = g_maxrtos_current_pcb;

    return ( pcb == NULL ) || !pcb->unprivileged;
}

/* PendSV, first half: choose and publish the next context, exactly as the
 * handler does before it restores it. Returns the context to run, or NULL
 * if the running context continues. Does not hand over the CPU. */
static sim_ctx_t * pendsv_select( void )
{
    maxrtos_process_control_block_t * next;
    maxrtos_process_control_block_t * previous;

    if( !s_pendsv_pending )
    {
        return NULL;
    }

    s_pendsv_pending = false;
    next = g_maxrtos_next_pcb;

    if( next == NULL )
    {
        return NULL;
    }

    previous = g_maxrtos_current_pcb;

    if( maxrtos_arch_mpu_configure_for_next_pcb( next ) != MAXRTOS_OK )
    {
        record_halt( "MPU configuration for the next context failed" );
        sim_port_halt();
    }

    maxrtos_arch_apply_resume_result( next );
    g_maxrtos_current_pcb = next;

    if( next == previous )
    {
        return NULL;
    }

    s_switches++;

    return ctx_for_pcb( next );
}

static void log_current( uint32_t tick );

/* The return from an exception on the running context: run the PendSV it
 * left pending. If `tick_boundary`, this is the end of a SysTick and the
 * observation log and the run limit apply.
 *
 * Returns when the calling context is running again. */
static void exception_return( bool tick_boundary )
{
    sim_ctx_t * self = s_running;
    sim_ctx_t * to = pendsv_select();

    if( tick_boundary )
    {
        log_current( s_ticks );

        if( s_ticks >= s_stop_tick )
        {
            /* Hand the CPU to the bench. sim_run() later resumes whichever
             * context is now current, which is `to` if a switch was made. */
            s_running = &s_bench;
            ( void ) pthread_cond_signal( &s_bench.wake );
            ctx_wait( self );
            return;
        }
    }

    if( to != NULL )
    {
        baton_transfer( to, self );
    }
}

/* ------------------------------------------------------------------------ */
/* Time, SysTick and exceptions                                              */
/* ------------------------------------------------------------------------ */

static void log_current( uint32_t tick )
{
    if( tick < SIM_MAX_TICKS )
    {
        maxrtos_process_control_block_t const * pcb = g_maxrtos_current_pcb;

        if( ( pcb == NULL ) || ( pcb == maxrtos_arch_idle_pcb() ) )
        {
            s_partition_log[ tick ] = ( int16_t ) SIM_IDLE;
            s_process_log[ tick ] = ( int16_t ) SIM_IDLE;
        }
        else
        {
            s_partition_log[ tick ] = ( int16_t ) pcb->partition_id;
            s_process_log[ tick ] = ( int16_t ) pcb->id;
        }
    }
}

void sim_fault( maxrtos_fault_type_t type )
{
    maxrtos_arch_handle_fault( type );
    exception_return( false );
}

static void sim_systick( void )
{
    unsigned i;

    s_ticks++;
    maxrtos_arch_systick();

    for( i = 0U; i < SIM_MAX_FAULTS; i++ )
    {
        if( s_faults[ i ].used && ( s_faults[ i ].tick == s_ticks ) )
        {
            s_faults[ i ].used = false;
            maxrtos_arch_handle_fault( s_faults[ i ].type );
        }
    }

    exception_return( true );
}

void sim_cpu_work( uint32_t cycles )
{
    s_cycles += cycles;

    while( s_cycles >= s_next_tick_cycle )
    {
        s_next_tick_cycle += SIM_CYCLES_PER_TICK;
        sim_systick();
    }
}

void sim_port_wfi( void )
{
    /* Sleep until the next interrupt: time jumps to the next SysTick. */
    sim_cpu_work( ( uint32_t ) ( s_next_tick_cycle - s_cycles ) );
}

void sim_entry_spin( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        sim_cpu_work( SIM_CYCLES_PER_TICK / 10U );
    }
}

void sim_inject_fault_at_tick( uint32_t tick, maxrtos_fault_type_t type )
{
    unsigned i;

    for( i = 0U; i < SIM_MAX_FAULTS; i++ )
    {
        if( !s_faults[ i ].used )
        {
            s_faults[ i ].used = true;
            s_faults[ i ].tick = tick;
            s_faults[ i ].type = type;
            return;
        }
    }
}

uint32_t sim_tick( void )
{
    return s_ticks;
}

int32_t sim_partition_at_tick( uint32_t tick )
{
    return ( tick < SIM_MAX_TICKS ) ? s_partition_log[ tick ] : SIM_IDLE;
}

maxrtos_process_id_t sim_process_at_tick( uint32_t tick )
{
    int16_t id = ( tick < SIM_MAX_TICKS ) ? s_process_log[ tick ] : ( int16_t ) SIM_IDLE;

    return ( id == ( int16_t ) SIM_IDLE ) ? MAXRTOS_INVALID_PROCESS_ID
                                          : ( maxrtos_process_id_t ) id;
}

uint32_t sim_context_switches( void )
{
    return s_switches;
}

/* ------------------------------------------------------------------------ */
/* Memory access and system calls made by simulated code                     */
/* ------------------------------------------------------------------------ */

bool sim_write32( uint32_t address, uint32_t value )
{
    void * target;

    sim_cpu_work( 1U );

    if( !sim_mpu_access_ok( address, 4U, true, current_is_privileged() ) )
    {
        sim_fault( MAXRTOS_FAULT_MEMORY_ACCESS );
        return false;
    }

    target = sim_mem_ptr( address, 4U );

    if( target == NULL )
    {
        sim_fault( MAXRTOS_FAULT_BUS_ERROR );
        return false;
    }

    ( void ) memcpy( target, &value, sizeof( value ) );

    return true;
}

bool sim_read32( uint32_t address, uint32_t * out_value )
{
    void * source;

    sim_cpu_work( 1U );

    if( !sim_mpu_access_ok( address, 4U, false, current_is_privileged() ) )
    {
        sim_fault( MAXRTOS_FAULT_MEMORY_ACCESS );
        return false;
    }

    source = sim_mem_ptr( address, 4U );

    if( source == NULL )
    {
        sim_fault( MAXRTOS_FAULT_BUS_ERROR );
        return false;
    }

    ( void ) memcpy( out_value, source, sizeof( *out_value ) );

    return true;
}

/* Execute an SVC as the exception would: stack r0-r3, enter the production
 * dispatcher, then run the PendSV it may have requested. */
static uint32_t sim_svc( uint8_t number, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3 )
{
    uint32_t frame[ 8 ] = { 0U };
    sim_ctx_t * self = s_running;
    uint32_t result;

    frame[ 0 ] = r0;
    frame[ 1 ] = r1;
    frame[ 2 ] = r2;
    frame[ 3 ] = r3;

    sim_cpu_work( SIM_SVC_CYCLES );

    self->result_valid = false;
    maxrtos_arch_svc_dispatch( frame, number );
    result = frame[ 0 ];

    exception_return( false );

    /* A call that blocked returns whatever the kernel resolved it with. */
    self = ctx_for_pcb( g_maxrtos_current_pcb );

    if( ( self != NULL ) && self->result_valid )
    {
        result = self->result;
        self->result_valid = false;
    }

    return result;
}

void maxrtos_yield( void )
{
    ( void ) sim_svc( MAXRTOS_SVC_YIELD, 0U, 0U, 0U, 0U );
}

maxrtos_status_t sim_queue_send_raw( uint32_t port, uint32_t message, uint32_t size, uint32_t timeout )
{
    return ( maxrtos_status_t ) sim_svc( MAXRTOS_SVC_QUEUE_SEND, port, message, size, timeout );
}

maxrtos_status_t sim_queue_receive_raw( uint32_t port, uint32_t buffer, uint32_t size, uint32_t timeout )
{
    return ( maxrtos_status_t ) sim_svc( MAXRTOS_SVC_QUEUE_RECEIVE, port, buffer, size, timeout );
}

uint32_t sim_queue_count_raw( uint32_t port )
{
    return sim_svc( MAXRTOS_SVC_QUEUE_COUNT, port, 0U, 0U, 0U );
}

maxrtos_status_t maxrtos_queue_port_send( maxrtos_queue_port_t * port, void const * message, size_t message_size, maxrtos_tick_t timeout )
{
    return sim_queue_send_raw( sim_ptr_to_uaddr( port ), sim_ptr_to_uaddr( message ), ( uint32_t ) message_size, timeout );
}

maxrtos_status_t maxrtos_queue_port_receive( maxrtos_queue_port_t * port, void * out_message, size_t buffer_size, maxrtos_tick_t timeout )
{
    return sim_queue_receive_raw( sim_ptr_to_uaddr( port ), sim_ptr_to_uaddr( out_message ), ( uint32_t ) buffer_size, timeout );
}

size_t maxrtos_queue_port_count( maxrtos_queue_port_t const * port )
{
    return sim_queue_count_raw( sim_ptr_to_uaddr( port ) );
}

/* ------------------------------------------------------------------------ */
/* Scheduler start and run control                                           */
/* ------------------------------------------------------------------------ */

void maxrtos_arch_start_first_process( maxrtos_process_control_block_t * pcb )
{
    g_maxrtos_current_pcb = NULL;
    g_maxrtos_next_pcb = pcb;
    maxrtos_arch_request_context_switch();

    s_started = true;
    s_running = &s_bench;

    /* Tick 0 is the scheduler start: the first process is chosen for the
     * period that follows it. */
    s_partition_log[ 0 ] = ( int16_t ) pcb->partition_id;
    s_process_log[ 0 ] = ( int16_t ) pcb->id;

    exception_return( false );  /* the first switch; parks the bench */
    s_normal_return = true;     /* the run reached its stop tick */
}

void sim_reset( void )
{
    unsigned i;

    sim_shutdown();

    s_shutdown = false;
    s_pendsv_pending = false;
    s_cycles = 0U;
    s_next_tick_cycle = SIM_CYCLES_PER_TICK;
    s_ticks = 0U;
    s_stop_tick = 0U;
    s_switches = 0U;
    s_started = false;
    s_normal_return = false;
    s_halted = false;
    s_halt_reason = NULL;
    s_model_violations = 0U;
    s_bench_jump_valid = false;
    g_maxrtos_current_pcb = NULL;
    g_maxrtos_next_pcb = NULL;

    ( void ) memset( s_faults, 0, sizeof( s_faults ) );

    for( i = 0U; i < SIM_MAX_TICKS; i++ )
    {
        s_partition_log[ i ] = ( int16_t ) SIM_IDLE;
        s_process_log[ i ] = ( int16_t ) SIM_IDLE;
    }

    ( void ) memset( s_ctx, 0, sizeof( s_ctx ) );
    ( void ) memset( &s_bench, 0, sizeof( s_bench ) );
    ( void ) pthread_cond_init( &s_bench.wake, NULL );

    sim_mpu_reset();

    s_bench_thread = pthread_self();

    if( !s_lock_held )
    {
        ( void ) pthread_mutex_lock( &s_lock );
        s_lock_held = true;
    }

    s_running = &s_bench;

    maxrtos_process_pool_init();
    maxrtos_kernel_tick_reset();
}

void sim_shutdown( void )
{
    unsigned i;

    if( s_lock_held )
    {
        s_shutdown = true;

        for( i = 0U; i < SIM_MAX_CTX; i++ )
        {
            if( s_ctx[ i ].created )
            {
                ( void ) pthread_cond_signal( &s_ctx[ i ].wake );
            }
        }

        ( void ) pthread_mutex_unlock( &s_lock );
        s_lock_held = false;

        for( i = 0U; i < SIM_MAX_CTX; i++ )
        {
            if( s_ctx[ i ].created )
            {
                ( void ) pthread_join( s_ctx[ i ].thread, NULL );
                s_ctx[ i ].created = false;
            }

            free( s_ctx[ i ].host_stack );
            s_ctx[ i ].host_stack = NULL;
        }
    }

    for( i = 0U; i < s_map_count; i++ )
    {
        free( s_maps[ i ].backing );
    }

    s_map_count = 0U;
}

static void run_bench_until_stop( void )
{
    sim_ctx_t * target = ctx_for_pcb( g_maxrtos_current_pcb );

    if( ( target != NULL ) && !s_halted )
    {
        baton_transfer( target, &s_bench );
    }
}

bool sim_start( uint32_t ticks )
{
    s_stop_tick = ticks;
    s_bench_jump_valid = true;

    if( setjmp( s_bench_jump ) == 0 )
    {
        maxrtos_scheduler_start();
    }

    s_bench_jump_valid = false;

    return !s_halted && ( s_ticks >= ticks );
}

bool sim_run( uint32_t ticks )
{
    s_stop_tick = s_ticks + ticks;

    if( s_started && !s_halted )
    {
        run_bench_until_stop();
    }

    return !s_halted && ( s_ticks >= s_stop_tick );
}
