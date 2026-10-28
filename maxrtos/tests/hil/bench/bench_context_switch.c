/**
 * @file bench_context_switch.c
 * @brief Context-switch latency benchmark for the NUCLEO-H753ZI.
 *
 * Timestamps come from the Cortex-M7 DWT cycle counter, so results are
 * exact core cycles. All partitions are "system" partitions (privileged)
 * because unprivileged code cannot read the DWT.
 *
 * Five latencies are measured, each as min / mean / max over many samples:
 *
 *   YIELD          maxrtos_yield() in one process -> first instruction of
 *                  its partition peer (SVC + kernel + PendSV + restore).
 *   TICK_ROTATE    SysTick entry -> first instruction of the process that
 *                  runs next in the SAME partition.
 *   TICK_SWITCH    SysTick entry -> first instruction of the first process
 *                  of a DIFFERENT partition (adds the MPU domain switch).
 *   QUEUE_HANDOFF  "t0; send; yield" -> the woken receiver returns from
 *                  receive. Includes the send and the yield.
 *   MUTEX_HANDOFF  "t0; unlock; yield" -> the woken waiter returns from
 *                  lock. Includes the unlock and the yield.
 *
 * Samples that span a tick are discarded (except the tick latencies). The
 * timestamp is taken in software, so SysTick exception entry (about 12
 * cycles) is not included in the TICK_* figures, and each figure contains
 * one CYCCNT read (see `overhead`).
 *
 * Results are left in g_bench (all 32-bit words) for run_bench.py.
 */

#include <stdint.h>
#include <stddef.h>

#include "maxrtos_config.h"
#include "maxrtos/mutex.h"
#include "maxrtos/queue_port.h"
#include "maxrtos/scheduler.h"
#include "maxrtos/yield.h"
#include "maxrtos/kernel/mutex.h"
#include "maxrtos/kernel/process.h"
#include "maxrtos/arch/cortex_m7/systick.h"

#define REG32( addr_ ) ( *( volatile uint32_t * ) ( addr_ ) )

#define DEMCR      REG32( 0xE000EDFCUL )
#define DWT_CTRL   REG32( 0xE0001000UL )
#define DWT_CYCCNT REG32( 0xE0001004UL )
#define DWT_LAR    REG32( 0xE0001FB0UL )

#define SYSTICK_CTRL REG32( 0xE000E010UL )
#define SYSTICK_LOAD REG32( 0xE000E014UL )
#define SYSTICK_VAL  REG32( 0xE000E018UL )

#define SCB_CCR     REG32( 0xE000ED14UL )
#define SCB_CSSELR  REG32( 0xE000ED84UL )
#define SCB_CCSIDR  REG32( 0xE000ED80UL )
#define SCB_ICIALLU REG32( 0xE000EF50UL )
#define SCB_DCISW   REG32( 0xE000EF60UL )

/* Core cycles per tick: the sampling rate. Each tick is lengthened by a
 * pseudo-random 0 to TICK_JITTER_MASK cycles. The processes loop with a fixed
 * period, so with a fixed tick period the ticks would always land on the same
 * few phases of the loop and the TICK_* means would depend on the loop length
 * rather than on the kernel. */
#define TICK_CYCLES      ( 100000UL )
#define TICK_JITTER_MASK ( 0x1FFUL )

#define BENCH_MAGIC ( 0xB3AC0001UL )
/* Latencies above this are counted separately as outliers. */
#define OUTLIER_CYCLES ( 30000UL )

#define NOT_SEEN ( 0xFFFFFFFFUL )

enum
{
    STAT_YIELD,
    STAT_TICK_ROTATE,
    STAT_TICK_SWITCH,
    STAT_QUEUE_HANDOFF,
    STAT_MUTEX_HANDOFF,
    STAT_COUNT
};

/* Samples wanted per statistic before the run is finished. */
static const uint32_t s_target[ STAT_COUNT ] = {
    20000U, 1000U, 1000U, 2000U, 2000U };

typedef struct
{
    uint32_t count;
    uint32_t min;
    uint32_t max;
    uint32_t sum_lo;
    uint32_t sum_hi;
    uint32_t max_tag; /* partition << 24 | tick, for the tick latencies */
    uint32_t over;    /* samples above OUTLIER_CYCLES */
} bench_stat_t;

typedef struct
{
    uint32_t magic;
    uint32_t done;
    uint32_t caches;   /* 1 if the caches were enabled */
    uint32_t overhead; /* cycles for one CYCCNT read pair */
    bench_stat_t stat[ STAT_COUNT ];
} bench_results_t;

volatile bench_results_t g_bench;

/* Tick bookkeeping, written only by SysTick_Handler and read with
 * interrupts masked. */
static volatile uint32_t s_tick;
static volatile uint32_t s_tick_entry;

/* Last tick each partition observed, to tell a partition switch from a
 * rotation inside the running partition. */
static volatile uint32_t s_last_seen[ 3 ] = { NOT_SEEN, NOT_SEEN, NOT_SEEN };

/* xorshift32; the seed is fixed, so runs repeat. */
static uint32_t next_jitter( void )
{
    static uint32_t state = 0x2545F491UL;

    state ^= state << 13U;
    state ^= state >> 17U;
    state ^= state << 5U;

    return state & TICK_JITTER_MASK;
}

static inline uint32_t cycles( void )
{
    return DWT_CYCCNT;
}

static inline void irq_off( void )
{
    __asm volatile( "cpsid i" ::: "memory" );
}

static inline void irq_on( void )
{
    __asm volatile( "cpsie i" ::: "memory" );
}

/* Caller has interrupts masked: several processes update the same
 * statistic. */
static void record_locked(
    uint32_t which,
    uint32_t value,
    uint32_t tag )
{
    volatile bench_stat_t * s;
    uint64_t sum;

    s = &g_bench.stat[ which ];

    if( s->count == 0U )
    {
        s->min = value;
        s->max = value;
        s->max_tag = tag;
    }
    else
    {
        if( value < s->min )
        {
            s->min = value;
        }
        if( value > s->max )
        {
            s->max = value;
            s->max_tag = tag;
        }
    }

    sum = ( ( uint64_t ) s->sum_hi << 32U ) | s->sum_lo;
    sum += value;
    s->sum_lo = ( uint32_t ) sum;
    s->sum_hi = ( uint32_t ) ( sum >> 32U );
    if( value > OUTLIER_CYCLES )
    {
        s->over++;
    }
    s->count++;
}

static void record(
    uint32_t which,
    uint32_t value,
    uint32_t tag )
{
    irq_off();
    record_locked( which, value, tag );
    irq_on();
}

static uint32_t enough( uint32_t which )
{
    return g_bench.stat[ which ].count >= s_target[ which ];
}

/* A sample is trusted only if the end timestamp is not before the start
 * one. It can be if a tick preempted the reader between reading the clock
 * and the peer's stored start time, and another process re-armed it. */
static uint32_t plausible(
    uint32_t now,
    uint32_t t0 )
{
    return ( ( int32_t ) ( now - t0 ) >= 0 ) ? 1U : 0U;
}

static void check_done( void )
{
    uint32_t i;

    for( i = 0U; i < STAT_COUNT; i++ )
    {
        if( enough( i ) == 0U )
        {
            return;
        }
    }

    g_bench.done = 1U;
}

/* Called by processes of partition `part` at the top of their loops. The
 * first call after a new tick measures SysTick entry -> here.
 *
 * Runs with interrupts masked. The tick number, its entry time, the clock
 * read and the partition's last-seen tick must all belong to one tick. If a
 * tick could land between reading them, the peer process would observe the
 * next tick meanwhile, and this call would then record a stale sample
 * (a full tick long) and move s_last_seen backwards. */
static void observe_tick( uint32_t part )
{
    uint32_t tick;
    uint32_t entry;
    uint32_t now;
    uint32_t seen;

    irq_off();

    tick = s_tick;
    entry = s_tick_entry;
    now = cycles();
    seen = s_last_seen[ part ];

    if( tick != seen )
    {
        /* The mutex group (partition 2) is not sampled: its timed window
         * (stamp, unlock, yield) cannot hold an observation, so a tick that
         * preempts it is followed by kernel calls before the next one. */
        if( ( seen != NOT_SEEN ) && ( part != 2U ) )
        {
            if( ( seen + 1U ) == tick )
            {
                record_locked(
                    STAT_TICK_ROTATE,
                    now - entry,
                    ( part << 24U ) | ( tick & 0xFFFFFFU ) );
            }
            else
            {
                record_locked(
                    STAT_TICK_SWITCH,
                    now - entry,
                    ( part << 24U ) | ( tick & 0xFFFFFFU ) );
            }
        }

        s_last_seen[ part ] = tick;

        /* Set the length of the next tick. This runs after the sample above
         * is taken, so it does not add to the latency measured. */
        SYSTICK_LOAD = ( TICK_CYCLES - 1U ) + next_jitter();
    }

    irq_on();
}

void SysTick_Handler( void )
{
    uint32_t c;

    c = cycles();

    s_tick_entry = c;
    s_tick++;

    maxrtos_arch_systick();
}

/* ---- YIELD: two processes ping-pong on maxrtos_yield() ---------------- */

static volatile uint32_t s_y_t0;
static volatile uint32_t s_y_tick;
static volatile uint32_t s_y_owner = NOT_SEEN;

static void yield_loop( uint32_t me )
{
    for( ;; )
    {
        uint32_t now;
        uint32_t t0;

        now = cycles();
        t0 = s_y_t0;

        if( ( s_y_owner != me ) && ( s_y_owner != NOT_SEEN ) &&
            ( s_y_tick == s_tick ) && ( enough( STAT_YIELD ) == 0U ) &&
            ( plausible( now, t0 ) != 0U ) )
        {
            record( STAT_YIELD, now - t0, 0U );
        }

        observe_tick( 0U );
        check_done();

        /* A process the tick preempted resumes here or just before; observe
         * the tick before the call below, so the sample does not include it. */
        observe_tick( 0U );

        s_y_tick = s_tick;
        s_y_owner = me;
        s_y_t0 = cycles();
        maxrtos_yield();
    }
}

static void yield_a( void * arg )
{
    ( void ) arg;
    yield_loop( 0U );
}
static void yield_b( void * arg )
{
    ( void ) arg;
    yield_loop( 1U );
}

/* ---- QUEUE_HANDOFF: send, yield, receiver resumes --------------------- */

static volatile uint32_t s_q_t0;
static volatile uint32_t s_q_tick;
static volatile uint32_t s_q_valid;

static void queue_tx( void * arg )
{
    uint32_t message;

    ( void ) arg;
    message = 0U;

    for( ;; )
    {
        observe_tick( 1U );

        if( enough( STAT_QUEUE_HANDOFF ) == 0U )
        {
            s_q_tick = s_tick;
            s_q_valid = 1U;
            s_q_t0 = cycles();
        }

        ( void ) maxrtos_queue_port_send(
            ( maxrtos_queue_port_t * ) ( void * ) maxrtos_port_bench_q,
            &message,
            sizeof( message ),
            0U );
        message++;

        maxrtos_yield();
    }
}

static void queue_rx( void * arg )
{
    uint32_t message;

    ( void ) arg;

    for( ;; )
    {
        uint32_t now;

        observe_tick( 1U );

        if( maxrtos_queue_port_receive(
                ( maxrtos_queue_port_t * ) ( void * ) maxrtos_port_bench_q,
                &message,
                sizeof( message ),
                MAXRTOS_TIMEOUT_INFINITE ) == MAXRTOS_OK )
        {
            uint32_t t0;

            now = cycles();
            t0 = s_q_t0;

            if( ( s_q_valid != 0U ) && ( s_q_tick == s_tick ) &&
                ( plausible( now, t0 ) != 0U ) )
            {
                record( STAT_QUEUE_HANDOFF, now - t0, 0U );
            }

            s_q_valid = 0U;
        }

        observe_tick( 1U );
    }
}

/* ---- MUTEX_HANDOFF: unlock, yield, waiter resumes owning the lock ----- */

#define BENCH_MUTEX ( 0U )

static volatile uint32_t s_m_t0;
static volatile uint32_t s_m_tick;
static volatile uint32_t s_m_valid;

static void mutex_a( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        observe_tick( 2U );
        ( void ) maxrtos_mutex_lock( BENCH_MUTEX, MAXRTOS_TIMEOUT_INFINITE );

        /* Let b run: it blocks on the lock and hands the CPU back. */
        observe_tick( 2U );
        maxrtos_yield();

        observe_tick( 2U );

        if( enough( STAT_MUTEX_HANDOFF ) == 0U )
        {
            s_m_tick = s_tick;
            s_m_valid = 1U;
            s_m_t0 = cycles();
        }

        ( void ) maxrtos_mutex_unlock( BENCH_MUTEX );
        maxrtos_yield();
    }
}

static void mutex_b( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        uint32_t now;

        observe_tick( 2U );

        if( maxrtos_mutex_lock( BENCH_MUTEX, MAXRTOS_TIMEOUT_INFINITE ) ==
            MAXRTOS_OK )
        {
            uint32_t t0;

            now = cycles();
            t0 = s_m_t0;

            if( ( s_m_valid != 0U ) && ( s_m_tick == s_tick ) &&
                ( plausible( now, t0 ) != 0U ) )
            {
                record( STAT_MUTEX_HANDOFF, now - t0, 0U );
            }

            s_m_valid = 0U;
            ( void ) maxrtos_mutex_unlock( BENCH_MUTEX );
        }

        observe_tick( 2U );
        maxrtos_yield();
    }
}

#ifdef BENCH_480MHZ
/* STM32H753 at 480 MHz: LDO supply, VOS0, HSE bypass (ST-LINK MCO, 8 MHz),
 * PLL1 = 8 / 4 * 480 / 2, AHB and APB /2 as the 240 MHz limit requires,
 * flash at 4 wait states. Every wait is bounded; a timeout halts on bkpt. */
#define PWR_CR3       REG32( 0x5802480CUL )
#define PWR_D3CR      REG32( 0x58024818UL )
#define RCC_CR        REG32( 0x58024400UL )
#define RCC_CFGR      REG32( 0x58024410UL )
#define RCC_D1CFGR    REG32( 0x58024418UL )
#define RCC_D2CFGR    REG32( 0x5802441CUL )
#define RCC_D3CFGR    REG32( 0x58024420UL )
#define RCC_PLLCKSELR REG32( 0x58024428UL )
#define RCC_PLLCFGR   REG32( 0x5802442CUL )
#define RCC_PLL1DIVR  REG32( 0x58024430UL )
#define RCC_APB4ENR   REG32( 0x580244F4UL )
#define SYSCFG_PWRCR  REG32( 0x5800042CUL )
#define FLASH_ACR     REG32( 0x52002000UL )

static void wait_for(
    volatile uint32_t * reg,
    uint32_t mask,
    uint32_t value )
{
    uint32_t n = 2000000UL;

    while( ( ( *reg & mask ) != value ) && ( --n != 0U ) )
    {
    }

    if( n == 0U )
    {
        for( ;; )
        {
            __asm volatile( "bkpt #1" );
        }
    }
}

static void clock_480mhz( void )
{
    PWR_CR3 = ( PWR_CR3 & ~0x5UL ) | 0x2UL;        /* LDOEN, no SMPS */
    wait_for( &PWR_D3CR, 1UL << 13U, 1UL << 13U ); /* VOSRDY */

    PWR_D3CR |= 3UL << 14U; /* VOS1 */
    wait_for( &PWR_D3CR, 1UL << 13U, 1UL << 13U );
    RCC_APB4ENR |= 1UL << 1U; /* SYSCFG */
    ( void ) RCC_APB4ENR;
    SYSCFG_PWRCR |= 1UL; /* ODEN: VOS0 */
    wait_for( &PWR_D3CR, 1UL << 13U, 1UL << 13U );

    RCC_CR |= ( 1UL << 18U ) | ( 1UL << 16U ); /* HSEBYP, HSEON */
    wait_for( &RCC_CR, 1UL << 17U, 1UL << 17U );

    RCC_PLLCKSELR = ( 4UL << 4U ) | 2UL;             /* DIVM1=4, HSE */
    RCC_PLLCFGR = ( 7UL << 16U ) | ( 1UL << 2U );    /* P,Q,R on; 2-4 MHz */
    RCC_PLL1DIVR = ( 1UL << 24U ) | ( 1UL << 16U ) | /* R=2, Q=2 */
                   ( 1UL << 9U ) | 479UL;            /* P=2, N=480 */

    FLASH_ACR = 0x24UL; /* 4 WS, WRHIGHFREQ=2 */
    wait_for( &FLASH_ACR, 0xFUL, 4UL );

    RCC_D1CFGR = 0x48UL;  /* CPRE /1, HPRE /2, D1PPRE /2 */
    RCC_D2CFGR = 0x440UL; /* D2PPRE1/2 /2 */
    RCC_D3CFGR = 0x40UL;  /* D3PPRE /2 */

    RCC_CR |= 1UL << 24U; /* PLL1ON */
    wait_for( &RCC_CR, 1UL << 25U, 1UL << 25U );

    RCC_CFGR = ( RCC_CFGR & ~7UL ) | 3UL; /* SW = PLL1 */
    wait_for( &RCC_CFGR, 7UL << 3U, 3UL << 3U );
}
#endif

/* ---- setup ------------------------------------------------------------ */

static void enable_cycle_counter( void )
{
    DEMCR |= ( 1UL << 24U ); /* TRCENA */
    DWT_LAR = 0xC5ACCE55UL;  /* unlock; required on Cortex-M7 */
    DWT_CYCCNT = 0U;
    DWT_CTRL |= 1UL; /* CYCCNTENA */
}

#ifdef BENCH_CACHES
static void enable_caches( void )
{
    uint32_t ccsidr;
    uint32_t sets;
    uint32_t ways;
    uint32_t way_shift;

    SCB_ICIALLU = 0U;
    __asm volatile( "dsb\n isb" ::: "memory" );
    SCB_CCR |= ( 1UL << 17U ); /* IC */

    SCB_CSSELR = 0U;
    __asm volatile( "dsb" ::: "memory" );
    ccsidr = SCB_CCSIDR;
    sets = ( ccsidr >> 13U ) & 0x7FFFUL;
    way_shift =
        ( uint32_t ) __builtin_clz( ( ccsidr >> 3U ) & 0x3FFUL ) & 0x1FU;

    do
    {
        ways = ( ccsidr >> 3U ) & 0x3FFUL;

        do
        {
            SCB_DCISW = ( sets << 5U ) | ( ways << way_shift );
        } while( ways-- != 0U );
    } while( sets-- != 0U );

    __asm volatile( "dsb" ::: "memory" );
    SCB_CCR |= ( 1UL << 16U ); /* DC */
    __asm volatile( "dsb\n isb" ::: "memory" );
}
#endif

static void fail( void )
{
    for( ;; )
    {
        __asm volatile( "bkpt #0" );
    }
}

static void create(
    maxrtos_process_id_t * id,
    uint8_t * stack,
    size_t stack_size,
    maxrtos_partition_id_t partition,
    uint8_t priority,
    void ( *entry )( void * ) )
{
    if( maxrtos_process_create(
            stack, stack_size, partition, priority, entry, NULL, id ) !=
        MAXRTOS_OK )
    {
        fail();
    }
}

int main( void )
{
    maxrtos_process_id_t id;
    maxrtos_mutex_id_t mutex;
    uint32_t t0;
    uint32_t t1;

#ifdef BENCH_480MHZ
    clock_480mhz();
#endif

    enable_cycle_counter();

#ifdef BENCH_CACHES
    enable_caches();
    g_bench.caches = 1U;
#endif

    /* Cost of the measurement itself: two back-to-back reads. */
    t0 = cycles();
    t1 = cycles();
    g_bench.overhead = t1 - t0;
    g_bench.magic = BENCH_MAGIC;

    maxrtos_config_init();

    if( ( maxrtos_mutex_create( PARTITION_ID_mutexp, &mutex ) != MAXRTOS_OK ) ||
        ( mutex != BENCH_MUTEX ) )
    {
        fail();
    }

#define CREATE( name_, part_, entry_ )                                         \
    create(                                                                    \
        &id,                                                                   \
        maxrtos_stack_##name_,                                                 \
        PROCESS_STACK_SIZE_##name_,                                            \
        PARTITION_ID_##part_,                                                  \
        PROCESS_PRIORITY_##name_,                                              \
        entry_ )

    CREATE( yield_a, yieldp, yield_a );
    CREATE( yield_b, yieldp, yield_b );
    CREATE( queue_tx, queuep, queue_tx );
    CREATE( queue_rx, queuep, queue_rx );
    CREATE( mutex_a, mutexp, mutex_a );
    CREATE( mutex_b, mutexp, mutex_b );

#undef CREATE

    SYSTICK_LOAD = TICK_CYCLES - 1U;
    SYSTICK_VAL = 0U;
    SYSTICK_CTRL = 7U; /* enable, interrupt, core clock */

    maxrtos_scheduler_start();

    fail();
    return 0;
}
