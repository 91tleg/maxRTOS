/**
 * @file test_waitlist.c
 * @brief Unit tests for generic FIFO wait list management.
 *
 * Verifies input validation, FIFO insertion and removal, full and empty
 * list behavior, arbitrary process removal, duplicate process IDs, and
 * wait list count reporting.
 */

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "maxrtos/kernel/waitlist.h"

static void test_init_rejects_null( void )
{
    maxrtos_waitlist_t list;

    assert(
        maxrtos_waitlist_init(
            NULL ) == MAXRTOS_ERR_INVALID_ARG );

    assert(
        maxrtos_waitlist_init(
            &list ) == MAXRTOS_OK );

    assert( list.count == 0U );

    printf( "test_init_rejects_null: PASS\n" );
}

static void test_init_clears_existing_list( void )
{
    maxrtos_waitlist_t list;

    list.count = 2U;
    list.items[ 0 ] = 10U;
    list.items[ 1 ] = 20U;

    assert(
        maxrtos_waitlist_init(
            &list ) == MAXRTOS_OK );

    assert( list.count == 0U );

    printf( "test_init_clears_existing_list: PASS\n" );
}

static void test_push_rejects_null( void )
{
    assert(
        maxrtos_waitlist_push(
            NULL,
            1U ) == MAXRTOS_ERR_INVALID_ARG );

    printf( "test_push_rejects_null: PASS\n" );
}

static void test_push_adds_process_ids_in_fifo_order( void )
{
    maxrtos_waitlist_t list;

    assert(
        maxrtos_waitlist_init(
            &list ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_push(
            &list,
            10U ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_push(
            &list,
            20U ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_push(
            &list,
            30U ) == MAXRTOS_OK );

    assert( list.count == 3U );
    assert( list.items[ 0 ] == 10U );
    assert( list.items[ 1 ] == 20U );
    assert( list.items[ 2 ] == 30U );

    printf( "test_push_adds_process_ids_in_fifo_order: PASS\n" );
}

static void test_push_rejects_full_list( void )
{
    maxrtos_waitlist_t list;
    size_t i;

    assert(
        maxrtos_waitlist_init(
            &list ) == MAXRTOS_OK );

    for( i = 0U; i < MAXRTOS_WAITLIST_MAX; i++ )
    {
        assert(
            maxrtos_waitlist_push(
                &list,
                (maxrtos_process_id_t)i ) == MAXRTOS_OK );
    }

    assert( list.count == MAXRTOS_WAITLIST_MAX );

    assert(
        maxrtos_waitlist_push(
            &list,
            (maxrtos_process_id_t)MAXRTOS_WAITLIST_MAX ) ==
        MAXRTOS_ERR_POOL_FULL );

    assert( list.count == MAXRTOS_WAITLIST_MAX );

    printf( "test_push_rejects_full_list: PASS\n" );
}

static void test_pop_front_rejects_null_arguments( void )
{
    maxrtos_waitlist_t list;
    maxrtos_process_id_t id;

    assert(
        maxrtos_waitlist_init(
            &list ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_pop_front(
            NULL,
            &id ) == MAXRTOS_ERR_INVALID_ARG );

    assert(
        maxrtos_waitlist_pop_front(
            &list,
            NULL ) == MAXRTOS_ERR_INVALID_ARG );

    printf( "test_pop_front_rejects_null_arguments: PASS\n" );
}

static void test_pop_front_rejects_empty_list( void )
{
    maxrtos_waitlist_t list;
    maxrtos_process_id_t id;

    assert(
        maxrtos_waitlist_init(
            &list ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_pop_front(
            &list,
            &id ) == MAXRTOS_ERR_QUEUE_EMPTY );

    assert( list.count == 0U );

    printf( "test_pop_front_rejects_empty_list: PASS\n" );
}

static void test_pop_front_preserves_fifo_order( void )
{
    maxrtos_waitlist_t list;
    maxrtos_process_id_t id;

    assert(
        maxrtos_waitlist_init(
            &list ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_push(
            &list,
            10U ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_push(
            &list,
            20U ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_push(
            &list,
            30U ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_pop_front(
            &list,
            &id ) == MAXRTOS_OK );

    assert( id == 10U );
    assert( list.count == 2U );

    assert(
        maxrtos_waitlist_pop_front(
            &list,
            &id ) == MAXRTOS_OK );

    assert( id == 20U );
    assert( list.count == 1U );

    assert(
        maxrtos_waitlist_pop_front(
            &list,
            &id ) == MAXRTOS_OK );

    assert( id == 30U );
    assert( list.count == 0U );

    printf( "test_pop_front_preserves_fifo_order: PASS\n" );
}

static void test_pop_front_shifts_remaining_items( void )
{
    maxrtos_waitlist_t list;
    maxrtos_process_id_t id;

    assert(
        maxrtos_waitlist_init(
            &list ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_push(
            &list,
            10U ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_push(
            &list,
            20U ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_push(
            &list,
            30U ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_pop_front(
            &list,
            &id ) == MAXRTOS_OK );

    assert( id == 10U );
    assert( list.count == 2U );
    assert( list.items[ 0 ] == 20U );
    assert( list.items[ 1 ] == 30U );

    printf( "test_pop_front_shifts_remaining_items: PASS\n" );
}

static void test_remove_rejects_null( void )
{
    assert(
        maxrtos_waitlist_remove(
            NULL,
            1U ) == false );

    printf( "test_remove_rejects_null: PASS\n" );
}

static void test_remove_returns_false_when_id_not_present( void )
{
    maxrtos_waitlist_t list;

    assert(
        maxrtos_waitlist_init(
            &list ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_push(
            &list,
            10U ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_push(
            &list,
            20U ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_remove(
            &list,
            30U ) == false );

    assert( list.count == 2U );
    assert( list.items[ 0 ] == 10U );
    assert( list.items[ 1 ] == 20U );

    printf(
        "test_remove_returns_false_when_id_not_present: PASS\n" );
}

static void test_remove_front( void )
{
    maxrtos_waitlist_t list;

    assert(
        maxrtos_waitlist_init(
            &list ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_push(
            &list,
            10U ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_push(
            &list,
            20U ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_push(
            &list,
            30U ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_remove(
            &list,
            10U ) == true );

    assert( list.count == 2U );
    assert( list.items[ 0 ] == 20U );
    assert( list.items[ 1 ] == 30U );

    printf( "test_remove_front: PASS\n" );
}

static void test_remove_middle( void )
{
    maxrtos_waitlist_t list;

    assert(
        maxrtos_waitlist_init(
            &list ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_push(
            &list,
            10U ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_push(
            &list,
            20U ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_push(
            &list,
            30U ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_push(
            &list,
            40U ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_remove(
            &list,
            30U ) == true );

    assert( list.count == 3U );
    assert( list.items[ 0 ] == 10U );
    assert( list.items[ 1 ] == 20U );
    assert( list.items[ 2 ] == 40U );

    printf( "test_remove_middle: PASS\n" );
}

static void test_remove_back( void )
{
    maxrtos_waitlist_t list;

    assert(
        maxrtos_waitlist_init(
            &list ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_push(
            &list,
            10U ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_push(
            &list,
            20U ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_push(
            &list,
            30U ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_remove(
            &list,
            30U ) == true );

    assert( list.count == 2U );
    assert( list.items[ 0 ] == 10U );
    assert( list.items[ 1 ] == 20U );

    printf( "test_remove_back: PASS\n" );
}

static void test_remove_duplicate_id_removes_first_occurrence( void )
{
    maxrtos_waitlist_t list;

    assert(
        maxrtos_waitlist_init(
            &list ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_push(
            &list,
            10U ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_push(
            &list,
            20U ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_push(
            &list,
            10U ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_remove(
            &list,
            10U ) == true );

    /* Only the first matching entry is removed. */
    assert( list.count == 2U );
    assert( list.items[ 0 ] == 20U );
    assert( list.items[ 1 ] == 10U );

    printf(
        "test_remove_duplicate_id_removes_first_occurrence: PASS\n" );
}

static void test_remove_only_item( void )
{
    maxrtos_waitlist_t list;

    assert(
        maxrtos_waitlist_init(
            &list ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_push(
            &list,
            42U ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_remove(
            &list,
            42U ) == true );

    assert( list.count == 0U );

    printf( "test_remove_only_item: PASS\n" );
}

static void test_count_rejects_null( void )
{
    assert(
        maxrtos_waitlist_count(
            NULL ) == 0U );

    printf( "test_count_rejects_null: PASS\n" );
}

static void test_count_tracks_insertions_and_removals( void )
{
    maxrtos_waitlist_t list;

    assert(
        maxrtos_waitlist_init(
            &list ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_count(
            &list ) == 0U );

    assert(
        maxrtos_waitlist_push(
            &list,
            10U ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_count(
            &list ) == 1U );

    assert(
        maxrtos_waitlist_push(
            &list,
            20U ) == MAXRTOS_OK );

    assert(
        maxrtos_waitlist_count(
            &list ) == 2U );

    assert(
        maxrtos_waitlist_remove(
            &list,
            10U ) == true );

    assert(
        maxrtos_waitlist_count(
            &list ) == 1U );

    printf( "test_count_tracks_insertions_and_removals: PASS\n" );
}

int main( void )
{
    test_init_rejects_null();
    test_init_clears_existing_list();
    test_push_rejects_null();
    test_push_adds_process_ids_in_fifo_order();
    test_push_rejects_full_list();
    test_pop_front_rejects_null_arguments();
    test_pop_front_rejects_empty_list();
    test_pop_front_preserves_fifo_order();
    test_pop_front_shifts_remaining_items();
    test_remove_rejects_null();
    test_remove_returns_false_when_id_not_present();
    test_remove_front();
    test_remove_middle();
    test_remove_back();
    test_remove_duplicate_id_removes_first_occurrence();
    test_remove_only_item();
    test_count_rejects_null();
    test_count_tracks_insertions_and_removals();

    printf( "all waitlist tests passed\n" );

    return 0;
}
