Poster - resize a postscript image to print on larger media and/or multiple sheets
Copyright (C) 1999 Jos T.J. van Eijndhoven

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA

The full text of the GNU General Public License is included
with poster in the file LICENSE.
=============================================================================

Hello candidate `poster' user!

Here you have the new release of `poster', to scale postscript
images to a larger size, and print them on larger media and/or
tile them to print on multiple sheets.
With respect to the earlier release:
- support is added for foreign (Non European A*) media sizes.
- options for scaling became more flexible
- original restrictions on white margins in your drawing are removed.
For a complete explanation see the accompanying manual.

This distribution has the following files:
==========================================
README     (which you are reading now)
Makefile   (To compile `poster' in UNIX environments)
poster.c   (The complete source code)
poster.1   (A troff-source manual page for online installation in UNIX)
manual.ps  (A formatted version of poster.1 in postscript)

Furthermore for your convenience:
poster.tar.gz    (The compressed collection of the above 5 files)
getopt.c         (you normally don't need to fetch this, see below)


Here a few words on the installation of `poster':
==================================================

The complete program consists of really only one file: `poster.c'.
Before starting compilation you might want to take a look on
the C sources in `poster.c', where you can set a few options around line 30:
  Maybe you want to change the `DefaultMedia' and `DefaultImage' from "A4"
  to better reflect your local situation (such as "Letter").
  Media names can be chosen from the `mediatable' further down the code.
  (Maybe you even want to add new media sizes/names there,
   you can do that without requiring any other change elsewhere)

  The `Gv_gs_orientbug 1' disables a feature of this program to
  ask for landscape (horizontal) previewing of rotated images.
  Our currently installed combination of ghostview 1.5 with ghostscript 3.33
  cannot properly do a landscape viewing of the `poster' output.
  The problem does not exist in combination with an older ghostscript 2.x,
  and has the attention of the ghostview authors.
  If you have a different postscript previewing environment, you might
  want to remove or comment-out the `#define Gv_gs_orientbug 1' line.

You should be able to compile this with any ansi-C
compiler in a Posix or Xopen environment.
You can probably compile it with a command like:
     cc -O -o poster poster.c -lm
(i.e. compile with optimization, and link with the math library)

(Some environments miss the required 'getopt()' call,
 with the <unistd.h> include file,
 if your environment supports none of the SVID, XPG or POSIX standards.
 If you have this problem, you can comment out the '#include <unistd.h>'
 line in `poster.c', fetch `getopt.c' from the poster directory,
 and compile and link these two files together.)

(Note that this program might trigger a stupid bug in the HPUX 9.? C library,
 causing the sscanf() call to produce a core dump.
 For proper operation, DON'T give the `+ESlit' option to the HP cc,
 or use gcc WITH the `-fwritable-strings' option.)

The resulting executable is fully self-contained,
and doesn't require you to install other files at
`special' places in the OS.

For UNIX environments (the primary target for the program)
there is a man page available `poster.1', which you can copy
to /usr/local/man/man1/ to obtain online manual support for
the users.

The formatted version of this manual is available as a
postscript file `manual.ps', such that people who
don't know how to format unix man pages, still have
documentation to read and print.

Success!

Jos van Eijndhoven
Dept. of Elec. Eng.
Eindhoven Univ of Technology
The Netherlands
email: J.T.J.v.Eijndhoven@ele.tue.nl

