/***
 *  Calculate a formula
 ***/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "z80_assembler.h"

int32_t GetValue(CommandP *c);
int32_t GetExpr(CommandP *c);
int32_t GetTerm(CommandP *c);
int32_t GetCalcTerm(CommandP *c);

SymbolP     ErrSymbol;
RecalcListP LastRecalc;

// Get a symbol, number or bracket
int32_t GetValue(CommandP *c)
{
int32_t value = 0;
SymbolP s;

    switch((*c)->typ) {
    case NUM:
        value = (*c)->val;
        break;
    case SYMBOL:
        s = (SymbolP)(*c)->val;     // Symbol ptr is in the value
        value = s->val;             // value of the symbol
        if(!s->defined) {           // is the symbol defined?
            if(!ErrSymbol)          // Already an undefined symbol?
                ErrSymbol = s;      // remember this symbol, if not
        }
        break;
    case OPCODE:
        if((*c)->val == '(') {
            (*c)++;         // Skip opening bracket
            value = GetCalcTerm(c);
            if(((*c)->typ != OPCODE)||((*c)->val != ')')) {
                Error("Closing bracket is missing");
            }
        } else
    default:
        Error("Illegal symbol in a formula");
    }
    (*c)++;                 // skip value, symbol or bracket
    return value;
}

// interpret a sign
int32_t GetExpr(CommandP *c)
{
int32_t value;
bool    negOp = false;
bool    notOp = false;

    if((*c)->typ == OPCODE) {
        if((*c)->val == '-') {
            (*c)++;         // skip the sign
            negOp = true;   // negative operator detected
        } else if((*c)->val == '+') {
            (*c)++;         // skip the sign
        } else if((*c)->val == '!') {
            (*c)++;         // skip the sign
            notOp = true;   // NOT operator detected
        }
    }
    value = GetValue(c);
    if(negOp)               // negative operator?
        value = -value;     // negate
    if(notOp)               // NOT operator?
        value = !value;     // invertieren
    return value;
}

// multiplications, etc.
int32_t GetTerm(CommandP *c)
{
int32_t value;
bool    exit = false;

    value = GetExpr(c);
    while(((*c)->typ == OPCODE)&&!exit) {
        switch((*c)->val) {
        case '*':   (*c)++;             // skip operator
                    value *= GetExpr(c);    // Multiply
                    break;
        case '/':   (*c)++;             // skip operator
                    value /= GetExpr(c);    // Divide
                    break;
        case '%':   (*c)++;             // skip operator
                    value %= GetExpr(c);    // Modulo
                    break;
        case '&':   (*c)++;             // skip operator
                    value &=  GetExpr(c);   // And operator
                    break;
        default:    exit = true;
        }
    }
    return value;
}

// addition, etc.
int32_t GetCalcTerm(CommandP *c)
{
int32_t value;
bool    exit = false;

    value = GetTerm(c);
    while(((*c)->typ == OPCODE)&&!exit) {
        switch((*c)->val) {
        case '+':   (*c)++;             // skip operator
                    value += GetTerm(c);    // plus
                    break;
        case '-':   (*c)++;             // skip operator
                    value -= GetTerm(c);    // minus
                    break;
        case '|':   (*c)++;             // skip operator
                    value |=  GetExpr(c);   // or
                    break;
        case '^':   (*c)++;             // skip operator
                    value ^=  GetExpr(c);   // Xor
                    break;
        case 0x120: (*c)++;             // skip operator
                    value >>= GetExpr(c);   // shift to the right
                    break;
        case 0x121: (*c)++;             // skip operator
                    value <<= GetExpr(c);   // shift to the left
                    break;
        default:    exit = true;
        }
    }
    return value;
}

// Calculate a term
int32_t CalcTerm(CommandP *c)
{
int32_t     value;
CommandP    cSave = *c;
CommandP    cp;
int32_t     len;
RecalcListP r;

    LastRecalc = nullptr;               // expression so far ok
    ErrSymbol = nullptr;                // no undefined symbol in formula
    value = GetCalcTerm(c);
    if(ErrSymbol) {                     // at least one symbol is undefined?
        len = (long)*c - (long)cSave + sizeof(Command); // space for the formula and end-marker
        cp = (CommandP)malloc(len);     // allocate memory for the formular
        if(!cp) exit(1);                // not enough memory
        memset(cp,0,len);               // erase memory
        memcpy(cp,cSave,(long)*c - (long)cSave);    // transfer the formular
        r = (RecalcListP)malloc(sizeof(RecalcList));    // allocate a recalculation list entry
        r->c = cp;                      // link to the formula
        r->typ = -1;                    // type: illegal (because unknown)
        r->adr = 0;                     // address to patch = 0
        r->next = ErrSymbol->recalc; ErrSymbol->recalc = r; // link expression to symbol
        LastRecalc = r;                 // save entry to correct the typ
    }
    return value;
}
