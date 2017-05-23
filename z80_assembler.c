/***
 *  Z80 Assembler
 ***/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#if __option(profile)
#include <profile.h>
#endif
#include "Z80 Assembler.h"

UWORD       PC;         // aktuelle Adresse
UBYTE       *RAM;       // RAM vom Z80
ULONG       LineNo;     // aktuelle Zeilennummer
CHAR        LineBuf[MAXLINELENGTH]; // Buffer für eine eingelesene ASCII-Zeile

/***
 *  Fehlermeldung ausgeben
 ***/
VOID        Error(STR s)
{
STR     p;

    printf("Fehler in Zeile %ld: %s\n",LineNo,s);   // Fehlermeldung ausgeben
    for(p = LineBuf;isspace(*p);p++); puts(p);      // Zeile ausgeben
    exit(1);
}

/***
 *  …
 ***/
VOID        main(VOID)
{
FILE    *f;
STR     s;
LONG    i,j;
UWORD   savePC;
UCHAR   EPROM[0x2000];
ULONG   ticks;

#if __option(profile)
    InitProfile(400,200);
    freopen("Profiler Report","w", stdout);
#endif

    LineNo = 1;
    InitSymTab();                       // Symboltabelle initialisieren

    RAM = malloc(65536);
    memset(RAM,0xFF,65536);             // 64K RAM löschen
    PC = 0;                             // Startadresse des Codes

    puts("TurboAss Z80 - ein kleiner 1-Pass-Assembler für Z80 Programmcode");
    puts("©1992/3 ∑-Soft, Markus Fritze");
    puts("");

    ticks = TickCount();
    f = fopen("Futura.src","r");
    if(!f) return;
    while(1) {
        s = fgets(LineBuf,sizeof(LineBuf),f);   // eine Zeile einlesen
        if(!s) break;                   // Sourcetext-Ende => raus
        *(s + strlen(s) - 1) = 0;       // CR am Zeilenende entfernen
//      puts(s);
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
    ticks = TickCount() - ticks;
    printf("%.3fs für %ld Zeilen = %ld Zeilen/min\n",(float)ticks/60.0,LineNo,3600*LineNo/ticks);

//  puts("undefinierte Symbole ausgeben");
    for(i=0;i<256;i++) {                // Symboltabelle durchgehen
        SymbolP     s;
        for(s = SymTab[i];s;s = s->next)
            if(s->recalc)               // Hängen noch Ausdrücke von einem Symbol ab?
                printf("Symbol \"%s\" undefiniert!\n",s->name);
    }

//  puts("Original-EPROM einlesen");
    f = fopen("EPROM","rb");
    if(!f) exit(1);
    fread(EPROM,sizeof(UCHAR),0x2000,f);// 8K EPROM einlesen
    fclose(f);

#if 1
//  puts("Programmcode mit dem Original vergleichen");
    j = 0;                          // und vergleichen…
    for(i=0;(i<0x2000)&&(j < 10);i++)
        if(RAM[i] != EPROM[i]) {
            printf("%4.4X : %2.2X != %2.2X\n",(UWORD)i,RAM[i],EPROM[i]);
            j++;
        }
    if(!j)
        puts("Programmcode stimmt mit dem Original-EPROM überein!");
    else
#endif
    {
        puts("Prüfsumme neu berechnen");
        j = 0;
        for(i=0;i<0x1FF0;i++)
            j += (UBYTE)RAM[i];
        {
        SymbolP     FindSymbol(STR name);
        SymbolP s = FindSymbol("ROMCHECKSUM");
            if(s) {
                RAM[s->val] = j;
                RAM[s->val+1] = j >> 8;
                puts("ROMChecksum angepaßt.");
            } else
                puts("ROMChecksum nicht gefunden!");
        }
    }

    f = fopen("Z80.code","wb");
    if(!f) exit(1);
    fwrite(RAM,sizeof(UCHAR),0x2000,f); // kompiliertes Programm wegschreiben
    fclose(f);
#if __option(profile)
    DumpProfile();
#endif
}
