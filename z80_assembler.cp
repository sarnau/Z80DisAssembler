/***
 *  Z80 Assembler
 ***/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "z80_assembler.h"

uint32_t    PC;         // current address
uint8_t     *RAM;       // RAM of the Z80
long        LineNo;     // current line number
char        LineBuf[MAXLINELENGTH]; // buffer for the current line

/***
 *  print fatal error message and exit
 ***/
void        Error(const char* s)
{
const char *p;

    printf("Fehler in Zeile %ld: %s\n",LineNo,s);
    for(p = LineBuf;isspace(*p);p++)
    	;
    puts(p);
    exit(1);
}

/***
 *  …
 ***/
int        main(void)
{
FILE     *f;
char*    s;
int32_t  i;

    LineNo = 1;
    InitSymTab();                       // init symbol table

    RAM = (uint8_t*)malloc(65536);
    memset(RAM,0xFF,65536);             // erase 64K RAM
    PC = 0x0000;                        // default start address of the code

    puts("TurboAss Z80 - a small 1-pass assembler for Z80 code");
    puts("(c)1992/3 Sigma-Soft, Markus Fritze");
    puts("");

    f = fopen("FuturaFirmware.asm","r");
    if(!f) {
    	Error("File 'FuturaFirmware.asm' not found");
    }
    while(1) {
        s = fgets(LineBuf,sizeof(LineBuf),f);   // read a single line
        if(!s) break;                   // end of the code => exit
        *(s + strlen(s) - 1) = 0;       // remove end of line marker
        TokenizeLine(s);                // tokenize line
        CompileLine();                  // generate machine code for the line
        LineNo++;                       // next line
    }
    fclose(f);

//  puts("undefined symbols:");
    for(i=0;i<256;i++) {                // iterate over symbol table
        SymbolP     s;
        for(s = SymTab[i];s;s = s->next)
            if(s->recalc)               // depend expressions on a symbol?
                printf("symbol \"%s\" undefined!\n",s->name);
    }

#if 0
    puts("Read original EPROM");
    f = fopen("EPROM","rb");
    if(!f) exit(1);
	uint8_t  EPROM[0x2000];
    fread(EPROM,sizeof(uint8_t),0x2000,f);// read 8K EPROM
    fclose(f);

    puts("Compare code with the original EPROM");
    int32_t  j = 0;                       // and compare
    for(i=0;(i<0x2000)&&(j < 10);i++)
        if(RAM[i] != EPROM[i]) {
            printf("%4.4X : %2.2X != %2.2X\n",(uint16_t)i,RAM[i],EPROM[i]);
            j++;
        }
    if(!j)
        puts("Code is identical to the original EPROM");
    else
    {
        puts("Calculate new 16-bit checksum");
        j = 0;
        for(i=0;i<0x1FF0;i++)
            j += (uint8_t)RAM[i];
        {
        SymbolP     FindSymbol(const char* name);
        SymbolP s = FindSymbol("ROMCHECKSUM");
            if(s) {
                RAM[s->val] = j;
                RAM[s->val+1] = j >> 8;
                puts("ROMChecksum adjusted.");
            } else
                puts("ROMChecksum not found");
        }
    }
#endif

    f = fopen("Z80.code","wb");
    if(!f) exit(1);
    fwrite(RAM,sizeof(uint8_t),0x2000,f); // write code to disk
    fclose(f);

	return 0;
}
