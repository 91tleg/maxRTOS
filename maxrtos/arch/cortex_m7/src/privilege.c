/**
 * @file privilege.c
 * @brief Privilege level of partitions.
 *
 * Hardware independent. The privilege of a process is the privilege of its
 * partition; the context switch reads it here to program CONTROL.nPRIV.
 */

#include <stdbool.h>
#include <stddef.h>

#include "maxrtos/arch/cortex_m7/context_switch.h"
#include "maxrtos/config.h"

/* Partitions are unprivileged unless the configuration says otherwise. */
static bool s_partition_privileged[ MAXRTOS_MAX_PARTITIONS ];

void maxrtos_arch_set_partition_privileged(
    maxrtos_partition_id_t partition_id,
    bool privileged )
{
    if( partition_id < MAXRTOS_MAX_PARTITIONS )
    {
        s_partition_privileged[ partition_id ] = privileged;
    }
}

bool maxrtos_arch_partition_is_privileged(
    maxrtos_partition_id_t partition_id )
{
    bool privileged;

    if( partition_id == MAXRTOS_INVALID_PARTITION_ID )
    {
        privileged = true; /* the architecture idle context */
    }
    else if( partition_id < MAXRTOS_MAX_PARTITIONS )
    {
        privileged = s_partition_privileged[ partition_id ];
    }
    else
    {
        privileged = false;
    }

    return privileged;
}
