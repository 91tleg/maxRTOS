/**
 * @file main.c
 * @brief Minimal Cortex-M7 hardware integration test.
 *
 * All MPU/partition-table/frame-schedule setup lives in maxrtos_config.h/.c
 * generated from tools/maxrtos_codegen.
 *
 * This file only does board bring-up and application logic.
 */

#include <stdint.h>
#include <stddef.h>

#include "generated/maxrtos_config.h"
#include "maxrtos/yield.h"
#include "maxrtos/scheduler.h"
#include "maxrtos/queue_port.h"

#define RCC_BASE            ( 0x58024400UL )
#define RCC_AHB4ENR         \
    ( *( volatile uint32_t * )( RCC_BASE + 0xE0UL ) )

#define RCC_AHB4ENR_GPIOBEN ( 1UL << 1U )
#define RCC_AHB4ENR_GPIOEEN ( 1UL << 4U )

#define GPIOB_BASE          ( 0x58020400UL )
#define GPIOB_MODER         \
    ( *( volatile uint32_t * )( GPIOB_BASE + 0x00UL ) )

#define GPIOB_ODR           \
    ( *( volatile uint32_t * )( GPIOB_BASE + 0x14UL ) )

#define GPIOE_BASE          ( 0x58021000UL )
#define GPIOE_MODER         \
    ( *( volatile uint32_t * )( GPIOE_BASE + 0x00UL ) )

#define GPIOE_ODR           \
    ( *( volatile uint32_t * )( GPIOE_BASE + 0x14UL ) )

#define LED_LD2_PIN         ( 1U )
#define LED_LD1_PIN         ( 0U )
#define LED_LD3_PIN         ( 14U )

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

static void board_systick_init( void )
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

static void board_leds_init( void )
{
    RCC_AHB4ENR |= RCC_AHB4ENR_GPIOBEN | RCC_AHB4ENR_GPIOEEN;

    GPIOB_MODER &= ~(
        ( 0x3UL << ( LED_LD1_PIN * 2U ) ) |
        ( 0x3UL << ( LED_LD3_PIN * 2U ) )
    );

    GPIOB_MODER |=
        ( 0x1UL << ( LED_LD1_PIN * 2U ) ) |
        ( 0x1UL << ( LED_LD3_PIN * 2U ) );

    GPIOE_MODER &= ~( 0x3UL << ( LED_LD2_PIN * 2U ) );
    GPIOE_MODER |= ( 0x1UL << ( LED_LD2_PIN * 2U ) );
}

static void led_ld1_toggle( void )
{
    GPIOB_ODR ^= ( 1UL << LED_LD1_PIN );
}

static void led_ld3_toggle( void )
{
    GPIOB_ODR ^= ( 1UL << LED_LD3_PIN );
}

static void led_ld2_toggle( void )
{
    GPIOE_ODR ^= ( 1UL << LED_LD2_PIN );
}

/* IPC is non-blocking (timeout 0). Blocking IPC across partitions is not
 * supported yet: a sender wakes a blocked receiver through its own
 * partition's scheduler context, not the receiver's. */
#define IPC_NO_WAIT ( 0U )

static maxrtos_queue_port_t * const s_cmd_channel = 
    ( maxrtos_queue_port_t * ) ( void * ) maxrtos_port_cmd_channel;

static void process_control_entry( void * arg )
{
    uint32_t counter;

    ( void ) arg;

    counter = 0U;

    for( ;; )
    {
        led_ld1_toggle();

        ( void ) maxrtos_queue_port_send(
            ( maxrtos_queue_port_t * ) ( void * ) s_cmd_channel,
            &counter,
            sizeof( counter ),
            IPC_NO_WAIT );
        counter++;

        for( volatile uint32_t i = 0U; i < 200000UL; i++ )
        {

        }

        maxrtos_yield();
    }
}

static void process_application_entry( void * arg )
{
    uint32_t received;

    ( void ) arg;

    for( ;; )
    {
        led_ld3_toggle();

        if( maxrtos_queue_port_receive(
                ( maxrtos_queue_port_t * ) ( void * ) s_cmd_channel,
                &received,
                sizeof( received ),
                IPC_NO_WAIT ) == MAXRTOS_OK )
        {
            
        }

        for( volatile uint32_t i = 0U; i < 200000UL; i++ )
        {

        }

        maxrtos_yield();
    }
}

/* Same priority as the partition's main process, so maxrtos_yield() in either
 * hands the CPU to the other. Yield goes only to another READY process in the
 * caller's own partition.
 *
 * These processes are unprivileged, so they may only touch what the MPU maps
 * for them: their partition's domain, their ports, and the peripheral region.
 * Kernel globals (DTCM) are privileged-only, so progress is shown on LD2
 * (shared by both aux processes) and not through a counter variable. */
static void process_control_aux_entry( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        led_ld2_toggle();

        for( volatile uint32_t i = 0U; i < 200000UL; i++ )
        {

        }

        maxrtos_yield();
    }
}

static void process_application_aux_entry( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        led_ld2_toggle();

        for( volatile uint32_t i = 0U; i < 200000UL; i++ )
        {

        }

        maxrtos_yield();
    }
}

void SysTick_Handler( void )
{
    maxrtos_arch_systick();
}

int main( void )
{
    maxrtos_process_id_t id;

    board_leds_init();

    maxrtos_config_init();

    board_systick_init();

    if( maxrtos_process_create(
            maxrtos_stack_control_main,
            PROCESS_STACK_SIZE_control_main,
            PARTITION_ID_control,
            PROCESS_PRIORITY_control_main,
            true,
            process_control_entry,
            NULL,
            &id ) != MAXRTOS_OK )
    {
        for( ;; )
        {
            __asm volatile ( "bkpt #0" );
        }
    }

    if( maxrtos_process_create(
            maxrtos_stack_control_aux,
            PROCESS_STACK_SIZE_control_aux,
            PARTITION_ID_control,
            PROCESS_PRIORITY_control_aux,
            true,
            process_control_aux_entry,
            NULL,
            &id ) != MAXRTOS_OK )
    {
        for( ;; )
        {
            __asm volatile ( "bkpt #0" );
        }
    }

    if( maxrtos_process_create(
            maxrtos_stack_application_main,
            PROCESS_STACK_SIZE_application_main,
            PARTITION_ID_application,
            PROCESS_PRIORITY_application_main,
            true,
            process_application_entry,
            NULL,
            &id ) != MAXRTOS_OK )
    {
        for( ;; )
        {
            __asm volatile ( "bkpt #0" );
        }
    }

    if( maxrtos_process_create(
            maxrtos_stack_application_aux,
            PROCESS_STACK_SIZE_application_aux,
            PARTITION_ID_application,
            PROCESS_PRIORITY_application_aux,
            true,
            process_application_aux_entry,
            NULL,
            &id ) != MAXRTOS_OK )
    {
        for( ;; )
        {
            __asm volatile ( "bkpt #0" );
        }
    }

    maxrtos_scheduler_start();

    for( ;; )
    {
        __asm volatile ( "bkpt #0" );
    }
}
