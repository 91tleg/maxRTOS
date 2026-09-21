/**
 * @file sil_test.c
 * @brief Runner, reporting and JUnit output for the SIL suites.
 */

#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "sil_test.h"

#define SIL_MAX_FAILURES ( 8U )
#define SIL_MESSAGE_SIZE ( 256U )

typedef struct
{
    int passed;
    double seconds;
    unsigned failure_count;
    char failures[ SIL_MAX_FAILURES ][ SIL_MESSAGE_SIZE ];
} sil_result_t;

static jmp_buf s_case_jump;
static sil_result_t * s_current;

static void add_failure( char const * text )
{
    if( ( s_current != NULL ) && ( s_current->failure_count < SIL_MAX_FAILURES ) )
    {
        ( void ) snprintf( s_current->failures[ s_current->failure_count ],
                           SIL_MESSAGE_SIZE, "%s", text );
    }

    if( s_current != NULL )
    {
        s_current->failure_count++;
        s_current->passed = 0;
    }
}

void sil_check_failed( char const * file, int line, char const * message, int fatal )
{
    char text[ SIL_MESSAGE_SIZE ];

    ( void ) snprintf( text, sizeof( text ), "%s:%d: check failed: %s", file, line, message );
    add_failure( text );

    if( fatal != 0 )
    {
        longjmp( s_case_jump, 1 );
    }
}

void sil_check_failed_u( char const * file, int line, char const * expression,
                         uint64_t actual, uint64_t expected, int fatal )
{
    char text[ SIL_MESSAGE_SIZE ];

    ( void ) snprintf( text, sizeof( text ),
                       "%s:%d: check failed: %s (actual %llu, expected %llu)",
                       file, line, expression,
                       ( unsigned long long ) actual, ( unsigned long long ) expected );
    add_failure( text );

    if( fatal != 0 )
    {
        longjmp( s_case_jump, 1 );
    }
}

static void xml_escape( FILE * out, char const * text )
{
    for( ; *text != '\0'; text++ )
    {
        switch( *text )
        {
            case '<': ( void ) fputs( "&lt;", out ); break;
            case '>': ( void ) fputs( "&gt;", out ); break;
            case '&': ( void ) fputs( "&amp;", out ); break;
            case '"': ( void ) fputs( "&quot;", out ); break;
            default: ( void ) fputc( *text, out ); break;
        }
    }
}

static void write_junit(
    char const * path,
    char const * suite,
    sil_case_t const * cases,
    sil_result_t const * results,
    size_t count,
    size_t failures )
{
    FILE * out = fopen( path, "w" );
    size_t i;

    if( out == NULL )
    {
        ( void ) fprintf( stderr, "cannot write %s\n", path );
        return;
    }

    ( void ) fprintf( out, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" );
    ( void ) fprintf( out, "<testsuite name=\"%s\" tests=\"%zu\" failures=\"%zu\">\n",
                      suite, count, failures );

    for( i = 0U; i < count; i++ )
    {
        unsigned f;

        ( void ) fprintf( out, "  <testcase classname=\"%s\" name=\"%s\" time=\"%.6f\">\n",
                          suite, cases[ i ].name, results[ i ].seconds );
        ( void ) fprintf( out, "    <properties>\n      <property name=\"requirement\" value=\"%s\"/>\n",
                          cases[ i ].requirement );
        ( void ) fprintf( out, "      <property name=\"title\" value=\"" );
        xml_escape( out, cases[ i ].title );
        ( void ) fprintf( out, "\"/>\n    </properties>\n" );

        for( f = 0U; ( f < results[ i ].failure_count ) && ( f < SIL_MAX_FAILURES ); f++ )
        {
            ( void ) fprintf( out, "    <failure message=\"" );
            xml_escape( out, results[ i ].failures[ f ] );
            ( void ) fprintf( out, "\"/>\n" );
        }

        ( void ) fprintf( out, "  </testcase>\n" );
    }

    ( void ) fprintf( out, "</testsuite>\n" );
    ( void ) fclose( out );
}

int sil_run_suite(
    char const * suite,
    sil_case_t const * cases,
    size_t count,
    int argc,
    char ** argv )
{
    sil_result_t results[ 64 ];
    char const * junit = NULL;
    char const * only = NULL;
    size_t failures = 0U;
    size_t ran = 0U;
    size_t i;
    int a;

    if( count > 64U )
    {
        ( void ) fprintf( stderr, "too many cases in suite\n" );
        return 2;
    }

    for( a = 1; a < argc; a++ )
    {
        if( ( strcmp( argv[ a ], "--junit" ) == 0 ) && ( a + 1 < argc ) )
        {
            junit = argv[ ++a ];
        }
        else if( ( strcmp( argv[ a ], "--case" ) == 0 ) && ( a + 1 < argc ) )
        {
            only = argv[ ++a ];
        }
    }

    ( void ) printf( "[==========] %s: %zu case(s)\n", suite, count );

    for( i = 0U; i < count; i++ )
    {
        struct timespec t0;
        struct timespec t1;
        unsigned f;

        ( void ) memset( &results[ i ], 0, sizeof( results[ i ] ) );
        results[ i ].passed = 1;

        if( ( only != NULL ) && ( strcmp( only, cases[ i ].name ) != 0 ) )
        {
            continue;
        }

        ran++;
        s_current = &results[ i ];

        ( void ) printf( "[ RUN      ] %s  (%s)\n", cases[ i ].name, cases[ i ].requirement );
        ( void ) clock_gettime( CLOCK_MONOTONIC, &t0 );

        if( setjmp( s_case_jump ) == 0 )
        {
            cases[ i ].run();
        }

        ( void ) clock_gettime( CLOCK_MONOTONIC, &t1 );
        results[ i ].seconds = ( double ) ( t1.tv_sec - t0.tv_sec ) +
                               ( double ) ( t1.tv_nsec - t0.tv_nsec ) / 1e9;
        s_current = NULL;

        if( results[ i ].passed != 0 )
        {
            ( void ) printf( "[       OK ] %s (%.0f ms)\n", cases[ i ].name, results[ i ].seconds * 1e3 );
        }
        else
        {
            failures++;
            ( void ) printf( "[  FAILED  ] %s\n", cases[ i ].name );

            for( f = 0U; ( f < results[ i ].failure_count ) && ( f < SIL_MAX_FAILURES ); f++ )
            {
                ( void ) printf( "             %s\n", results[ i ].failures[ f ] );
            }
        }
    }

    ( void ) printf( "[==========] %zu ran, %zu failed\n", ran, failures );

    if( junit != NULL )
    {
        write_junit( junit, suite, cases, results, count, failures );
    }

    return ( failures == 0U ) ? 0 : 1;
}
