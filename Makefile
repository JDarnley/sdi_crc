CC=gcc
CFLAGS=-I.

sdi_crc: sdi_crc.o
	$(CC) -o sdi_crc sdi_crc.o

clean:
	rm -f sdi_crc.o sdi_crc