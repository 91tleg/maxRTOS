/**
 * @file waitlist.c
 * @brief Implementation of the generic FIFO wait list.
 */

#include <stdbool.h>
#include <stddef.h>

#include "maxrtos/kernel/waitlist.h"

maxrtos_status_t maxrtos_waitlist_init(
    maxrtos_waitlist_t * list )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( list != NULL )
    {
        list->count = 0U;
        status = MAXRTOS_OK;
    }

    return status;
}

maxrtos_status_t maxrtos_waitlist_push(
    maxrtos_waitlist_t * list,
    maxrtos_process_id_t id )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( list != NULL )
    {
        if( list->count >= MAXRTOS_WAITLIST_MAX )
        {
            status = MAXRTOS_ERR_POOL_FULL;
        }
        else
        {
            list->items[ list->count ] = id;
            list->count++;
            status = MAXRTOS_OK;
        }
    }

    return status;
}

maxrtos_status_t maxrtos_waitlist_pop_front(
    maxrtos_waitlist_t * list,
    maxrtos_process_id_t * out_id )
{
    maxrtos_status_t status;

    status = MAXRTOS_ERR_INVALID_ARG;

    if( ( list != NULL ) && ( out_id != NULL ) )
    {
        if( list->count == 0U )
        {
            status = MAXRTOS_ERR_QUEUE_EMPTY;
        }
        else
        {
            size_t i;

            *out_id = list->items[ 0 ];

            for( i = 1U; i < list->count; i++ )
            {
                list->items[ i - 1U ] = list->items[ i ];
            }

            list->count--;
            status = MAXRTOS_OK;
        }
    }

    return status;
}

bool maxrtos_waitlist_remove(
    maxrtos_waitlist_t * list,
    maxrtos_process_id_t id )
{
    bool removed;

    removed = false;

    if( list != NULL )
    {
        size_t scan;
        size_t found_index;
        bool found;

        found = false;
        found_index = 0U;

        for( scan = 0U;
             ( scan < list->count ) && ( found == false );
             scan++ )
        {
            if( list->items[ scan ] == id )
            {
                found = true;
                found_index = scan;
            }
        }

        if( found == true )
        {
            for( scan = found_index;
                 ( scan + 1U ) < list->count;
                 scan++ )
            {
                list->items[ scan ] = list->items[ scan + 1U ];
            }

            list->count--;
            removed = true;
        }
    }

    return removed;
}

size_t maxrtos_waitlist_count(
    maxrtos_waitlist_t const * list )
{
    size_t count;

    count = 0U;

    if( list != NULL )
    {
        count = list->count;
    }

    return count;
}
