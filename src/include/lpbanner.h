/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpbanner.h
 * PURPOSE: lpbanner program declarations
 * "lpbanner.h,v 3.1 1996/12/28 21:40:30 papowell Exp"
 **************************************************************************/

#ifndef _LP_BANNER_
#define _LP_BANNER_

#ifndef EXTERN
#define EXTERN extern
#endif

EXTERN int errorcode;
EXTERN char *name;		/* name of filter */
EXTERN int debug, verbose, width, length, xwidth, ylength, literal, indent;
EXTERN char *zopts, *class, *job, *login, *accntname, *host;
EXTERN char *printer, *accntfile, *format;
EXTERN char *controlfile;
EXTERN char *bnrname, *comment;
EXTERN int npages;	/* number of pages */
EXTERN int special;
EXTERN char *queuename, *errorfile;

#define GLYPHSIZE 15
struct glyph{
	int ch, x, y;	/* baseline location relative to x and y position */
	char bits[GLYPHSIZE];
};

struct font{
	int height;	/* height from top to bottom */
	int width;	/* width in pixels */
	int above;	/* max height above baseline */
	struct glyph *glyph;	/* glyphs */
};

extern struct font Font9x8;
#endif
