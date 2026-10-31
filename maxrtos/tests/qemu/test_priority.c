/**
 * @file test_priority.c
 * @brief Process priorities on the real SVC, SysTick and PendSV paths.
 *
 * The control partition holds a high-priority receiver and a low-priority
 * sender. A message wake-up must preempt the sender before its next
 * statement: right after each send returns, the receiver must already have
 * taken the message.
 *
 * The application partition holds a high-priority sleeper and a
 * low-priority process that never blocks or yields. The sleeper's timed wait
 * expires on a tick and must preempt the busy process every time, while the
 * busy process still gets the CPU in between.
 */

#include "harness.h"
#include "maxrtos/queue_port.h"
#include "maxrtos/timing.h"

enum
{
    C_SENT,
    C_RECEIVED,
    C_NOT_PREEMPTED,
    C_WAKES,
    C_BUSY
};

#define QA ( PORT_ID_qa )

#define MIN_RECEIVED ( 20U )
#define MIN_WAKES    ( 5U )
#define MIN_BUSY     ( 100U )

/* Priority 2: runs whenever it has a message. */
static void control_main( void * arg )
{
    uint32_t value;

    ( void ) arg;

    for( ;; )
    {
        if( maxrtos_queue_port_receive(
                QA, &value, sizeof( value ), MAXRTOS_TIMEOUT_INFINITE ) ==
            MAXRTOS_OK )
        {
            EMU_RES[ C_RECEIVED ]++;
        }
    }
}

/* Priority 5: never blocks, so it only stops when the receiver preempts it. */
static void control_aux( void * arg )
{
    uint32_t value;

    ( void ) arg;
    value = 0U;

    for( ;; )
    {
        EMU_RES[ C_SENT ]++;
        ( void ) maxrtos_queue_port_send( QA, &value, sizeof( value ), 0U );

        if( EMU_RES[ C_RECEIVED ] != EMU_RES[ C_SENT ] )
        {
            EMU_RES[ C_NOT_PREEMPTED ]++;
        }

        value++;
    }
}

/* Priority 2: wakes on a tick. */
static void application_main( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        ( void ) maxrtos_timed_wait( 3U );
        EMU_RES[ C_WAKES ]++;
    }
}

/* Priority 5: never blocks or yields. */
static void application_aux( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        EMU_RES[ C_BUSY ]++;
    }
}

void emu_test_setup( void )
{
    EMU_CREATE( control_main, PARTITION_ID_control, control_main );
    EMU_CREATE( control_aux, PARTITION_ID_control, control_aux );
    EMU_CREATE( application_main, PARTITION_ID_application, application_main );
    EMU_CREATE( application_aux, PARTITION_ID_application, application_aux );
}

int emu_test_poll( uint32_t tick )
{
    ( void ) tick;

    if( EMU_RES[ C_NOT_PREEMPTED ] != 0U )
    {
        return EMU_FAILED(
            "a message wake-up did not preempt the lower-priority sender" );
    }

    if( ( EMU_RES[ C_RECEIVED ] >= MIN_RECEIVED ) &&
        ( EMU_RES[ C_WAKES ] >= MIN_WAKES ) &&
        ( EMU_RES[ C_BUSY ] >= MIN_BUSY ) )
    {
        return EMU_PASS;
    }

    return EMU_RUNNING;
}
