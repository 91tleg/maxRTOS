/**
 * @file ipc_operation.h
 * @brief IPC operation continuation data.
 *
 * Defines the operation kind and operation-specific continuation data
 * required to resume a blocked IPC operation.
 */

#ifndef MAXRTOS_KERNEL_IPC_OPERATION_H
#define MAXRTOS_KERNEL_IPC_OPERATION_H

#include <stddef.h>

#include "maxrtos/status.h"

typedef enum
{
    MAXRTOS_IPC_OP_NONE = 0,
    MAXRTOS_IPC_OP_QUEUE_SEND,
    MAXRTOS_IPC_OP_QUEUE_RECEIVE,
} maxrtos_ipc_operation_kind_t;

/**
 * @brief Continuation data for one blocked IPC operation.
 *
 * The operation kind identifies the active payload member. Each IPC
 * operation has a dedicated payload containing only the data required
 * to resume that operation.
 */
typedef struct
{
    maxrtos_ipc_operation_kind_t kind;

    union
    {
        struct
        {
            struct maxrtos_queue_port_s * port;
            void const * message;
            size_t message_size;
        } queue_send;

        struct
        {
            struct maxrtos_queue_port_s * port;
            void * out_message;
            size_t buffer_size;
        } queue_receive;

        /* Future IPC operation payloads shall be added as distinct types. */
    } payload;
} maxrtos_ipc_operation_t;

#endif /* MAXRTOS_KERNEL_IPC_OPERATION_H */
