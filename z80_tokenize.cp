/***
 *  Z80 Tokenizer
 ***/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "z80_assembler.h"

uint16_t    CalcHash(const char *name);
SymbolP     FindSymbol(const char* name);
void        InitSymTab(void);


typedef struct {
    int16_t    id;         // ID for the symbol
    const char *s;         // string
    uint16_t   p;          // additional parameter
} ShortSym;


static const ShortSym    Pseudo[] = {
                            { DEFB,"DEFB",0x0000}, { DEFB,"DB",0x0000},
                            { DEFM,"DEFM",0x0000}, { DEFM,"DM",0x0000},
                            { DEFS,"DEFS",0x0000}, { DEFS,"DS",0x0000},
                            { DEFW,"DEFW",0x0000}, { DEFW,"DW",0x0000},
                            { ELSE,"ELSE",0x0000}, { END,"END",0x0000},
                            { ENDIF,"ENDIF",0x0000},{ EQU,"EQU",0x0000}, { IF,"IF",0x0000},
                            { ORG,"ORG",0x0000},  { PRINT,"PRINT",0x0000}
};
// type: (+ 0x200)
// 0x00 : IN,OUT
// 0x01 : 1 byte opcode, no parameter
// 0x02 : 2 byte opcode, no parameter
// 0x03 : 2 byte opcode, (HL) required
// 0x04 : 1.parameter = bit number, 2.parameter = <ea> (BIT,RES,SET)
// 0x05 : IM (one parameter: 0,1,2)
// 0x06 : ADD,ADC,SUB,SBC,AND,XOR,OR,CP
// 0x07 : INC, DEC, like 0x06 with absolute address
// 0x08 : JP, CALL, JR (Warning! Different <ea>!)
// 0x09 : RET (c or nothing)
// 0x0A : RST (00,08,10,18,20,28,30,38)
// 0x0B : DJNZ
// 0x0C : EX: (SP),dreg or DE,HL or AF,AF'
// 0x0D : LD
// 0x0E : PUSH, POP: dreg
// 0x0F : RR,RL,RRC,RLC,SRA,SLA,SRL
static const ShortSym    Opcodes[] = {   { 0x206,"ADC",0x88CE}, { 0x206,"ADD",0x80C6}, { 0x206,"AND",0xA0E6},
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
                            { 0x20F,"SLL",0x3036}, { 0x20F,"SRA",0x282E}, { 0x20F,"SRL",0x383E},
                            { 0x206,"SUB",0x90D6}, { 0x206,"XOR",0xA8EE} };
static const ShortSym    Register[] = {  { 0x307,"A",0x0000}, { 0x323,"AF",0x0000},      // 00…07: B,C,D,E,H,L,(HL),A
                            { 0x300,"B",0x0000}, { 0x310,"BC",0x0000},      // 10…13: BC,DE,HL,SP
                            { 0x301,"C",0x0000}, { 0x302,"D",0x0000},       //    23:         ,AF
                            { 0x311,"DE",0x0000},{ 0x303,"E",0x0000},       // 30…31: IX,IY
                            { 0x304,"H",0x0000}, { 0x312,"HL",0x0000},      // 40…41: R,I
                            { 0x341,"I",0x0000}, { 0x330,"IX",0x0000},      // 54…55: X,HX
                            { 0x331,"IY",0x0000},{ 0x305,"L",0x0000},       // 64…65: Y,HY
                            { 0x340,"R",0x0000}, { 0x313,"SP",0x0000},
                            { 0x355,"X",0x0000}, { 0x354,"HX",0x0000},
                            { 0x365,"Y",0x0000}, { 0x364,"HY",0x0000} };
static const ShortSym    Conditions[] = {/*{ 0x403,"C",0x0000},*/{ 0x407,"M",0x0000},    // Condition C = Register C!
                            { 0x402,"NC",0x0000},{ 0x400,"NZ",0x0000},
                            { 0x406,"P",0x0000},{ 0x405,"PE",0x0000},
                            { 0x404,"PO",0x0000},{ 0x401,"Z",0x0000} };


typedef struct {
    const ShortSym    *table;     // ptr to an opcode list
    int16_t     tablesize;  // length of the table in bytes
} TokenTable;

static const TokenTable  Token[] = { { Pseudo,sizeof(Pseudo)/sizeof(ShortSym) },
                        { Opcodes,sizeof(Opcodes)/sizeof(ShortSym) },
                        { Register,sizeof(Register)/sizeof(ShortSym) },
                        { Conditions,sizeof(Conditions)/sizeof(ShortSym) },
                        { 0,0 }
                        };

Command     Cmd[80];            // a tokenized line
SymbolP     SymTab[256];        // symbol table (split with the hash byte)

/***
 *  calculate a simple hash for a string
 ***/
uint16_t CalcHash(const char *name)
{
uint16_t hash_val = 0;
uint16_t i;
uint8_t  c;

    while((c = *name++) != 0) {
#if 0
        hash_val += c;
#else
        hash_val = (hash_val << 4) + c;
        if((i = (hash_val >> 12)) != 0)
            hash_val ^= i;
#endif
    }
    return hash_val;
}

/***
 *  search for a symbol, generate one if it didn't already exist.
 ***/
SymbolP     FindSymbol(const char* name)
{
uint16_t    hash = CalcHash(name);      // hash value for the name
uint8_t     hashb = hash;
SymbolP     s;

    s = SymTab[hashb];                  // ptr to the first symbol
    while(s) {
        if(s->hash == hash)             // search for a matching hash
            if(!strcmp(s->name,name)) return s; // found the symbol?
        s = s->next;                    // to the next symbol
    }
    s = (SymbolP)malloc(sizeof(Symbol)); // allocate memory for a symbol
    if(!s) return nullptr;              // not enough memory
    memset(s,0,sizeof(Symbol));
    s->next = SymTab[hashb]; SymTab[hashb] = s; // link the symbol into the list
    s->hash = hash;
    strcpy(s->name,name);               // and copy the name
    return s;
}

/***
 *  initialize the symbol table
 ***/
void        InitSymTab(void)
{
int16_t     i;
SymbolP     s;
const TokenTable  *t;

    for(i=0;i<256;i++)
        SymTab[i] = nullptr; // reset all entries

    for(t = Token;t->table;t++) {           // check all token tables
        for(i=0;i<t->tablesize;i++) {       // and all tokens for a single table
            s = FindSymbol(t->table[i].s);  // add all opcodes to the symbol table
            s->type = t->table[i].id;       // ID (<> 0!)
            s->val = ((int32_t)t->table[i].p<<16)|s->type; // merge parameter and id
        }
    }
}

// Is this an alphanumeric character _or_ an unterline, which is a valid symbol
int isalnum_(char c)
{
    return isalnum(c) || c == '_';
}

/***
 *  tokenize a single line
 ***/
void        TokenizeLine(char* sp)
{
char        *sp2;
char        c;
char        stemp[MAXLINELENGTH];
char        maxc;
int16_t     base;
bool        dollar;
Type        typ;
long        val;
char        AktUpLine[MAXLINELENGTH];
char        *AktLine = sp;          // remember the beginning of the line
CommandP    cp = Cmd;               // ptr to the command buffer

    sp2=AktUpLine;
    while((*sp2++ = toupper(*sp++))); // convert to capital letters
    sp = AktUpLine;
    while(1) {                      // parse the whole string
        while((isspace(c = *sp++))) ;  // ignore spaces
        if(c == ';') break;         // a comment => ignore the rest of the line
        if(c == 0) break;           // end of line => done
        typ = ILLEGAL;              // default: an illegal type
        base = 0;
        dollar = false;
        if (c == '$') {             // PC or the beginning of a hex number
            if (isalnum(*sp) && *sp <= 'F') {
                base = 16;
                c = *sp++;
            } else
                dollar = true;
        } else if ( strlen( sp ) > 2 && c == '0' && *sp == 'X' && isxdigit( sp[ 1 ] ) ) { // 0x.. ?
            sp++;      // skip 0X
            c = *sp++; // 1st hex digit
            base = 16;
        }
        if (dollar) {
            typ = NUM;
            val = PC;
        } else if (isalnum_(c)) {   // A…Z, a…z, 0-9
            sp2 = stemp;            // ptr to the beginning
            maxc = 0;               // highest ASCII character
            do {
                *sp2++ = c;
                if(isalnum_(*sp)) { // not the last character?
                    if(c > maxc)
                        maxc = c;   // remember the highest ASCII character
                } else {            // last character
                    if ( base == 16 ) {
                        base = ( maxc <= 'F' && c <= 'F' ) ? 16 : 0; // invalid hex digits?
                    } else if (stemp + 1 != sp2) {          // at least one character
                        if (c == 'H' && maxc <= 'F') base = 16; // "H" after a number: hexadecimal number
                        else if (c == 'D' && maxc <= '9') base = 10; // "D" after a number: decimal number
                        else if (c == 'B' && maxc <= '1') base = 2;  // "B" after a number: binary number
                        if (base > 0) --sp2;
                    }
                    if (!base && c >= '0' && c <= '9' && maxc <= '9') base = 10;
                }
                c = *sp++;          // get the next character
            } while(isalnum_(c));
            sp--;
            *sp2 = 0;
            if(base > 0) {                  // a valid number?
                sp2 = stemp;
                val = 0;
                while((c = *sp2++) != 0) {      // read the value
                    val *= base;                // multiply with the number base
                    val += (c <= '9')?c - '0':c - 'A' + 10;
                }
                typ = NUM;                      // type: a number
            } else {
                if(*stemp >= 'A') {             // first character not a digit?
                    SymbolP sym = FindSymbol(stemp);    // an opcode or a symbol
                    if(!sym) break;             // error (out of memory)
                    if(!sym->type) {            // typ = symbol?
                        typ = SYMBOL;
                        val = (long)sym;        // value = address of the symbol ptr
                        if(!sym->first) {       // symbol already exists?
                            sym->first = true;  // no, then implicitly define it
                            sym->defined = false;// symbol value not defined
                        }
                    } else {
                        typ = OPCODE;           // an opcode
                        val = sym->val;         // parameter, ID
                    }
                } else
                    Error("symbols can't start with digits");
            }
        } else {
            typ = OPCODE;
            switch(c) {
            case '>':
                if(*sp == '>') {
                    val = 0x120;            // >> recognized
                    sp++;
                }
                break;
            case '<':
                if(*sp == '<') {
                    val = 0x121;            // << recognized
                    sp++;
                }
                break;
            case '=':
                val = 0x105;                // = matches EQU
                break;
            case '\'':  // an ASCII character with '.'
                val = AktLine[sp - AktUpLine];  // not capitalized ASCII character
                if((!val)||(sp[1]!='\'')) {
                    val = '\'';
                } else {
                    sp++;
                    typ = NUM;              // typ: a number
                    if(*sp++ != '\'') break;
                }
                break;
            case '\"':  // an ASCII string with "..."
                sp2 = sp;
                base = sp - AktUpLine;      // offset to the line
                while(*sp2++ != '\"');      // search for the end of the string
                sp2 = (char *)malloc(sp2 - sp); // allocate a buffer for the string
                val = (long)sp2;
                if(!sp2) break;
                else {
                    while(*sp++ != '\"')    // end of the string found?
                        *sp2++ = AktLine[base++];   // copy characters
                    *sp2 = 0;
                }
                typ = STRING;               // type: a string
                break;
            default:
                val = c;
            }
        }
#if DEBUG
        printf("type:%2.2X value:%8.8lX\n",typ,val);
#endif
        cp->typ = typ; cp->val = val;      // copy into the command buffer
        cp++;
    }
    cp->typ = ILLEGAL; cp->val = 0;        // terminate the command buffer
}
