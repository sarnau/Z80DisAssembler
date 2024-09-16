/***
 *  Z80 Assembler
 ***/

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits.h>

#include "kk_ihex_write.h"
#include "z80_assembler.h"

uint32_t PC;     // current address
uint32_t nextPC; // remember address
uint8_t *RAM;    // RAM of the Z80
const uint32_t RAMSIZE = 0x10000;
uint32_t minPC = RAMSIZE;
uint32_t maxPC = 0;
bool listing = false;

static FILE *infile;
static FILE *outbin;
static FILE *outz80;
static FILE *outhex;

int verboseMode = 0;

long LineNo;                   // current line number
char LineBuf[ MAXLINELENGTH ]; // buffer for the current line

/***
 *  print fatal error message and exit
 ***/
void Error( const char *s ) {
    const char *p;

    printf( "Error in line %ld: %s\n", LineNo, s );
    for ( p = LineBuf; isspace( *p ); p++ )
        ;
    puts( p );
    exit( 1 );
}


void usage( const char *fullpath ) {
    const char *progname = nullptr;
    char c;
    while ( ( c = *fullpath++ ) )
        if ( c == '/' || c == '\\' )
            progname = fullpath;
    printf( "Usage: %s [-l] [-n] [-v] INFILE\n"
            "  -c       CP/M com file format for binary\n"
            "  -fXX     fill ram with byte XX (default: 00)\n"
            "  -l       show listing\n"
            "  -n       no output files\n"
            "  -oXXXX   offset address = 0x0000 .. 0xFFFF\n"
            "  -v       increase verbosity\n",
            progname );
}


static void listOneLine( uint32_t firstPC, uint32_t lastPC, const char *oneLine );
static void write_header( FILE *stream, uint32_t address );

/***
 *  …
 ***/
int main( int argc, char **argv ) {
    char *inputfilename = nullptr;
    char outputfilename[ PATH_MAX ];

    char *oneLine;
    int i, j;

    int com = 0;
    int offset = 0;
    // int start = 0;
    int fill = 0;
    int result;
    bool no_outfile = false;


    puts( "TurboAss Z80 - a small 1-pass assembler for Z80 code" );
    puts( "(c)1992/3 Sigma-Soft, Markus Fritze" );
    puts( "" );


    for ( i = 1, j = 0; i < argc; i++ ) {
        if ( '-' == argv[ i ][ 0 ] ) {
            switch ( argv[ i ][ ++j ] ) {
            case 'c': // create cp/m com file
                com = 0x100;
                break;
            case 'f':                   // fill
                if ( argv[ i ][ ++j ] ) // "-fXX"
                    result = sscanf( argv[ i ] + j, "%x", &fill );
                else if ( i < argc - 1 ) // "-f XX"
                    result = sscanf( argv[ ++i ], "%x", &fill );
                if ( result )
                    fill &= 0x00FF; // limit to byte size
                else {
                    fprintf( stderr, "Error: option -f needs a hexadecimal argument\n" );
                    return 1;
                }
                j = 0; // end of this arg group
                break;
            case 'l': // parse program flow
                listing = true;
                break;
            case 'n': // parse program flow
                no_outfile = true;
                break;
            case 'o':                   // program offset
                if ( argv[ i ][ ++j ] ) // "-oXXXX"
                    result = sscanf( argv[ i ] + j, "%x", &offset );
                else if ( i < argc - 1 ) // "-o XXXX"
                    result = sscanf( argv[ ++i ], "%x", &offset );
                if ( result )
                    offset &= 0xFFFF; // limit to 64K
                else {
                    fprintf( stderr, "Error: option -o needs a hexadecimal argument\n" );
                    return 1;
                }
                j = 0; // end of this arg group
                break;
            case 'v':
                ++verboseMode;
                break;
            default:
                usage( argv[ 0 ] );
                return 1;
            }

            if ( j && argv[ i ][ j + 1 ] ) { // one more arg char
                --i;                         // keep this arg group
                continue;
            }
            j = 0; // start from the beginning in next arg group
        } else {
            if ( !inputfilename )
                inputfilename = argv[ i ];
            else {
                usage( argv[ 0 ] );
                return 1;
            } // check next arg string
        }
    }
    if ( !inputfilename ) {
        usage( argv[ 0 ] );
        return 1;
    }

    infile = fopen( inputfilename, "r" );
    if ( !infile ) {
        fprintf( stderr, "Error: cannot open infile %s\n", inputfilename );
        return 1;
    }
    MSG( 1, "Processing infile \"%s\"\n", inputfilename );

    LineNo = 1;
    InitSymTab(); // init symbol table

    RAM = (uint8_t *)malloc( RAMSIZE + 256 ); // guard against overflow at ram top
    memset( RAM, fill, RAMSIZE );             // erase 64K RAM
    PC = 0x0000;                              // default start address of the code

    while ( true ) {
        uint32_t prevPC = PC;
        oneLine = fgets( LineBuf, sizeof( LineBuf ), infile ); // read a single line
        if ( !oneLine )
            break;                                // end of the code => exit
        *( oneLine + strlen( oneLine ) - 1 ) = 0; // remove end of line marker
        TokenizeLine( oneLine );                  // tokenize line
        CompileLine();                            // generate machine code for the line
        listOneLine( prevPC, PC, oneLine );       // create listing if enabled
        LineNo++;                                 // next line
    }
    list( "\n" );
    fclose( infile );

    // cross reference
    for ( i = 0; i < 256; i++ ) { // iterate over symbol table
        SymbolP s;
        for ( s = SymTab[ i ]; s; s = s->next )
            if ( s->recalc ) // depend expressions on a symbol?
                printf( "----    %s is undefined!\n", s->name );
            else if ( !s->type )
                list( "%04X%*s\n", s->val, 20 + int( strlen( s->name ) ), s->name );
    }

    if ( minPC < 0x100 || maxPC <= 0x100 ) // cannot be a CP/M com file
        com = 0;

    if ( listing || verboseMode ) {
        if ( minPC <= maxPC )
            printf( "\nUsing RAM range [0x%04X...0x%04X]\n", minPC, maxPC );
        else {
            printf( "\nNo data created\n" );
            exit( 1 );
        }
    }

    if ( !no_outfile && strlen( inputfilename ) > 4 && !strcmp( inputfilename + strlen( inputfilename ) - 4, ".asm" ) ) {
        strncpy( outputfilename, inputfilename, sizeof( outputfilename ) );

        // create out file name(s) from in file name
        size_t fnamelen = strlen( outputfilename );
        // bin or com (=bin file that starts at PC=0x100) file
        strncpy( outputfilename + fnamelen - 3, com ? "com" : "bin", sizeof( outputfilename ) - fnamelen - 3 );
        MSG( 1, "Creating output file %s\n", outputfilename );
        outbin = fopen( outputfilename, "wb" );
        if ( !outbin ) {
            fprintf( stderr, "Error: Can't open output file \"%s\".\n", outputfilename );
            return 1;
        }
        // z80 file is a bin file with a header telling the file offset
        strncpy( outputfilename + fnamelen - 3, "z80", sizeof( outputfilename ) - fnamelen - 3 );
        MSG( 1, "Creating output file %s\n", outputfilename );
        outz80 = fopen( outputfilename, "wb" );
        if ( !outz80 ) {
            fprintf( stderr, "Error: Can't open output file \"%s\".\n", outputfilename );
            return 1;
        }
        // intel hex file
        strncpy( outputfilename + fnamelen - 3, "hex", sizeof( outputfilename ) - fnamelen - 3 );
        MSG( 1, "Creating output file %s\n", outputfilename );
        outhex = fopen( outputfilename, "wb" );
        if ( !outhex ) {
            fprintf( stderr, "Error: Can't open output file \"%s\".\n", outputfilename );
            return 1;
        }
    } else {
        MSG( 1, "No output files created\n" );
        exit( 0 );
    }
    if ( outbin ) {
        if ( com )
            fwrite( RAM + 0x100, sizeof( uint8_t ), maxPC + 1 - 0x100, outbin );
        else
            fwrite( RAM + offset, sizeof( uint8_t ), maxPC + 1 - offset, outbin );
        fclose( outbin );
    }
    if ( outz80 ) {
        write_header( outz80, minPC );
        fwrite( RAM + minPC, sizeof( uint8_t ), maxPC + 1 - minPC, outz80 );
    }
    if ( outhex ) {
        // write the data as intel hex
        struct ihex_state ihex;
        ihex_init( &ihex );

        ihex_write_at_address( &ihex, minPC );
        ihex_write_bytes( &ihex, RAM + minPC, maxPC + 1 - minPC );
        ihex_end_write( &ihex );
        fclose( outhex );
    }
    return 0;
}


void checkPC( uint32_t pc ) {
    MSG( 3, "checkPC( %04X )", pc );
    if ( pc >= RAMSIZE ) {
        Error( "Address overflow -> exit" );
        exit( 0 );
    }
    if ( pc < minPC )
        minPC = pc;
    if ( pc > maxPC )
        maxPC = pc;
    MSG( 3, "[%04X..%04X]\n", minPC, maxPC );
}


void MSG( int mode, const char *format, ... ) {
    if ( verboseMode >= mode ) {
        while ( mode-- )
            fprintf( stderr, " " );
        va_list argptr;
        va_start( argptr, format );
        vfprintf( stderr, format, argptr );
        va_end( argptr );
    }
}


void list( const char *format, ... ) {
    if ( listing ) {
        va_list argptr;
        va_start( argptr, format );
        vprintf( format, argptr );
        va_end( argptr );
    }
}

// create listing for one sorce code line
// address    data bytes    source code
// break long data block (e.g. defm) into lines of 4 data bytes
static void listOneLine( uint32_t firstPC, uint32_t lastPC, const char *oneLine ) {
    if ( !listing )
        return;
    if ( firstPC == lastPC ) {
        printf( "%*s\n", 24 + int( strlen( oneLine ) ), oneLine );
    } else {
        printf( "%4.4X   ", firstPC );
        uint32_t adr = firstPC;
        int i = 0;
        while ( adr < lastPC ) {
            printf( " %2.2X", RAM[ adr++ ] );
            if ( i == 3 )
                printf( "     %s", oneLine );
            if ( ( i & 3 ) == 3 ) {
                printf( "\n" );
                if ( adr < lastPC )
                    printf( "%4.4X   ", adr );
            }
            ++i;
        }
        if ( i < 4 )
            printf( "%*s\n", 5 + 3 * ( 4 - i ) + int( strlen( oneLine ) ), oneLine );
        else if ( ( i & 3 ) )
            printf( "\n" );
    }
}


// the z80 format is used by the z80-asm
// http://wwwhomes.uni-bielefeld.de/achim/z80-asm.html
// *.z80 files are bin files with a header telling the bin offset
// struct z80_header {
//     const char  *MAGIC = Z80MAGIC;
//     uint16_t    offset;
// }
static void write_header( FILE *stream, uint32_t address ) {
    const char *Z80MAGIC = "Z80ASM\032\n";
    unsigned char c[ 2 ];
    c[ 0 ] = address & 255;
    c[ 1 ] = address >> 8;
    fwrite( Z80MAGIC, 1, strlen( Z80MAGIC ), stream );
    fwrite( c, 1, 2, stream );
}


void ihex_flush_buffer( struct ihex_state *ihex, char *buffer, char *eptr ) {
    (void)ihex;
    *eptr = '\0';
    (void)fputs( buffer, outhex );
}
