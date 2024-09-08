CC=gcc
CFLAGS=-I.
DEPS = z80_assembler.h

%.o: %.cp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

all: z80assembler z80disassembler

z80assembler: z80_assembler.o z80_tokenize.o z80_compile.o z80_calc.o
	$(CC) -o $@ $^ $(CFLAGS)

z80disassembler: z80_disassembler.o
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -f *.o
