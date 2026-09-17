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
#include "maxrtos/arch/cortex_m7/yield.h"
#include "maxrtos/kernel/queue_port.h"

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

static maxrtos_queue_port_t * const s_cmd_channel =
    ( maxrtos_queue_port_t * ) maxrtos_port_cmd_channel;

#define CMD_MESSAGE_SIZE  ( 4U )
#define CMD_QUEUE_CAPACITY ( 4U )

static void process_control_entry( void * arg )
{
    uint32_t counter;

    ( void ) arg;

    _Static_assert( sizeof( maxrtos_queue_port_t ) <= PORT_SIZE_cmd_channel,
                     "cmd_channel MPU region too small for maxrtos_queue_port_t" );

    if( maxrtos_queue_port_init( s_cmd_channel, CMD_MESSAGE_SIZE, CMD_QUEUE_CAPACITY ) != MAXRTOS_OK )
    {
        for( ;; )
        {
            __asm volatile ( "bkpt #0" );
        }
    }

    counter = 0U;

    for( ;; )
    {
        led_ld1_toggle();

        ( void ) maxrtos_queue_port_send( s_cmd_channel, &counter, sizeof( counter ) );
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

        if( maxrtos_queue_port_receive( s_cmd_channel, &received, sizeof( received ) ) == MAXRTOS_OK )
        {
            
        }

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
    maxrtos_process_id_t id_control;
    maxrtos_process_id_t id_application;

    board_leds_init();
    board_systick_init();

    maxrtos_config_init();

    if( maxrtos_process_create(
            maxrtos_stack_control,
            PARTITION_STACK_SIZE_control,
            PARTITION_ID_control,
            5U,
            true,
            process_control_entry,
            NULL,
            &id_control ) != MAXRTOS_OK )
    {
        for( ;; )
        {
            __asm volatile ( "bkpt #0" );
        }
    }

    if( maxrtos_process_create(
            maxrtos_stack_application,
            PARTITION_STACK_SIZE_application,
            PARTITION_ID_application,
            5U,
            true,
            process_application_entry,
            NULL,
            &id_application ) != MAXRTOS_OK )
    {
        for( ;; )
        {
            __asm volatile ( "bkpt #0" );
        }
    }

    maxrtos_config_start( id_control, id_application );

    for( ;; )
    {
        __asm volatile ( "bkpt #0" );
    }
}
