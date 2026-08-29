/**
 * @file main.c
 * @brief Minimal Cortex-M7 hardware integration test.
 * 
 * Demonstrates the end-to-end path:
 *
 *     two partitions
 *        |
 *        v
 *     one process per partition
 *        |
 *        v
 *     major-frame scheduling
 *        |
 *        v
 *     SysTick
 *        |
 *        v
 *     PendSV context switching
 *        |
 *        v
 *     two LED processes
 *
 * This test excludes MPU enforcement, health monitoring, fault recovery,
 * and inter-process communication.
 */

#include <stdint.h>
#include <stddef.h>

#include "maxrtos/kernel/process.h"
#include "maxrtos/kernel/partition.h"
#include "maxrtos/kernel/frame.h"
#include "maxrtos/kernel/tick.h"

#include "maxrtos/arch/cortex_m7/context_switch.h"

#define RCC_BASE            ( 0x58024400UL )
#define RCC_AHB4ENR         \
    ( *( volatile uint32_t * )( RCC_BASE + 0xE0UL ) )

#define RCC_AHB4ENR_GPIOBEN ( 1UL << 1U )

#define GPIOB_BASE          ( 0x58020400UL )
#define GPIOB_MODER         \
    ( *( volatile uint32_t * )( GPIOB_BASE + 0x00UL ) )

#define GPIOB_ODR           \
    ( *( volatile uint32_t * )( GPIOB_BASE + 0x14UL ) )

#define LED_LD1_PIN         ( 0U )
#define LED_LD3_PIN         ( 14U )

#define STACK_SIZE          ( 512U )

#define SYSTICK_HZ          ( 100U )
#define CPU_CLOCK_HZ        ( 200000000UL )

#define SYSTICK_BASE        ( 0xE000E010UL )
#define SYSTICK_CTRL        \
    ( *( volatile uint32_t * )( SYSTICK_BASE + 0x00UL ) )

#define SYSTICK_LOAD        \
    ( *( volatile uint32_t * )( SYSTICK_BASE + 0x04UL ) )

#define SYSTICK_VAL         \
    ( *( volatile uint32_t * )( SYSTICK_BASE + 0x08UL ) )

#define SYSTICK_CTRL_ENABLE     ( 1UL << 0U )
#define SYSTICK_CTRL_TICKINT    ( 1UL << 1U )
#define SYSTICK_CTRL_CLKSOURCE  ( 1UL << 2U )

static void systick_init( void )
{
    uint32_t reload;

    reload = ( uint32_t ) ( CPU_CLOCK_HZ / SYSTICK_HZ ) - 1U;

    SYSTICK_CTRL = 0U;
    SYSTICK_LOAD = reload;
    SYSTICK_VAL = 0U;

    SYSTICK_CTRL =
        SYSTICK_CTRL_ENABLE |
        SYSTICK_CTRL_TICKINT |
        SYSTICK_CTRL_CLKSOURCE;
}

static maxrtos_partition_table_t s_partition_table;
static maxrtos_frame_schedule_t s_frame_schedule;

static uint32_t s_tick_count = 0U;

static uint8_t s_stack_p0[ STACK_SIZE ]
    __attribute__( ( aligned( STACK_SIZE ) ) );

static uint8_t s_stack_p1[ STACK_SIZE ]
    __attribute__( ( aligned( STACK_SIZE ) ) );

static void board_leds_init( void )
{
    RCC_AHB4ENR |= RCC_AHB4ENR_GPIOBEN;

    GPIOB_MODER &= ~(
        ( 0x3UL << ( LED_LD1_PIN * 2U ) ) |
        ( 0x3UL << ( LED_LD3_PIN * 2U ) )
    );

    GPIOB_MODER |=
        ( 0x1UL << ( LED_LD1_PIN * 2U ) ) |
        ( 0x1UL << ( LED_LD3_PIN * 2U ) );
}

static void led_ld1_toggle( void )
{
    GPIOB_ODR ^= ( 1UL << LED_LD1_PIN );
}

static void led_ld3_toggle( void )
{
    GPIOB_ODR ^= ( 1UL << LED_LD3_PIN );
}

static void process_p0_entry( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        led_ld1_toggle();

        for( volatile uint32_t i = 0U; i < 200000UL; i++ )
        {

        }
    }
}

static void process_p1_entry( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        led_ld3_toggle();

        for( volatile uint32_t i = 0U; i < 200000UL; i++ )
        {

        }
    }
}

void SysTick_Handler( void )
{
    maxrtos_process_id_t next_id;
    maxrtos_process_control_block_t * next_pcb;
    maxrtos_process_control_block_t * current_pcb;

    s_tick_count++;

    if( maxrtos_kernel_on_tick(
            &s_frame_schedule,
            &s_partition_table,
            s_tick_count,
            &next_id ) == MAXRTOS_OK )
    {
        next_pcb = maxrtos_process_get( next_id );
        current_pcb = maxrtos_arch_get_current_pcb();

        if( ( next_pcb != NULL ) && ( next_pcb != current_pcb ) )
        {
            maxrtos_arch_set_current_pcb( current_pcb );
            maxrtos_arch_set_next_pcb( next_pcb );
            maxrtos_arch_request_context_switch();
        }
    }
}

int main( void )
{
    maxrtos_process_id_t id_p0;
    maxrtos_process_id_t id_p1;
    maxrtos_process_id_t first_id;

    maxrtos_process_control_block_t * pcb_p0;
    maxrtos_process_control_block_t * pcb_p1;
    maxrtos_process_control_block_t * first_pcb;

    maxrtos_frame_slot_t slots[ 2 ];

    board_leds_init();

    maxrtos_process_pool_init();

    if( maxrtos_partition_table_init(
            &s_partition_table ) != MAXRTOS_OK )
    {
        for( ;; )
        {
            __asm volatile ( "bkpt #0" );
        }
    }

    maxrtos_arch_context_switch_init();

    systick_init();

    if( maxrtos_process_create(
            s_stack_p0,
            STACK_SIZE,
            (maxrtos_partition_id_t) 0U,
            5U,
            process_p0_entry,
            NULL,
            &id_p0 ) != MAXRTOS_OK )
    {
        for( ;; )
        {
            __asm volatile ( "bkpt #0" );
        }
    }

    if( maxrtos_process_create(
            s_stack_p1,
            STACK_SIZE,
            (maxrtos_partition_id_t) 1U,
            5U,
            process_p1_entry,
            NULL,
            &id_p1 ) != MAXRTOS_OK )
    {
        for( ;; )
        {
            __asm volatile ( "bkpt #0" );
        }
    }

    pcb_p0 = maxrtos_process_get( id_p0 );
    pcb_p1 = maxrtos_process_get( id_p1 );

    if( ( pcb_p0 == NULL ) ||
        ( pcb_p1 == NULL ) ||
        ( maxrtos_arch_init_stack( pcb_p0 ) != MAXRTOS_OK ) ||
        ( maxrtos_arch_init_stack( pcb_p1 ) != MAXRTOS_OK ) )
    {
        for( ;; )
        {
            __asm volatile ( "bkpt #0" );
        }
    }

    /* Register each process with its partition. */
    if( maxrtos_partition_add_process(
            &s_partition_table,
            id_p0 ) != MAXRTOS_OK )
    {
        for( ;; )
        {
            __asm volatile ( "bkpt #0" );
        }
    }

    if( maxrtos_partition_add_process(
            &s_partition_table,
            id_p1 ) != MAXRTOS_OK )
    {
        for( ;; )
        {
            __asm volatile ( "bkpt #0" );
        }
    }

    /*
     * Major frame:
     *     partition 0: 5 ticks
     *     partition 1: 3 ticks
     */
    slots[ 0 ].partition_id = 0U;
    slots[ 0 ].duration_ticks = 5U;

    slots[ 1 ].partition_id = 1U;
    slots[ 1 ].duration_ticks = 3U;

    if( maxrtos_frame_init(
            &s_frame_schedule,
            slots,
            2U ) != MAXRTOS_OK )
    {
        for( ;; )
        {
            __asm volatile ( "bkpt #0" );
        }
    }

    /* Select the first process using the same kernel dispatch
     * path used by subsequent timer ticks. */
    if( maxrtos_kernel_on_tick(
            &s_frame_schedule,
            &s_partition_table,
            0U,
            &first_id ) != MAXRTOS_OK )
    {
        for( ;; )
        {
            __asm volatile ( "bkpt #0" );
        }
    }

    first_pcb = maxrtos_process_get( first_id );

    if( first_pcb == NULL )
    {
        for( ;; )
        {
            __asm volatile ( "bkpt #0" );
        }
    }

    /* Never returns. */
    maxrtos_arch_start_first_process( first_pcb );

    for( ;; )
    {
        __asm volatile ( "bkpt #0" );
    }
}
