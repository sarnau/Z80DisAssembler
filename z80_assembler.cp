/***
 *  Z80 Assembler
 ***/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "z80_assembler.h"

uint16_t    PC;         // aktuelle Adresse
uint8_t     *RAM;       // RAM vom Z80
long        LineNo;     // aktuelle Zeilennummer
char        LineBuf[MAXLINELENGTH]; // Buffer für eine eingelesene ASCII-Zeile

/***
 *  Fehlermeldung ausgeben
 ***/
void        Error(const char* s)
{
const char *p;

    printf("Fehler in Zeile %ld: %s\n",LineNo,s);   // Fehlermeldung ausgeben
    for(p = LineBuf;isspace(*p);p++)
    	;
    puts(p);      									// Zeile ausgeben
    exit(1);
}

/***
 *  …
 ***/
int        main(void)
{
FILE     *f;
char*    s;
int32_t  i,j;
uint16_t savePC;
uint8_t  EPROM[0x2000];

    LineNo = 1;
    InitSymTab();                       // Symboltabelle initialisieren

    RAM = (uint8_t*)malloc(65536);
    memset(RAM,0xFF,65536);             // 64K RAM löschen
    PC = 0;                             // Startadresse des Codes

    puts("TurboAss Z80 - ein kleiner 1-Pass-Assembler für Z80 Programmcode");
    puts("(c)1992/3 Sigma-Soft, Markus Fritze");
    puts("");

    f = fopen("FuturaFirmware.asm","r");
    if(!f) {
    	Error("File 'FuturaFirmware.asm' not found!");
    }
    while(1) {
        s = fgets(LineBuf,sizeof(LineBuf),f);   // eine Zeile einlesen
        if(!s) break;                   // Sourcetext-Ende => raus
        *(s + strlen(s) - 1) = 0;       // CR am Zeilenende entfernen
//	    puts(s);
        TokenizeLine(s);                // Zeile tokenisieren
        savePC = PC;
        CompileLine();                  // Zeile übersetzen
#if 0
        if(savePC != PC) {
            printf("%4.4X:",savePC);
            for(i=savePC;i<PC;i++)
                printf("%2.2X ",RAM[i]);
            printf("\n");
        }
#endif
        LineNo++;                       // nächste Zeile
    }
    fclose(f);

//  puts("undefinierte Symbole ausgeben");
    for(i=0;i<256;i++) {                // Symboltabelle durchgehen
        SymbolP     s;
        for(s = SymTab[i];s;s = s->next)
            if(s->recalc)               // Hängen noch Ausdrücke von einem Symbol ab?
                printf("Symbol \"%s\" undefiniert!\n",s->name);
    }

#if 0
    puts("Original-EPROM einlesen");
    f = fopen("EPROM","rb");
    if(!f) exit(1);
    fread(EPROM,sizeof(uint8_t),0x2000,f);// 8K EPROM einlesen
    fclose(f);

    puts("Programmcode mit dem Original vergleichen");
    j = 0;                          // und vergleichen…
    for(i=0;(i<0x2000)&&(j < 10);i++)
        if(RAM[i] != EPROM[i]) {
            printf("%4.4X : %2.2X != %2.2X\n",(uint16_t)i,RAM[i],EPROM[i]);
            j++;
        }
    if(!j)
        puts("Programmcode stimmt mit dem Original-EPROM überein!");
    else
    {
        puts("Prüfsumme neu berechnen");
        j = 0;
        for(i=0;i<0x1FF0;i++)
            j += (uint8_t)RAM[i];
        {
        SymbolP     FindSymbol(const char* name);
        SymbolP s = FindSymbol("ROMCHECKSUM");
            if(s) {
                RAM[s->val] = j;
                RAM[s->val+1] = j >> 8;
                puts("ROMChecksum angepaßt.");
            } else
                puts("ROMChecksum nicht gefunden!");
        }
    }
#endif

    f = fopen("Z80.code","wb");
    if(!f) exit(1);
    fwrite(RAM,sizeof(uint8_t),0x2000,f); // kompiliertes Programm wegschreiben
    fclose(f);

	return 0;
}
