/**
 * @file status.h
 * @brief Common MaxRTOS operation status codes.
 *
 * Defines the status type returned by MaxRTOS APIs.
 *
 * Functions returning maxrtos_status_t shall return MAXRTOS_OK when
 * the requested operation completes successfully. An error status
 * shall be returned when the requested operation cannot be completed.
 */

#ifndef MAXRTOS_STATUS_H
#define MAXRTOS_STATUS_H

typedef enum
{
    MAXRTOS_OK = 0U,

    /* Argument and precondition errors. */
    MAXRTOS_ERR_INVALID_ARG,
    MAXRTOS_ERR_INVALID_ID,
    MAXRTOS_ERR_INVALID_STATE,

    /* Resource availability errors. */
    MAXRTOS_ERR_POOL_FULL,
    MAXRTOS_ERR_QUEUE_FULL,
    MAXRTOS_ERR_QUEUE_EMPTY,

    /* Arithmetic errors */
    MAXRTOS_ERR_OVERFLOW,

    /* Partition state errors */
    MAXRTOS_ERR_PARTITION_HALTED,

    /* Generic operation errors. */
    MAXRTOS_ERR_NOT_FOUND,
} maxrtos_status_t;

#endif /* MAXRTOS_STATUS_H */
