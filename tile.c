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
# --------------------------------------------------------------
#  Tile is a fork of 'poster' by Jos T.J. van Eijndhoven 
#  <J.T.J.v.Eijndhoven@ele.tue.nl>
#
#  Forked by Joost De Cock for freesewing.org
#
#  Copyright (C) 1999 Jos T.J. van Eijndhoven
#  Copyright (C) 2017 Joost De Cock
# --------------------------------------------------------------
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

	while ((opt = getopt( argc, argv, "vafi:c:w:m:p:s:o:t:h:")) != EOF)
	{	switch( opt)
		{ case 'v':	verbose++; break;
		  case 'f':     manualfeed = 1; break;
		  case 'a':     alignment = 1; break;
		  case 'i':	imagespec = optarg; break;
		  case 'c':	cutmarginspec = optarg; break;
		  case 'w':	whitemarginspec = optarg; break;
		  case 'm':	mediaspec = optarg; break;
		  case 'p':	posterspec = optarg; break;
		  case 's':	scalespec = optarg; break;
		  case 'o':     filespec = optarg; break;
		  case 't':     patterntitle = optarg; break;
		  case 'h':     patternhandle = optarg; break;
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
	fprintf( stderr, "   -a:         add alignment marks\n");
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
	        "	leftmargin clipmargin 3 mul add clipmargin labelsize add neg botmargin add moveto\n"
	        "	(Page ) show\n"
	        "	pagenr strg cvs show\n"
	        "	(: row ) show\n"
	        "	rowcount strg cvs show\n"
	        "	(, column ) show\n"
	        "	colcount strg cvs show\n"
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
	        "	leftmargin clipmargin 3 mul add clipmargin labelsize add neg botmargin add moveto\n"
	        "	( cover page ) show\n"
	        "	leftmargin clipmargin 3 mul add pageheight 10 add moveto\n"
            "   /Helvetica findfont 24 scalefont setfont\n"
	        "	(freesewing) show\n"
	        "	leftmargin clipmargin 3 mul add pageheight 5 sub moveto\n"
            "   /Helvetica findfont 11 scalefont setfont\n"
	        "	(an open source platform for made-to-measure sewing patterns ) show\n"
	        "	leftmargin clipmargin 3 mul add pageheight 62 sub moveto\n"
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
            "   44.996 -0.002 m 42.082 0.209 39.371 0.623 36.664 1.248 c 32.914 2.287\n" 
            "   31.039 2.912 27.496 4.584 c 21.871 7.287 17.914 10.416 14.582 15.416 c\n"
            "   13.746 16.662 13.539 16.873 12.707 18.537 c 11.871 19.998 11.664 20.209\n" 
            "   10.828 22.494 c 9.996 24.584 9.371 26.873 8.746 29.166 c 8.328 30.83\n" 
            "   8.328 32.494 8.121 34.166 c 8.121 34.166 7.914 34.166 7.496 34.377 c\n" 
            "   6.664 34.584 5.852 34.998 5.391 35.83 c 5.914 36.873 7.082 37.287 8.121\n" 
            "   37.494 c 8.746 37.494 8.746 37.705 8.746 38.537 c 8.746 40.83 8.328\n" 
            "   42.916 6.871 44.584 c 5.082 46.248 2.27 45.623 0.828 43.748 c 0.539\n" 
            "   43.541 0.227 42.916 0.059 42.709 c -0.086 42.709 -0.004 43.541 0.703\n" 
            "   44.584 c 3.559 48.33 7.289 47.291 9.164 43.541 c 9.789 42.084 9.996\n" 
            "   40.83 10.207 38.752 c 10.207 37.912 l 10.414 37.705 10.621 37.705\n" 
            "   11.453 37.494 c 16.246 36.662 16.246 36.662 21.871 36.662 c 25.828\n" 
            "   36.662 27.289 36.662 30.207 36.873 c 31.039 36.873 31.039 36.873 31.039\n" 
            "   37.08 c 31.246 37.287 31.039 37.912 30.621 39.377 c 30.207 41.662\n" 
            "   29.789 43.123 29.164 44.373 c 28.746 44.791 28.746 44.998 28.539 45.209\n" 
            "   c 28.328 45.416 28.121 45.416 27.914 45.416 c 27.496 45.623 26.871\n" 
            "   46.041 26.664 46.666 c 26.453 46.873 26.246 48.123 26.453 48.748 c\n" 
            "   26.453 49.166 26.664 49.791 27.082 50.416 c 27.496 51.248 27.707 51.666\n" 
            "   27.707 51.873 c 27.707 52.291 27.707 53.123 27.289 54.584 c 27.082\n" 
            "   55.83 26.871 57.084 26.871 58.33 c 26.871 58.748 26.871 59.584 27.082\n" 
            "   59.998 c 27.082 60.416 27.082 60.416 26.871 60.209 c 26.453 59.584\n" 
            "   26.246 58.541 26.246 57.291 c 26.246 56.666 26.453 56.248 26.871 54.791\n" 
            "   c 27.082 53.955 27.082 53.123 26.871 52.709 c 26.664 52.291 26.246\n" 
            "   51.455 26.039 51.455 c 26.039 51.666 l 25.621 52.291 25.207 53.123\n" 
            "   24.996 53.748 c 24.996 54.166 24.582 55.623 24.371 57.291 c 24.164\n" 
            "   58.955 23.746 60.83 23.328 62.291 c 22.914 64.373 22.496 67.498 22.289\n" 
            "   68.748 c 22.289 72.498 l 22.289 73.33 22.914 75.623 23.539 76.873 c\n" 
            "   23.953 78.123 25.207 80.436 26.246 81.791 c 26.453 82.041 26.664 82.397\n" 
            "   26.871 82.58 c 27.914 84.061 29.371 85.643 31.871 87.643 c 32.914\n" 
            "   88.498 35.621 90.537 35.621 90.537 c 35.621 90.537 35.621 90.58 35.828\n" 
            "   90.643 c 36.039 90.916 37.289 91.686 38.121 92.061 c 38.328 92.162\n" 
            "   38.746 92.354 38.953 92.475 c 39.371 92.6 39.789 92.811 40.207 92.955 c\n" 
            "   40.828 93.229 40.828 93.162 41.246 93.998 c 43.121 96.229 45.414 97.498\n" 
            "   48.121 98.381 c 50.414 99.096 52.496 99.432 54.789 99.448 c 59.371\n" 
            "   99.428 62.082 97.354 66.871 98.561 c 67.496 98.826 68.539 99.385 68.953\n" 
            "   99.791 c 69.164 99.924 69.371 100.014 69.371 99.994 c 69.371 99.936\n" 
            "   68.539 99.104 67.914 98.748 c 66.453 97.623 65.414 97.248 63.539 97.084\n" 
            "   c 63.121 97.018 62.707 96.979 62.496 96.959 c 62.289 96.916 62.496\n" 
            "   96.85 63.539 96.768 c 66.871 96.498 70.203 95.518 73.121 94.123 c\n" 
            "   74.789 93.354 76.246 92.162 77.703 90.479 c 78.953 89.291 79.789 87.893\n" 
            "   80.828 86.498 c 81.664 85.186 82.496 82.998 83.121 81.416 c 84.164\n" 
            "   77.916 84.578 74.373 84.789 67.084 c 84.789 65.83 84.996 64.584 84.996\n" 
            "   63.33 c 85.203 59.791 85.414 58.541 87.078 54.166 c 87.496 52.916\n" 
            "   87.496 52.709 87.914 52.084 c 88.328 51.248 88.746 50.623 89.164 50.209\n" 
            "   c 89.371 49.998 89.371 49.791 89.371 49.791 c 89.371 49.584 88.328\n" 
            "   50.623 87.703 51.455 c 87.289 52.084 86.246 53.955 85.621 54.791 c\n" 
            "   85.203 56.041 84.164 59.373 83.953 60.623 c 83.746 61.666 83.539 64.584\n" 
            "   83.328 66.248 c 83.121 69.791 82.703 72.709 82.289 74.998 c 81.453\n" 
            "   77.916 80.414 80.58 78.746 83.037 c 77.914 84.311 76.453 86.33 75.621\n" 
            "   87.287 c 74.164 88.936 72.496 90.393 70.828 91.666 c 68.746 93.186\n" 
            "   66.246 94.061 63.539 94.662 c 60.828 95.209 59.371 95.354 56.453 95.311\n" 
            "   c 52.914 95.268 49.996 94.787 47.707 93.936 c 46.246 93.354 45.414\n" 
            "   92.979 44.371 92.272 c 43.539 91.666 42.289 90.643 42.289 90.561 c\n" 
            "   42.496 90.537 42.496 90.643 42.914 90.791 c 47.914 94.041 51.871 94.998\n" 
            "   57.707 94.975 c 59.164 94.936 61.246 94.772 62.082 94.643 c 65.621\n" 
            "   94.248 68.539 92.873 71.453 90.748 c 72.082 90.166 73.953 88.373 74.582\n" 
            "   87.623 c 76.664 85.268 78.539 82.541 79.996 79.705 c 81.246 77.498\n" 
            "   81.871 75.209 82.289 72.709 c 82.496 71.248 82.496 70.83 82.496 69.998\n" 
            "   c 82.496 69.166 82.496 69.166 82.289 70.209 c 81.871 71.248 81.453\n" 
            "   72.291 81.246 73.33 c 80.621 74.787 79.789 76.873 79.164 77.916 c\n" 
            "   77.703 80.854 75.621 83.416 73.328 85.729 c 73.121 86.084 72.289 86.643\n" 
            "   71.871 86.979 c 69.996 88.393 68.121 89.623 66.039 90.623 c 63.746\n" 
            "   91.604 61.453 92.018 58.953 92.354 c 55.621 92.58 52.496 92.33 49.164\n" 
            "   91.666 c 47.289 91.205 45.207 90.416 43.539 89.291 c 42.707 88.729\n" 
            "   42.496 88.518 42.707 88.604 c 42.707 88.623 43.121 88.897 43.746 89.186\n" 
            "   c 45.621 90.455 47.914 91.018 49.996 91.455 c 52.289 91.791 54.789\n" 
            "   91.85 57.082 91.643 c 63.328 91.268 66.246 90.248 71.246 86.729 c\n" 
            "   73.328 85.166 74.996 83.205 76.453 81.166 c 77.082 80.166 77.914 78.748\n" 
            "   79.582 75.83 c 80.203 74.787 80.828 73.123 81.246 72.291 c 81.664 70.83\n" 
            "   82.496 68.33 82.496 67.709 c 82.914 65.416 82.914 63.123 83.328 60.83 c\n" 
            "   83.328 60.416 83.539 59.373 83.746 58.541 c 83.746 57.498 83.953 56.455\n" 
            "   84.164 55.83 c 84.371 54.166 84.789 51.873 84.996 50.83 c 85.414 48.541\n" 
            "   85.828 46.666 86.664 43.541 c 87.078 41.455 87.703 39.584 87.914 38.537\n" 
            "   c 88.121 36.455 88.328 36.248 88.328 34.584 c 88.328 33.119 l 88.121\n" 
            "   33.119 88.121 33.334 87.914 33.752 c 87.703 34.998 87.078 36.873 86.871\n" 
            "   37.494 c 86.246 38.959 85.828 39.791 85.621 40.623 c 84.789 42.916\n" 
            "   83.953 45.416 83.539 47.709 c 82.914 49.998 82.703 51.666 82.078 55.416\n" 
            "   c 81.453 60.416 81.039 62.709 80.414 65.209 c 79.371 69.373 78.328\n" 
            "   72.084 77.082 74.373 c 75.828 76.455 74.582 78.541 73.121 80.416 c\n" 
            "   71.664 82.287 68.746 84.897 67.082 85.873 c 65.621 86.686 64.164 87.272\n" 
            "   61.664 87.936 c 59.582 88.459 57.496 88.897 55.414 88.916 c 52.496\n" 
            "   88.916 50.207 88.623 47.496 87.893 c 46.039 87.518 44.164 86.791 44.164\n" 
            "   86.604 c 43.953 86.522 44.164 86.541 44.996 86.811 c 48.746 88.123\n" 
            "   52.707 88.83 56.664 88.393 c 60.828 87.709 64.996 86.143 68.539 83.916\n" 
            "   c 70.414 82.623 72.082 81.084 73.539 79.248 c 74.789 77.287 76.039\n" 
            "   75.416 76.871 73.33 c 78.328 69.998 79.371 66.455 79.996 62.709 c\n" 
            "   80.414 60.209 80.828 57.916 81.246 55.416 c 81.664 52.916 81.871 51.455\n" 
            "   82.289 49.791 c 82.703 47.084 83.539 44.584 84.371 42.084 c 84.996\n" 
            "   40.209 85.203 39.377 86.039 37.705 c 86.664 36.455 87.078 35.209 87.496\n" 
            "   34.166 c 88.121 32.705 88.539 31.248 88.539 29.791 c 88.328 27.705\n" 
            "   88.121 26.041 87.496 23.119 c 87.078 21.873 86.664 20.623 86.246 19.377\n" 
            "   c 86.039 18.537 85.621 17.912 85.621 17.912 c 85.414 17.912 85.621\n" 
            "   17.912 85.828 19.166 c 86.246 21.041 86.453 22.494 86.664 24.166 c\n" 
            "   86.871 25.209 86.871 27.287 86.871 27.912 c 86.664 28.537 86.246 31.248\n" 
            "   86.039 31.873 c 85.828 32.705 85.203 34.584 84.164 36.873 c 83.121\n" 
            "   39.166 82.078 41.662 81.453 44.166 c 81.453 44.998 81.039 47.084 80.828\n" 
            "   48.748 c 80.621 49.584 80.414 50.83 80.203 51.873 c 79.789 54.373\n" 
            "   79.789 54.584 79.164 58.541 c 78.121 64.166 77.496 67.498 76.453 70.83\n" 
            "   c 75.621 73.748 74.371 76.041 72.082 78.541 c 71.039 79.748 69.996\n" 
            "   80.705 68.328 81.705 c 67.703 82.229 65.621 83.248 64.371 83.729 c\n" 
            "   62.496 84.416 59.164 85.186 57.082 85.393 c 55.828 85.518 51.871 85.537\n" 
            "   51.039 85.416 c 49.582 85.248 46.664 84.662 46.453 84.498 c 46.453\n" 
            "   84.416 46.664 84.436 46.871 84.518 c 46.871 84.561 47.082 84.604 47.289\n" 
            "   84.623 c 47.496 84.643 47.914 84.705 48.328 84.772 c 52.289 85.416\n" 
            "   54.582 85.498 57.289 85.1 c 59.996 84.604 62.914 84.104 65.414 83.022 c\n" 
            "   67.289 82.143 68.539 81.291 69.789 80.209 c 70.828 79.291 71.453 78.748\n" 
            "   72.289 77.498 c 72.914 76.666 73.121 76.041 73.539 75.209 c 74.371\n" 
            "   73.748 74.582 73.123 75.414 71.041 c 75.414 70.623 75.621 69.998 75.828\n" 
            "   69.584 c 76.246 68.748 76.664 66.873 76.871 66.248 c 77.082 64.998\n" 
            "   77.496 63.123 77.703 61.666 c 77.703 61.041 77.914 59.998 77.914 59.584\n" 
            "   c 78.121 58.123 78.539 54.791 78.746 54.373 c 79.164 52.291 79.996\n" 
            "   50.209 80.414 47.916 c 80.414 47.498 80.828 46.455 80.828 45.623 c\n" 
            "   81.246 43.955 81.453 43.334 81.453 43.123 c 81.246 43.334 l 81.246\n" 
            "   43.541 80.828 44.373 80.414 45.209 c 79.164 47.709 78.121 50.416 77.082\n" 
            "   53.123 c 76.664 54.373 76.039 56.873 75.621 58.541 c 75.203 60.209\n" 
            "   75.203 60.416 75.203 62.709 c 75.203 65.209 l 74.996 67.709 74.164\n" 
            "   70.623 73.539 72.498 c 72.496 74.584 71.453 76.041 69.789 77.709 c\n" 
            "   68.746 78.955 67.289 80.018 65.621 80.666 c 62.914 81.768 59.789 82.123\n" 
            "   56.664 82.104 c 53.953 82.061 52.914 81.936 50.621 81.436 c 48.953\n" 
            "   81.084 47.707 80.705 47.289 80.436 c 47.082 80.311 47.082 80.33 47.914\n" 
            "   80.537 c 48.746 80.705 49.996 80.959 51.246 81.186 c 53.121 81.498\n" 
            "   56.664 81.748 58.539 81.666 c 61.039 81.58 63.328 81.205 64.582 80.666\n" 
            "   c 66.039 80.1 67.496 79.436 68.539 78.541 c 70.621 76.666 72.082 74.998\n" 
            "   72.914 73.123 c 73.539 72.084 73.953 70.623 74.164 68.748 c 74.371\n" 
            "   66.873 74.371 66.455 74.371 65.209 c 74.371 63.541 73.746 61.873 73.328\n" 
            "   59.998 c 72.703 58.748 72.496 57.291 72.082 55.83 c 71.664 54.166\n" 
            "   71.664 53.748 71.664 52.291 c 71.664 50.83 71.871 49.373 72.289 47.709\n" 
            "   c 72.289 47.084 l 72.496 46.873 72.496 46.873 72.289 47.084 c 72.082\n" 
            "   47.291 71.246 48.748 70.828 49.373 c 69.996 51.455 69.371 53.955 69.996\n" 
            "   56.248 c 70.203 57.084 70.621 57.916 71.453 60.83 c 72.082 62.709\n" 
            "   72.289 63.123 72.496 64.584 c 72.496 65.416 72.496 67.498 72.289 68.541\n" 
            "   c 72.082 70.209 71.453 71.666 70.621 73.123 c 70.203 73.748 68.953\n" 
            "   75.209 68.328 75.83 c 67.703 76.455 66.871 77.084 66.039 77.498 c\n" 
            "   65.207 77.916 65.207 77.916 65.414 77.287 c 65.828 76.873 66.453 75.209\n" 
            "   66.453 74.373 c 67.289 71.041 67.496 68.748 67.082 65.623 c 67.082\n" 
            "   63.955 66.664 62.291 66.246 60.623 c 65.828 58.748 65.621 56.873 64.996\n" 
            "   55.209 c 64.371 53.541 63.746 52.084 62.496 50.83 c 62.496 50.83 62.082\n" 
            "   51.248 62.082 51.666 c 61.871 51.873 61.871 52.084 61.871 52.291 c\n" 
            "   61.871 52.291 61.871 53.123 62.082 53.748 c 62.496 55.416 62.496 55.623\n" 
            "   62.289 56.666 c 62.289 57.498 62.289 57.709 61.871 58.748 c 61.453\n" 
            "   59.998 61.246 60.623 61.039 60.83 c 60.828 60.83 60.828 60.83 61.039\n" 
            "   60.623 c 61.039 60.416 61.453 59.584 61.453 59.166 c 61.664 58.123\n" 
            "   61.664 57.084 61.246 54.998 c 61.039 52.498 61.039 52.084 61.453 51.455\n" 
            "   c 61.453 51.248 61.871 50.83 62.082 50.416 c 62.914 49.373 63.328\n" 
            "   47.916 62.914 46.666 c 62.496 45.83 61.664 44.998 60.828 44.791 c\n" 
            "   60.414 44.584 l 59.996 43.748 59.582 42.709 59.164 41.662 c 58.539\n" 
            "   39.998 57.496 37.08 57.707 36.873 c 57.707 36.662 l 57.914 36.662\n" 
            "   59.789 36.662 62.082 36.455 c 70.203 36.455 72.289 36.455 76.664 36.041\n" 
            "   c 78.328 36.041 78.953 35.83 78.746 35.83 c 78.328 35.83 l 76.871\n" 
            "   35.623 72.496 35.416 71.453 35.416 c 70.414 35.416 69.164 35.416 66.246\n" 
            "   35.209 c 62.289 35.209 l 60.207 34.998 57.082 34.998 56.871 34.791 c\n" 
            "   56.871 34.791 56.871 34.584 56.664 34.166 c 56.453 33.119 56.246 32.494\n" 
            "   56.039 31.873 c 55.828 31.455 55.828 31.248 55.207 30.83 c 53.746\n" 
            "   29.377 50.414 26.662 48.539 25.623 c 48.121 25.416 l 47.496 25.416 l\n" 
            "   45.828 25.416 41.453 25.623 41.246 25.623 c 40.828 25.416 40.621 25.623\n" 
            "   39.789 26.041 c 38.328 26.662 36.871 27.494 35.828 28.334 c 33.746\n" 
            "   29.998 32.707 30.83 32.496 31.455 c 32.496 31.662 32.289 32.494 32.082\n" 
            "   33.119 c 31.871 33.959 31.664 34.584 31.664 34.791 c 27.289 34.791 l\n" 
            "   22.496 34.791 20.621 34.791 18.121 34.377 c 17.082 34.377 l 16.871\n" 
            "   34.377 16.039 34.166 15.414 34.166 c 13.953 33.959 10.828 33.537 9.996\n" 
            "   33.537 c 9.789 33.537 9.789 32.287 9.996 31.041 c 10.414 28.537 11.039\n" 
            "   26.248 12.082 23.959 c 13.121 21.041 14.789 18.537 16.664 16.041 c\n" 
            "   17.914 14.166 19.996 11.873 21.664 10.416 c 24.789 7.705 28.746 5.83\n" 
            "   32.707 4.584 c 36.039 3.537 39.582 2.912 43.121 2.494 c 46.664 1.873\n" 
            "   50.621 1.873 54.164 2.494 c 57.914 2.912 61.453 4.166 64.789 5.623 c\n" 
            "   66.871 6.455 69.582 7.912 71.246 9.166 c 74.371 11.455 76.664 14.377\n" 
            "   78.746 17.705 c 80.828 20.623 81.664 24.377 82.703 27.705 c 83.121\n" 
            "   29.584 83.328 31.662 83.328 33.537 c 83.328 35.623 83.328 36.455 82.703\n" 
            "   38.537 c 82.703 38.752 82.703 39.166 82.496 39.377 c 82.496 39.791\n" 
            "   82.496 39.791 82.703 39.791 c 82.703 39.377 l 82.703 39.166 82.914\n" 
            "   38.537 83.953 36.455 c 84.789 34.791 84.996 34.377 84.996 33.752 c\n" 
            "   85.621 32.08 85.828 30.416 85.828 28.537 c 86.039 26.455 85.203 24.166\n" 
            "   84.164 22.287 c 83.328 20.416 82.914 19.584 81.453 17.287 c 80.414\n" 
            "   15.623 78.746 13.334 77.703 12.08 c 77.082 11.455 75.414 9.584 74.582\n" 
            "   8.959 c 72.082 6.873 70.414 5.623 67.496 4.377 c 64.582 2.912 63.121\n" 
            "   2.287 59.164 1.248 c 57.082 0.623 54.164 0.209 51.871 -0.002 c 44.996\n" 
            "   -0.002 l h\n"
            "   46.246 33.752 m 46.664 34.377 46.871 34.791 46.664 34.998 c 46.039\n" 
            "   34.998 l 45.207 34.998 44.996 34.998 44.996 34.584 c 44.996 34.166\n" 
            "   45.414 33.537 45.621 33.334 c 45.621 33.119 45.621 33.119 45.828 33.119\n" 
            "   c 46.039 33.119 46.039 33.334 46.246 33.752 c h\n"
            "   44.582 33.334 m 44.582 33.752 44.789 34.791 44.789 34.791 c 44.164\n" 
            "   34.791 l 43.539 34.791 l 43.539 34.584 l 43.539 34.377 43.746 33.959\n" 
            "   43.953 33.537 c 44.164 33.119 44.371 33.119 44.582 33.334 c h\n"
            "   41.664 33.752 m 41.664 33.959 41.664 34.166 41.453 34.584 c 41.453\n" 
            "   34.791 l 41.246 34.998 40.414 34.791 40.414 34.791 c 40.207 34.791\n" 
            "   40.621 34.166 40.828 33.959 c 41.246 33.752 41.246 33.752 41.453 33.752\n" 
            "   c 41.664 33.752 l h\n"
            "   47.707 33.959 m 47.914 34.166 48.121 34.998 48.121 34.998 c 48.121\n" 
            "   35.209 47.914 35.209 47.707 35.209 c 47.289 35.209 47.289 35.209 47.289\n" 
            "   34.377 c 47.289 33.752 l 47.496 33.752 47.496 33.959 47.707 33.959 c h\n"
            "   42.707 33.959 m 42.914 33.959 42.914 34.377 42.914 34.584 c 42.914\n" 
            "   34.791 42.914 34.791 42.289 34.791 c 41.664 34.791 41.664 34.791 42.289\n" 
            "   34.166 c 42.496 33.752 42.496 33.752 42.707 33.959 c h\n"
            "   48.953 34.166 m 49.164 34.377 49.582 34.998 49.582 35.209 c 49.582\n" 
            "   35.209 49.371 35.209 48.953 35.416 c 48.539 35.416 48.539 35.416 48.328\n" 
            "   34.998 c 48.328 34.377 48.328 33.959 48.746 33.959 c 48.746 33.959\n" 
            "   48.953 33.959 48.953 34.166 c h\n"
            "   39.789 34.584 m 39.789 34.998 l 39.371 34.998 l 38.953 34.998 l 38.953\n" 
            "   34.998 39.164 34.791 39.371 34.584 c 39.789 34.584 l h\n"
            "   50.414 34.584 m 50.828 34.791 50.828 35.209 50.621 35.209 c 50.207\n" 
            "   35.416 49.996 35.209 49.789 34.998 c 49.789 34.791 49.789 34.791 49.996\n" 
            "   34.584 c 50.414 34.584 l h\n"
            "   12.289 35.623 m 12.496 35.83 12.496 35.83 12.289 36.041 c 12.082 36.248\n" 
            "   12.082 36.248 11.453 36.455 c 10.414 36.455 l 10.414 36.248 10.207\n" 
            "   35.83 10.414 35.83 c 10.414 35.623 11.871 35.416 12.289 35.623 c h\n"
            "   8.539 36.041 m 8.539 36.248 l 8.328 36.248 l 7.914 36.248 7.914 36.041\n" 
            "   7.914 36.041 c 7.914 36.041 8.121 35.83 8.328 35.83 c 8.539 35.83 8.539\n" 
            "   35.83 8.539 36.041 c h\n"
            "   50.621 36.873 m 50.621 37.08 50.621 37.287 50.828 38.119 c 50.828\n" 
            "   38.959 50.621 39.584 50.414 39.584 c 50.207 39.584 49.996 39.377 49.996\n" 
            "   39.166 c 49.582 38.537 49.582 38.334 49.582 37.705 c 49.371 37.287\n" 
            "   49.371 37.287 49.582 37.08 c 49.789 36.873 50.414 36.662 50.621 36.873\n" 
            "   c h\n"
            "   44.371 37.08 m 44.582 37.08 l 44.789 37.287 44.582 38.752 44.582 38.959\n" 
            "   c 44.582 39.166 44.371 39.377 43.953 39.377 c 43.746 39.584 43.539\n" 
            "   39.584 43.539 39.377 c 43.328 39.166 42.914 37.287 42.914 37.08 c\n" 
            "   42.914 37.08 43.539 36.873 44.371 37.08 c h\n"
            "   40.207 37.08 m 40.414 37.287 40.414 37.287 40.414 37.912 c 40.414\n" 
            "   38.752 l 40.207 38.959 l 39.996 39.377 39.582 39.584 39.371 39.584 c\n" 
            "   38.953 39.584 38.953 39.584 38.953 38.537 c 38.953 37.705 38.953 37.287\n" 
            "   39.164 37.08 c 39.789 37.08 l 40.207 37.08 l h\n"
            "   46.871 37.287 m 47.082 37.287 47.082 37.287 46.871 38.119 c 46.871\n" 
            "   38.537 46.664 39.166 46.453 39.377 c 46.246 39.377 l 45.828 39.377 l\n" 
            "   45.828 39.166 45.414 38.334 45.414 37.705 c 45.207 37.08 45.414 37.08\n" 
            "   46.039 37.08 c 46.246 37.08 46.664 37.08 46.871 37.287 c h\n"
            "   48.539 37.287 m 48.746 37.287 48.746 37.287 48.953 37.494 c 48.953\n" 
            "   38.119 l 48.953 38.752 48.953 38.752 48.746 38.959 c 48.539 39.584\n" 
            "   48.328 39.584 47.914 39.166 c 47.914 38.752 47.496 37.912 47.496 37.494\n" 
            "   c 47.496 37.287 l 47.707 37.287 l 47.914 37.287 l 48.121 37.08 48.121\n" 
            "   37.08 48.539 37.287 c h\n"
            "   42.082 37.287 m 42.082 37.287 42.289 37.705 42.496 38.119 c 42.496\n" 
            "   38.752 l 42.496 38.959 l 42.289 39.166 42.289 39.377 42.082 39.584 c\n" 
            "   41.871 39.791 41.664 39.791 41.453 39.584 c 41.246 39.377 41.039 39.166\n" 
            "   41.039 38.334 c 40.828 37.705 l 41.039 37.494 l 41.039 37.287 41.246\n" 
            "   37.287 41.664 37.287 c 42.082 37.287 l h\n"
            "   51.871 37.287 m 51.871 37.287 52.082 37.494 52.082 37.705 c 52.289\n" 
            "   37.705 l 52.496 38.537 l 52.496 39.166 52.496 39.377 52.289 39.791 c\n" 
            "   52.082 39.791 l 51.871 39.584 51.453 38.752 51.453 38.537 c 51.246\n" 
            "   38.119 51.246 37.494 51.246 37.287 c 51.871 37.287 l h\n"
            "   38.539 37.287 m 38.539 38.119 l 38.539 38.959 l 38.121 39.166 l 37.914\n" 
            "   39.584 37.496 39.791 37.289 39.791 c 37.082 39.584 37.082 39.166 37.289\n" 
            "   38.119 c 37.289 37.912 37.289 37.705 37.496 37.494 c 37.707 37.287 l\n" 
            "   38.121 37.287 l 38.539 37.287 l h\n"
            "   36.664 37.705 m 36.871 37.912 36.664 39.377 36.453 39.584 c 36.246\n" 
            "   39.791 35.828 39.998 35.621 39.998 c 35.414 39.998 35.414 39.998 35.414\n" 
            "   39.584 c 35.414 39.377 35.621 38.119 35.828 37.912 c 35.828 37.494\n" 
            "   36.453 37.494 36.664 37.705 c h\n"
            "   53.539 37.912 m 53.746 37.912 53.953 38.119 53.953 38.959 c 54.164\n" 
            "   39.584 54.164 39.998 53.953 39.998 c 53.746 40.209 53.746 40.209 53.539\n" 
            "   39.791 c 53.121 39.166 52.914 38.752 52.914 38.334 c 52.914 37.705 l\n" 
            "   53.121 37.705 l 53.328 37.705 53.539 37.705 53.539 37.912 c h\n"
            "   34.996 38.119 m 34.996 38.537 34.789 39.791 34.789 39.791 c 34.582\n" 
            "   39.998 34.164 39.998 34.164 39.998 c 33.953 39.791 33.953 39.166 34.371\n" 
            "   38.334 c 34.582 37.912 34.789 37.912 34.996 37.912 c 34.996 38.119 l h\n"
            "   54.789 38.334 m 54.996 38.334 54.996 38.752 55.207 39.584 c 55.207\n" 
            "   40.416 55.207 40.416 54.996 40.416 c 54.789 40.416 54.789 40.209 54.582\n" 
            "   40.209 c 54.582 38.334 l 54.789 38.334 l h\n"
            "   33.539 40.209 m 33.746 40.416 33.746 41.248 33.746 41.455 c 33.746\n" 
            "   41.873 33.328 43.123 33.121 43.334 c 32.914 43.748 31.664 44.584 31.246\n" 
            "   44.584 c 31.039 44.584 31.039 44.373 31.664 43.334 c 32.289 42.084\n" 
            "   33.328 40.209 33.328 39.998 c 33.539 39.998 33.539 39.998 33.539 40.209\n" 
            "   c h\n"
            "   56.039 40.416 m 56.246 40.83 56.246 41.041 56.453 41.662 c 57.082\n" 
            "   43.541 57.082 43.955 56.871 43.955 c 56.871 44.166 56.453 43.955 56.039\n" 
            "   43.541 c 55.414 43.334 l 55.414 42.916 l 55.207 42.498 55.207 41.873\n" 
            "   55.414 41.248 c 55.621 40.416 55.621 40.416 55.828 40.416 c 56.039\n" 
            "   40.416 l h\n"
            "   43.746 46.455 m 43.953 46.666 44.164 46.873 44.371 47.084 c 44.371\n" 
            "   47.291 44.582 47.498 44.582 47.498 c 44.582 47.498 44.789 47.291 45.207\n" 
            "   46.666 c 45.621 46.248 45.828 46.248 46.664 46.041 c 47.289 46.041\n" 
            "   47.707 46.248 48.121 46.666 c 48.953 47.291 48.953 47.498 48.328 48.541\n" 
            "   c 47.914 49.166 47.914 49.373 47.707 50.416 c 47.496 51.041 47.289\n" 
            "   51.873 47.082 52.291 c 46.871 53.33 46.664 54.373 46.453 54.998 c\n" 
            "   46.246 55.623 46.039 55.83 45.828 55.83 c 45.828 55.83 45.414 55.416\n" 
            "   45.207 54.998 c 45.207 54.791 45.207 54.791 44.996 54.791 c 44.996\n" 
            "   54.791 44.789 54.998 44.582 55.416 c 44.582 55.623 44.371 55.83 44.371\n" 
            "   55.83 c 44.164 56.041 l 44.164 55.83 l 43.746 55.416 43.539 54.998\n" 
            "   43.121 53.541 c 42.496 52.084 42.496 51.666 41.664 49.791 c 41.039\n" 
            "   48.33 40.828 47.916 40.828 47.084 c 40.828 46.455 40.828 46.455 41.039\n" 
            "   46.455 c 41.246 46.455 41.453 46.455 41.453 46.248 c 41.664 46.248\n" 
            "   42.914 46.041 43.121 46.248 c 43.539 46.248 43.539 46.248 43.746 46.455\n" 
            "   c h\n"
            "   34.582 49.998 m 35.828 49.998 36.246 50.209 36.871 50.623 c 38.121\n" 
            "   51.041 38.746 52.084 39.371 53.123 c 39.996 54.584 40.414 55.83 40.414\n" 
            "   57.291 c 40.414 58.123 40.414 58.748 39.996 59.373 c 39.789 59.791\n" 
            "   39.371 59.998 38.539 60.623 c 37.496 61.455 34.582 61.248 33.328 61.041\n" 
            "   c 32.082 60.83 31.664 60.623 31.246 60.416 c 31.039 60.416 30.621\n" 
            "   59.998 30.207 59.791 c 29.371 59.373 28.953 59.166 28.746 58.955 c\n" 
            "   28.746 58.748 28.539 58.541 28.539 58.33 c 28.328 57.916 28.328 57.709\n" 
            "   28.328 56.666 c 28.328 55.623 28.746 54.584 28.746 53.541 c 28.953\n" 
            "   52.916 29.164 52.291 29.164 51.873 c 29.164 51.666 29.371 51.455 29.371\n" 
            "   51.455 c 29.582 50.623 30.414 49.998 31.039 49.791 c 31.246 49.791\n" 
            "   33.746 49.998 34.582 49.998 c h\n"
            "   57.707 50.416 m 58.328 50.623 59.371 51.666 59.582 52.709 c 59.582\n" 
            "   52.916 59.789 53.123 59.789 53.33 c 59.789 53.541 59.789 54.166 59.996\n" 
            "   54.373 c 60.207 55.623 60.207 56.455 59.996 57.291 c 59.789 58.955\n" 
            "   59.582 59.584 59.164 59.998 c 58.953 60.209 l 58.953 60.416 58.328\n" 
            "   60.83 57.914 61.041 c 55.621 61.666 53.121 61.455 50.621 60.83 c 49.582\n" 
            "   60.623 49.164 59.998 48.953 58.748 c 48.539 57.291 48.953 55.623 49.582\n" 
            "   53.955 c 49.996 53.123 50.828 51.455 51.039 51.248 c 51.871 50.416\n" 
            "   52.707 49.998 53.746 49.998 c 54.996 50.209 56.453 50.209 57.707 50.416\n" 
            "   c h\n"
            "   44.164 94.436 m 45.414 95.123 48.328 96.268 49.789 96.643 c 51.039\n" 
            "   96.998 54.164 97.205 57.289 97.186 c 58.121 97.162 58.746 97.186 58.746\n" 
            "   97.205 c 58.746 97.229 58.328 97.287 57.914 97.354 c 57.289 97.436\n" 
            "   56.871 97.455 55.621 97.455 c 52.707 97.455 50.828 97.248 48.953 96.748\n" 
            "   c 47.707 96.373 44.582 94.897 43.746 94.311 c 43.539 94.104 43.539\n" 
            "   94.104 44.164 94.436 c h\n"
            "   44.164 94.436 m f\n"
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
