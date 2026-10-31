/**
 * @file harness.h
 * @brief Self-checking firmware harness for QEMU emulation tests.
 *
 * Each test is one firmware image. It creates its processes in
 * emu_test_setup() and is judged by emu_test_poll(), which the harness
 * calls from the SysTick handler in privileged handler mode after every
 * kernel tick. The verdict is reported through ARM semihosting, so the
 * QEMU process exit status is the test result and no debugger is needed.
 *
 * Processes are unprivileged and can only write their own partition's
 * memory, so they record results in a block of RAM (EMU_RES) that the test
 * module maps read-write for every partition with a background MPU region
 * (number 5 in module*.json, over EMU_RESULTS_BASE). The privileged poll
 * function reads it. It is test scaffolding: a real module has no such
 * region, and IPC ports are never memory a partition can reach.
 */

#ifndef MAXRTOS_EMU_HARNESS_H
#define MAXRTOS_EMU_HARNESS_H

#include <stdint.h>

#include "maxrtos_config.h"
#include "maxrtos/queue_port.h"
#include "maxrtos/scheduler.h"
#include "maxrtos/yield.h"

#define EMU_RUNNING ( 0 )
#define EMU_PASS    ( 1 )
#define EMU_FAIL    ( -1 )

/* Give up (fail) if the test has not finished after this many ticks. */
#define EMU_TICK_LIMIT ( 1000UL )

/* The result block: 2 KiB at EMU_RESULTS_BASE, matching the module's MPU
 * region. The HIL build moves it to the board's SRAM. */
#ifndef EMU_RESULTS_BASE
#define EMU_RESULTS_BASE ( 0x200C0000UL )
#endif
#define EMU_RESULTS_SIZE ( 2048U )

/* Result slots. */
#define EMU_RES ( ( volatile uint32_t * ) EMU_RESULTS_BASE )

/* A word of kernel (DTCM) memory that unprivileged code may not write. It is
 * a variable of its own, so no live kernel object shares it. */
extern volatile uint32_t g_emu_kernel_word;
#define EMU_KERNEL_ADDRESS ( &g_emu_kernel_word )

/** Create the test's processes. Called after maxrtos_config_init(). */
void emu_test_setup( void );

/**
 * Judge the test. Called once per tick from the SysTick handler.
 *
 * @return EMU_RUNNING to keep going, EMU_PASS or EMU_FAIL to finish. On
 *         EMU_FAIL set emu_fail_reason.
 */
int emu_test_poll( uint32_t tick );

extern char const * emu_fail_reason;

/** End the test now with a verdict (EMU_PASS or EMU_FAIL). Does not return.
 *  For code that runs where the per-tick poll cannot, such as a fatal hook. */
void emu_finish( int verdict );

/** Fail with a message: use as `return EMU_FAILED( "why" );`. */
#define EMU_FAILED( why_ ) ( emu_fail_reason = ( why_ ), EMU_FAIL )

/** Create a process in a partition with the generated stack and priority. */
#define EMU_CREATE( proc_, partition_, entry_ )                                \
    do                                                                         \
    {                                                                          \
        maxrtos_process_id_t id_;                                              \
        if( maxrtos_process_create(                                            \
                maxrtos_stack_##proc_,                                         \
                PROCESS_STACK_SIZE_##proc_,                                    \
                ( partition_ ),                                                \
                PROCESS_PRIORITY_##proc_,                                      \
                ( entry_ ),                                                    \
                NULL,                                                          \
                &id_ ) != MAXRTOS_OK )                                         \
        {                                                                      \
            emu_fail_reason = "process create failed";                         \
            for( ;; )                                                          \
            {                                                                  \
            }                                                                  \
        }                                                                      \
    } while( 0 )

#endif /* MAXRTOS_EMU_HARNESS_H */
