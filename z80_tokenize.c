/***
 *  Z80 Tokenizer
 ***/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <SigmaStr.h>
#include "Z80 Assembler.h"

UWORD       CalcHash(USTR name);
SymbolP     FindSymbol(STR name);
VOID        InitSymTab(VOID);


typedef struct {
    WORD    id;         // ID vom Symbol
    STR     s;          // String
    UWORD   p;          // Parameter
} ShortSym;

ShortSym    Pseudo[] = {    { 0x100,"DEFB",0x0000}, { 0x101,"DEFM",0x0000},{ 0x102,"DEFS",0x0000},
                            { 0x103,"DEFW",0x0000}, { 0x109,"ELSE",0x0000},{ 0x104,"END",0x0000},
                            { 0x108,"ENDIF",0x0000},{ 0x105,"EQU",0x0000}, { 0x107,"IF",0x0000},
                            { 0x106,"ORG",0x0000},  { 0x10A,"PRINT",0x0000} };
// Typ: (+ 0x200)
// 0x00 : IN,OUT
// 0x01 : 1 Byte Opcode, keine Parameter
// 0x02 : 2 Byte Opcode, keine Parameter
// 0x03 : 2 Byte Opcode, (HL) nötig
// 0x04 : 1.Parameter = Bitno., 2.Parameter = <ea> (BIT,RES,SET)
// 0x05 : IM (ein Parameter: 0,1,2)
// 0x06 : ADD,ADC,SUB,SBC,AND,XOR,OR,CP
// 0x07 : INC, DEC, wie 0x06 nur ohne absolute Adr.
// 0x08 : JP, CALL, JR (Achtung! Unterschiedliche <ea>!)
// 0x09 : RET (c oder nichts)
// 0x0A : RST (00,08,10,18,20,28,30,38)
// 0x0B : DJNZ
// 0x0C : EX: (SP),dreg oder DE,HL oder AF,AF'
// 0x0D : LD
// 0x0E : PUSH, POP: dreg
// 0x0F : RR,RL,RRC,RLC,SRA,SLA,SRL
ShortSym    Opcodes[] = {   { 0x206,"ADC",0x88CE}, { 0x206,"ADD",0x80C6}, { 0x206,"AND",0xA0E6},
                            { 0x204,"BIT",0xCB40}, { 0x208,"CALL",0xC4CD},{ 0x201,"CCF",0x3F00},
                            { 0x206,"CP",0xB8FE},  { 0x202,"CPD",0xEDA9}, { 0x202,"CPDR",0xEDB9},
                            { 0x202,"CPI",0xEDA1}, { 0x202,"CPIR",0xEDB1},{ 0x201,"CPL",0x2F00},
                            { 0x201,"DAA",0x2700}, { 0x207,"DEC",0x0500}, { 0x201,"DI",0xF300},
                            { 0x20B,"DJNZ",0x1000},{ 0x201,"EI",0xFB00},  { 0x20C,"EX",0xE3EB},
                            { 0x201,"EXX",0xD900}, { 0x201,"HALT",0x7600},{ 0x205,"IM",0xED46},
                            { 0x200,"IN",0x40DB},  { 0x207,"INC",0x0400}, { 0x202,"IND",0xEDAA},
                            { 0x202,"INDR",0xEDBA},{ 0x202,"INI",0xEDA2}, { 0x202,"INIR",0xEDB2},
                            { 0x208,"JP",0xC2C3},  { 0x208,"JR",0x2018},  { 0x20D,"LD",0x0000},
                            { 0x202,"LDD",0xEDA8}, { 0x202,"LDDR",0xEDB8},{ 0x202,"LDI",0xEDA0},
                            { 0x202,"LDIR",0xEDB0},{ 0x202,"NEG",0xED44}, { 0x201,"NOP",0x0000},
                            { 0x206,"OR",0xB0F6},  { 0x202,"OTDR",0xEDBB},{ 0x202,"OTIR",0xEDB3},
                            { 0x200,"OUT",0x41D3}, { 0x202,"OUTD",0xEDAB},{ 0x202,"OUTI",0xEDA3},
                            { 0x20E,"POP",0xC1E1}, { 0x20E,"PUSH",0xC5E5},{ 0x204,"RES",0xCB80},
                            { 0x209,"RET",0xC0C9}, { 0x202,"RETI",0xED4D},{ 0x202,"RETN",0xED45},
                            { 0x20F,"RL",0x1016},  { 0x201,"RLA",0x1700}, { 0x20F,"RLC",0x0016},
                            { 0x201,"RLCA",0x0700},{ 0x203,"RLD",0xED6F}, { 0x20F,"RR",0x181E},
                            { 0x201,"RRA",0x1F00}, { 0x20F,"RRC",0x080E}, { 0x201,"RRCA",0x0F00},
                            { 0x203,"RRD",0xED67}, { 0x20A,"RST",0xC700}, { 0x206,"SBC",0x98DE},
                            { 0x201,"SCF",0x3700}, { 0x204,"SET",0xCBC0}, { 0x20F,"SLA",0x2026},
                            { 0x20F,"SRA",0x282E}, { 0x20F,"SRL",0x383E}, { 0x206,"SUB",0x90D6},
                            { 0x206,"XOR",0xA8EE} };
ShortSym    Register[] = {  { 0x307,"A",0x0000}, { 0x323,"AF",0x0000},      // 00…07: B,C,D,E,H,L,(HL),A
                            { 0x300,"B",0x0000}, { 0x310,"BC",0x0000},      // 10…13: BC,DE,HL,SP
                            { 0x301,"C",0x0000}, { 0x302,"D",0x0000},       //    23:         ,AF
                            { 0x311,"DE",0x0000},{ 0x303,"E",0x0000},       // 30…31: IX,IY
                            { 0x304,"H",0x0000}, { 0x312,"HL",0x0000},      // 40…41: R,I
                            { 0x341,"I",0x0000}, { 0x330,"IX",0x0000},      // 54…55: X,HX
                            { 0x331,"IY",0x0000},{ 0x305,"L",0x0000},       // 64…65: Y,HY
                            { 0x340,"R",0x0000}, { 0x313,"SP",0x0000},
                            { 0x355,"X",0x0000}, { 0x354,"HX",0x0000},
                            { 0x365,"Y",0x0000}, { 0x364,"HY",0x0000} };
ShortSym    Conditions[] = {/*{ 0x403,"C",0x0000},*/{ 0x407,"M",0x0000},    // Condition C = Register C!
                            { 0x402,"NC",0x0000},{ 0x400,"NZ",0x0000},
                            { 0x406,"P",0x0000},{ 0x405,"PE",0x0000},
                            { 0x404,"PO",0x0000},{ 0x401,"Z",0x0000} };


typedef struct {
    ShortSym    *table;     // Ptr auf eine Opcode-Liste
    WORD        tablesize;  // Länge der Tabelle in Byte
} TokenTable;

TokenTable  Token[] = { { Pseudo,sizeof(Pseudo)/sizeof(ShortSym) },
                        { Opcodes,sizeof(Opcodes)/sizeof(ShortSym) },
                        { Register,sizeof(Register)/sizeof(ShortSym) },
                        { Conditions,sizeof(Conditions)/sizeof(ShortSym) },
                        { 0,0 }
                        };

Command     Cmd[80];            // eine tokenisierte Zeile
SymbolP     SymTab[256];        // Symboltabelle (nach unterem Hashbyte geteilt)

/***
 *  Hash-Wert über einen String berechnen
 ***/
UWORD       CalcHash(REG USTR name)
{
UWORD       hash_val = 0;
UWORD       i;
UCHAR       c;

    while((c = *name++) != 0) {
#if 0
        hash_val += c;
#else
        hash_val = (hash_val << 4) + c;
        if((i = (hash_val >> 12)) != 0)
            hash_val ^= i;
#endif
    }
    return(hash_val);
}

/***
 *  Symbol suchen, wenn nicht gefunden automatisch eintragen
 ***/
SymbolP     FindSymbol(STR name)
{
UWORD       hash = CalcHash((USTR)name);// Hash-Wert über den Namen
UBYTE       hashb = hash;
SymbolP     s;

    s = SymTab[hashb];                  // Ptr auf das erste Symbol
    while(s) {
        if(s->hash == hash)             // Stimmt der Hashwert?
            if(!Strcmp(s->name,name)) return(s);// Symbol gefunden (Strcmp = schnelle Inline-Routine)
        s = s->next;                    // zum nächsten Symbol
    }
    s = malloc(sizeof(Symbol));         // Symbol allozieren
    if(!s) return(nil);                 // Speicher reicht nicht!
    memset(s,0,sizeof(Symbol));
    s->next = SymTab[hashb]; SymTab[hashb] = s; // Symbol in die Liste einklinken
    s->hash = hash;
    strcpy(s->name,name);               // Namen kopieren
    return(s);
}

/***
 *  Symboltabelle initialisieren
 ***/
VOID        InitSymTab(VOID)
{
WORD        i;
SymbolP     s;
TokenTable  *t;

    for(i=0;i<256;i++)
        SymTab[i] = nil;        // alle Symboleinträge zurücksetzen

    for(t = Token;t->table;t++) {           // alle Token-Tabellen durchgehen
        for(i=0;i<t->tablesize;i++) {       // und für jede Tabelle alle Tokens
            s = FindSymbol(t->table[i].s);  // Opcode in die Tabelle eintragen
            s->type = t->table[i].id;       // ID (<> 0!)
            s->val = ((LONG)t->table[i].p<<16)|s->type; // Parameter und ID gleich zusammenpacken
        }
    }
}

/***
 *  eine Zeile tokenisieren
 ***/
VOID        TokenizeLine(STR sp)
{
STR         sp2;
CHAR        c;
CHAR        stemp[MAXLINELENGTH];
CHAR        maxc;
WORD        base;
Type        typ;
LONG        val;
CHAR        AktUpLine[MAXLINELENGTH];
STR         AktLine = sp;           // Zeilenanfang merken
CommandP    cp = Cmd;               // Ptr auf den Command-Buffer

    sp2=AktUpLine;
    while(*sp2++ = toupper(*sp++)); // in Großbuchstaben wandeln
    sp = AktUpLine;
    while(1) {                      // gesamten String parsen
        while(isspace(c = *sp++));  // Leerzeichen werden überlesen
        if(c == ';') break;         // ab hier: ein Kommentar => fertig
        if(c == 0) break;           // Zeilenende => fertig
        typ = ILLEGAL;              // kein gültiger Typ
        if(isalnum(c)) {            // A…Z, a…z, 0-9
            sp2 = stemp;            // Ptr auf den Anfang
            maxc = 0;               // maximales Zeichen
            base = 0;               // ein String
            do {
                *sp2++ = c;
                if(isalnum(*sp)) {  // noch nicht das letzte Zeichen?
                    if(c > maxc)
                        maxc = c;   // maximales Zeichen merken
                } else {            // letztes Zeichen!
                    if(stemp + 1 != sp2) {          // wenigstens eine Ziffer nötig…
                        if(c == 'D') if(maxc <= '9') base = 10; // "D" hinter der Zahl: Dezimal
                        if(c == 'H') if(maxc <= 'F') base = 16; // "H" hinter der Zahl: Hexadezimal
                        if(c == 'B') if(maxc <= '1') base = 2;  // "B" hinter der Zahl: Binär
                        if(base > 0) sp2--;
                    }
                    if(c >= '0' && c <= '9') if(maxc <= '9') base = 10;
                }
                c = *sp++;          // Folgezeichen holen
            } while(isalnum(c));
            sp--;
            *sp2 = 0;
            if(base > 0) {                  // eine gültige Zahl?
                sp2 = stemp;
                val = 0;
                while(c = *sp2++) {         // Wert einlesen
                    val *= base;            // mal Zahlenbasis
                    val += (c <= '9')?c - '0':c - 'A' + 10;
                }
                typ = NUM;                  // Typ: eine Zahl
            } else {
                if(*stemp >= 'A') {             // erstes Zeichen keine Ziffer?
                    SymbolP sym = FindSymbol(stemp);    // Symbol,Opcodes, etc suchen, ggf. eintragen
                    if(!sym) break;             // Fehler =>
                    if(!sym->type) {            // Typ = Symbol?
                        typ = SYMBOL;
                        val = (LONG)sym;        // Wert = Symboladresse
                        if(!sym->first) {       // Symbol bereits existent =>
                            sym->first = true;  // Symbol implizit definiert
                            sym->defined = false;// Symbol noch nicht definiert
                        }
                    } else {
                        typ = OPCODE;           // ein Opcode
                        val = sym->val;         // Parameter, ID
                    }
                } else
                    Error("Symbole dürfen nicht mit Ziffern anfangen!");
            }
        } else {
            typ = OPCODE;
            switch(c) {
            case '>':
                if(*sp == '>') {
                    val = 0x120;            // >> erkannt
                    sp++;
                }
                break;
            case '<':
                if(*sp == '<') {
                    val = 0x121;            // << erkannt
                    sp++;
                }
                break;
            case '=':
                val = 0x105;                // = entspricht EQU
                break;
            case '\'':  // ASCII-Zeichen in ''
                val = AktLine[sp - AktUpLine];  // ungewandeltes ASCII-Zeichen
                if((!val)||(sp[1]!='\'')) {
                    val = '\'';
                } else {
                    sp++;
                    typ = NUM;              // Typ: eine Zahl
                    if(*sp++ != '\'') break;
                }
                break;
            case '\"':  // ASCII-String in ""
                sp2 = sp;
                base = sp - AktUpLine;      // Offset auf die Zeile
                while(*sp2++ != '\"');      // Stringende suchen
                sp2 = malloc(sp2 - sp);     // Stringbuffer allozieren
                val = (LONG)sp2;
                if(!sp2) break;
                else {
                    while(*sp++ != '\"')    // bis zum Ende der Anführungszeichen
                        *sp2++ = AktLine[base++];   // übertragen
                    *sp2 = 0;
                }
                typ = STRING;               // Typ: ein String
                break;
            default:
                val = c;
            }
        }
#if DEBUG
        printf("Typ:%2.2X Wert:%8.8lX\n",typ,val);
#endif
        cp->typ = typ; cp->val = val;       // in den Command-Buffer übertragen
        cp++;
    }
    cp->typ = 0; cp->val = 0;               // Command-Buffer abschließen
}
