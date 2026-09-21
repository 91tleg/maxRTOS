/**
 * @file sil_test.h
 * @brief Minimal xUnit-style framework for the SIL suites.
 *
 * Unlike assert(), the checks here are never compiled out by NDEBUG, so a
 * release build cannot silently pass. Each test case carries the ID of the
 * requirement it verifies; the runner prints a result line per case, writes
 * a JUnit XML report (--junit <file>) whose test cases carry a `requirement`
 * property, and exits non-zero on any failure. That report feeds the
 * requirements traceability check (tools/traceability.py).
 *
 * Checks must be made on the test (bench) thread. Simulated processes
 * record what they observe in shared variables and the bench judges them.
 */

#ifndef MAXRTOS_SIL_TEST_H
#define MAXRTOS_SIL_TEST_H

#include <stddef.h>
#include <stdint.h>

typedef struct
{
    char const * name;
    char const * requirement; /* e.g. "REQ-TP-001" */
    char const * title;
    void ( * run )( void );
} sil_case_t;

#define SIL_CASE( function_, requirement_, title_ ) \
    { #function_, ( requirement_ ), ( title_ ), ( function_ ) }

/**
 * Run all cases of a suite. Recognised arguments:
 *   --junit <file>   write a JUnit XML report
 *   --case <name>    run only the named case
 * @return process exit status: 0 if every case passed.
 */
int sil_run_suite(
    char const * suite,
    sil_case_t const * cases,
    size_t count,
    int argc,
    char ** argv );

/* Implementation hooks used by the macros below. */
void sil_check_failed( char const * file, int line, char const * message, int fatal );
void sil_check_failed_u( char const * file, int line, char const * expression,
                         uint64_t actual, uint64_t expected, int fatal );

/** Abort the case if the condition is false. */
#define SIL_REQUIRE( cond_ )                                                  \
    do { if( !( cond_ ) ) { sil_check_failed( __FILE__, __LINE__, #cond_, 1 ); } } while( 0 )

/** Record a failure but keep going. */
#define SIL_EXPECT( cond_ )                                                   \
    do { if( !( cond_ ) ) { sil_check_failed( __FILE__, __LINE__, #cond_, 0 ); } } while( 0 )

/** Equality of unsigned values, reporting both on failure. */
#define SIL_REQUIRE_EQ( actual_, expected_ )                                  \
    do {                                                                      \
        uint64_t a_ = ( uint64_t ) ( actual_ );                               \
        uint64_t e_ = ( uint64_t ) ( expected_ );                             \
        if( a_ != e_ ) { sil_check_failed_u( __FILE__, __LINE__,              \
            #actual_ " == " #expected_, a_, e_, 1 ); }                        \
    } while( 0 )

#define SIL_EXPECT_EQ( actual_, expected_ )                                   \
    do {                                                                      \
        uint64_t a_ = ( uint64_t ) ( actual_ );                               \
        uint64_t e_ = ( uint64_t ) ( expected_ );                             \
        if( a_ != e_ ) { sil_check_failed_u( __FILE__, __LINE__,              \
            #actual_ " == " #expected_, a_, e_, 0 ); }                        \
    } while( 0 )

#endif /* MAXRTOS_SIL_TEST_H */
