CC=gcc
CFLAGS=-I. -g -mavx2 -mpclmul

sdi_crc: sdi_crc.o sdi_crc_asm.o

%.o: %.asm
	nasm -f elf64 -DPIC -Pconfig.asm $< -o $@

clean:
	rm -f sdi_crc.o sdi_crc_asm.o sdi_crc
