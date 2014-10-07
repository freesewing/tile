mmposter: mmposter.c
	gcc -O -o mmposter mmposter.c -lm

# HPUX:	cc -O -Aa -D_POSIX_SOURCE -o poster poster.c -lm
#       Note that this program might trigger a stupid bug in the HPUX C library,
#       causing the sscanf() call to produce a core dump.
#       For proper operation, DON'T give the `+ESlit' option to the HP cc,
#       or use gcc WITH the `-fwritable-strings' option.

install: mmposter
	strip mmposter
	cp mmposter /usr/local/bin
	cp mmposter.1 /usr/local/man/man1

clean:
	rm -f mmposter core mmposter.o getopt.o

tar: README Makefile mmposter.c mmposter.1 manual.ps LICENSE
	tar -cvf mmposter.tar README Makefile mmposter.c mmposter.1 manual.ps LICENSE
	rm -f mmposter.tar.gz
	gzip mmposter.tar
