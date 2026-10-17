/**
 * @file hil_stages.h
 * @brief One-fault-per-stage injection protocol for HIL fault tests.
 *
 * A fault test is a table of stages. In each stage one unprivileged "injector"
 * process provokes exactly one hardware fault, the health monitor restarts it,
 * and the privileged poll function compares what the hardware recorded in
 * g_maxrtos_arch_last_fault with what the stage expects, then moves on.
 *
 * The injector must not repeat a stage after it is restarted, or the fault
 * count could not be tied to a stage. The protocol uses EMU_RES words:
 *
 *   HS_STAGE    stage in progress; only the poll function advances it.
 *   HS_ATTEMPT  stage + 1 once an injector has started that stage. After the
 *               restart the injector sees it is set and does not inject
 *               again. A restart is therefore itself evidence that the
 *               process was restarted after the fault.
 *   HS_ESCAPED  stage + 1 if an injection returned: the hardware did not stop
 *               it, which is a failure.
 *
 * Test-specific words start at HS_FIRST_FREE.
 */

#ifndef MAXRTOS_HIL_STAGES_H
#define MAXRTOS_HIL_STAGES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hil.h"

enum
{
    HS_STAGE,
    HS_ATTEMPT,
    HS_ESCAPED,
    HS_ENTRIES_CONTROL, /* times the control injector (re)started */
    HS_ENTRIES_APPLICATION,
    HS_BEAT_CONTROL, /* bystander progress counters */
    HS_BEAT_APPLICATION,
    HS_FIRST_FREE
};

/* After the last stage the system runs this long (three major frames of the
 * test configuration) and every bystander must have made progress. */
#define HS_SETTLE_TICKS ( 24U )
#define HS_MIN_PROGRESS ( 5U )

typedef struct
{
    uint32_t cfsr;  /* CFSR the fault must leave, exactly */
    uint32_t mmfar; /* MMFAR it must report (0 if the address is not valid) */
    uint32_t bfar;  /* BFAR it must report (0 if the address is not valid) */
} hs_expect_t;

/** Injector side: true if this call should inject the stage. */
static inline bool hs_claim( uint32_t stage )
{
    bool claimed = false;

    if( EMU_RES[ HS_ATTEMPT ] != ( stage + 1U ) )
    {
        EMU_RES[ HS_ATTEMPT ] = stage + 1U;
        claimed = true;
    }

    return claimed;
}

/** Injector side: the injection returned without faulting. */
static inline void hs_escaped( uint32_t stage )
{
    EMU_RES[ HS_ESCAPED ] = stage + 1U;
}

/** Fail text such as "s3 cfsr": the stage and the quantity that was wrong. */
static inline char const * hs_label(
    uint32_t stage,
    char const * what )
{
    static char text[ 32 ];
    size_t n = 0U;
    size_t i;

    text[ n++ ] = 's';

    if( stage >= 10U )
    {
        text[ n++ ] = ( char ) ( '0' + ( stage / 10U ) );
    }

    text[ n++ ] = ( char ) ( '0' + ( stage % 10U ) );
    text[ n++ ] = ' ';

    for( i = 0U; ( what[ i ] != '\0' ) && ( n < ( sizeof( text ) - 1U ) ); i++ )
    {
        text[ n++ ] = what[ i ];
    }

    text[ n ] = '\0';

    return text;
}

#define HS_RUNNING ( 0 )
#define HS_DONE    ( 1 )

/**
 * Poll side: judge the stage in progress.
 *
 * @param verify  Optional. Called once per stage after the fault matched;
 *                returns EMU_RUNNING if the stage's side effects are right,
 *                or the result of a failing check.
 * @return HS_RUNNING, HS_DONE when every stage has passed, or EMU_FAIL.
 */
static inline int hs_poll(
    hs_expect_t const * expect,
    uint32_t stages,
    int ( *verify )( uint32_t stage ) )
{
    static uint32_t s_seen = 0U;
    uint32_t stage = EMU_RES[ HS_STAGE ];
    uint32_t count;
    int result = HS_RUNNING;

    if( EMU_RES[ HS_ESCAPED ] != 0U )
    {
        result = hil_fail_values(
            "access not stopped, stage", EMU_RES[ HS_ESCAPED ] - 1U, 0U );
    }
    else if( stage >= stages )
    {
        result = HS_DONE;
    }
    else
    {
        count = g_maxrtos_arch_last_fault.count;

        if( count == ( s_seen + 1U ) )
        {
            hs_expect_t const * e = &expect[ stage ];

            if( EMU_RES[ HS_ATTEMPT ] != ( stage + 1U ) )
            {
                result = hil_fail_values( "fault outside a stage", stage, 0U );
            }
            else if( g_maxrtos_arch_last_fault.cfsr != e->cfsr )
            {
                result = hil_fail_values(
                    hs_label( stage, "cfsr" ),
                    g_maxrtos_arch_last_fault.cfsr,
                    e->cfsr );
            }
            else if( g_maxrtos_arch_last_fault.hfsr != 0U )
            {
                result = hil_fail_values(
                    hs_label( stage, "hfsr" ),
                    g_maxrtos_arch_last_fault.hfsr,
                    0U );
            }
            else if( g_maxrtos_arch_last_fault.mmfar != e->mmfar )
            {
                result = hil_fail_values(
                    hs_label( stage, "mmfar" ),
                    g_maxrtos_arch_last_fault.mmfar,
                    e->mmfar );
            }
            else if( g_maxrtos_arch_last_fault.bfar != e->bfar )
            {
                result = hil_fail_values(
                    hs_label( stage, "bfar" ),
                    g_maxrtos_arch_last_fault.bfar,
                    e->bfar );
            }
            else if(
                ( verify != NULL ) &&
                ( ( result = verify( stage ) ) != EMU_RUNNING ) )
            {
                /* verify() reported the failure. */
            }
            else
            {
                s_seen = count;
                EMU_RES[ HS_STAGE ] = stage + 1U;
                result = HS_RUNNING;
            }
        }
        else if( count != s_seen )
        {
            result = hil_fail_values( "extra faults", count - s_seen, 1U );
        }
    }

    return result;
}

/**
 * Closing checks, once every stage has passed: each injector was restarted
 * after its faults (the restart after the last one may still be pending), and
 * after the last fault the bystanders ran on. Returns EMU_RUNNING while the
 * settle window is open, then EMU_PASS or a failure.
 */
static inline int hs_check_containment(
    uint32_t tick,
    uint32_t control_faults,
    uint32_t application_faults )
{
    static uint32_t s_window_start = 0U;
    static uint32_t s_control_beats;
    static uint32_t s_application_beats;
    int result = EMU_RUNNING;

    if( s_window_start == 0U )
    {
        s_window_start = tick;
        s_control_beats = EMU_RES[ HS_BEAT_CONTROL ];
        s_application_beats = EMU_RES[ HS_BEAT_APPLICATION ];
    }
    else if( ( tick - s_window_start ) >= HS_SETTLE_TICKS )
    {
        if( EMU_RES[ HS_ENTRIES_CONTROL ] < control_faults )
        {
            result = hil_fail_values(
                "control injector restarts",
                EMU_RES[ HS_ENTRIES_CONTROL ],
                control_faults );
        }
        else if( EMU_RES[ HS_ENTRIES_APPLICATION ] < application_faults )
        {
            result = hil_fail_values(
                "application injector restarts",
                EMU_RES[ HS_ENTRIES_APPLICATION ],
                application_faults );
        }
        else if(
            ( EMU_RES[ HS_BEAT_CONTROL ] - s_control_beats ) < HS_MIN_PROGRESS )
        {
            result = hil_fail_values(
                "control bystander stalled",
                EMU_RES[ HS_BEAT_CONTROL ] - s_control_beats,
                HS_MIN_PROGRESS );
        }
        else if(
            ( EMU_RES[ HS_BEAT_APPLICATION ] - s_application_beats ) <
            HS_MIN_PROGRESS )
        {
            result = hil_fail_values(
                "application bystander stalled",
                EMU_RES[ HS_BEAT_APPLICATION ] - s_application_beats,
                HS_MIN_PROGRESS );
        }
        else
        {
            result = EMU_PASS;
        }
    }
    else
    {
        /* settling */
    }

    return result;
}

#endif /* MAXRTOS_HIL_STAGES_H */
