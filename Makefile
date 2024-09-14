CC=gcc
CFLAGS=-I. -Wall
DEPS = z80_assembler.h

%.o: %.cp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

all: z80assembler z80disassembler

z80assembler: z80_assembler.o z80_tokenize.o z80_compile.o z80_calc.o kk_ihex_write.o
	$(CC) -o $@ $^ $(CFLAGS)

z80disassembler: z80_disassembler.o file.o kk_ihex_read.o
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -f *.o
