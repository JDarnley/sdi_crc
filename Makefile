CC=gcc
CFLAGS=-I.

sdi_crc: sdi_crc.o sdi_crc_asm
	$(CC) -o sdi_crc sdi_crc.o sdi_crc_asm.o

sdi_crc_asm:
	nasm -f elf64 -DPIC -Pconfig.asm sdi_crc_asm.asm

clean:
	rm -f sdi_crc.o sdi_crc_asm.o sdi_crc