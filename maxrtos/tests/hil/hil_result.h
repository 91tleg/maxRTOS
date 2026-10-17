/**
 * @file hil_result.h
 * @brief Verdict block shared between the HIL firmware and runner.py.
 *
 * The harness keeps one hil_result_t, g_hil_result, in RAM. runner.py reads it
 * over SWD while the target runs, so the layout must match HIL_RESULT_FORMAT
 * in runner.py. The reason text is copied into the block so that the runner
 * does not have to chase a pointer into flash.
 */

#ifndef MAXRTOS_HIL_RESULT_H
#define MAXRTOS_HIL_RESULT_H

#include <stdint.h>

#define HIL_MAGIC ( 0x48494C31UL ) /* "HIL1" */

#define HIL_STATE_RUNNING ( 0UL )
#define HIL_STATE_PASS    ( 1UL )
#define HIL_STATE_FAIL    ( 2UL )

#define HIL_REASON_SIZE ( 64U )

typedef struct
{
    uint32_t magic;   /* HIL_MAGIC once the harness is up. */
    uint32_t state;   /* HIL_STATE_*; written last, after reason. */
    uint32_t ticks;   /* Kernel ticks the harness has seen. */
    uint32_t tick_hz; /* Tick rate the harness programmed into SysTick. */
    char reason[ HIL_REASON_SIZE ]; /* Failure text, NUL-terminated. */
} hil_result_t;

extern volatile hil_result_t g_hil_result;

#endif /* MAXRTOS_HIL_RESULT_H */
