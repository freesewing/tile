poster: poster.c
	gcc -O -o poster poster.c -lm

# HPUX:	cc -O -Aa -D_POSIX_SOURCE -o poster poster.c -lm
#       Note that this program might trigger a stupid bug in the HPUX C library,
#       causing the sscanf() call to produce a core dump.
#       For proper operation, DON'T give the `+ESlit' option to the HP cc,
#       or use gcc WITH the `-fwritable-strings' option.

install: poster
	strip poster
	cp poster /usr/local/bin
	cp poster.1 /usr/local/man/man1

clean:
	rm -f poster core poster.o getopt.o

tar: README Makefile poster.c poster.1 manual.ps LICENSE
	tar -cvf poster.tar README Makefile poster.c poster.1 manual.ps LICENSE
	rm -f poster.tar.gz
	gzip poster.tar
