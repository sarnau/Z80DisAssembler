/***
 *  eine tokenisierte Zeile übersetzen
 ***/

#include <stdio.h>
#include <stdlib.h>
#include "Z80 Assembler.h"

VOID            DoPseudo(CommandP *cp);
VOID            DoOpcode(CommandP *cp);
WORD            GetOperand(CommandP *cp,LONG *value);

/***
 *  Operanden holen
 ***/
WORD            GetOperand(CommandP *c,LONG *value)
{
WORD        typ;
WORD        val,sval;

    LastRecalc = nil;   // (sicher ist sicher: Recalc-Eintrag löschen)
    *value = 0;
    typ = (*c)->typ;val = (*c)++->val;                      // Wert und Typ holen
    if(typ == OPCODE) {
        if((val >= 0x300)&&(val <= 0x4FF)) {
            if((val == 0x323)&&((*c)->typ == OPCODE)&&((*c)->val == '\'')) {    // AF'?
                (*c)++;                                     // ' überspringen
                val = 0x324;                                // AF' zurückgeben
            }
            return(val);                                    // Register bzw. Conditions
        }
        if((val >= 0x100)&&(val <= 0x2FF))                  // ein Opcode!
            Error("Illegaler Operand!");
        if((val == '('))    {                               // Indirekte Adreßierung?
            typ = (*c)->typ;sval = (*c)++->val;             // Wert und Typ holen
            if(typ == OPCODE) {
                if(((sval & 0xFF0) == 0x310)||(sval == 0x301)) {// Register
                    typ = (*c)->typ;val = (*c)++->val;      // Wert und Typ holen
                    if((typ == OPCODE)&&(val == ')')) {
                        if(sval == 0x312)                   // (HL)?
                            return(0x306);                  // zum besseren Zusammensetzen!!!
                        return(sval + 0x200);               // (C),(BC),(DE),(SP)
                    }
                    Error("Klammer zu nach (BC,(DE,(HL oder (SP fehlt!");
                }
                if((sval & 0xFF0) == 0x330) {               // IX,IY
                    if((*c)->typ == OPCODE) {               // folgt ein Rechenzeichen?
                        val = (*c)->val;
                        if((val == '+')||(val == '-')) {
                            *value = CalcTerm(c);
                            typ = (*c)->typ;val = (*c)++->val;  // Klammer zu holen
                            if((typ == OPCODE)&&(val == ')'))
                                return(sval + 0x300);       // (IX+d) oder (IY+d)
                            Error("Klammer zu nach (IX oder (IY fehlt!");
                        } else {
                            if((typ == OPCODE)&&(val == ')')) {
                                (*c)++;                     // Klammer zu überlesen
                                return(sval + 0x200);       // (IX) oder (IY)
                            }
                            Error("Klammer zu nach (IX oder (IY fehlt!");
                        }
                    } else
                        Error("Illegales Zeichen nach (IX oder (IY");
                }
            }
            (*c)--;     // Ptr auf das vorherige Zeichen zurück
            *value = CalcTerm(c);
            typ = (*c)->typ;val = (*c)++->val;      // Klammer zu holen
            if((typ == OPCODE)&&(val == ')'))
                return(0x280);                      // (Adr)
            Error("Klammer zu nach (Adr) fehlt!");
        }
    }                                       // Absolute Adreßierung
    (*c)--;                                 // Ptr auf das vorherige Zeichen zurück
    *value = CalcTerm(c);
    return(0x281);                          // Adr
}

/***
 *  Opcode abtesten
 ***/
VOID            DoOpcode(CommandP *cp)
{
CommandP    c = *cp;
UCHAR       *iRAM = RAM + PC;
ULONG       op0;
UBYTE       Op0_24,Op0_16;
WORD        op1,op2;
LONG        value1,value2;
RecalcListP op1Recalc,op2Recalc;

    op0 = c++->val;                             // Opcode
    op1 = op2 = 0;
    if(c->typ) {
        op1 = GetOperand(&c,&value1);           // 1. Operand holen
        op1Recalc = LastRecalc;                 // evtl. Recalc-Eintrag
        if((c->typ == OPCODE)&&(c->val == ',')) {   // ein 2. Operand?
            c++;
            op2 = GetOperand(&c,&value2);       // diesen auch holen
            op2Recalc = LastRecalc;             // evtl. Recalc-Eintrag
        }
    }
    Op0_24 = op0 >> 24;
    Op0_16 = op0 >> 16;
    switch(op0 & 0xFF) {        // Opcode
    case 0x00:                              // IN/OUT
                if(Op0_24 & 0x01) {         // OUT?
                    LONG    t;
                    t = op1; op1 = op2; op2 = t;                // Operanden drehen
                    t = value1; value1 = value2; value2 = t;
                    t = (LONG)op1Recalc; op1Recalc = op2Recalc; op2Recalc = (RecalcListP)t;
                }
                if(((op1 & 0xFF0)== 0x300)&&(op2 == 0x501)) {   // IN ?,(C) bzw. OUT (C),?
                    if(op1 == 0x306)
                        Error("IN (HL),(C) bzw. OUT (C),(HL) geht nicht!");
                    else {
                        *iRAM++ = 0xED;
                        *iRAM++ = Op0_24|((op1 & 0x7) << 3);
                    }
                } else if((op1 == 0x307)&&(op2 == 0x280)) {     // IN A,(n) bzw. OUT (n),A
                    *iRAM++ = Op0_16;
                    if(op2Recalc) {         // Ausdruck undefiniert?
                        op2Recalc->typ = 0; // ein Byte einsetzen
                        op2Recalc->adr = iRAM - RAM;
                        op2Recalc = nil;    // Term eingesetzt
                    }
                    *iRAM++ = value2;
                } else
                    Error("Operanden bei IN/OUT nicht erlaubt!");
                break;
    case 0x01:                              // 1.Byte Befehl ohne Parameter
                if(op1|op2)                             // Operanden angegeben?
                    Error("Keine Operanden erlaubt");   // das ist falsch!
                else
                    *iRAM++ = Op0_24;
                break;
    case 0x02:                              // 2.Byte Befehl ohne Parameter
                if(op1|op2)                             // Operanden angegeben?
                    Error("Keine Operanden erlaubt");   // das ist falsch!
                else {
                    *iRAM++ = Op0_24;
                    *iRAM++ = Op0_16;
                }
                break;
    case 0x03:  if(((op1 != 0x306)&&(op1))||(op2))      // RRD (HL) oder RLD (HL)
                    Error("Falscher Operand!");
                else {
                    *iRAM++ = Op0_24;
                    *iRAM++ = Op0_16;
                }
                break;
    case 0x04:  // 1.Parameter = Bitno., 2.Parameter = <ea> (BIT,RES,SET)
                if((op1 != 0x281)||(value1 < 0)||(value1 > 7))
                    Error("1.Operand muß 0…7 sein!");
                else {
                    if((op2 & 0xFF0) == 0x300) {    // A,B,C,D,E,H,L,(HL)
                        *iRAM++ = Op0_24;
                        *iRAM++ = Op0_16|(value1 << 3)|(op2 & 0x07);
                    } else if((op2 & 0xFF0) == 0x630) { // (IX+d) oder (IY+d)
                        *iRAM++ = (op2 & 0x01)?0xFD:0xDD;
                        *iRAM++ = Op0_24;
                        if(op2Recalc) {         // Ausdruck undefiniert?
                            op2Recalc->typ = 0; // ein Byte einsetzen
                            op2Recalc->adr = iRAM - RAM;
                            op2Recalc = nil;    // Term eingesetzt
                        }
                        *iRAM++ = value2;
                        *iRAM++ = Op0_16|(value1 << 3)|6;
                    } else
                        Error("2.Operand falsch");
                }
                break;
    case 0x05:  // IM (ein Parameter: 0,1,2)
                if((op1 != 0x281)||(op2))
                    Error("Operandenangabe falsch");
                else {
                    if((value1 < 0)||(value1 > 2))
                        Error("Nur 0, 1 oder 2 erlaubt!");
                    else {
                        if(value1 > 0) value1++;
                        *iRAM++ = Op0_24;
                        *iRAM++ = Op0_16|((value1 & 0x07)<<3);
                    }
                }
                break;

    case 0x06:  // ADD,ADC,SUB,SBC,AND,XOR,OR,CP
                switch(op1) {
                case 0x312: // HL
                    if((op2 >= 0x310)&&(op2 <= 0x313)) {    // BC,DE,HL,SP
                        switch(Op0_24) {
                        case 0x80:  // ADD
                            *iRAM++ = 0x09|((op2 & 0x03)<<4);
                            break;
                        case 0x88:  // ADC
                            *iRAM++ = 0xED;
                            *iRAM++ = 0x4A|((op2 & 0x03)<<4);
                            break;
                        case 0x98:  // SBC
                            *iRAM++ = 0xED;
                            *iRAM++ = 0x42|((op2 & 0x03)<<4);
                            break;
                        default:
                            Error("Befehl mit dieser <ea> nicht möglich");
                        }
                    } else
                        Error("Doppelregister erwartet");
                    break;
                case 0x330: // IX
                case 0x331: // IY
                    switch(op2) {
                    case 0x310: // BC
                        *iRAM++ = (op1 & 0x01)?0xFD:0xDD;
                        *iRAM++ = 0x09;
                        break;
                    case 0x311: // DE
                        *iRAM++ = (op1 & 0x01)?0xFD:0xDD;
                        *iRAM++ = 0x19;
                        break;
                    case 0x330: // IX
                    case 0x331: // IY
                        if(op1 == op2) {
                            *iRAM++ = (op1 & 0x01)?0xFD:0xDD;
                            *iRAM++ = 0x29;
                        } else
                            Error("Nur ADD IX,IY oder ADD IY,IY möglich!");
                        break;
                    case 0x313: // SP
                        *iRAM++ = (op1 & 0x01)?0xFD:0xDD;
                        *iRAM++ = 0x39;
                        break;
                    default:
                        Error("Befehl mit dieser <ea> nicht möglich");
                    }
                    break;
                default:
                    if((op1 == 0x307)&&(op2)) { // Akku?
                        op1 = op2;
                        value1 = value2;        // 2. Operanden nach vorne schieben
                        op1Recalc = op2Recalc;
                        op2Recalc = nil;
                    }
                    switch(op1 & 0xFF0) {
                    case 0x350: // X,HX
                                *iRAM++ = 0xDD;
                                *iRAM++ = Op0_24|(op1 & 7);
                                break;
                    case 0x360: // Y,HY
                                *iRAM++ = 0xFD;
                                *iRAM++ = Op0_24|(op1 & 7);
                                break;
                    case 0x300: // A,B,C,D,E,H,L,(HL)
                                *iRAM++ = Op0_24|(op1 & 7);
                                break;
                    case 0x630: // (IX+d) oder (IY+d)
                                *iRAM++ = (op1 & 0x01)?0xFD:0xDD;
                                *iRAM++ = Op0_24|6;
                                if(op1Recalc) {         // Ausdruck undefiniert?
                                    op1Recalc->typ = 0; // ein Byte einsetzen
                                    op1Recalc->adr = iRAM - RAM;
                                    op1Recalc = nil;    // Term eingesetzt
                                }
                                *iRAM++ = value1;
                                break;
                    case 0x280: // n
                                if(op1 == 0x281) {
                                    *iRAM++ = Op0_16;
                                    if(op1Recalc) {         // Ausdruck undefiniert?
                                        op1Recalc->typ = 0; // ein Byte einsetzen
                                        op1Recalc->adr = iRAM - RAM;
                                        op1Recalc = nil;    // Term eingesetzt
                                    }
                                    *iRAM++ = value1;
                                    break;
                                }
                    default:
                                Error("2.Operand falsch");
                    }
                }
                break;
    case 0x07:  // INC, DEC, wie 0x06 nur ohne absolute Adr.
                if(op2)
                    Error("2.Operand nicht erlaubt!");
                if((op1 & 0xFF0) == 0x300) {        // A,B,C,D,E,H,L,(HL)
                    *iRAM++ = Op0_24|((op1 & 7)<<3);
                } else if((op1 & 0xFF0) == 0x630) { // (IX+d) oder (IY+d)
                    *iRAM++ = (op1 & 1)?0xFD:0xDD;
                    *iRAM++ = Op0_24|(6<<3);
                    if(op1Recalc) {         // Ausdruck undefiniert?
                        op1Recalc->typ = 0; // ein Byte einsetzen
                        op1Recalc->adr = iRAM - RAM;
                        op1Recalc = nil;    // Term eingesetzt
                    }
                    *iRAM++ = value1;
                } else {
                    Boolean     decFlag = Op0_24 & 1;   // True: DEC, False: INC
                    switch(op1) {
                    case 0x354: // HX
                            *iRAM++ = 0xDD;
                            *iRAM++ = decFlag?0x25:0x24;
                            break;
                    case 0x355: // X
                            *iRAM++ = 0xDD;
                            *iRAM++ = decFlag?0x2D:0x2C;
                            break;
                    case 0x364: // HY
                            *iRAM++ = 0xFD;
                            *iRAM++ = decFlag?0x25:0x24;
                            break;
                    case 0x365: // Y
                            *iRAM++ = 0xFD;
                            *iRAM++ = decFlag?0x2D:0x2C;
                            break;
                    case 0x310: // BC
                            *iRAM++ = decFlag?0x0B:0x03;
                            break;
                    case 0x311: // DE
                            *iRAM++ = decFlag?0x1B:0x13;
                            break;
                    case 0x312: // HL
                            *iRAM++ = decFlag?0x2B:0x23;
                            break;
                    case 0x313: // HL
                            *iRAM++ = decFlag?0x3B:0x33;
                            break;
                    case 0x330: // IX
                            *iRAM++ = 0xDD;
                            *iRAM++ = decFlag?0x2B:0x23;
                            break;
                    case 0x331: // IY
                            *iRAM++ = 0xFD;
                            *iRAM++ = decFlag?0x2B:0x23;
                            break;
                    default:
                            Error("Adressierungsart nicht erlaubt!");
                    }
                }
                break;
    case 0x08:  // JP, CALL, JR (Achtung! Unterschiedliche <ea>!)
                if(op1 == 0x301) op1 = 0x403;   // Register C in Condition C wandeln
                switch(Op0_24) {
                case 0xC2:  // JP
                        if((op1 >= 0x400)&&(op1 <= 0x4FF)&&(op2 == 0x281)) {    // Cond,Adr
                            *iRAM++ = Op0_24|((op1 & 0x07)<<3);
                            if(op2Recalc) {         // Ausdruck undefiniert?
                                op2Recalc->typ = 1; // zwei Byte einsetzen
                                op2Recalc->adr = iRAM - RAM;
                                op2Recalc = nil;    // Term eingesetzt
                            }
                            *iRAM++ = value2;
                            *iRAM++ = value2 >> 8;
                        } else if((op1 == 0x306)&&!op2) {   // JP (HL)
                            *iRAM++ = 0xE9;
                        } else if((op1 == 0x530)&&!op2) {   // JP (IX)
                            *iRAM++ = 0xDD;
                            *iRAM++ = 0xE9;
                        } else if((op1 == 0x531)&&!op2) {   // JP (IY)
                            *iRAM++ = 0xFD;
                            *iRAM++ = 0xE9;
                        } else if((op1 == 0x281)|!op2) {    //  JP Adr
                            *iRAM++ = Op0_16;
                            if(op1Recalc) {         // Ausdruck undefiniert?
                                op1Recalc->typ = 1; // zwei Byte einsetzen
                                op1Recalc->adr = iRAM - RAM;
                                op1Recalc = nil;    // Term eingesetzt
                            }
                            *iRAM++ = value1;
                            *iRAM++ = value1 >> 8;
                        } else
                            Error("1.Operand falsch");
                        break;
                case 0x20:  // JR
                        if((op1 >= 0x400)&&(op1 <= 0x403)&&(op2 == 0x281)) {    // Cond,Adr
                            *iRAM++ = Op0_24|((op1 & 0x07)<<3);
                            if(op2Recalc) {         // Ausdruck undefiniert?
                                op2Recalc->typ = 2; // ein PC-rel-Byte einsetzen
                                op2Recalc->adr = iRAM - RAM;
                                op2Recalc = nil;    // Term eingesetzt
                            }
                            *iRAM++ = value2 - (iRAM - RAM) - 1;
                        } else if((op1 == 0x281)&&!op2) {   //  JR Adr
                            *iRAM++ = Op0_16;
                            if(op1Recalc) {         // Ausdruck undefiniert?
                                op1Recalc->typ = 2; // ein PC-rel-Byte einsetzen
                                op1Recalc->adr = iRAM - RAM;
                                op1Recalc = nil;    // Term eingesetzt
                            }
                            *iRAM++ = value1 - (iRAM - RAM) - 1;
                        } else
                            Error("Bedingung nicht erlaubt!");
                        break;
                case 0xC4:  // CALL
                        if((op1 >= 0x400)&&(op1 <= 0x4FF)&&(op2 == 0x281)) {    // Cond,Adr
                            *iRAM++ = Op0_24|((op1 & 0x07)<<3);
                            if(op2Recalc) {         // Ausdruck undefiniert?
                                op2Recalc->typ = 1; // zwei Byte einsetzen
                                op2Recalc->adr = iRAM - RAM;
                                op2Recalc = nil;    // Term eingesetzt
                            }
                            *iRAM++ = value2;
                            *iRAM++ = value2 >> 8;
                        } else if((op1 == 0x281)&&!op2) {   //  CALL Adr
                            *iRAM++ = Op0_16;
                            if(op1Recalc) {         // Ausdruck undefiniert?
                                op1Recalc->typ = 1; // zwei Byte einsetzen
                                op1Recalc->adr = iRAM - RAM;
                                op1Recalc = nil;    // Term eingesetzt
                            }
                            *iRAM++ = value1;
                            *iRAM++ = value1 >> 8;
                        } else
                            Error("1.Operand falsch");
                        break;
                default:
                        Error("Opcode-Tabelle defekt!");
                }
                break;
    case 0x09:  if(op2)                         // RET-Befehl
                    Error("zu viele Operanden");
                else if(!op1)                   // keine Condition angegeben?
                    *iRAM++ = Op0_16;           // normalen Opcode nehmen
                else {
                    if(op1 == 0x301) op1 = 0x403;   // Register C in Condition C wandeln
                    if((op1 & 0xF00) != 0x400)
                        Error("falscher Operand angegeben!");
                    else
                        *iRAM++ = Op0_24|((op1 & 0x07)<<3);
                }
                break;
    case 0x0A:  // RST (00,08,10,18,20,28,30,38)
                if(op2)
                    Error("zu viele Operanden");
                else if(op1 == 0x281) {     // n
                    WORD    i = -1;
                    switch(value1) {
                    case 0: i = 0x00;
                            break;
                    case 1:
                    case 8: i = 0x08;
                            break;
                    case 2:
                    case 10:i = 0x10;
                            break;
                    case 3:
                    case 18:i = 0x18;
                            break;
                    case 4:
                    case 20:i = 0x20;
                            break;
                    case 5:
                    case 28:i = 0x28;
                            break;
                    case 6:
                    case 30:i = 0x30;
                            break;
                    case 7:
                    case 38:i = 0x38;
                            break;
                    default:
                            Error("Nur 00,08,10,18,20,28,30,38 ist erlaubt!");
                    }
                    if(i>=0)
                        *iRAM++ = Op0_24|i;
                } else
                    Error("Adressierungsart nicht erlaubt!");
                break;
    case 0x0B:  // DJNZ
                *iRAM++ = Op0_24;
                if(op1Recalc) {         // Ausdruck undefiniert?
                    op1Recalc->typ = 2; // ein PC-rel-Byte einsetzen
                    op1Recalc->adr = iRAM - RAM;
                    op1Recalc = nil;    // Term eingesetzt
                }
                *iRAM++ = value1 - (iRAM - RAM) - 1;        // relozieren
                break;
    case 0x0C:  // EX: (SP),dreg oder DE,HL oder AF,AF'
                if((op1 == 0x311)&&(op2 == 0x312))      // EX DE,HL
                    *iRAM++ = 0xEB;
                else if((op1 == 0x323)&&(op2 == 0x324)) {   // EX AF,AF'
                    *iRAM++ = 0x08;
                }
                else if((op1 == 0x513)&&(op2 == 0x312))     // EX (SP),HL
                    *iRAM++ = 0xE3;
                else if((op1 == 0x513)&&(op2 == 0x330)) {   // EX (SP),IX
                    *iRAM++ = 0xDD;
                    *iRAM++ = 0xE3;
                }
                else if((op1 == 0x513)&&(op2 == 0x331)) {   // EX (SP),IY
                    *iRAM++ = 0xFD;
                    *iRAM++ = 0xE3;
                }
                else
                    Error("Operanden bei Exchange nicht möglich!");
                break;
    case 0x0D:  // LD
                if(!(op1 & op2))
                    Error("Operand fehlt!");
                else {
                    UBYTE   FirstByte = 0;
                    switch(op1) {
                    case 0x530: // LD (IX),
                    case 0x531: // LD (IY),
                            op1 = (op1 == 0x530)?0x356:0x366;
                    case 0x354: //  HX
                    case 0x355: //  X
                    case 0x364: //  HY
                    case 0x365: //  Y
                            FirstByte = ((op1 & 0xFF0)==0x350)?0xDD:0xFD;
                            *iRAM++ = FirstByte;
                            op1 &= 0xF0F;   // auf H und L ummappen
                    case 0x300: // B
                    case 0x301: // C
                    case 0x302: // D
                    case 0x303: // E
                    case 0x304: // H
                    case 0x305: // L
                    case 0x306: // (HL)
                    case 0x307: // A
                            switch(op2 & 0xFF0) {
                            case 0x530: // LD <ea>,(IX), bzw. LD <ea>,(IY)
                                    op2 = (op2 == 0x530)?0x356:0x366;
                            case 0x350:     // X,HX
                            case 0x360:     // Y,HY
                                    {
                                    Boolean flag = ((op2 & 0xFF0)==0x350);
                                    switch(FirstByte) {
                                    case 0xDD:  // IX
                                            if(!flag)
                                                Error("IX,IY geht nicht!");
                                            break;
                                    case 0xFD:  // IY
                                            if(flag)
                                                Error("IY,IX geht nicht!");
                                            break;
                                    default:    // noch nix
                                            *iRAM++ = (flag)?0xDD:0xFD;
                                            break;
                                    }
                                    }
                                    op2 &= 0xF0F;   // auf H und L ummappen
                            case 0x300:     // B,C,D,E,H,L,(HL),A
                                    *iRAM++ = 0x40|((op1 & 0x07)<<3)|(op2 & 0x07);
                                    break;
                            case 0x510:     // (BC),(DE),(SP)
                                    if(op1 == 0x307) {
                                        if(op2 == 0x510)
                                            *iRAM++ = 0x0A;
                                        else if(op2 == 0x511)
                                            *iRAM++ = 0x1A;
                                        else
                                            Error("(SP) nicht erlaubt");
                                    } else
                                        Error("nur LD A,(BC) oder LD A,(DE) möglich!");
                                    break;
                            case 0x630: // (IX+d), (IY+d)
                                    if(op1 != 0x306) {  // (HL)
                                        *iRAM++ = (op2 & 0x01)?0xFD:0xDD;
                                        *iRAM++ = 0x46|((op1 & 0x07)<<3);
                                        if(op2Recalc) {         // Ausdruck undefiniert?
                                            op2Recalc->typ = 0; // ein Byte einsetzen
                                            op2Recalc->adr = iRAM - RAM;
                                            op2Recalc = nil;    // Term eingesetzt
                                        }
                                        *iRAM++ = value2;
                                    } else
                                        Error("LD (HL),(IX/IY+d) nicht erlaubt!");
                                    break;
                            case 0x280: // (n), n
                                    if(op2 == 0x281) {
                                        *iRAM++ = 0x06|((op1 & 0x07)<<3);
                                        if(op2Recalc) {         // Ausdruck undefiniert?
                                            op2Recalc->typ = 0; // ein Byte einsetzen
                                            op2Recalc->adr = iRAM - RAM;
                                            op2Recalc = nil;    // Term eingesetzt
                                        }
                                        *iRAM++ = value2;
                                    } else {
                                        if(op1 == 0x307) {
                                            *iRAM++ = 0x3A;
                                            if(op2Recalc) {         // Ausdruck undefiniert?
                                                op2Recalc->typ = 1; // zwei Byte einsetzen
                                                op2Recalc->adr = iRAM - RAM;
                                                op2Recalc = nil;    // Term eingesetzt
                                            }
                                            *iRAM++ = value2;
                                            *iRAM++ = value2 >> 8;
                                        } else
                                            Error("Nur LD A,(n) ist erlaubt!");
                                    }
                                    break;
                            case 0x340: // I,R
                                    if(op1 == 0x307) {
                                        *iRAM++ = 0xED;
                                        *iRAM++ = (op2 != 0x340)?0x57:0x5F;
                                    } else
                                        Error("Nur LD A,I oder LD A,R ist erlaubt!");
                                    break;
                            default:
                                    Error("2.Operand fehlerhaft");
                            }
                            break;
                    case 0x340: // I,R
                    case 0x341: // I,R
                            if(op2 == 0x307) {  // A
                                *iRAM++ = 0xED;
                                *iRAM++ = (op1 != 0x340)?0x47:0x4F;
                            } else
                                Error("nur LD I,A oder LD R,A erlaubt!");
                            break;
                    case 0x510: // (BC)
                    case 0x511: // (DE)
                            if(op2 == 0x307) {  // A
                                *iRAM++ = (op1 == 0x510)?0x02:0x12;
                            } else
                                Error("nur LD (BC),A oder LD (DE),A erlaubt!");
                            break;
                    case 0x630: // (IX+d)
                    case 0x631: // (IY+d)
                            switch(op2) {
                            case 0x300: // B
                            case 0x301: // C
                            case 0x302: // D
                            case 0x303: // E
                            case 0x304: // H
                            case 0x305: // L
                            case 0x307: // A
                                *iRAM++ = (op1 & 0x01)?0xFD:0xDD;
                                *iRAM++ = 0x70|(op2 & 0x07);
                                if(op1Recalc) {         // Ausdruck undefiniert?
                                    op1Recalc->typ = 0; // ein Byte einsetzen
                                    op1Recalc->adr = iRAM - RAM;
                                    op1Recalc = nil;    // Term eingesetzt
                                }
                                *iRAM++ = value1;
                                break;
                            case 0x281: // n
                                *iRAM++ = (op1 & 0x01)?0xFD:0xDD;
                                *iRAM++ = 0x36;
                                if(op1Recalc) {         // Ausdruck undefiniert?
                                    op1Recalc->typ = 0; // ein Byte einsetzen
                                    op1Recalc->adr = iRAM - RAM;
                                    op1Recalc = nil;    // Term eingesetzt
                                }
                                *iRAM++ = value1;
                                if(op2Recalc) {         // Ausdruck undefiniert?
                                    op2Recalc->typ = 0; // ein Byte einsetzen
                                    op2Recalc->adr = iRAM - RAM;
                                    op2Recalc = nil;    // Term eingesetzt
                                }
                                *iRAM++ = value2;
                                break;
                            default:
                                Error("2.Operand fehlerhaft");
                            }
                            break;
                    case 0x280: // (n)
                            switch(op2) {
                            case 0x307: // A
                                *iRAM++ = 0x32;
                                if(op1Recalc) {         // Ausdruck undefiniert?
                                    op1Recalc->typ = 1; // zwei Byte einsetzen
                                    op1Recalc->adr = iRAM - RAM;
                                    op1Recalc = nil;    // Term eingesetzt
                                }
                                *iRAM++ = value1;
                                *iRAM++ = value1 >> 8;
                                break;
                            case 0x312: // HL
                                *iRAM++ = 0x22;
                                if(op1Recalc) {         // Ausdruck undefiniert?
                                    op1Recalc->typ = 1; // zwei Byte einsetzen
                                    op1Recalc->adr = iRAM - RAM;
                                    op1Recalc = nil;    // Term eingesetzt
                                }
                                *iRAM++ = value1;
                                *iRAM++ = value1 >> 8;
                                break;
                            case 0x310: // BC
                            case 0x311: // DE
                            case 0x313: // SP
                                *iRAM++ = 0xED;
                                *iRAM++ = 0x43|((op2 & 0x03)<<4);
                                if(op1Recalc) {         // Ausdruck undefiniert?
                                    op1Recalc->typ = 1; // zwei Byte einsetzen
                                    op1Recalc->adr = iRAM - RAM;
                                    op1Recalc = nil;    // Term eingesetzt
                                }
                                *iRAM++ = value1;
                                *iRAM++ = value1 >> 8;
                                break;
                            case 0x330: // IX
                            case 0x331: // IY
                                *iRAM++ = (op2 & 0x01)?0xFD:0xDD;
                                *iRAM++ = 0x22;
                                if(op1Recalc) {         // Ausdruck undefiniert?
                                    op1Recalc->typ = 1; // zwei Byte einsetzen
                                    op1Recalc->adr = iRAM - RAM;
                                    op1Recalc = nil;    // Term eingesetzt
                                }
                                *iRAM++ = value1;
                                *iRAM++ = value1 >> 8;
                                break;
                            default:
                                Error("2.Operand fehlerhaft");
                            }
                            break;
                    case 0x313: // SP
                            switch(op2) {
                            case 0x312:     // HL
                                *iRAM++ = 0xF9;
                                break;
                            case 0x330:     // IX
                                *iRAM++ = 0xDD;
                                *iRAM++ = 0xF9;
                                break;
                            case 0x331:     // IY
                                *iRAM++ = 0xFD;
                                *iRAM++ = 0xF9;
                                break;
                            }
                    case 0x310: // BC
                    case 0x311: // DE
                    case 0x312: // HL
                            if((op2 == 0x280)||(op2 == 0x281))  {   // (n), n
                                if(op2 == 0x281) {      // n
                                    *iRAM++ = 0x01|((op1 & 0x03)<<4);
                                } else {                // (n)
                                    if(op1 == 0x312)    // HL
                                        *iRAM++ = 0x2A;
                                    else {
                                        *iRAM++ = 0xED;
                                        *iRAM++ = 0x4B|((op1 & 0x03)<<4);
                                    }
                                }
                                if(op2Recalc) {         // Ausdruck undefiniert?
                                    op2Recalc->typ = 1; // zwei Byte einsetzen
                                    op2Recalc->adr = iRAM - RAM;
                                    op2Recalc = nil;    // Term eingesetzt
                                }
                                *iRAM++ = value2;
                                *iRAM++ = value2 >> 8;
                            } else
                                Error("2.Operand fehlerhaft");
                            break;
                    case 0x330: // IX
                    case 0x331: // IY
                            if((op2 == 0x280)||(op2 == 0x281))  {   // (n), n
                                *iRAM++ = (op1 & 0x01)?0xFD:0xDD;
                                *iRAM++ = (op2 == 0x281)?0x21:0x2A;
                                if(op2Recalc) {         // Ausdruck undefiniert?
                                    op2Recalc->typ = 1; // zwei Byte einsetzen
                                    op2Recalc->adr = iRAM - RAM;
                                    op2Recalc = nil;    // Term eingesetzt
                                }
                                *iRAM++ = value2;
                                *iRAM++ = value2 >> 8;
                            } else
                                Error("2.Operand fehlerhaft");
                            break;
                    default:
                            Error("Adreßierungsart nicht erlaubt!");
                    }
                }
                break;
    case 0x0E:  // PUSH, POP: dreg
                if(op2)
                    Error("zu viele Operanden");
                else if(((op1 & 0xFF0) >= 0x310)&&((op1 & 0xFF0) <= 0x33F)) {   // Doppel-Register?
                    if((op1 >= 0x310)&&(op1 <= 0x312))
                        *iRAM++ = Op0_24|((op1-0x310)<<4);  // PUSH BC,DE,HL
                    else if(op1 == 0x323)
                        *iRAM++ = Op0_24|((op1-0x320)<<4);  // PUSH AF
                    else if((op1 == 0x330)|(op1 == 0x331)) {    // PUSH IX,IY
                        *iRAM++ = (op1 & 0x01)?0xFD:0xDD;
                        *iRAM++ = Op0_16;
                    }
                } else
                    Error("nur Doppelregister sind möglich!");
                break;
    case 0x0F:  // RR,RL,RRC,RLC,SRA,SLA,SRL
                if(op2)
                    Error("nur ein Operand erlaubt!");
                else if((op1 & 0xFF0) == 0x300) {           // B,C,D,E,H,L,(HL),A
                    *iRAM++ = 0xCB;
                    *iRAM++ = Op0_24|(op1 & 0x07);
                } else if((op1 == 0x630)||(op1 = 0x631)) {  // (IX+d), (IY+d)
                    *iRAM++ = (op1 & 0x01)?0xFD:0xDD;
                    *iRAM++ = 0xCB;
                    if(op1Recalc) {         // Ausdruck undefiniert?
                        op1Recalc->typ = 0; // ein Byte einsetzen
                        op1Recalc->adr = iRAM - RAM;
                        op1Recalc = nil;    // Term eingesetzt
                    }
                    *iRAM++ = value1;
                    *iRAM++ = Op0_24|6;
                } else
                    Error("Operand hier nicht erlaubt");
                break;
    default:
                Error("Unbekannte Opcode-Typ!");
                while(c->typ) c++;
    }
    *cp = c;
    PC = iRAM - RAM;
}

/***
 *  Pseudo-Opcode abtesten
 ***/
Boolean     IgnoreUntilIF = false;  // Zeilen bis zum "ENDIF" ignorieren


VOID            DoPseudo(CommandP *cp)
{
CommandP    c = *cp;
CommandP    cptr;
UWORD       iPC = PC;

    switch(c++->val) {      // Opcode durchgehen
    case 0x100:             // DEFB
                c--;
                do {
                    c++;
                    cptr = c;
                    RAM[iPC++] = CalcTerm(&cptr);
                    c = cptr;
                    if(LastRecalc) {            // Ausdruck undefiniert?
                        LastRecalc->typ = 0;    // ein Byte einsetzen
                        LastRecalc->adr = iPC - 1;
                    }
                } while((c->typ == OPCODE)&&(c->val == ','));
                break;
    case 0x101:             // DEFM
                if(c->typ != STRING) {
                    Error("Kein String bei DEFM!");
                } else {
                    STR     sp;
                    c--;
                    do {
                        c++;                        // Opcode bzw. Komma skippen
                        sp = (STR)c++->val;         // Wert = Ptr auf den String
                        while(*sp)
                            RAM[iPC++] = *sp++;     // String übertragen
                    } while((c->typ == OPCODE)&&(c->val == ','));
                }
                break;
    case 0x102:             // DEFS
                cptr = c;
                iPC +=  CalcTerm(&cptr);        // PC weitersetzen
                c = cptr;
                if(LastRecalc)
                    Error("Symbol nicht definiert!");
                break;
    case 0x103:             // DEFW
                c--;
                do {
                    ULONG   val;
                    c++;
                    cptr = c;
                    val = CalcTerm(&cptr);      // Ausdruck auswerten
                    c = cptr;
                    if(LastRecalc) {            // Ausdruck undefiniert?
                        LastRecalc->typ = 1;    // zwei Byte einsetzen
                        LastRecalc->adr = iPC;
                    }
                    RAM[iPC++] = val >> 8;
                    RAM[iPC++] = val;
                } while((c->typ == OPCODE)&&(c->val == ','));
                break;
    case 0x104:             // END
                if(IgnoreUntilIF)                   // dann nicht mehr übersetzen
                    Error("IF ohne ENDIF!");
                Error("Ende vom Sourcetext erreicht");
                exit(0);
    case 0x106:             // ORG
                cptr = c;
                iPC = CalcTerm(&cptr);              // PC setzen
                c = cptr;
                if(LastRecalc)
                    Error("Symbol nicht definiert!");
                break;
    case 0x107:             // IF
                cptr = c;
                if(!CalcTerm(&cptr))                // IF-Bedinung false?
                    IgnoreUntilIF = true;           // dann nicht mehr übersetzen
                c = cptr;
                break;
    case 0x108:             // ENDIF
                IgnoreUntilIF = false;              // ab hier stets übersetzen
                break;
    case 0x109:             // ELSE
                IgnoreUntilIF = !IgnoreUntilIF;     // Zeilen-Übersetzen-Flag toggeln
                break;
    case 0x10A:             // PRINT
                if(c->typ != STRING)
                    Error("Kein String bei PRINT!");
                else
                    puts((STR)c++->val);            // Message ausgeben
                break;
    }
    *cp = c;
    PC = iPC;
}

/***
 *  Zeile übersetzen
 ***/
VOID        CompileLine(VOID)
{
CommandP        c = Cmd;
CommandP        cptr;
SymbolP         s;
UWORD           val;

    if(!c->typ) return;                 // Leerzeile => raus
    if((c->typ == SYMBOL)&& !IgnoreUntilIF) {           // ein Symbol am Zeilenanfang und _kein_ IF?
        s = (SymbolP)c->val;            // Wert = Symbol-Ptr
        if(s->defined) {
            Error("Symbol bereits definiert");
            return;
        }
        c++;                            // ein Command weiter
        if((c->typ == OPCODE)&&(c->val == ':'))
            c++;                        // eventuellen ':' überspringen
        if((c->typ == OPCODE)&&(c->val == 0x105)) { // EQU?
            c++;                        // EQU überspringen
            cptr = c;
            s->val = CalcTerm(&cptr);   // Ausduck ausrechnen
            c = cptr;
            if(LastRecalc)
                Error("Symbol in Formel nicht definiert");
            s->defined = true;          // Symbol definiert
            if(c->typ != ILLEGAL) {
                Error("Daten nach EQU!");
                return;
            }
        } else {
            s->val = PC;                // Adresse = aktueller PC
            s->defined = true;          // Symbol definiert
        }
        while(s->recalc) {              // vom Symbol abhängige Ausdrücke?
            RecalcListP r = s->recalc;
            CommandP    saveC;
            LONG        value;

            s->recalc = r->next;        // zum nächsten Symbol vorrücken
            saveC = r->c;
            value = CalcTerm(&(r->c));  // Formel (mit jetzt def. Symbol) erneut ausrechnen
            if(!LastRecalc) {           // Ausdruck nun gültig (oder gar noch ein Symbol?)
                UWORD   adr = r->adr;
                switch(r->typ) {
                case 0x00:              // ein Byte einsetzen
                            RAM[adr] = value;
                            break;
                case 0x01:              // zwei Byte einsetzen
                            RAM[adr++] = value;
                            RAM[adr] = value>>8;
                            break;
                case 0x02:              // PC-rel-Byte einsetzen
                            RAM[adr] = value - (adr + 1);
                            break;
                default:    Error("unbekannter Recalc-Typ!");
                }
            } else {                    // Ausdruck immer noch nicht zu berechnen
                LastRecalc->typ = r->typ;   // Typ übertragen
                LastRecalc->adr = r->adr;   // Adresse übertragen
            }
            free(saveC);                // Formel freigeben
            free(r);                    // Recalc-Term freigeben
        }
    }
    if(IgnoreUntilIF) {                 // innerhalb eines IFs?
        if(c->typ == OPCODE) {
            switch(c->val) {
            case 0x108:     // ENDIF erreicht?
                    IgnoreUntilIF = false;          // Zeilen wieder übersetzen
                    break;
            case 0x109:     // ELSE erreicht?
                    IgnoreUntilIF = !IgnoreUntilIF; // Zeilen-Übersetzen-Flag toggeln
                    break;
            }
        }
    } else
        while(c->typ) {                 // bis zum Zeilenende scannen
            val = c->val;
            if((val < 0x100)||(val > 0x2FF))    // kein Opcode bzw. Pseudo-Opcode?
                Error("Illegales Zeichen");     // => Fehler!
            cptr = c;
            if((val >= 0x100)&&(val <= 0x1FF))  // Pseudo-Opcode
                DoPseudo(&cptr);
            else
                DoOpcode(&cptr);                // Opcode scannen
            c = cptr;
        }
}
