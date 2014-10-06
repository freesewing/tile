/*
#  poster - resize a postscript image to print on larger media and/or multiple sheets
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
#        cc -O -o poster poster.c -lm
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
# --------------------------------------------------------------
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation.
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY;
#  The full text of the GNU General Public License is supplied
#  with 'poster' in the file LICENSE.
#
#  Copyright (C) 1999 Jos T.J. van Eijndhoven
# --------------------------------------------------------------
# email: J.T.J.v.Eijndhoven@ele.tue.nl
*/

#define Gv_gs_orientbug 1
#define DefaultMedia  "A4"
#define DefaultImage  "A4"
#define DefaultCutMargin "5%"
#define DefaultWhiteMargin "0"
#define BUFSIZE 1024

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <math.h>


extern char *optarg;        /* silently set by getopt() */
extern int optind, opterr;  /* silently set by getopt() */

static void usage();
static void dsc_head1();
static int dsc_infile( double ps_bb[4]);
static void dsc_head2( void);
static void printposter( void);
static void printprolog();
static void tile ( int row, int col);
static void printfile( void);
static void postersize( char *scalespec, char *posterspec);
static void box_convert( char *boxspec, double psbox[4]);
static void boxerr( char *spec);
static void margin_convert( char *spec, double margin[2]);
static int mystrncasecmp( const char *s1, const char *s2, int n);

int verbose;
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

	while ((opt = getopt( argc, argv, "vfi:c:w:m:p:s:o:")) != EOF)
	{	switch( opt)
		{ case 'v':	verbose++; break;
		  case 'f':     manualfeed = 1; break;
		  case 'i':	imagespec = optarg; break;
		  case 'c':	cutmarginspec = optarg; break;
		  case 'w':	whitemarginspec = optarg; break;
		  case 'm':	mediaspec = optarg; break;
		  case 'p':	posterspec = optarg; break;
		  case 's':	scalespec = optarg; break;
		  case 'o':     filespec = optarg; break;
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

	exit (0);
}

static void usage()
{
	fprintf( stderr, "Usage: %s <options> infile\n\n", myname);
	fprintf( stderr, "options are:\n");
	fprintf( stderr, "   -v:         be verbose\n");
	fprintf( stderr, "   -f:         ask manual feed on plotting/printing device\n");
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
	for (row = 1; row <= nrows; row++)
		for (col = 1; col <= ncols; col++)
			tile( row, col);
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
	        "	%% print the cutmarks\n"
	        "	gsave\n"
		"       leftmargin botmargin translate\n"
	        "	pagewidth pageheight translate cutmark 90 rotate cutmark\n"
	        "	0 pagewidth translate cutmark 90 rotate cutmark\n"
	        "	0 pageheight translate cutmark 90 rotate cutmark\n"
	        "	0 pagewidth translate cutmark 90 rotate cutmark\n"
	        "	grestore\n"
	        "	%% print the page label\n"
	        "	0 setgray\n"
	        "	leftmargin clipmargin 3 mul add clipmargin labelsize add neg botmargin add moveto\n"
	        "	(Page \\() show\n"
	        "	rowcount strg cvs show\n"
	        "	(,) show\n"
	        "	colcount strg cvs show\n"
	        "	(\\) - MakeMyPattern.com) show\n"
	        "	showpage\n"
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

	printf( "%%%%EndSetup\n");
}

/*****************************/
/* output one tile at a time */
/*****************************/
static void tile ( int row, int col)
{
	static int page=1;

	if (verbose) fprintf( stderr, "print page %d\n", page);

	printf ("\n%%%%Page: %d %d\n", page, page);
	printf ("%d %d tileprolog\n", row, col);
	printf ("%%%%BeginDocument: %s\n", infile);
	printfile ();
	printf ("\n%%%%EndDocument\n");
	printf ("tileepilog\n");

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
