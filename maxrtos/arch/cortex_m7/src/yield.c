/**
 * @file yield.c
 * @brief Cortex-M7 implementation of the public yield interface.
 */

#include <stdbool.h>
#include <stddef.h>

#include "maxrtos/yield.h"
#include "maxrtos/arch/cortex_m7/svc.h"

void maxrtos_yield( void )
{
    __asm volatile (
        "svc %0"
        :
        : "i" ( MAXRTOS_SVC_YIELD )
        : "memory" );
}
