/**
 * @file waitlist.h
 * @brief Fixed-capacity FIFO wait list for blocked processes.
 *
 * Provides a caller-owned wait list for blocking kernel primitives.
 * The wait list stores process identifiers in FIFO order and supports
 * insertion, removal from the front, removal by process identifier,
 * and entry count retrieval.
 *
 * The wait list does not modify process state, interact with the
 * scheduler, or perform context switching. The caller is responsible
 * for all process-state and scheduler operations associated with
 * blocking and unblocking.
 */

#ifndef MAXRTOS_KERNEL_WAITLIST_H
#define MAXRTOS_KERNEL_WAITLIST_H

#include <stdbool.h>
#include <stddef.h>

#include "maxrtos/status.h"
#include "maxrtos/config.h"
#include "maxrtos/types.h"

/**
 * @brief Fixed-capacity FIFO wait list.
 *
 * The storage is owned by the caller and is embedded in the kernel
 * object that requires a wait list. The list shall be initialized
 * with maxrtos_waitlist_init() before use.
 */
typedef struct maxrtos_waitlist_s
{
    maxrtos_process_id_t items[ MAXRTOS_WAITLIST_MAX ];
    size_t count;
} maxrtos_waitlist_t;

/**
 * @brief Initialize a wait list to an empty state.
 *
 * @param[out] list
 *     Wait list to initialize. Shall not be NULL.
 *
 * @return
 *     MAXRTOS_OK if the wait list was initialized.
 *     MAXRTOS_ERR_INVALID_ARG if list is NULL.
 */
maxrtos_status_t maxrtos_waitlist_init(
    maxrtos_waitlist_t * list );

/**
 * @brief Append a process identifier to the wait list.
 *
 * The process is added after all existing entries.
 *
 * @param[in,out] list
 *     Wait list to modify. Shall not be NULL.
 *
 * @param[in] id
 *     Process identifier to add.
 *
 * @return
 *     MAXRTOS_OK if the process was added.
 *     MAXRTOS_ERR_INVALID_ARG if list is NULL.
 *     MAXRTOS_ERR_POOL_FULL if the wait list is at capacity.
 */
maxrtos_status_t maxrtos_waitlist_push(
    maxrtos_waitlist_t * list,
    maxrtos_process_id_t id );

/**
 * @brief Remove and return the first process identifier.
 *
 * The first entry is the process that has been waiting the longest.
 * The relative order of all remaining entries is preserved.
 *
 * @param[in,out] list
 *     Wait list to modify. Shall not be NULL.
 *
 * @param[out] out_id
 *     Location receiving the removed process identifier. Shall not
 *     be NULL.
 *
 * @return
 *     MAXRTOS_OK if an entry was removed.
 *     MAXRTOS_ERR_INVALID_ARG if list or out_id is NULL.
 *     MAXRTOS_ERR_QUEUE_EMPTY if the wait list is empty.
 */
maxrtos_status_t maxrtos_waitlist_pop_front(
    maxrtos_waitlist_t * list,
    maxrtos_process_id_t * out_id );

/**
 * @brief Remove the first occurrence of a process identifier.
 *
 * The relative order of all remaining entries is preserved.
 *
 * @param[in,out] list
 *     Wait list to modify. Shall not be NULL.
 *
 * @param[in] id
 *     Process identifier to remove.
 *
 * @return
 *     true if the process identifier was found and removed.
 *     false if list is NULL or the process identifier was not found.
 */
bool maxrtos_waitlist_remove(
    maxrtos_waitlist_t * list,
    maxrtos_process_id_t id );

/**
 * @brief Return the current number of entries in a wait list.
 *
 * @param[in] list
 *     Wait list to query. If NULL, zero is returned.
 *
 * @return
 *     Number of process identifiers currently stored in the list.
 */
size_t maxrtos_waitlist_count(
    maxrtos_waitlist_t const * list );

#endif /* MAXRTOS_KERNEL_WAITLIST_H */
