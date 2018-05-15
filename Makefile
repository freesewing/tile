tile: tile.c tilelang.c tilelang.h 
	gcc -O -o tile tile.c tilelang.c -lm

# HPUX:	cc -O -Aa -D_POSIX_SOURCE -o tile tile.c -lm
#       Note that this program might trigger a stupid bug in the HPUX C library,
#       causing the sscanf() call to produce a core dump.
#       For proper operation, DON'T give the `+ESlit' option to the HP cc,
#       or use gcc WITH the `-fwritable-strings' option.

install: tile
	strip tile
	cp tile /usr/local/bin
	cp tile.1 /usr/local/man/man1

clean:
	rm -f tile core tile.o getopt.o

tar: README Makefile tile.c tile.1 manual.ps LICENSE
	tar -cvf tile.tar README Makefile tile.c tile.1 manual.ps LICENSE
	rm -f tile.tar.gz
	gzip tile.tar
