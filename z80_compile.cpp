/***
 *  compile a tokenzied line
 ***/

#include "z80_assembler.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

void DoPseudo( CommandP *cp );
void DoOpcode( CommandP *cp );
int16_t GetOperand( CommandP *cp, int32_t *value );


/***
 *  get operands for an opcode
 ***/
int16_t GetOperand( CommandP *c, int32_t *value ) {
    int16_t typ;
    int16_t val, sval;

    LastRecalc = nullptr; // (to be safe: reset recalc entry)
    *value = 0;
    typ = ( *c )->typ;
    val = ( *c )++->val; // get value and type
    MSG( 2, "GetOperand( %d, %X, %d )\n", typ, val, *value );
    if ( typ == OPCODE ) {
        if ( ( val >= 0x300 ) && ( val <= 0x4FF ) ) {
            if ( ( val == 0x323 ) && ( ( *c )->typ == OPCODE ) && ( ( *c )->val == '\'' ) ) { // AF'?
                ( *c )++;                                                                     // skip '
                val = 0x324;                                                                  // AF' returned
            }
            return val; // register or condition
        }
        if ( ( val >= 0x100 ) && ( val <= 0x2FF ) ) // an opcode
            Error( "Illegal operand" );
        if ( ( val == '(' ) != 0 ) { // indirect addressing?
            typ = ( *c )->typ;
            sval = ( *c )++->val; // get value and type
            if ( typ == OPCODE ) {
                if ( ( ( sval & 0xFF0 ) == 0x310 ) || ( sval == 0x301 ) ) { // register
                    typ = ( *c )->typ;
                    val = ( *c )++->val; // get value and type
                    if ( ( typ == OPCODE ) && ( val == ')' ) ) {
                        if ( sval == 0x312 ) // (HL)?
                            return 0x306;    // to combine them easier
                        return sval + 0x200; // (C),(BC),(DE),(SP)
                    }
                    Error( "Closing bracket missing after (BC,(DE,(HL or (SP" );
                }
                if ( ( sval & 0xFF0 ) == 0x330 ) { // IX,IY
                    if ( ( *c )->typ == OPCODE ) { // does an operator follow?
                        val = ( *c )->val;
                        if ( ( val == '+' ) || ( val == '-' ) ) {
                            *value = CalcTerm( c );
                            typ = ( *c )->typ;
                            val = ( *c )++->val; // get a braket
                            if ( ( typ == OPCODE ) && ( val == ')' ) )
                                return sval + 0x300; // (IX+d) or (IY+d)
                            Error( "Closing bracket missing after (IX or (IY" );
                        } else {
                            if ( ( typ == OPCODE ) && ( val == ')' ) ) {
                                ( *c )++;            // skip the bracket
                                return sval + 0x200; // (IX) or (IY)
                            }
                            Error( "Closing bracket missing after (IX or (IY" );
                        }
                    } else
                        Error( "Illegal character after (IX or (IY" );
                }
            }
            ( *c )--; // Ptr auf das vorherige Zeichen zurück
            *value = CalcTerm( c );
            typ = ( *c )->typ;
            val = ( *c )++->val; // get the closing bracket
            if ( ( typ == OPCODE ) && ( val == ')' ) )
                return 0x280; // (Adr)
            Error( "Closing bracket missing after (adr)" );
        }
    }         // absolute addressing
    ( *c )--; // return to the previous token
    *value = CalcTerm( c );
    return 0x281; // return an address
}


/***
 *  test for an opcode
 ***/
void DoOpcode( CommandP *cp ) {
    MSG( 2, "DoOpcode( %X )\n", (*cp)->val );
    CommandP c = *cp;
    uint8_t *iRAM = RAM + PC;
    uint32_t op0;
    uint8_t Op0_24, Op0_16;
    int16_t op1, op2;
    int32_t value1 = 0, value2 = 0;
    RecalcListP op1Recalc, op2Recalc;

    checkPC( PC ); // detect min, max and overflow (wrap around)

    op0 = c++->val; // opcode
    op1 = op2 = 0;
    if ( c->typ ) {
        op1 = GetOperand( &c, &value1 );                   // get 1. operand
        op1Recalc = LastRecalc;                            // store Recalc ptr for 1. operand
        if ( ( c->typ == OPCODE ) && ( c->val == ',' ) ) { // get a potential 2. operand
            c++;
            op2 = GetOperand( &c, &value2 ); // get the 2. operand
            op2Recalc = LastRecalc;          // store Recalc ptr for 2. operand
        }
    }
    Op0_24 = op0 >> 24;
    Op0_16 = op0 >> 16;

    // helpful for debugging and enhancement
    MSG( 3, "op0: 0x%08x, op1: 0x%03x, value1: 0x%08x, op2: 0x%03x, value2: 0x%08x\n", op0, op1, value1, op2, value2 );

    switch ( op0 & 0xFF ) {    // opcode
    case 0x00:                 // IN/OUT
        if ( Op0_24 & 0x01 ) { // OUT?
            int32_t t;
            RecalcListP tr;
            t = op1;
            op1 = op2;
            op2 = t; // flip operands
            t = value1;
            value1 = value2;
            value2 = t;
            tr = op1Recalc;
            op1Recalc = op2Recalc;
            op2Recalc = tr;
        }
        if ( ( ( op1 & 0xFF0 ) == 0x300 ) && ( op2 == 0x501 ) ) { // IN ?,(C) or OUT (C),?
            if ( op1 == 0x306 )
                Error( "IN (HL),(C) or OUT (C),(HL) geht nicht" );
            else {
                *iRAM++ = 0xED;
                *iRAM++ = Op0_24 | ( ( op1 & 0x7 ) << 3 );
            }
        } else if ( ( op1 == 0x307 ) && ( op2 == 0x280 ) ) { // IN A,(n) or OUT (n),A
            *iRAM++ = Op0_16;
            if ( op2Recalc ) {      // undefined expression?
                op2Recalc->typ = 0; // add a single byte
                op2Recalc->adr = iRAM - RAM;
                op2Recalc = nullptr; // processing done
            }
            *iRAM++ = value2;
        } else if ( !(Op0_24 & 0x01) && ( op1 == 0x501 ) && ( op2 == 0 ) ) { //  undoc: IN (C))
            *iRAM++ = 0xED;
            *iRAM++ = 0x70;
        } else if ( (Op0_24 & 0x01) && ( op1 == 0x281 ) && ( op2 == 0x501 ) && value1 == 0 ) { // undoc: OUT (C),0
            *iRAM++ = 0xED;
            *iRAM++ = 0x71;
        } else
            Error( "operands not allowed for IN/OUT" );
        break;
    case 0x01:           // one byte opcode without parameter
        if ( op1 | op2 ) // operands provied?
            Error( "operands not allowed" );
        else
            *iRAM++ = Op0_24;
        break;
    case 0x02:           // two byte opcode without parameter
        if ( op1 | op2 ) // operands provied?
            Error( "operands not allowed" );
        else {
            *iRAM++ = Op0_24;
            *iRAM++ = Op0_16;
        }
        break;
    case 0x03:
        if ( ( ( op1 != 0x306 ) && ( op1 ) ) || ( op2 ) ) // RRD (HL) or RLD (HL)
            Error( "Illegal Operand" );
        else {
            *iRAM++ = Op0_24;
            *iRAM++ = Op0_16;
        }
        break;
    case 0x04: // 1.parameter = bit number, 2.parameter = <ea> (BIT,RES,SET)
        if ( ( op1 != 0x281 ) || ( value1 < 0 ) || ( value1 > 7 ) )
            Error( "1st operand has to be between 0 and 7" );
        else {
            if ( ( op2 & 0xFF0 ) == 0x300 ) { // A,B,C,D,E,H,L,(HL)
                *iRAM++ = Op0_24;
                *iRAM++ = Op0_16 | ( value1 << 3 ) | ( op2 & 0x07 );
            } else if ( ( op2 & 0xFF0 ) == 0x630 ) { // (IX+d) or (IY+d)
                *iRAM++ = ( op2 & 0x01 ) ? 0xFD : 0xDD;
                *iRAM++ = Op0_24;
                if ( op2Recalc ) {      // expression undefined?
                    op2Recalc->typ = 0; // add a single byte
                    op2Recalc->adr = iRAM - RAM;
                    op2Recalc = nullptr; // processing done
                }
                *iRAM++ = value2;
                *iRAM++ = Op0_16 | ( value1 << 3 ) | 6;
            } else
                Error( "2nd operand wrong" );
        }
        break;
    case 0x05: // IM (one parameter: 0,1,2)
        if ( ( op1 != 0x281 ) || ( op2 ) )
            Error( "operand wrong" );
        else {
            if ( ( value1 < 0 ) || ( value1 > 2 ) )
                Error( "Operand value has to be 0, 1 or 2" );
            else {
                if ( value1 > 0 )
                    value1++;
                *iRAM++ = Op0_24;
                *iRAM++ = Op0_16 | ( ( value1 & 0x07 ) << 3 );
            }
        }
        break;

    case 0x06: // ADD,ADC,SUB,SBC,AND,XOR,OR,CP
        switch ( op1 ) {
        case 0x312:                                       // HL
            if ( ( op2 >= 0x310 ) && ( op2 <= 0x313 ) ) { // BC,DE,HL,SP
                switch ( Op0_24 ) {
                case 0x80: // ADD
                    *iRAM++ = 0x09 | ( ( op2 & 0x03 ) << 4 );
                    break;
                case 0x88: // ADC
                    *iRAM++ = 0xED;
                    *iRAM++ = 0x4A | ( ( op2 & 0x03 ) << 4 );
                    break;
                case 0x98: // SBC
                    *iRAM++ = 0xED;
                    *iRAM++ = 0x42 | ( ( op2 & 0x03 ) << 4 );
                    break;
                default:
                    Error( "Opcode with this <ea> not allowed" );
                }
            } else
                Error( "Expecting a double-register" );
            break;
        case 0x330:               // IX
        case 0x331:               // IY
            if ( Op0_24 != 0x80 ) // only ADD IX/IY,RR
                Error( "Only ADD IX,[BC,DE,IX,SP] or ADD IY,[BC,DE,IY,SP] are possible" );
            switch ( op2 ) {
            case 0x310: // BC
                *iRAM++ = ( op1 & 0x01 ) ? 0xFD : 0xDD;
                *iRAM++ = 0x09;
                break;
            case 0x311: // DE
                *iRAM++ = ( op1 & 0x01 ) ? 0xFD : 0xDD;
                *iRAM++ = 0x19;
                break;
            case 0x330: // IX
            case 0x331: // IY
                if ( op1 == op2 ) {
                    *iRAM++ = ( op1 & 0x01 ) ? 0xFD : 0xDD;
                    *iRAM++ = 0x29;
                } else
                    Error( "Only ADD IX,IY or ADD IY,IY are possible" );
                break;
            case 0x313: // SP
                *iRAM++ = ( op1 & 0x01 ) ? 0xFD : 0xDD;
                *iRAM++ = 0x39;
                break;
            default:
                Error( "Opcode with this <ea> not allowed" );
            }
            break;
        default:
            if ( ( op1 == 0x307 ) && ( op2 ) ) { // accumulator?
                op1 = op2;
                value1 = value2; // Shift 2nd operand to the beginning
                op1Recalc = op2Recalc;
                op2Recalc = nullptr;
            }
            switch ( op1 & 0xFF0 ) {
            case 0x350: // X,HX
                *iRAM++ = 0xDD;
                *iRAM++ = Op0_24 | ( op1 & 7 );
                break;
            case 0x360: // Y,HY
                *iRAM++ = 0xFD;
                *iRAM++ = Op0_24 | ( op1 & 7 );
                break;
            case 0x300: // A,B,C,D,E,H,L,(HL)
                *iRAM++ = Op0_24 | ( op1 & 7 );
                break;
            case 0x630: // (IX+d) or (IY+d)
                *iRAM++ = ( op1 & 0x01 ) ? 0xFD : 0xDD;
                *iRAM++ = Op0_24 | 6;
                if ( op1Recalc ) {      // expression undefined?
                    op1Recalc->typ = 0; // add a single byte
                    op1Recalc->adr = iRAM - RAM;
                    op1Recalc = nullptr; // processing done
                }
                *iRAM++ = value1;
                break;
            case 0x280: // n
                if ( op1 == 0x281 ) {
                    *iRAM++ = Op0_16;
                    if ( op1Recalc ) {      // expression undefined?
                        op1Recalc->typ = 0; // add a single byte
                        op1Recalc->adr = iRAM - RAM;
                        op1Recalc = nullptr; // processing done
                    }
                    *iRAM++ = value1;
                    break;
                }
            default:
                Error( "2nd operand wrong" );
            }
        }
        break;
    case 0x07: // INC, DEC, like 0x06 without absolute address
        if ( op2 )
            Error( "2nd operand not allowed" );
        if ( ( op1 & 0xFF0 ) == 0x300 ) { // A,B,C,D,E,H,L,(HL)
            *iRAM++ = Op0_24 | ( ( op1 & 7 ) << 3 );
        } else if ( ( op1 & 0xFF0 ) == 0x630 ) { // (IX+d) or (IY+d)
            *iRAM++ = ( op1 & 1 ) ? 0xFD : 0xDD;
            *iRAM++ = Op0_24 | ( 6 << 3 );
            if ( op1Recalc ) {      // expression undefined?
                op1Recalc->typ = 0; // add a single byte
                op1Recalc->adr = iRAM - RAM;
                op1Recalc = nullptr; // processing done
            }
            *iRAM++ = value1;
        } else {
            bool decFlag = Op0_24 & 1; // True: DEC, False: INC
            switch ( op1 ) {
            case 0x354: // HX
                *iRAM++ = 0xDD;
                *iRAM++ = decFlag ? 0x25 : 0x24;
                break;
            case 0x355: // X
                *iRAM++ = 0xDD;
                *iRAM++ = decFlag ? 0x2D : 0x2C;
                break;
            case 0x364: // HY
                *iRAM++ = 0xFD;
                *iRAM++ = decFlag ? 0x25 : 0x24;
                break;
            case 0x365: // Y
                *iRAM++ = 0xFD;
                *iRAM++ = decFlag ? 0x2D : 0x2C;
                break;
            case 0x310: // BC
                *iRAM++ = decFlag ? 0x0B : 0x03;
                break;
            case 0x311: // DE
                *iRAM++ = decFlag ? 0x1B : 0x13;
                break;
            case 0x312: // HL
                *iRAM++ = decFlag ? 0x2B : 0x23;
                break;
            case 0x313: // HL
                *iRAM++ = decFlag ? 0x3B : 0x33;
                break;
            case 0x330: // IX
                *iRAM++ = 0xDD;
                *iRAM++ = decFlag ? 0x2B : 0x23;
                break;
            case 0x331: // IY
                *iRAM++ = 0xFD;
                *iRAM++ = decFlag ? 0x2B : 0x23;
                break;
            default:
                Error( "Addressing mode not allowed" );
            }
        }
        break;
    case 0x08: // JP, CALL, JR (Warning! Different <ea>!)
        if ( op1 == 0x301 )
            op1 = 0x403; // convert register 'C' into condition 'C'
        switch ( Op0_24 ) {
        case 0xC2:                                                            // JP
            if ( ( op1 >= 0x400 ) && ( op1 <= 0x4FF ) && ( op2 == 0x281 ) ) { // cond,Adr
                *iRAM++ = Op0_24 | ( ( op1 & 0x07 ) << 3 );
                if ( op2Recalc ) {      // expression undefined?
                    op2Recalc->typ = 1; // add two bytes
                    op2Recalc->adr = iRAM - RAM;
                    op2Recalc = nullptr; // processing done
                }
                *iRAM++ = value2;
                *iRAM++ = value2 >> 8;
            } else if ( ( op1 == 0x306 ) && !op2 ) { // JP (HL)
                *iRAM++ = 0xE9;
            } else if ( ( op1 == 0x530 ) && !op2 ) { // JP (IX)
                *iRAM++ = 0xDD;
                *iRAM++ = 0xE9;
            } else if ( ( op1 == 0x531 ) && !op2 ) { // JP (IY)
                *iRAM++ = 0xFD;
                *iRAM++ = 0xE9;
            } else if ( ( op1 == 0x281 ) | !op2 ) { //  JP Adr
                *iRAM++ = Op0_16;
                if ( op1Recalc ) {      // expression undefined?
                    op1Recalc->typ = 1; // add two bytes
                    op1Recalc->adr = iRAM - RAM;
                    op1Recalc = nullptr; // processing done
                }
                *iRAM++ = value1;
                *iRAM++ = value1 >> 8;
            } else
                Error( "1st operand wrong" );
            break;
        case 0x20:                                                            // JR
            if ( ( op1 >= 0x400 ) && ( op1 <= 0x403 ) && ( op2 == 0x281 ) ) { // Cond,Adr
                *iRAM++ = Op0_24 | ( ( op1 & 0x07 ) << 3 );
                if ( op2Recalc ) {      // expression undefined?
                    op2Recalc->typ = 2; // ein PC-rel-Byte einsetzen
                    op2Recalc->adr = iRAM - RAM;
                    op2Recalc = nullptr; // processing done
                }
                {
                    uint8_t b = value2 - ( iRAM - RAM ) - 1;
                    *iRAM++ = b;
                }
            } else if ( ( op1 == 0x281 ) && !op2 ) { //  JR Adr
                *iRAM++ = Op0_16;
                if ( op1Recalc ) {      // expression undefined?
                    op1Recalc->typ = 2; // ein PC-rel-Byte einsetzen
                    op1Recalc->adr = iRAM - RAM;
                    op1Recalc = nullptr; // processing done
                }
                {
                    uint8_t b = value1 - ( iRAM - RAM ) - 1;
                    *iRAM++ = b;
                }
            } else
                Error( "Condition not allowed" );
            break;
        case 0xC4:                                                            // CALL
            if ( ( op1 >= 0x400 ) && ( op1 <= 0x4FF ) && ( op2 == 0x281 ) ) { // Cond,Adr
                *iRAM++ = Op0_24 | ( ( op1 & 0x07 ) << 3 );
                if ( op2Recalc ) {      // expression undefined?
                    op2Recalc->typ = 1; // add two bytes
                    op2Recalc->adr = iRAM - RAM;
                    op2Recalc = nullptr; // processing done
                }
                *iRAM++ = value2;
                *iRAM++ = value2 >> 8;
            } else if ( ( op1 == 0x281 ) && !op2 ) { //  CALL Adr
                *iRAM++ = Op0_16;
                if ( op1Recalc ) {      // expression undefined?
                    op1Recalc->typ = 1; // add two bytes
                    op1Recalc->adr = iRAM - RAM;
                    op1Recalc = nullptr; // processing done
                }
                *iRAM++ = value1;
                *iRAM++ = value1 >> 8;
            } else
                Error( "1st operand wrong" );
            break;
        default:
            Error( "opcode table has a bug" );
        }
        break;
    case 0x09:
        if ( op2 ) // RET-Befehl
            Error( "Too many operands" );
        else if ( !op1 )      // keine Condition angegeben?
            *iRAM++ = Op0_16; // normalen Opcode nehmen
        else {
            if ( op1 == 0x301 )
                op1 = 0x403; // Register C in Condition C wandeln
            if ( ( op1 & 0xF00 ) != 0x400 )
                Error( "Wrong Operand" );
            else
                *iRAM++ = Op0_24 | ( ( op1 & 0x07 ) << 3 );
        }
        break;
    case 0x0A: // RST (00,08,10,18,20,28,30,38)
        if ( op2 )
            Error( "Too many operands" );
        else if ( op1 == 0x281 ) { // n
            int16_t i = -1;
            switch ( value1 ) {
            case 0:
                i = 0x00;
                break;
            case 1:
            case 8:
                i = 0x08;
                break;
            case 2:
            case 10:
            case 0x10:
                i = 0x10;
                break;
            case 3:
            case 18:
            case 0x18:
                i = 0x18;
                break;
            case 4:
            case 20:
            case 0x20:
                i = 0x20;
                break;
            case 5:
            case 28:
            case 0x28:
                i = 0x28;
                break;
            case 6:
            case 30:
            case 0x30:
                i = 0x30;
                break;
            case 7:
            case 38:
            case 0x38:
                i = 0x38;
                break;
            default:
                Error( "Only 00,08,10,18,20,28,30,38 are allowed" );
            }
            if ( i >= 0 )
                *iRAM++ = Op0_24 | i;
        } else
            Error( "Addressing mode not allowed" );
        break;
    case 0x0B: // DJNZ
        *iRAM++ = Op0_24;
        if ( op1Recalc ) {      // expression undefined?
            op1Recalc->typ = 2; // ein PC-rel-Byte einsetzen
            op1Recalc->adr = iRAM - RAM;
            op1Recalc = nullptr; // processing done
        }
        {
            uint8_t b = value1 - ( iRAM - RAM ) - 1;
            *iRAM++ = b; // relocate
        }
        break;
    case 0x0C:                                      // EX: (SP),dreg or DE,HL or AF,AF'
        if ( ( op1 == 0x311 ) && ( op2 == 0x312 ) ) // EX DE,HL
            *iRAM++ = 0xEB;
        else if ( ( op1 == 0x323 ) && ( op2 == 0x324 ) ) { // EX AF,AF'
            *iRAM++ = 0x08;
        } else if ( ( op1 == 0x513 ) && ( op2 == 0x312 ) ) // EX (SP),HL
            *iRAM++ = 0xE3;
        else if ( ( op1 == 0x513 ) && ( op2 == 0x330 ) ) { // EX (SP),IX
            *iRAM++ = 0xDD;
            *iRAM++ = 0xE3;
        } else if ( ( op1 == 0x513 ) && ( op2 == 0x331 ) ) { // EX (SP),IY
            *iRAM++ = 0xFD;
            *iRAM++ = 0xE3;
        } else
            Error( "Operand combination not allowed with EX" );
        break;
    case 0x0D: // LD
        if ( !( op1 & op2 ) )
            Error( "Operand missing" );
        else {
            uint8_t FirstByte = 0;
            switch ( op1 ) {
            case 0x530: // LD (IX),
            case 0x531: // LD (IY),
                op1 = ( op1 == 0x530 ) ? 0x356 : 0x366;
            case 0x354: //  HX
            case 0x355: //  X
            case 0x364: //  HY
            case 0x365: //  Y
                FirstByte = ( ( op1 & 0xFF0 ) == 0x350 ) ? 0xDD : 0xFD;
                *iRAM++ = FirstByte;
                op1 &= 0xF0F; // remap H and L
            case 0x300:       // B
            case 0x301:       // C
            case 0x302:       // D
            case 0x303:       // E
            case 0x304:       // H
            case 0x305:       // L
            case 0x306:       // (HL)
            case 0x307:       // A
                switch ( op2 & 0xFF0 ) {
                case 0x530: // LD <ea>,(IX), or LD <ea>,(IY)
                    op2 = ( op2 == 0x530 ) ? 0x356 : 0x366;
                case 0x350: // X,HX
                case 0x360: // Y,HY
                {
                    bool flag = ( ( op2 & 0xFF0 ) == 0x350 );
                    switch ( FirstByte ) {
                    case 0xDD: // IX
                        if ( !flag )
                            Error( "IX,IY geht nicht" );
                        break;
                    case 0xFD: // IY
                        if ( flag )
                            Error( "IY,IX geht nicht" );
                        break;
                    default: // noch nix
                        *iRAM++ = ( flag ) ? 0xDD : 0xFD;
                        break;
                    }
                }
                    op2 &= 0xF0F; // remap H and L
                case 0x300:       // B,C,D,E,H,L,(HL),A
                    *iRAM++ = 0x40 | ( ( op1 & 0x07 ) << 3 ) | ( op2 & 0x07 );
                    break;
                case 0x510: // (BC),(DE),(SP)
                    if ( op1 == 0x307 ) {
                        if ( op2 == 0x510 )
                            *iRAM++ = 0x0A;
                        else if ( op2 == 0x511 )
                            *iRAM++ = 0x1A;
                        else
                            Error( "(SP) not allowed" );
                    } else
                        Error( "Only LD A,(BC) or LD A,(DE) allowed" );
                    break;
                case 0x630:               // (IX+d), (IY+d)
                    if ( op1 != 0x306 ) { // (HL)
                        *iRAM++ = ( op2 & 0x01 ) ? 0xFD : 0xDD;
                        *iRAM++ = 0x46 | ( ( op1 & 0x07 ) << 3 );
                        if ( op2Recalc ) {      // expression undefined?
                            op2Recalc->typ = 0; // add a single byte
                            op2Recalc->adr = iRAM - RAM;
                            op2Recalc = nullptr; // processing done
                        }
                        *iRAM++ = value2;
                    } else
                        Error( "LD (HL),(IX/IY+d) not allowed" );
                    break;
                case 0x280: // (n), n
                    if ( op2 == 0x281 ) {
                        *iRAM++ = 0x06 | ( ( op1 & 0x07 ) << 3 );
                        if ( op2Recalc ) {      // expression undefined?
                            op2Recalc->typ = 0; // add a single byte
                            op2Recalc->adr = iRAM - RAM;
                            op2Recalc = nullptr; // processing done
                        }
                        *iRAM++ = value2;
                    } else {
                        if ( op1 == 0x307 ) {
                            *iRAM++ = 0x3A;
                            if ( op2Recalc ) {      // expression undefined?
                                op2Recalc->typ = 1; // add two bytes
                                op2Recalc->adr = iRAM - RAM;
                                op2Recalc = nullptr; // processing done
                            }
                            *iRAM++ = value2;
                            *iRAM++ = value2 >> 8;
                        } else
                            Error( "Only LD A,(n) allowed" );
                    }
                    break;
                case 0x340: // I,R
                    if ( op1 == 0x307 ) {
                        *iRAM++ = 0xED;
                        *iRAM++ = ( op2 != 0x340 ) ? 0x57 : 0x5F;
                    } else
                        Error( "Only LD A,I or LD A,R allowed" );
                    break;
                default:
                    Error( "2nd operand wrong" );
                }
                break;
            case 0x340:               // I,R
            case 0x341:               // I,R
                if ( op2 == 0x307 ) { // A
                    *iRAM++ = 0xED;
                    *iRAM++ = ( op1 != 0x340 ) ? 0x47 : 0x4F;
                } else
                    Error( "Only LD I,A or LD R,A allowed" );
                break;
            case 0x510:               // (BC)
            case 0x511:               // (DE)
                if ( op2 == 0x307 ) { // A
                    *iRAM++ = ( op1 == 0x510 ) ? 0x02 : 0x12;
                } else
                    Error( "Only LD (BC),A or LD (DE),A allowed" );
                break;
            case 0x630: // (IX+d)
            case 0x631: // (IY+d)
                switch ( op2 ) {
                case 0x300: // B
                case 0x301: // C
                case 0x302: // D
                case 0x303: // E
                case 0x304: // H
                case 0x305: // L
                case 0x307: // A
                    *iRAM++ = ( op1 & 0x01 ) ? 0xFD : 0xDD;
                    *iRAM++ = 0x70 | ( op2 & 0x07 );
                    if ( op1Recalc ) {      // expression undefined?
                        op1Recalc->typ = 0; // add a single byte
                        op1Recalc->adr = iRAM - RAM;
                        op1Recalc = nullptr; // processing done
                    }
                    *iRAM++ = value1;
                    break;
                case 0x281: // n
                    *iRAM++ = ( op1 & 0x01 ) ? 0xFD : 0xDD;
                    *iRAM++ = 0x36;
                    if ( op1Recalc ) {      // expression undefined?
                        op1Recalc->typ = 0; // add a single byte
                        op1Recalc->adr = iRAM - RAM;
                        op1Recalc = nullptr; // processing done
                    }
                    *iRAM++ = value1;
                    if ( op2Recalc ) {      // expression undefined?
                        op2Recalc->typ = 0; // add a single byte
                        op2Recalc->adr = iRAM - RAM;
                        op2Recalc = nullptr; // processing done
                    }
                    *iRAM++ = value2;
                    break;
                default:
                    Error( "2nd operand wrong" );
                }
                break;
            case 0x280: // (n)
                switch ( op2 ) {
                case 0x307: // A
                    *iRAM++ = 0x32;
                    if ( op1Recalc ) {      // expression undefined?
                        op1Recalc->typ = 1; // add two bytes
                        op1Recalc->adr = iRAM - RAM;
                        op1Recalc = nullptr; // processing done
                    }
                    *iRAM++ = value1;
                    *iRAM++ = value1 >> 8;
                    break;
                case 0x312: // HL
                    *iRAM++ = 0x22;
                    if ( op1Recalc ) {      // expression undefined?
                        op1Recalc->typ = 1; // add two bytes
                        op1Recalc->adr = iRAM - RAM;
                        op1Recalc = nullptr; // processing done
                    }
                    *iRAM++ = value1;
                    *iRAM++ = value1 >> 8;
                    break;
                case 0x310: // BC
                case 0x311: // DE
                case 0x313: // SP
                    *iRAM++ = 0xED;
                    *iRAM++ = 0x43 | ( ( op2 & 0x03 ) << 4 );
                    if ( op1Recalc ) {      // expression undefined?
                        op1Recalc->typ = 1; // add two bytes
                        op1Recalc->adr = iRAM - RAM;
                        op1Recalc = nullptr; // processing done
                    }
                    *iRAM++ = value1;
                    *iRAM++ = value1 >> 8;
                    break;
                case 0x330: // IX
                case 0x331: // IY
                    *iRAM++ = ( op2 & 0x01 ) ? 0xFD : 0xDD;
                    *iRAM++ = 0x22;
                    if ( op1Recalc ) {      // expression undefined?
                        op1Recalc->typ = 1; // add two bytes
                        op1Recalc->adr = iRAM - RAM;
                        op1Recalc = nullptr; // processing done
                    }
                    *iRAM++ = value1;
                    *iRAM++ = value1 >> 8;
                    break;
                default:
                    Error( "2nd operand wrong" );
                }
                break;
            case 0x313: // SP
                switch ( op2 ) {
                case 0x312: // HL
                    *iRAM++ = 0xF9;
                    break;
                case 0x330: // IX
                    *iRAM++ = 0xDD;
                    *iRAM++ = 0xF9;
                    break;
                case 0x331: // IY
                    *iRAM++ = 0xFD;
                    *iRAM++ = 0xF9;
                    break;
                }
            case 0x310:                                       // BC
            case 0x311:                                       // DE
            case 0x312:                                       // HL
                if ( ( op2 == 0x280 ) || ( op2 == 0x281 ) ) { // (n), n
                    if ( op2 == 0x281 ) {                     // n
                        *iRAM++ = 0x01 | ( ( op1 & 0x03 ) << 4 );
                    } else {                // (n)
                        if ( op1 == 0x312 ) // HL
                            *iRAM++ = 0x2A;
                        else {
                            *iRAM++ = 0xED;
                            *iRAM++ = 0x4B | ( ( op1 & 0x03 ) << 4 );
                        }
                    }
                    if ( op2Recalc ) {      // expression undefined?
                        op2Recalc->typ = 1; // add two bytes
                        op2Recalc->adr = iRAM - RAM;
                        op2Recalc = nullptr; // processing done
                    }
                    *iRAM++ = value2;
                    *iRAM++ = value2 >> 8;
                } else if ( op2 != 0x312 && op2 != 0x330 && op2 != 0x331 )
                    Error( "2nd operand wrong" );
                break;
            case 0x330:                                       // IX
            case 0x331:                                       // IY
                if ( ( op2 == 0x280 ) || ( op2 == 0x281 ) ) { // (n), n
                    *iRAM++ = ( op1 & 0x01 ) ? 0xFD : 0xDD;
                    *iRAM++ = ( op2 == 0x281 ) ? 0x21 : 0x2A;
                    if ( op2Recalc ) {      // expression undefined?
                        op2Recalc->typ = 1; // add two bytes
                        op2Recalc->adr = iRAM - RAM;
                        op2Recalc = nullptr; // processing done
                    }
                    *iRAM++ = value2;
                    *iRAM++ = value2 >> 8;
                } else
                    Error( "2nd operand wrong" );
                break;
            default:
                Error( "Addressing mode not allowed" );
            }
        }
        break;
    case 0x0E: // PUSH, POP: dreg
        if ( op2 )
            Error( "Too many operands" );
        else if ( ( ( op1 & 0xFF0 ) >= 0x310 ) && ( ( op1 & 0xFF0 ) <= 0x33F ) ) { // double register?
            if ( ( op1 >= 0x310 ) && ( op1 <= 0x312 ) )
                *iRAM++ = Op0_24 | ( ( op1 - 0x310 ) << 4 ); // PUSH BC,DE,HL
            else if ( op1 == 0x323 )
                *iRAM++ = Op0_24 | ( ( op1 - 0x320 ) << 4 );  // PUSH AF
            else if ( ( op1 == 0x330 ) | ( op1 == 0x331 ) ) { // PUSH IX,IY
                *iRAM++ = ( op1 & 0x01 ) ? 0xFD : 0xDD;
                *iRAM++ = Op0_16;
            }
        } else
            Error( "only double-registers are allowed" );
        break;
    case 0x0F: // RR,RL,RRC,RLC,SRA,SLA,SRL
        if ( op2 )
            Error( "Only one operand allowed" );
        else if ( ( op1 & 0xFF0 ) == 0x300 ) { // B,C,D,E,H,L,(HL),A
            *iRAM++ = 0xCB;
            *iRAM++ = Op0_24 | ( op1 & 0x07 );
        } else if ( ( op1 == 0x630 ) || ( op1 == 0x631 ) ) { // (IX+d), (IY+d)
            *iRAM++ = ( op1 & 0x01 ) ? 0xFD : 0xDD;
            *iRAM++ = 0xCB;
            if ( op1Recalc ) {      // expression undefined?
                op1Recalc->typ = 0; // add a single byte
                op1Recalc->adr = iRAM - RAM;
                op1Recalc = nullptr; // processing done
            }
            *iRAM++ = value1;
            *iRAM++ = Op0_24 | 6;
        } else
            Error( "operand not allowed" );
        break;
    default:
        Error( "unknown opcode type" );
        while ( c->typ )
            c++;
    }
    *cp = c;
    PC = iRAM - RAM;   // PC -> next opcode
    checkPC( PC - 1 ); // last RAM position used
}


/***
 *  test for pseudo-opcodes
 ***/
bool IgnoreUntilIF = false; // ignore all lines till next "ENDIF" (this could be a stack for nesting support)

void DoPseudo( CommandP *cp ) {
    MSG( 2, "DoPseudo( %d, %X )\n", (*cp)->typ, (*cp)->val );
    CommandP c = *cp;
    CommandP cptr;
    uint16_t iPC = PC;

    switch ( c++->val ) { // all pseudo opcodes
    case DEFB:
    case DEFM:
        c--;
        do {
            c++;                               // skip opcode or comma
            if ( c->typ != STRING ) {
                cptr = c;
                checkPC( iPC );
                RAM[ iPC++ ] = CalcTerm( &cptr );
                c = cptr;
                if ( LastRecalc ) {      // expression undefined?
                    LastRecalc->typ = 0; // add a single byte
                    LastRecalc->adr = iPC - 1;
                }
            } else {
                char *sp;
                sp = (char *)c++->val;             // value = ptr to the string
                checkPC( iPC + strlen( sp ) - 1 ); // will it overflow?
                while ( *sp )
                    RAM[ iPC++ ] = *sp++; // transfer the string
            }
        } while ( ( c->typ == OPCODE ) && ( c->val == ',' ) );
        break;
    case DEFS:
        cptr = c;
        iPC += CalcTerm( &cptr ); // advance the PC
        c = cptr;
        if ( LastRecalc )
            Error( "symbol not defined" );
        break;
    case DEFW:
        c--;
        do {
            uint32_t val;
            c++;
            cptr = c;
            val = CalcTerm( &cptr ); // evaluate the express
            c = cptr;
            if ( LastRecalc ) {      // expression undefined?
                LastRecalc->typ = 1; // add two bytes
                LastRecalc->adr = iPC;
            }
            checkPC( iPC + 1 ); // will it overflow?
            RAM[ iPC++ ] = val;
            RAM[ iPC++ ] = val >> 8;
        } while ( ( c->typ == OPCODE ) && ( c->val == ',' ) );
        break;
    case END:
        if ( IgnoreUntilIF )
            Error( "IF without ENDIF" );
        Error( "Reached the end of the source code -> exit" );
        exit( 0 );
    case ORG:
        cptr = c;
        iPC = CalcTerm( &cptr ); // set the PC
        c = cptr;
        if ( LastRecalc )
            Error( "symbol not defined" );
        break;
    case IF:
        cptr = c;
        if ( !CalcTerm( &cptr ) ) // IF condition false?
            IgnoreUntilIF = true; // then ignore the next block
        c = cptr;
        break;
    case ENDIF:
        IgnoreUntilIF = false; // never ignore from here on
        break;
    case ELSE:
        IgnoreUntilIF = !IgnoreUntilIF; // flip the condition
        break;
    case PRINT:
        if ( c->typ != STRING )
            Error( "PRINT requires a string parameter" );
        else
            puts( (char *)c++->val ); // print a message
        break;
    }
    *cp = c;
    PC = iPC;
}


/***
 *  Compile a single line
 ***/
void CompileLine( void ) {
    MSG( 2, "CompileLine()\n" );
    CommandP c = Cmd;
    CommandP cptr;
    SymbolP s;
    uint16_t val;

    if ( !c->typ )
        return;                                     // empty line => done
    if ( ( c->typ == SYMBOL ) && !IgnoreUntilIF ) { // symbol at the beginning, but not IF?
        s = (SymbolP)c->val;                        // value = ptr to the symbol
        if ( s->defined ) {
            Error( "symbol already defined" );
            return;
        }
        c++; // next command
        if ( ( c->typ == OPCODE ) && ( c->val == ':' ) )
            c++;                                             // ignore a ":" after a symbol
        if ( ( c->typ == OPCODE ) && ( c->val == 0x105 ) ) { // EQU?
            c++;                                             // skip EQU
            cptr = c;
            s->val = CalcTerm( &cptr ); // calculate the expression
            c = cptr;
            if ( LastRecalc )
                Error( "symbol not defined in a formula" );
            s->defined = true; // symbol now defined
            if ( c->typ != ILLEGAL ) {
                Error( "EQU is followed by illegal data" );
                return;
            }
        } else {
            s->val = PC;       // adresse = current PC
            s->defined = true; // symbol is defined
        }
        while ( s->recalc ) { // do expressions depend on the symbol?
            RecalcListP r = s->recalc;
            CommandP saveC;
            int32_t value;

            s->recalc = r->next; // to the next symbol
            saveC = r->c;
            value = CalcTerm( &( r->c ) ); // Recalculate the symbol (now with the defined symbol)
            if ( !LastRecalc ) {           // Is the expression now valid? (or is there another open dependency?)
                uint16_t adr = r->adr;
                switch ( r->typ ) {
                case 0x00: // add a single byte
                    list( "%04X <- %02X\n", adr, value );
                    RAM[ adr ] = value;
                    break;
                case 0x01: // add two bytes
                    list( "%04X <- %02X %02X\n", adr, value & 0xFF, value >> 8 );
                    RAM[ adr++ ] = value;
                    RAM[ adr ] = value >> 8;
                    break;
                case 0x02: // PC-rel-byte
                    value -= ( adr + 1 );
                    list( "%04X <- %02X\n", adr, value );
                    RAM[ adr ] = value;
                    break;
                default:
                    Error( "unknown recalc type" );
                }
            } else {                      // Expression still can't be calculated
                LastRecalc->typ = r->typ; // transfer the type
                LastRecalc->adr = r->adr; // transfer the address
            }
            free( saveC ); // release the formula
            free( r );     // release the Recalc term
        }
    }
    if ( IgnoreUntilIF ) { // inside an IFs?
        if ( c->typ == OPCODE ) {
            switch ( c->val ) {
            case 0x108:                // ENDIF reached?
                IgnoreUntilIF = false; // start compiling
                break;
            case 0x109:                         // ELSE reached?
                IgnoreUntilIF = !IgnoreUntilIF; // toggle IF flag
                break;
            }
        }
    } else
        while ( c->typ ) { // scan to the end of the line
            val = c->val;
            if ( ( val < 0x100 ) || ( val > 0x2FF ) ) // no Opcode or Pseudo-Opcode?
                Error( "Illegal token" );             // => error
            cptr = c;
            if ( ( val >= 0x100 ) && ( val <= 0x1FF ) ) // Pseudo-Opcode
                DoPseudo( &cptr );
            else
                DoOpcode( &cptr ); // opcode
            c = cptr;
        }
}
