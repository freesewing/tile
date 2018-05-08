/*
#  tile - resize a postscript image to print on larger media and/or multiple sheets
#
#  This program scales a PostScript page to a given size (a poster).
#  The output can be tiled on multiple sheets, and output
#  media size can be chosen independently.
#  Each tile (sheet) of a will bear cropmarks and slightly overlapping
#  image for easier poster assembly.
#  In principle it requires the input file to adhere to 'eps'
#  (encapsulated postscript) conventions but it will work for many
#  'normal' postscript files as well.
#
#  Compile this program with:
#        cc -O -o tile tile.c -lm
#  or something alike.
#
#  Maybe you want to change the `DefaultMedia' and `DefaultImage'
#  settings in the few lines below, to reflect your local situation.
#  Names can to be chosen from the `mediatable' further down.
#
#  The `Gv_gs_orientbug 1' disables a feature of this program to
#  ask for landscape previewing of rotated images.
#  Our currently installed combination of ghostview 1.5 with ghostscript 3.33
#  cannot properly do a landscape viewing of the `poster' output.
#  The problem does not exist in combination with an older ghostscript 2.x,
#  and has the attention of the ghostview authors.
#  (The problem is in the evaluation of the `setpagedevice' call.)
#  If you have a different previewing environment,
#  you might want to set `Gv_gs_orientbug 0'
#
#  Tile now adds a coverpage, and takes the t and h options.
#  I (joost) have yet to make this optional, and update the man page.
#
#  Tile has now support for language files
#  (tile.<2 letter language code>.yml, "tile.en.yml")
#
# --------------------------------------------------------------
#  Tile is a fork of 'poster' by Jos T.J. van Eijndhoven
#  <J.T.J.v.Eijndhoven@ele.tue.nl>
#
#  Forked by Joost De Cock for freesewing.org
#  Language extention by Wouter van Wageningen
#
#  Copyright (C) 1999 Jos T.J. van Eijndhoven
#  Copyright (C) 2017 Joost De Cock
# --------------------------------------------------------------a
*/

#define Gv_gs_orientbug 1
#define DefaultMedia  "A4"
#define DefaultImage  "A4"
#define DefaultCutMargin "5%"
#define DefaultWhiteMargin "0"
#define BUFSIZE 1024
#define DefaultLanguage "en"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "tilelang.h"


extern char *optarg;        /* silently set by getopt() */
extern int optind, opterr;  /* silently set by getopt() */

static void usage();
static void dsc_head1();
static int dsc_infile( double ps_bb[4]);
static void dsc_head2( void);
static void printposter( void );
static void printprolog();
static void tile ( int row, int col, int nrows, int ncols);
static void cover ( int row, int col);
static void printfile( void);
static void postersize( char *scalespec, char *posterspec);
static void box_convert( char *boxspec, double psbox[4]);
static void boxerr( char *spec);
static void margin_convert( char *spec, double margin[2]);
static int mystrncasecmp( const char *s1, const char *s2, int n);

int verbose;
int alignment = 0;
char *myname;
char *infile;
int rotate, nrows, ncols;
int manualfeed = 0;
int tail_cntl_D = 0;
#define Xl 0
#define Yb 1
#define Xr 2
#define Yt 3
#define X 0
#define Y 1
double posterbb[4];	/* final image in ps units */
double imagebb[4];	/* original image in ps units */
double mediasize[4];	/* [34] = size of media to print on, [01] not used! */
double cutmargin[2];
double whitemargin[2];
double scale;		/* linear scaling factor */

/* defaults: */
char *imagespec = NULL;
char *posterspec = NULL;
char *mediaspec = NULL;
char *cutmarginspec = NULL;
char *whitemarginspec = NULL;
char *scalespec = NULL;
char *filespec = NULL;
char *patterntitle = NULL;
char *patternhandle = NULL;
char *language = NULL;

/* media sizes in ps units (1/72 inch) */
static char *mediatable[][2] =
{	{ "Letter",   "612,792"},
	{ "Legal",    "612,1008"},
	{ "Tabloid",  "792,1224"},
	{ "Ledger",   "792,1224"},
	{ "Executive","540,720"},
	{ "Monarch",  "279,540"},
	{ "Statement","396,612"},
	{ "Folio",    "612,936"},
	{ "Quarto",   "610,780"},
	{ "C5",       "459,649"},
	{ "B4",       "729,1032"},
	{ "B5",       "516,729"},
	{ "Dl",       "312,624"},
	{ "A0",	      "2380,3368"},
	{ "A1",	      "1684,2380"},
	{ "A2",	      "1190,1684"},
	{ "A3",	      "842,1190"},
	{ "A4",       "595,842"},
	{ "A5",	      "420,595"},
	{ "A6",	      "297,421"},

	/* as fall-back: linear units of measurement: */
	{ "p",        "1,1"},
	{ "i",        "72,72"},
	{ "ft",       "864,864"},
	{ "mm",       "2.83465,2.83465"},
	{ "cm",       "28.3465,28.3465"},
	{ "m",        "2834.65,2834.65"},
	{ NULL, NULL}
};


int main( int argc, char *argv[])
{
	int opt;
	double ps_bb[4];
	int got_bb;

	myname = argv[0];

	while ((opt = getopt( argc, argv, "vafi:c:l:w:m:p:s:o:t:h:")) != EOF)
	{	switch( opt)
		{ case 'v':	verbose++; break;
		  case 'f': manualfeed = 1; break;
		  case 'a': alignment = 1; break;
			case 'l': language = optarg; break;
		  case 'i':	imagespec = optarg; break;
		  case 'c':	cutmarginspec = optarg; break;
		  case 'w':	whitemarginspec = optarg; break;
		  case 'm':	mediaspec = optarg; break;
		  case 'p':	posterspec = optarg; break;
		  case 's':	scalespec = optarg; break;
		  case 'o': filespec = optarg; break;
		  case 't': patterntitle = optarg; break;
		  case 'h': patternhandle = optarg; break;
		  default:	usage(); break;
		}
	}

	/*** check command line arguments ***/
	if (scalespec && posterspec)
	{	fprintf( stderr, "Please don't specify both -s and -o, ignoring -s!\n");
		scalespec = NULL;
	}

	if (optind < argc)
		infile = argv[ optind];
	else
	{	fprintf( stderr, "Filename argument missing!\n");
		usage();
	}

	/*** decide on media size ***/
	if (!mediaspec)
	{	mediaspec = DefaultMedia;
		if (verbose)
			fprintf( stderr,
				"Using default media of %s\n",
				mediaspec);
	}
	box_convert( mediaspec, mediasize);
	if (mediasize[3] < mediasize[2])
	{	fprintf( stderr, "Media should always be specified in portrait format!\n");
		exit(1);
	}
	if (mediasize[2]-mediasize[0] <= 10.0 || mediasize[3]-mediasize[1] <= 10.0)
	{	fprintf( stderr, "Media size is ridiculous!\n");
		exit(1);
	}

	/*** defaulting poster size ? **/
	if (!scalespec && !posterspec)
	{	/* inherit postersize from given media size */
		posterspec = mediaspec;
		if (verbose)
			fprintf( stderr,
				"Defaulting poster size to media size of %s\n",
				mediaspec);
	}

	/*** decide the cutmargin size, after knowing media size ***/
	if (!cutmarginspec)
	{	/* if (!strcmp( posterspec, mediaspec)) */
			/* zero cutmargin if printing to 1 sheet */
		/*	marginspec = "0%";
		else */	cutmarginspec = DefaultCutMargin;
		if (verbose)
			fprintf( stderr,
				"Using default cutmargin of %s\n",
				cutmarginspec);
	}
	margin_convert( cutmarginspec, cutmargin);

	/*** decide the whitemargin size, after knowing media size ***/
	if (!whitemarginspec)
	{	whitemarginspec = DefaultWhiteMargin;
		if (verbose)
			fprintf( stderr,
				"Using default whitemargin of %s\n",
				whitemarginspec);
	}
	margin_convert( whitemarginspec, whitemargin);

	/*** get the language code ***/
	if (!language)
	{	language = DefaultLanguage;
		if (verbose)
			fprintf( stderr,
				"Using default language of %s\n",
				language);
	}
	if( strlen( language ) != 2 )
	{
		fprintf( stderr,
			"Invalid language code '%s''\n",
			language);
	}
	else if( LangRead( language ) )
	{
		fprintf( stderr,
			"Error reading language file for '%s'. Using default language of 'en'\n",
			language);
	}

	/******************* now start doing things **************************/
	/* open output file */
	if (filespec)
	{	if (!freopen( filespec, "w", stdout))
		{	fprintf( stderr, "Cannot open '%s' for writing!\n",
				 filespec);
			exit(1);
		} else if (verbose)
			fprintf( stderr, "Opened '%s' for writing\n",
				 filespec);
	}

	/******* I might need to read some input to find picture size ********/
	/* start DSC header on output */
	dsc_head1();

	/* pass input DSC lines to output, get BoundingBox spec if there */
	got_bb = dsc_infile( ps_bb);

	/**** decide the input image bounding box ****/
	if (!got_bb && !imagespec)
	{	imagespec = DefaultImage;
		if (verbose)
			fprintf( stderr,
				"Using default input image of %s\n",
				imagespec);
	}
	if (imagespec)
		box_convert( imagespec, imagebb);
	else
	{	int i;
		for (i=0; i<4; i++)
			imagebb[i] = ps_bb[i];
	}

	if (verbose > 1)
		fprintf( stderr, "   Input image is: [%g,%g,%g,%g]\n",
			imagebb[0], imagebb[1], imagebb[2], imagebb[3]);

	if (imagebb[2]-imagebb[0] <= 0.0 || imagebb[3]-imagebb[1] <= 0.0)
	{	fprintf( stderr, "Input image should have positive size!\n");
		exit(1);
	}


	/*** decide on the scale factor and poster size ***/
	postersize( scalespec, posterspec);

	if (verbose > 1)
		fprintf( stderr, "   Output image is: [%g,%g,%g,%g]\n",
			posterbb[0], posterbb[1], posterbb[2], posterbb[3]);


	dsc_head2();

	printposter();

	LangClose();

	exit (0);
}

static void usage()
{
	fprintf( stderr, "Usage: %s <options> infile\n\n", myname);
	fprintf( stderr, "options are:\n");
	fprintf( stderr, "   -v:         be verbose\n");
	fprintf( stderr, "   -a:         add alignment marks\n");
	fprintf( stderr, "   -f:         ask manual feed on plotting/printing device\n");
	fprintf( stderr, "   -l<lang>:   specify language code (en, nl, fr)\n");
	fprintf( stderr, "   -i<box>:    specify input image size\n");
	fprintf( stderr, "   -c<margin>: horizontal and vertical cutmargin\n");
	fprintf( stderr, "   -w<margin>: horizontal and vertical additional white margin\n");
	fprintf( stderr, "   -m<box>:    media paper size\n");
	fprintf( stderr, "   -p<box>:    output poster size\n");
	fprintf( stderr, "   -s<number>: linear scale factor for poster\n");
	fprintf( stderr, "   -o<file>:   output redirection to named file\n\n");
	fprintf( stderr, "   At least one of -s -p -m is mandatory, and don't give both -s and -p\n");
	fprintf( stderr, "   <box> is like 'A4', '3x3letter', '10x25cm', '200x200+10,10p'\n");
	fprintf( stderr, "   <margin> is either a simple <box> or <number>%%\n\n");

	fprintf( stderr, "   Defaults are: '-m%s', '-c%s', '-i<box>' read from input file.\n",
		DefaultMedia, DefaultCutMargin);
	fprintf( stderr, "                 and output written to stdout.\n");

	exit(1);
}

#define exch( x, y)	{double h; h=x; x=y; y=h;}

static void postersize( char *scalespec, char *posterspec)
{	/* exactly one the arguments is NULL ! */
	/* media and image sizes are fixed already */

	int nx0, ny0, nx1, ny1;
	double sizex, sizey;    /* size of the scaled image in ps units */
	double drawablex, drawabley; /* effective drawable size of media */
	double mediax, mediay;
	double tmpposter[4];

	/* available drawing area per sheet: */
	drawablex = mediasize[2] - 2.0*cutmargin[0];
	drawabley = mediasize[3] - 2.0*cutmargin[1];

	/*** decide on number of pages  ***/
	if (scalespec)
	{	/* user specified scale factor */
		scale = atof( scalespec);
		if (scale < 0.01 || scale > 1.0e6)
		{	fprintf( stderr, "Illegal scale value %s!\n", scalespec);
			exit(1);
		}
		sizex = (imagebb[2] - imagebb[0]) * scale + 2*whitemargin[0];
		sizey = (imagebb[3] - imagebb[1]) * scale + 2*whitemargin[1];

		/* without rotation */
		nx0 = ceil( sizex / drawablex);
		ny0 = ceil( sizey / drawabley);

		/* with rotation */
		nx1 = ceil( sizex / drawabley);
		ny1 = ceil( sizey / drawablex);

	} else
	{	/* user specified output size */
		box_convert( posterspec, tmpposter);
		if (tmpposter[0]!=0.0 || tmpposter[1]!=0.0)
		{	fprintf( stderr, "Poster lower-left coordinates are assumed 0!\n");
			tmpposter[0] = tmpposter[1] = 0.0;
		}
		if (tmpposter[2]-tmpposter[0] <= 0.0 || tmpposter[3]-tmpposter[1] <= 0.0)
		{	fprintf( stderr, "Poster should have positive size!\n");
			exit(1);
		}

		if ((tmpposter[3]-tmpposter[1]) < (tmpposter[2]-tmpposter[0]))
		{	/* hmmm... landscape spec, change to portrait for now */
			exch( tmpposter[0], tmpposter[1]);
			exch( tmpposter[2], tmpposter[3]);
		}


		/* Should we tilt the poster to landscape style? */
		if ((imagebb[3] - imagebb[1]) < (imagebb[2] - imagebb[0]))
		{	/* image has landscape format ==> make landscape poster */
			exch( tmpposter[0], tmpposter[1]);
			exch( tmpposter[2], tmpposter[3]);
		}

		/* without rotation */ /* assuming tmpposter[0],[1] = 0,0 */
		nx0 = ceil( 0.95 * tmpposter[2] / mediasize[2]);
		ny0 = ceil( 0.95 * tmpposter[3] / mediasize[3]);

		/* with rotation */
		nx1 = ceil( 0.95 * tmpposter[2] / mediasize[3]);
		ny1 = ceil( 0.95 * tmpposter[3] / mediasize[2]);
		/* (rotation is considered as media versus image, which is totally */
		/*  independent of the portrait or landscape style of the final poster) */
	}

	/* decide for rotation to get the minimum page count */
	rotate = nx0*ny0 > nx1*ny1;

	ncols = rotate ? nx1 : nx0;
	nrows = rotate ? ny1 : ny0;

	if (verbose)
		fprintf( stderr,
			"Deciding for %d column%s and %d row%s of %s pages.\n",
			ncols, (ncols==1)?"":"s", nrows, (nrows==1)?"":"s",
			rotate?"landscape":"portrait");

	if (nrows * ncols > 400)
	{	fprintf( stderr, "However %dx%d pages seems ridiculous to me!\n",
			ncols, nrows);
		exit(1);
	}

	mediax = ncols * (rotate ? drawabley : drawablex);
	mediay = nrows * (rotate ? drawablex : drawabley);

	if (!scalespec)  /* no scaling number given by user */
	{	double scalex, scaley;
		scalex = (mediax - 2*whitemargin[0]) / (imagebb[2] - imagebb[0]);
		scaley = (mediay - 2*whitemargin[1]) / (imagebb[3] - imagebb[1]);
		scale = (scalex < scaley) ? scalex : scaley;

		if (verbose)
			fprintf( stderr,
				"Deciding for a scale factor of %g\n", scale);
		sizex = scale * (imagebb[2] - imagebb[0]);
		sizey = scale * (imagebb[3] - imagebb[1]);
	}

	/* set poster size as if it were a continuous surface without margins */
	posterbb[0] = (mediax - sizex) / 2.0; /* center picture on paper */
	posterbb[1] = (mediay - sizey) / 2.0; /* center picture on paper */
	posterbb[2] = posterbb[0] + sizex;
	posterbb[3] = posterbb[1] + sizey;

}

static void margin_convert( char *spec, double margin[2])
{	double x;
	int i, n;

	if (1==sscanf( spec, "%lf%n", &x, &n) && x==0.0 && n==strlen(spec))
	{	/* margin spec of 0, dont bother about a otherwise mandatory unit */
		margin[0] = margin[1] = 0.0;
	} else if (spec[ strlen( spec) - 1] == '%')
	{	/* margin relative to media size */
		if (1 != sscanf( spec, "%lf%%", &x))
		{	fprintf( stderr, "Illegal margin specification!\n");
			exit( 1);
		}
		margin[0] = 0.01 * x * mediasize[2];
		margin[1] = 0.01 * x * mediasize[3];
	} else
	{	/* absolute margin value */
		double marg[4];
		box_convert( spec, marg);
		margin[0] = marg[2];
		margin[1] = marg[3];
	}

	for (i=0; i<2; i++)
	{	if (margin[i] < 0 || 2.0*margin[i] >= mediasize[i+2])
		{	fprintf( stderr, "Margin value '%s' out of range!\n",
				spec);
			exit(1);
		}
	}
}

static void box_convert( char *boxspec, double psbox[4])
{	/* convert user textual box spec into numbers in ps units */
	/* box = [fixed x fixed][+ fixed , fixed] unit */
	/* fixed = digits [ . digits] */
	/* unit = medianame | i | cm | mm | m | p */

	double mx, my, ox, oy, ux, uy;
	int n, r, i, l, inx;
	char *spec;

	mx = my = 1.0;
	ox = oy = 0.0;

	spec = boxspec;
	/* read 'fixed x fixed' */
	if (isdigit( spec[0]))
	{	r = sscanf( spec, "%lfx%lf%n", &mx, &my, &n);
		if (r != 2)
		{	r = sscanf( spec, "%lf*%lf%n", &mx, &my, &n);
			if (r != 2) boxerr( boxspec);
		}
		spec += n;
	}

	/* read '+ fixed , fixed' */
	if (1 < (r = sscanf( spec, "+%lf,%lf%n", &ox, &oy, &n)))
	{	if (r != 2) boxerr( boxspec);
		spec += n;
	}

	/* read unit */
	l = strlen( spec);
	for (n=i=0; mediatable[i][0]; i++)
	{	if (!mystrncasecmp( mediatable[i][0], spec, l))
		{	/* found */
			n++;
			inx = i;
			if (l == strlen( mediatable[i][0]))
			{	/* match is exact */
				n = 1;
				break;
			}
		}
	}
	if (!n) boxerr( boxspec);
	if (n>1)
	{	fprintf( stderr, "Your box spec '%s' is not unique! (give more chars)\n",
			spec);
		exit(1);
	}
	sscanf( mediatable[inx][1], "%lf,%lf", &ux, &uy);

	psbox[0] = ox * ux;
	psbox[1] = oy * uy;
	psbox[2] = mx * ux;
	psbox[3] = my * uy;

	if (verbose > 1)
		fprintf( stderr, "   Box_convert: '%s' into [%g,%g,%g,%g]\n",
			boxspec, psbox[0], psbox[1], psbox[2], psbox[3]);

	for (i=0; i<2; i++)
	{	if (psbox[i] < 0.0 || psbox[i+2] < psbox[i])
		{	fprintf( stderr, "Your specification `%s' leads to "
				"negative values!\n", boxspec);
			exit(1);
		}
	}
}

static void boxerr( char *spec)
{	int i;

	fprintf( stderr, "I don't understand your box specification `%s'!\n",
		spec);

	fprintf( stderr, "The proper format is: ([text] meaning optional text)\n");
	fprintf( stderr, "  [multiplier][offset]unit\n");
	fprintf( stderr, "  with multiplier:  numberxnumber\n");
	fprintf( stderr, "  with offset:      +number,number\n");
	fprintf( stderr, "  with unit one of:");

	for (i=0; mediatable[i][0]; i++)
		fprintf( stderr, "%c%-10s", (i%7)?' ':'\n', mediatable[i][0]);
	fprintf( stderr, "\nYou can use a shorthand for these unit names,\n"
		"provided it resolves unique.\n");
	exit( 1);
}

/*********************************************/
/* output first part of DSC header           */
/*********************************************/
static void dsc_head1()
{
	printf ("%%!PS-Adobe-3.0\n");
	printf ("%%%%Creator: %s\n", myname);
}

/*********************************************/
/* pass some DSC info from the infile in the new DSC header */
/* such as document fonts and */
/* extract BoundingBox info from the PS file */
/*********************************************/
static int dsc_infile( double ps_bb[4])
{
	char *c, buf[BUFSIZE];
	int gotall, atend, level, dsc_cont, inbody, got_bb;

	if (freopen (infile, "r", stdin) == NULL) {
		fprintf (stderr, "%s: fail to open file '%s'!\n",
			myname, infile);
		exit (1);
	}

	got_bb = 0;
	dsc_cont = inbody = gotall = level = atend = 0;
	while (!gotall && (gets(buf) != NULL))
	{
		if (buf[0] != '%')
		{	dsc_cont = 0;
			if (!inbody) inbody = 1;
			if (!atend) gotall = 1;
			continue;
		}

		if (!strncmp( buf, "%%+",3) && dsc_cont)
		{	puts( buf);
			continue;
		}

		dsc_cont = 0;
		if      (!strncmp( buf, "%%EndComments", 13))
		{	inbody = 1;
			if (!atend) gotall = 1;
		}
		else if (!strncmp( buf, "%%BeginDocument", 15) ||
		         !strncmp( buf, "%%BeginData", 11)) level++;
		else if (!strncmp( buf, "%%EndDocument", 13) ||
		         !strncmp( buf, "%%EndData", 9)) level--;
		else if (!strncmp( buf, "%%Trailer", 9) && level == 0)
			inbody = 2;
		else if (!strncmp( buf, "%%BoundingBox:", 14) &&
			 inbody!=1 && !level)
		{	for (c=buf+14; *c==' ' || *c=='\t'; c++);
			if (!strncmp( c, "(atend)", 7)) atend = 1;
			else
			{	sscanf( c, "%lf %lf %lf %lf",
				       ps_bb, ps_bb+1, ps_bb+2, ps_bb+3);
				got_bb = 1;
			}
		}
		else if (!strncmp( buf, "%%Document", 10) &&
			 inbody!=1 && !level)  /* several kinds of doc props */
		{	for (c=buf+10; *c && *c!=' ' && *c!='\t'; c++);
			for (; *c==' ' || *c=='\t'; c++);
			if (!strncmp( c, "(atend)", 7)) atend = 1;
			else
			{	/* pass this DSC to output */
				puts( buf);
				dsc_cont = 1;
			}
		}
	}
	return got_bb;
}

/*********************************************/
/* output last part of DSC header            */
/*********************************************/
static void dsc_head2()
{
	printf ("%%%%Pages: %d\n", nrows*ncols);

#ifndef Gv_gs_orientbug
	printf ("%%%%Orientation: %s\n", rotate?"Landscape":"Portrait");
#endif
	printf ("%%%%DocumentMedia: %s %d %d 0 white ()\n",
		mediaspec, (int)(mediasize[2]), (int)(mediasize[3]));
	printf ("%%%%BoundingBox: 0 0 %d %d\n", (int)(mediasize[2]), (int)(mediasize[3]));
	printf ("%%%%EndComments\n\n");

	printf ("%% Print poster %s in %dx%d tiles with %.3g magnification\n",
		infile, nrows, ncols, scale);
}

/*********************************************/
/* output the poster, create tiles if needed */
/*********************************************/
static void printposter()
{
	int row, col;

	printprolog();

    cover(nrows,ncols);
	for (row = 1; row <= nrows; row++)
		for (col = 1; col <= ncols; col++)
			tile( row, col, nrows, ncols);
	printf ("%%%%EOF\n");

	if (tail_cntl_D)
	{	printf("%c", 0x4);
	}
}

/*******************************************************/
/* output PS prolog of the scaling and tiling routines */
/*******************************************************/
static void printprolog()
{
	char *extraCode, *test1, *test2;

	printf( "%%%%BeginProlog\n");

	printf( "/cutmark	%% - cutmark -\n"
		"{		%% draw cutline\n"
		"	0.5 setlinewidth 0 setgray\n"
		"	clipmargin\n"
		"	dup 0 moveto\n"
		"	dup neg leftmargin add 0 rlineto stroke\n"
		"	%% draw sheet alignment mark\n"
		"	dup dup neg moveto\n"
		"	dup 0 rlineto\n"
		"	dup dup lineto\n"
		"	0 rlineto\n"
		"	closepath fill\n"
		"} bind def\n\n");

	if( alignment )
	{
		printf ("/alignmark\n"
			"{\n"
			"    gsave\n"
			"    0 setgray 1 setlinewidth\n"
			"    10 neg 10 neg rmoveto\n"
			"    20 20 rlineto \n"
			"    20 neg 0 rmoveto\n"
			"    20 20 neg rlineto stroke\n"
			"    grestore\n"
			"} bind def\n"
			"\n"
			"/alignmarkhor\n"
			"{\n"
			"    120 0 rmoveto\n"
			"    alignmark\n"
			"    pagewidth 0 rmoveto\n"
			"    240 neg 0 rmoveto\n"
			"    alignmark\n"
			"} bind def\n"
			"\n"
			"/alignmarkver\n"
			"{\n"
			"    0 120 rmoveto\n"
			"    alignmark\n"
			"    0 pageheight rmoveto\n"
			"    0 240 neg rmoveto\n"
			"    alignmark\n"
			"} bind def\n");
	}

	printf( "%% usage: 	row col tileprolog ps-code tilepilog\n"
		"%% these procedures output the tile specified by row & col\n"
		"/tileprolog\n"
		"{ 	%%def\n"
		"	gsave\n"
		"       leftmargin botmargin translate\n"
	        "	do_turn {exch} if\n"
		"	/colcount exch def\n"
		"	/rowcount exch def\n"
		"	%% clip page contents\n"
		"	clipmargin neg dup moveto\n"
		"	pagewidth clipmargin 2 mul add 0 rlineto\n"
		"	0 pageheight clipmargin 2 mul add rlineto\n"
		"	pagewidth clipmargin 2 mul add neg 0 rlineto\n"
		"	closepath clip\n"
		"	%% set page contents transformation\n"
		"	do_turn\n"
	        "	{	pagewidth 0 translate\n"
		"		90 rotate\n"
	        "	} if\n"
		"	pagewidth colcount 1 sub mul neg\n"
		"	pageheight rowcount 1 sub mul neg\n"
	        "	do_turn {exch} if\n"
		"	translate\n"
	        "	posterxl posteryb translate\n"
		"	sfactor dup scale\n"
	        "	imagexl neg imageyb neg translate\n"
		"	tiledict begin\n"
	        "	0 setgray 0 setlinecap 1 setlinewidth\n"
	        "	0 setlinejoin 10 setmiterlimit [] 0 setdash newpath\n"
	        "} bind def\n\n");

	printf( "/tileepilog\n"
	        "{	end %% of tiledict\n"
	        "	grestore\n"
	        "	%% print the bounding box\n"
	        "	gsave\n"
            "	do_turn {\n"
		        "	/totalrows exch def\n"
		        "	/totalcols exch def\n"
                "       /pagenr { colcount 1 sub totalrows mul rowcount add } bind def\n"
            "   } {\n"
		        "	/totalcols exch def\n"
		        "	/totalrows exch def\n"
                "       /pagenr { rowcount 1 sub totalcols mul colcount add } bind def\n"
            "	} ifelse\n"
	        "	0 setgray 1 setlinewidth\n"
	        "	leftmargin botmargin moveto\n"
	        "	0 pageheight rlineto\n"
	        "	pagewidth 0 rlineto\n"
	        "	0 pageheight neg rlineto closepath stroke\n"
	        "	grestore\n"
	        "	%% print the page label\n"
	        "	0 setgray\n"
	        "	leftmargin clipmargin 3 mul add clipmargin labelsize add neg botmargin add moveto\n" );
	printf( "	(%s ) show\n", LangPrompt( "Page" ) );
	printf( "	pagenr strg cvs show\n"
	        "	(: %s ) show\n", LangPrompt( "row" ) );
	printf( "	rowcount strg cvs show\n"
	        "	(, %s ) show\n", LangPrompt( "column" ) );
	printf( "	colcount strg cvs show\n"
	        "	pagewidth 69 sub clipmargin labelsize add neg botmargin add moveto\n"
	        "	(freesewing.org ) show\n" );
	if( alignment )
	{
		if( rotate ) {
			test1 = "	colcount totalcols lt\n";
			test2 = "	colcount 1 gt\n";
		} else {
			test1 = "	colcount 1 gt\n";
			test2 = "	colcount totalcols lt\n";
		}
		printf( "	gsave\n"
			"%s"
			"	{\n"
			"		leftmargin botmargin moveto\n"
			"		alignmarkver\n"
			"	} if\n"
			"	rowcount 1 gt\n"
			"	{\n"
			"		leftmargin botmargin moveto\n"
			"		alignmarkhor\n"
			"	} if\n"
			"%s"
			"	{\n"
			"		leftmargin botmargin moveto\n"
			"		pagewidth 0 rmoveto\n"
			"		alignmarkver\n"
			"	} if\n"
			"	rowcount totalrows lt\n"
			"	{\n"
			"		leftmargin botmargin moveto\n"
			"		0 pageheight rmoveto\n"
			"		alignmarkhor\n"
			"	} if\n"
			"	grestore\n", test1, test2 );
	}
	printf( "	showpage\n"
                "} bind def\n\n");

	printf( "%% usage: 	row col coverprolog ps-code coverepilog\n"
		"%% these procedures output the cover page\n"
		"/coverprolog\n"
		"{ 	%%def\n"
		"	gsave\n"
		"       leftmargin botmargin translate\n"
	        "	do_turn {exch} if\n"
		"	/colcount exch def\n"
		"	/rowcount exch def\n"
		"	%% clip page contents\n"
		"	clipmargin neg dup moveto\n"
		"	pagewidth clipmargin 2 mul add 0 rlineto\n"
		"	0 pageheight clipmargin 2 mul add rlineto\n"
		"	pagewidth clipmargin 2 mul add neg 0 rlineto\n"
		"	closepath clip\n"
		"	%% set page contents transformation\n"
		"   pagewidth colcount 1 sub mul neg\n"
		"   pageheight rowcount 1 sub mul neg\n"
        "	do_turn {\n"
	        "		pagewidth 0 translate\n"
		    "		90 rotate\n"
            "	    botmargin leftmargin translate\n"
		    "	    0.78 rowcount div dup scale\n"
	        "	    imagexl neg posterxl add imageyb neg posteryb add translate\n"
        "   } {\n"
            "	    0.1 pagewidth mul botmargin translate\n"
		    "	    0.8 colcount div dup scale\n"
	        "	    imagexl neg posterxl add imageyb neg posteryb add translate\n"
        "	} ifelse\n"
		"	tiledict begin\n"
	        "	0 setgray 0 setlinecap 1 setlinewidth\n"
	        "	0 setlinejoin 10 setmiterlimit [] 0 setdash newpath\n"
	        "} bind def\n\n");

	printf( "/coverepilog\n"
	        "{	end %% of tiledict\n"
	        "	grestore\n"
	        "	%% print the page label\n"
	        "	0 setgray\n"
	        "	leftmargin clipmargin 3 mul add clipmargin labelsize add neg botmargin add moveto\n" );
	printf( "	( %s ) show\n", LangPrompt( "cover page" ));
	printf(	"	leftmargin clipmargin 3 mul add pageheight 10 add moveto\n"
          "   /Helvetica findfont 24 scalefont setfont\n"
	        "	(freesewing) show\n"
	        "	leftmargin clipmargin 3 mul add pageheight 5 sub moveto\n"
          "   /Helvetica findfont 11 scalefont setfont\n" );
	printf( "	(%s ) show\n", LangPrompt( "an open source platform for made-to-measure sewing patterns" ));
	printf( "	leftmargin clipmargin 3 mul add pageheight 62 sub moveto\n"
          "   /Helvetica findfont 42 scalefont setfont\n"
	        "	patterntitle show\n"
	        "	leftmargin clipmargin 4 mul add pageheight 80 sub moveto\n"
            "   /Helvetica findfont 9 scalefont setfont\n"
	        "	0.5 setgray\n"
	        "	(freesewing.org/drafts/) show\n"
	        "	patternhandle show\n"
	        "	0 setgray\n"
            "   /Helvetica findfont labelsize scalefont setfont\n"
	        "	pagewidth 69 sub clipmargin labelsize add neg botmargin add moveto\n"
	        "	(freesewing.org ) show\n"
	        "	leftmargin clipmargin 3 mul add pageheight 70 sub moveto\n"
	        "	pagewidth clipmargin 2 mul add pageheight 70 sub lineto stroke\n"
	        "	gsave\n"
            "   pagewidth 75 sub pageheight 60 sub translate\n"
	        "   logo\n"
	        "	grestore\n"
	        "	showpage\n"
                "} bind def\n\n");

	printf( "/covergrid\n"
	        "{	%% print the page label\n"
		    "	/curcol exch def\n"
		    "	/currow exch def\n"
	        "	gsave\n"
	        "	0.8 setgray 0.2 setlinewidth\n"
		    "	do_turn\n"
	            "	{	\n"
            "       /pagenr { currow 1 sub rowcount mul curcol add } bind def\n"
	        "	    curcol 1 sub pageheight mul currow 1 sub pagewidth mul moveto\n"
	        "	    posterxl neg posteryb neg rmoveto\n"
	        "	    0 pagewidth rlineto\n"
	        "	    pageheight 0 rlineto\n"
	        "	    0 pagewidth neg rlineto closepath stroke\n"
            "       /Helvetica findfont 60 scalefont setfont\n"
	        "	    curcol 1 sub pageheight mul currow 1 sub pagewidth mul moveto\n"
	        "	    posterxl neg 20 add posteryb neg 20 add rmoveto\n"
	        "	    (row ) show\n"
	        "	    curcol strg cvs show\n"
	        "	    (, column ) show\n"
	        "	    currow strg cvs show\n"
	        "	    curcol 1 sub pageheight mul currow 1 sub pagewidth mul moveto\n"
	        "	    posterxl neg 150 add posteryb neg 150 add rmoveto\n"
            "       /Helvetica findfont 300 scalefont setfont\n"
	        "	    pagenr strg cvs true charpath\n"
            "       0.3 setlinewidth 0.6 setgray stroke\n"
                "   }\n"
	            "	{\n"
            "       /pagenr { currow 1 sub colcount mul curcol add } bind def\n"
	        "	    curcol 1 sub pagewidth mul currow 1 sub pageheight mul moveto\n"
	        "	    posterxl neg posteryb neg rmoveto\n"
	        "	    0 pageheight rlineto\n"
	        "	    pagewidth 0 rlineto\n"
	        "	    0 pageheight neg rlineto closepath stroke\n"
            "       /Helvetica findfont 60 scalefont setfont\n"
	        "	    curcol 1 sub pagewidth mul currow 1 sub pageheight mul moveto\n"
	        "	    posterxl neg 20 add posteryb neg 20 add rmoveto\n"
	        "	    (row ) show\n"
	        "	    currow strg cvs show\n"
	        "	    (, column ) show\n"
	        "	    curcol strg cvs show\n"
	        "	    curcol 1 sub pagewidth mul currow 1 sub pageheight mul moveto\n"
	        "	    posterxl neg 150 add posteryb neg 150 add rmoveto\n"
            "       /Helvetica findfont 300 scalefont setfont\n"
	        "	    pagenr strg cvs true charpath\n"
            "       0.3 setlinewidth 0.6 setgray stroke\n"
	            "	} ifelse\n"
	        "	grestore\n"
                "} bind def\n\n");

	printf( "/logo\n"
	        "{	%% print the logo\n"
            "   /m { moveto } bind def\n"
            "   /c { curveto } bind def\n"
            "   /l { lineto } bind def\n"
            "   /h { closepath } bind def\n"
	        "   /f { fill } bind def\n"
            "	gsave\n"
	        	"	0 setgray\n"
						"   36.75 52.931 m 35.656 52.158 35.715 52.255 34.832 51.966 c 32.812 51.306\n"
            "   30.875 51.669 28.578 51.861 c 27.887 51.939 27.199 51.986 26.531 51.99\n"
            "   c 23.148 52.013 20.277 51.021 19.734 48.251 c 18.734 47.646 17.812 46.908\n"
            "   16.898 46.173 c 14.949 44.634 13.48 42.755 12.48 40.49 c 11.113 37.142\n"
            "   12.348 33.548 12.961 30.158 c 13.105 29.365 13.258 28.607 13.34 28.314 c\n"
            "   13.453 27.904 13.66 27.509 13.879 27.158 c 13.934 27.15 14.207 27.572 14.27\n"
            "   27.755 c 14.367 28.029 14.355 28.462 14.25 28.837 c 14.023 29.681 13.805\n"
            "   30.369 13.785 30.712 c 13.754 31.357 13.879 31.955 14.113 32.248 c 14.199\n"
            "   32.353 14.391 31.814 14.34 31.615 c 14.281 31.373 14.238 30.959 14.258\n"
            "   30.767 c 14.309 30.123 14.402 29.431 14.52 28.826 c 14.672 28.044 14.738\n"
            "   27.544 14.711 27.349 c 14.691 27.209 14.625 27.068 14.371 26.63 c 14.148\n"
            "   26.248 14.035 25.939 14.016 25.666 c 14 25.4 14.078 24.732 14.152 24.568\n"
            "   c 14.262 24.314 14.59 24.044 14.879 23.978 c 15.152 23.873 15.289 23.658\n"
            "   15.43 23.412 c 15.773 22.759 16.039 21.962 16.301 20.763 c 16.43 20.189\n"
            "   16.535 19.677 16.57 19.408 c 14.453 19.404 11.742 19.404 9.273 19.404 c\n"
            "   8.441 19.392 6.938 19.783 5.562 19.873 c 5.363 22.025 4.414 24.529 2.797\n"
            "   24.673 c 1.691 24.775 0.773 24.353 0 22.611 c 0.023 22.517 l 0.48 22.9\n"
            "   0.961 24.068 2.703 23.986 c 4.039 23.927 4.484 21.396 4.617 19.876 c 3.805\n"
            "   19.818 3.109 19.564 2.75 18.939 c 2.742 18.939 l 2.742 18.939 2.742 18.939\n"
            "   2.746 18.935 c 2.742 18.931 2.742 18.931 2.742 18.927 c 2.75 18.927 l 3.121\n"
            "   18.287 3.84 18.041 4.684 17.99 c 5.246 2.056 20.227 0.001 24.32 0.001 c\n"
            "   39.59 0.001 44.934 10.376 45.738 14.486 c 46.113 12.736 44.93 10.505 44.648\n"
            "   8.857 c 47.867 12.584 47.285 16.162 46.699 19.763 c 47.188 19.181 47.852\n"
            "   18.822 48.75 19.021 c 48.109 19.451 47.238 19.15 46.719 20.955 c 46.492\n"
            "   21.732 46.293 22.318 46.102 22.81 c 45.68 24.388 45.082 25.9 44.348 27.369\n"
            "   c 43.727 28.966 44.137 30.001 44.074 31.369 c 45.188 27.693 45.887 26.716\n"
            "   47.188 26.33 c 43.914 30.275 45.035 36.38 43.184 41.83 c 44.023 41.337\n"
            "   44.977 41.189 45.992 41.791 c 45.012 42.072 44.027 41.33 42.617 43.404 c\n"
            "   41.426 45.724 39.699 47.435 37.602 48.947 c 36.48 49.677 35.234 50.169\n"
            "   33.98 50.626 c 35.258 50.947 36.684 52.084 36.75 52.931 c h\n"
            "   34.609 40.681 m 36.289 40.072 38.141 37.74 38.121 35.455 c 38.121 35.33\n"
            "   l 38.074 32.224 36.664 30.556 36.723 28.49 c 36.801 26.056 37.848 25.209\n"
            "   38.055 24.802 c 37.699 26.302 37.488 28.189 37.961 29.685 c 38.566 31.607\n"
            "   39.18 33.201 39.137 34.709 c 39.113 35.06 38.965 36.38 38.863 36.966 c\n"
            "   40.664 34.365 38.473 30.584 39.105 28.107 c 40.199 23.798 45.113 23.251\n"
            "   43.652 14.568 c 42.363 6.931 33.547 1.138 24.508 1.138 c 17.809 1.138 6.199\n"
            "   4.845 5.625 18.001 c 6.984 18.099 8.449 18.478 9.273 18.462 c 11.754 18.462\n"
            "   14.59 18.462 16.727 18.462 c 17.129 16.65 17.43 16.216 18.953 15.009 c\n"
            "   20.648 13.677 21.043 13.513 23.547 13.49 c 26.055 13.462 26.797 14.001 29.23\n"
            "   16.302 c 29.754 16.916 29.98 17.724 30.234 18.482 c 32.738 18.505 34.562\n"
            "   18.548 36.355 18.63 c 38.184 18.716 38.441 18.63 41.383 18.935 c 38.441\n"
            "   19.24 38.184 19.154 36.355 19.24 c 34.609 19.318 32.84 19.365 30.449 19.384\n"
            "   c 30.648 20.287 30.934 21.166 31.242 22.037 c 31.43 22.548 31.652 23.048\n"
            "   31.879 23.548 c 32.125 23.623 l 32.605 23.794 33.027 24.158 33.195 24.65\n"
            "   c 33.414 25.302 33.258 26.033 32.84 26.568 c 32.695 26.767 32.516 27.021\n"
            "   32.441 27.134 c 32.219 27.482 32.215 27.677 32.402 28.982 c 32.559 30.072\n"
            "   32.578 30.646 32.469 31.197 c 32.445 31.513 32.445 32.166 32.434 32.341\n"
            "   c 32.516 32.294 32.699 31.88 32.883 31.318 c 33.066 30.755 33.098 30.638\n"
            "   33.137 30.158 c 33.184 29.615 32.98 29.209 32.812 28.396 c 32.73 27.978\n"
            "   32.656 27.587 32.656 27.537 c 32.656 27.482 32.695 27.361 32.738 27.267\n"
            "   c 32.836 27.06 33.02 26.771 33.055 26.771 c 33.652 27.431 34.008 28.251\n"
            "   34.32 29.08 c 34.652 30.021 34.75 31.017 34.98 31.986 c 35.168 32.826 35.383\n"
            "   33.669 35.453 34.537 c 35.586 36.224 35.508 37.412 35.137 39.169 c 35.047\n"
            "   39.568 34.789 40.326 34.609 40.681 c h\n"
            "   28.699 32.373 m 29.344 32.38 29.98 32.302 30.59 32.126 c 31.414 31.81 31.531\n"
            "   31.072 31.684 30.244 c 31.766 29.712 31.766 29.334 31.668 28.712 c 31.602\n"
            "   27.81 31.242 27.111 30.461 26.623 c 29.277 26.404 28.016 26.193 27.016\n"
            "   26.974 c 26.176 28.13 25.668 29.595 25.859 31.021 c 25.984 31.607 26.258\n"
            "   31.912 26.816 32.095 c 27.418 32.267 28.059 32.361 28.699 32.373 c h\n"
            "   18.902 32.267 m 19.504 32.271 20.113 32.209 20.449 31.994 c 21.891 31.021\n"
            "   21.426 29.435 20.859 28.044 c 20.031 26.224 18.238 26.291 16.504 26.306\n"
            "   c 15.543 26.587 15.453 27.376 15.316 28.216 c 15.195 29.029 14.941 29.9\n"
            "   15.156 30.716 c 15.699 32.033 17.688 32.185 18.902 32.267 c h\n"
            "   40.852 32.041 m 40.855 28.541 41.453 26.822 42.656 24.345 c 41.949 25.693\n"
            "   40.938 26.459 40.562 28.072 c 39.844 31.185 40.617 31.009 40.852 32.041\n"
            "   c h\n"
            "   23.414 29.505 m 23.602 29.314 23.691 29.048 23.859 28.837 c 23.984 29.056\n"
            "   24.113 29.271 24.277 29.466 c 24.363 29.462 24.465 29.294 24.559 28.994\n"
            "   c 24.961 27.884 25.039 26.666 25.559 25.599 c 26.047 24.826 25.383 24.376\n"
            "   24.652 24.341 c 24.121 24.341 23.898 24.705 23.586 25.064 c 23.504 24.83\n"
            "   23.352 24.677 23.168 24.521 c 22.703 24.13 21.793 24.275 21.648 24.888\n"
            "   c 21.836 26.064 22.438 27.146 22.828 28.271 c 23.023 28.685 23.027 28.998\n"
            "   23.414 29.505 c h\n"
            "   16.586 23.568 m 16.82 23.568 17.465 23.13 17.594 22.884 c 17.688 22.72\n"
            "   17.891 22.08 17.906 21.923 c 17.93 21.74 17.875 21.298 17.816 21.181 c 17.773\n"
            "   21.095 17.742 21.084 17.691 21.138 c 17.652 21.185 17.113 22.22 16.766\n"
            "   22.908 c 16.496 23.455 16.469 23.568 16.586 23.568 c h\n"
            "   30.062 23.224 m 30.07 23.224 30.082 23.22 30.098 23.216 c 30.168 23.189\n"
            "   30.117 22.927 29.844 21.994 c 29.793 21.709 29.668 21.486 29.484 21.302\n"
            "   c 29.422 21.302 29.395 21.373 29.301 21.763 c 29.219 22.099 29.211 22.396\n"
            "   29.266 22.662 c 29.363 22.998 29.758 23.099 30.062 23.224 c h\n"
            "   28.426 21.181 m 28.449 21.185 28.473 21.173 28.5 21.15 c 28.598 21.08 28.613\n"
            "   20.896 28.547 20.541 c 28.469 20.08 28.449 20.029 28.332 19.982 c 28.277\n"
            "   19.959 28.188 19.939 28.125 19.939 c 28.012 19.962 27.984 20.052 28.012\n"
            "   20.189 c 28.012 20.474 28.094 20.724 28.285 21.017 c 28.355 21.126 28.391\n"
            "   21.177 28.426 21.181 c h\n"
            "   18.906 21.162 m 18.984 21.162 19.211 20.994 19.297 20.88 c 19.391 20.759\n"
            "   19.5 20.037 19.438 19.912 c 19.371 19.755 19.031 19.834 18.938 20.029 c\n"
            "   18.879 20.146 18.742 20.775 18.742 20.927 c 18.742 21.064 18.809 21.162\n"
            "   18.906 21.162 c h\n"
            "   27.609 20.97 m 27.699 20.837 27.727 20.666 27.703 20.294 c 27.688 19.955\n"
            "   l 27.59 19.857 l 27.531 19.802 27.43 19.74 27.371 19.709 c 27.242 19.65\n"
            "   27.141 19.642 27.121 19.693 c 27.102 19.759 27.137 20.181 27.18 20.322\n"
            "   c 27.227 20.47 27.438 20.908 27.508 20.998 c 27.539 21.084 27.578 20.978\n"
            "   27.609 20.97 c h\n"
            "   22.07 20.99 m 22.27 20.939 22.359 20.759 22.469 20.595 c 22.543 20.451\n"
            "   l 22.438 20.091 l 22.383 19.892 22.324 19.712 22.309 19.693 c 22.289 19.669\n"
            "   22.215 19.658 22.105 19.662 c 21.812 19.669 21.781 19.681 21.703 19.798\n"
            "   c 21.629 19.908 l 21.691 20.212 l 21.781 20.646 21.824 20.779 21.922 20.892\n"
            "   c 21.973 20.951 22.027 20.986 22.07 20.99 c h\n"
            "   20.23 20.677 m 20.395 20.513 l 20.422 20.142 l 20.434 19.939 20.441 19.751\n"
            "   20.43 19.728 c 20.422 19.693 20.359 19.685 20.176 19.685 c 19.938 19.685\n"
            "   l 19.809 19.826 19.77 19.998 19.715 20.173 c 19.59 20.705 19.59 20.947\n"
            "   19.719 20.978 c 19.934 20.962 20.074 20.81 20.23 20.677 c h\n"
            "   26.625 20.931 m 26.883 20.861 26.832 20.451 26.832 20.154 c 26.812 19.673\n"
            "   26.793 19.576 26.73 19.513 c 26.625 19.408 26.328 19.466 26.184 19.626\n"
            "   c 26.133 19.681 26.129 19.701 26.168 19.931 c 26.211 20.216 26.266 20.392\n"
            "   26.387 20.634 c 26.477 20.81 26.562 20.919 26.625 20.931 c h\n"
            "   20.75 20.912 m 20.766 20.912 20.789 20.912 20.812 20.908 c 20.977 20.896\n"
            "   21.129 20.787 21.266 20.587 c 21.379 20.427 l 21.387 20.162 21.379 19.88\n"
            "   21.332 19.634 c 21.301 19.619 21.184 19.595 21.066 19.587 c 20.863 19.568\n"
            "   20.844 19.572 20.773 19.634 c 20.676 19.716 20.648 19.873 20.625 20.388\n"
            "   c 20.609 20.81 20.617 20.904 20.75 20.912 c h\n"
            "   23.254 20.822 m 23.43 20.783 23.543 20.689 23.594 20.533 c 23.633 20.4\n"
            "   23.656 19.654 23.617 19.603 c 23.605 19.58 23.52 19.552 23.426 19.541 c\n"
            "   23.066 19.494 22.754 19.529 22.723 19.615 c 22.684 19.712 22.949 20.673\n"
            "   23.047 20.791 c 23.09 20.884 23.188 20.814 23.254 20.822 c h\n"
            "   25.566 20.814 m 25.645 20.822 25.73 20.748 25.812 20.587 c 25.875 20.455\n"
            "   25.887 20.408 25.887 20.111 c 25.887 19.873 25.871 19.767 25.848 19.748\n"
            "   c 25.824 19.728 25.734 19.697 25.641 19.673 c 25.477 19.63 25.438 19.626\n"
            "   25.387 19.642 c 25.141 19.689 l 25.156 19.837 l 25.176 20.029 25.301 20.459\n"
            "   25.391 20.642 c 25.445 20.751 25.504 20.806 25.566 20.814 c h\n"
            "   24.488 20.81 m 24.539 20.814 24.566 20.802 24.598 20.767 c 24.676 20.673\n"
            "   24.773 20.4 24.84 20.107 c 24.922 19.697 24.922 19.697 24.742 19.642 c\n"
            "   24.664 19.619 24.5 19.595 24.379 19.587 c 23.992 19.568 23.961 19.591 24.008\n"
            "   19.935 c 24.047 20.24 24.195 20.724 24.262 20.759 c 24.332 20.798 24.414\n"
            "   20.794 24.488 20.81 c h\n"
            "   4.648 19.259 m 4.648 19.259 4.656 19.259 4.656 19.259 c 4.664 19.044 4.664\n"
            "   18.826 4.668 18.611 c 4.125 18.65 3.723 18.783 3.723 18.935 c 3.723 19.087\n"
            "   4.109 19.216 4.648 19.259 c h\n"
            "   5.602 19.24 m 6.043 19.185 6.348 19.068 6.344 18.935 c 6.344 18.802 6.047\n"
            "   18.685 5.609 18.63 c 5.605 18.834 5.605 19.037 5.602 19.24 c h\n"
            "   26.25 18.65 m 26.289 18.654 26.305 18.658 26.363 18.654 c 26.59 18.63 26.625\n"
            "   18.615 26.625 18.537 c 26.625 18.435 26.449 18.13 26.332 18.025 c 26.277\n"
            "   17.974 26.199 17.927 26.176 17.927 c 26.043 17.927 25.965 18.209 26.027\n"
            "   18.462 c 26.062 18.603 26.133 18.646 26.25 18.65 c h\n"
            "   25.418 18.56 m 25.52 18.568 25.621 18.541 25.719 18.529 c 25.691 18.24\n"
            "   25.559 18.001 25.32 17.845 c 25.258 17.845 25.23 17.959 25.23 18.205 c 25.23\n"
            "   18.501 25.246 18.556 25.418 18.56 c h\n"
            "   24.504 18.455 m 24.719 18.455 24.906 18.443 24.914 18.431 c 24.953 18.392\n"
            "   24.84 18.111 24.672 17.822 c 24.527 17.564 24.504 17.544 24.414 17.537\n"
            "   c 24.066 17.576 24.074 17.99 24.02 18.271 c 24.02 18.439 24.059 18.455 24.504\n"
            "   18.455 c h\n"
            "   21.188 18.419 m 21.43 18.416 21.523 18.38 21.523 18.287 c 21.523 18.169\n"
            "   21.434 17.931 21.383 17.9 c 21.305 17.857 21.297 17.861 21.145 18.021 c\n"
            "   20.898 18.294 20.871 18.345 21.188 18.419 c h\n"
            "   22.359 18.408 m 22.363 18.404 22.41 18.318 22.461 18.22 c 22.562 18.017\n"
            "   22.57 17.931 22.496 17.861 c 22.324 17.763 22.285 17.818 22.129 17.919\n"
            "   c 21.945 18.091 21.801 18.326 21.848 18.373 c 22.012 18.431 22.191 18.412\n"
            "   22.359 18.408 c h\n"
            "   22.824 18.404 m 23.262 18.396 l 23.43 18.396 23.578 18.388 23.586 18.373\n"
            "   c 23.613 18.349 23.52 17.779 23.469 17.654 c 23.406 17.509 23.199 17.509\n"
            "   23.09 17.677 c 22.938 17.923 22.836 18.126 22.832 18.267 c h f\n"
	            "	grestore\n"
            "} bind def\n\n");

	printf( "%%%%EndProlog\n\n");
	printf( "%%%%BeginSetup\n");
	printf( "%% Try to inform the printer about the desired media size:\n"
	        "/setpagedevice where 	%% level-2 page commands available...\n"
	        "{	pop		%% ignore where found\n"
	        "	3 dict dup /PageSize [ %d %d ] put\n"
	        "	dup /Duplex false put\n%s"
	        "	setpagedevice\n"
                "} if\n",
	       (int)(mediasize[2]), (int)(mediasize[3]),
	       manualfeed?"       dup /ManualFeed true put\n":"");

	printf( "/sfactor %.10f def\n"
	        "/leftmargin %d def\n"
	        "/botmargin %d def\n"
	        "/pagewidth %d def\n"
	        "/pageheight %d def\n"
	        "/imagexl %d def\n"
	        "/imageyb %d def\n"
	        "/posterxl %d def\n"
	        "/posteryb %d def\n"
	        "/do_turn %s def\n"
	        "/strg 10 string def\n"
	        "/clipmargin 6 def\n"
	        "/labelsize 9 def\n"
	        "/tiledict 250 dict def\n"
	        "tiledict begin\n"
	        "%% delay users showpage until cropmark is printed.\n"
	        "/showpage {} def\n"
		"/setpagedevice { pop } def\n"
	        "end\n",
	        scale, (int)(cutmargin[0]), (int)(cutmargin[1]),
	        (int)(mediasize[2]-2.0*cutmargin[0]), (int)(mediasize[3]-2.0*cutmargin[1]),
	        (int)imagebb[0], (int)imagebb[1], (int)posterbb[0], (int)posterbb[1],
	        rotate?"true":"false");

	printf( "/Helvetica findfont labelsize scalefont setfont\n");

    printf( "/patterntitle (%s) def\n", patterntitle);
    printf( "/patternhandle (%s) def\n", patternhandle);

	printf( "%%%%EndSetup\n");
}

/*****************************/
/* output one tile at a time */
/*****************************/
static void tile ( int row, int col, int nrows, int ncols)
{
	static int page=2;

	if (verbose) fprintf( stderr, "print page %d\n", page);

	printf ("\n%%%%Page: %d %d\n", page, page);
	printf ("%d %d tileprolog\n", row, col);
	printf ("%%%%BeginDocument: %s\n", infile);
	printfile ();
	printf ("\n%%%%EndDocument\n");
	printf ("%d %d tileepilog\n", nrows, ncols);

	page++;
}

/*****************************/
/* cover page                */
/*****************************/
static void cover ( int rows, int cols)
{
	static int page=1;
	int row, col;

	if (verbose) fprintf( stderr, "print page %d\n", page);

	printf ("\n%%%%Page: %d %d\n", page, page);
	printf ("%d %d coverprolog\n", rows, cols);
	printf ("%%%%BeginDocument: %s\n", infile);
	printfile ();
	printf ("\n%%%%EndDocument\n");
	for (row = 1; row <= nrows; row++)
	    for (col = 1; col <= ncols; col++)
	        printf ("%d %d covergrid\n", row, col);
	printf ("coverepilog\n");

	page++;
}

/******************************/
/* copy the PS file to output */
/******************************/
static void printfile ()
{
	/* use a double line buffer, so that when I print */
	/* a line, I know whether it is the last or not */
	/* I surely dont want to print a 'cntl_D' on the last line */
	/* The double buffer removes the need to scan each line for each page*/
	/* Furthermore allows cntl_D within binary transmissions */

	char buf[2][BUFSIZE];
	int bp;
	char *c;

	if (freopen (infile, "r", stdin) == NULL) {
		fprintf (stderr, "%s: fail to open file '%s'!\n",
			myname, infile);
		printf ("/systemdict /showpage get exec\n");
		printf ("%%%%EOF\n");
		exit (1);
	}

	/* fill first buffer for the first time */
	fgets( buf[bp=0], BUFSIZE, stdin);

	/* read subsequent lines by rotating the buffers */
	while (fgets(buf[1-bp], BUFSIZE, stdin))
	{	/* print line from the previous fgets */
		/* do not print postscript comment lines: those (DSC) lines */
		/* sometimes disturb proper previewing of the result with ghostview */
		if (buf[bp][0] != '%')
			fputs( buf[bp], stdout);
		bp = 1-bp;
	}

	/* print buf from last successfull fgets, after removing cntlD */
	for (c=buf[bp]; *c && *c != '\04'; c++);
	if (*c == '\04')
	{	tail_cntl_D = 1;
		*c = '\0';
	}
	if (buf[bp][0] != '%' && strlen( buf[bp]))
		fputs( buf[bp], stdout);
}

static int mystrncasecmp( const char *s1, const char *s2, int n)
{	/* compare case-insensitive s1 and s2 for at most n chars */
	/* return 0 if equal. */
	/* Although standard available on some machines, */
	/* this function seems not everywhere around... */

	char c1, c2;
	int i;

	if (!s1) s1 = "";
	if (!s2) s2 = "";

	for (i=0; i<n && *s1 && *s2; i++, s1++, s2++)
	{	c1 = tolower( *s1);
		c2 = tolower( *s2);
		if (c1 != c2) break;
	}

	return (i < n && (*s1 || *s2));
}
