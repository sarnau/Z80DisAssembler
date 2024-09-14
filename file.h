#ifndef __FILE_H
#define __FILE_H

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif


extern void write_header( FILE *stream, uint16_t address );
extern int read_header( FILE *stream, uint32_t *address, uint32_t *len );

#ifdef __cplusplus
}
#endif

#endif
