#include <sys/stat.h>
#include <stddef.h>

int _close( int file )
{
    ( void ) file;
    return -1;
}

int _fstat( int file, struct stat * st )
{
    ( void ) file;
    ( void ) st;
    return 0;
}

int _isatty( int file )
{
    ( void ) file;
    return 1;
}

int _lseek( int file, int ptr, int dir )
{
    ( void ) file;
    ( void ) ptr;
    ( void ) dir;
    return 0;
}

int _read( int file, char * ptr, int len )
{
    ( void ) file;
    ( void ) ptr;
    ( void ) len;
    return 0;
}

int _write( int file, char const * ptr, int len )
{
    ( void ) file;
    ( void ) ptr;
    return len;
}

void _exit( int status )
{
    ( void ) status;

    for( ;; )
    {
        __asm volatile ( "bkpt #0" );
    }
}

void * _sbrk( ptrdiff_t incr )
{
    ( void )incr;
    return ( void * ) -1;
}
