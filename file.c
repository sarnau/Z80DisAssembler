/* Routines for manipulating with Z80 machine code files */

//#include "z80_assembler.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define _Z80HEADER "Z80ASM\032\n"


void write_header( FILE *stream, uint32_t address ) {
    unsigned char c[ 2 ];
    c[ 0 ] = address & 255;
    c[ 1 ] = address >> 8;
    fwrite( _Z80HEADER, 1, strlen( _Z80HEADER ), stream );
    fwrite( c, 1, 2, stream );
}


/* reads header of a file and tests if it's Z80 ASM file, reads address */
/* return value: 0=OK, 1=this is not a z80 asm file, 2,3=seek malfunction */
int read_header( FILE *stream, uint32_t *address, uint32_t *len ) {
    char tmp[ 9 ];
    unsigned char c[ 2 ];
    unsigned a, b;
    int ret = 0;

    b = strlen( _Z80HEADER );
    tmp[ b ] = 0;
    a = 0;
    if ( ( a = fread( tmp, 1, b, stream ) ) != b )
        printf( "1: %d, %d\n", a, ret = 1 );
    else if ( strcmp( tmp, _Z80HEADER ) )
        printf( "1: %d\n", ret = 1 );
    else if ( fread( c, 1, 2, stream ) != 2 )
        printf( "1: %d\n", ret = 1 );
    else {
        *address = ( c[ 1 ] << 8 ) | c[ 0 ];
        a = b + 2;
    }
    if ( fseek( stream, 0, SEEK_END ) )
        ret = 2;
    else if ( ( b = ftell( stream ) ) < a )
        ret = 2;
    else
        *len = b - a;
    if ( fseek( stream, a, SEEK_SET ) )
        ret = 3;
    return ret;
}
