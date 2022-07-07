# Z80DisAssembler

The Z80 CPU was widely used in the 80s in many home computer. Even today it is often used as a cheap embedded CPU. If you need more information can get one of the following books:

Z80 Disassembler
----------------

I created this small disassembler for a Z80 cpu at one afternoon. It is a commandline tool. The size of the ROM and entry points have to be coded directly in the sourcecode.

Every ANSI C compiler should compile this program. It is tested with Think C 5.0 on a Macintosh. It only uses some ANSI functions (look into ''main()'') for loading a file called "EPROM".

The program has two parts:

  - Analyze the code. The disassembler tries to analyze what part of the binary data is program code and what part is data. It start with all hardware vectors of the Z80 (''RST'' opcodes, NMI) and parses all jumps via a recursive analyze via ''ParseOpcode()''. Every opcode is marked in an array (''OpcodesFlags''). There are some exceptions, the parser can't recognize:
    - self modifying code. A ROM shouldn't contain such code.
    - calculated branches with ''JP (IY)'', ''JP (IX)'' or ''JP (HL)''. The parser can't recognize them, either. On a Macintosh the low-level debugger is called, when such a jump is found. Set the Symbol ''DEBUGGER'' to 0 to disable this behavior...
    - Jumptables. These are quite often in a ROM. Only solution: disassemble the program and look into the code. If you found a jumptable - like on my Futura aquarium computer - insert some more calls of ''ParseOpcodes()''.
    - Unused code. Code that is never called by anybody, could not be found. Make sure that the code is not called via a jump table!
  - Disassembly of the code. With the help of the OpcodesFlags table the disassembler now creates the output. This subroutine is quite long. It disassembles one opcode at a specific address in ROM into a buffer. It is coded directly from a list of Z80 opcodes, so the handling of ''IX'' and ''IY'' could be optimized quite a lot.

The subroutine ''OpcodeLen()'' returns the size of one opcode in bytes. It is called while parsing and while disassembling.

The disassembler recognizes no hidden opcodes (the assembler does!). I didn't had a table for them while writing the disassembler and they were not needed anyway.

If a routine wanted an "address" to the Z80 code, it is in fact an **offset** to the array of code. **No** pointers! Longs are not necessary for a Z80, because the standard Z80 only supports 64k.

In ''main()'' is a switch for disassembly with address and hexdump instead of disassembly with labels. This is useful for findings bugs in the disassembler and creating a list of variables.

This program is freeware. It is not allowed to be used as a base for a commercial product!

Z80 Assembler
-------------

I created the assembler for the Z80 a few days later to compile the changes code from the disassembler into an EPROM image and build a new firmware for my aquarium computer. I needed almost two days for the assembler, this means: commandline only... If you want to change the filename of the sourcefile, you have to change main().

This small assembler has some nice gadgets: it is a quite fast tokenizing single-pass assembler with backpatching. It knows all official Z80 opcodes and some undocumented opcodes (mainly with ''IX'' and ''IY''). The Z80 syntax is documented in the Zilog documentation.

The assembler allows mathematical expressions in operands: ''+'', ''-'', ''*'', ''/'', ''%'' (modulo), ''&'' (and), ''|'' (or), ''!'' (not), ''^'' (xor), ''<<'' (shift left) and ''>>'' (shift right). Brackets are also available. The expression parser is located in [[Z80 Calc.c]]. Number can be postpended by a ''D'', ''H'' or ''B'' for decimal, hexadecimal and binary numbers.

The assembler also knows the most commend pseudo opcodes (look into the sourcefile 'Z80 Tokenize.c'):

  * '';'' This line is a comment.
  * ''IF'' Start the conditional expression. If false, the following sourcecode will be skipped (until ''ELSE'' or ''ENDIF'').
  * ''ENDIF'' End of the condition expression.
  * ''ELSE'' Include the following code, when the expression on IF was false.
  * ''END'' End of the sourcecode. The assembler stops here. Optional.
  * ''ORG'' Set the PC in the 64k address space. E.g. to generate code for address $2000.
  * ''PRINT'' Print the following text on the console. Great for testing the assembler.
  * ''EQU'' or ''='' Set a variable.
  * ''DEFB'' Put a byte at the current address
  * ''DEFW'' But a word at the current address (little endian!)
  * ''DEFM'' But several bytes in the memory, starting at the current address. Seperated with a "," or a string.
  * ''DEFS'' Set the current address n bytes ahead. Defines space for global variables that have no given value.

The Sourcecode
--------------

  * [Z80 Assembler.c](z80_assembler.c)
  * [Z80 Assembler.h](z80_assembler.h)
  * [Z80 Calc.c](z80_calc.c)
  * [Z80 Compile.c](z80_compile.c)
  * [Z80 Disassembler.c](z80_disassembler.c)
  * [Z80 Tokenize.c](z80_tokenize.c)
