/***
 *  Einen Ausdruck ausrechnen
 ***/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Z80 Assembler.h"

LONG        GetValue(CommandP *c);
LONG        GetExpr(CommandP *c);
LONG        GetTerm(CommandP *c);
LONG        GetCalcTerm(CommandP *c);

SymbolP     ErrSymbol;
RecalcListP LastRecalc;

/***
 *  Ausdruck ab c ausrechnen
 ***/
// Symbol, Zahl oder Klammern holen
LONG        GetValue(CommandP *c)
{
LONG    value = 0;
SymbolP s;

    switch((*c)->typ) {
    case NUM:
        value = (*c)->val;
        break;
    case SYMBOL:
        s = (SymbolP)(*c)->val;     // Symbol-Ptr steht im Wert
        value = s->val;             // Wert vom Symbol holen
        if(!s->defined) {           // Symbol definiert?
            if(!ErrSymbol)          // erstes nicht definiertes Symbol in der Formel?
                ErrSymbol = s;      // Symbol-Ptr merken
        }
        break;
    case OPCODE:
        if((*c)->val == '(') {
            (*c)++;         // Klammer überspringen
            value = GetCalcTerm(c);
            if(((*c)->typ != OPCODE)||((*c)->val != ')')) {
                puts("Klammer zu fehlt!");
            }
        } else
    default:
            puts("Unbekanntes Zeichen in einer Formel");
    }
    (*c)++;                 // Zahl, Symbol, Klammer zu überspringen
    return(value);
}

// Vorzeichen auswerten
LONG        GetExpr(CommandP *c)
{
LONG    value;
Boolean neg = false;
Boolean not = false;

    if((*c)->typ == OPCODE) {
        if((*c)->val == '-') {
            (*c)++;         // Rechenzeichen überspringen
            neg = true;     // negatives Vorzeichen erkannt
        } else if((*c)->val == '+') {
            (*c)++;         // Vorzeichen überspringen
        } else if((*c)->val == '!') {
            (*c)++;         // NOT überspringen
            not = true;     // NOT erkannt
        }
    }
    value = GetValue(c);
    if(neg)                 // negatives Vorzeichen?
        value = -value;     // negieren
    if(not)                 // NOT?
        value = !value;     // invertieren
    return(value);
}

// Punktrechnung
LONG        GetTerm(CommandP *c)
{
LONG    value;
Boolean exit = false;

    value = GetExpr(c);
    while(((*c)->typ == OPCODE)&&!exit) {
        switch((*c)->val) {
        case '*':   (*c)++;             // Rechenzeichen überspringen
                    value *= GetExpr(c);    // Mal
                    break;
        case '/':   (*c)++;             // Rechenzeichen überspringen
                    value /= GetExpr(c);    // Durch
                    break;
        case '%':   (*c)++;             // Rechenzeichen überspringen
                    value %= GetExpr(c);    // Modulo
                    break;
        case '&':   (*c)++;             // Rechenzeichen überspringen
                    value &=  GetExpr(c);   // And
                    break;
        default:    exit = true;
        }
    }
    return(value);
}

// Strichrechnung
LONG        GetCalcTerm(CommandP *c)
{
LONG    value;
Boolean exit = false;

    value = GetTerm(c);
    while(((*c)->typ == OPCODE)&&!exit) {
        switch((*c)->val) {
        case '+':   (*c)++;             // Rechenzeichen überspringen
                    value += GetTerm(c);    // Plus
                    break;
        case '-':   (*c)++;             // Rechenzeichen überspringen
                    value -= GetTerm(c);    // Minus
                    break;
        case '|':   (*c)++;             // Rechenzeichen überspringen
                    value |=  GetExpr(c);   // or
                    break;
        case '^':   (*c)++;             // Rechenzeichen überspringen
                    value ^=  GetExpr(c);   // Xor
                    break;
        case 0x120: (*c)++;             // Rechenzeichen überspringen
                    value >>= GetExpr(c);   // Shift nach rechts
                    break;
        case 0x121: (*c)++;             // Rechenzeichen überspringen
                    value <<= GetExpr(c);   // Shift nach links
                    break;
        default:    exit = true;
        }
    }
    return(value);
}

// Ausdruck ausrechnen
LONG        CalcTerm(CommandP *c)
{
LONG        value;
CommandP    cSave = *c;
CommandP    cp;
LONG        len;
RecalcListP r;

    LastRecalc = nil;                   // Ausdruck (bis jetzt) ok!
    ErrSymbol = nil;                    // nicht definiertes Symbol in der Formel?
    value = GetCalcTerm(c);
    if(ErrSymbol) {                     // min. ein Symbol undefiniert?
        len = (LONG)*c - (LONG)cSave + sizeof(Command); // Platz für Formel + Endekennung
        cp = (CommandP)malloc(len);     // Speicher für die Formel allozieren
        if(!cp) exit(1);                // Speicher reicht nicht!
        memset(cp,0,len);               // Speicher löschen
        memcpy(cp,cSave,(LONG)*c - (LONG)cSave);    // Formel übertragen
        r = (RecalcListP)malloc(sizeof(RecalcList));    // Recalc-Eintrag anfordern
        r->c = cp;                      // Formel einklinken
        r->typ = -1;                    // Typ: illegal (da unbekannt)
        r->adr = 0;                     // Einsetzadresse = 0
        r->next = ErrSymbol->recalc; ErrSymbol->recalc = r; // Ausdruck ans Symbol hängen
        LastRecalc = r;                 // Eintrag merken! (damit der Typ eingesetzt werden kann)
    }
    return(value);
}
