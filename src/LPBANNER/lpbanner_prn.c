/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: banner.c
 * PURPOSE: print a banner
 **************************************************************************/

static char *const _id =
"lpbanner_prn.c,v 3.2 1997/09/18 19:45:43 papowell Exp";

#ifdef HAVE_CONFIG_H
# include "config.h"
# include "portable.h"
#endif

#include "lpbanner.h"

void Out_line( void );
void breakline( int c );
void bigprint( struct font *font, char *line );
void do_char( struct font *font, struct glyph *glyph,
	char *str, int line, int width );
/*
 * Print a banner
 * 
 * banner(): print the banner
 * 1. Allocate a buffer for output string
 * 2. Calculate the various proportions
 *     - topblast 
 *     - topbreak 
 *     - big letters  
 *     - info -          6 lines
 *     - <filler>
 *     - bottombreak
 *     - bottomblast
 */

char bline[1024];
int bigjobnumber, biglogname, bigfromhost, bigjobname;
int top_break,	/* break lines at top of page */
	top_sep,	/* separator from info at top of page */
	bottom_sep,	/* separator from info at bottom of page */
	bottom_break;	/* break lines at bottom of page */
int breaksize = 3;	/* numbers of rows in break */
char *Time_str(int shortform, time_t tm);

/*
 * userinfo: just printf the information
 */
static void userinfo( void )
{
	(void) sprintf( bline, "User:  %s@%s (%s)", login, host, bnrname);
	Out_line();
	(void) sprintf( bline, "Date:  %s", Time_str(0,0));
	Out_line();
	(void) sprintf( bline, "Job:   %s", job );
	Out_line();
	(void) sprintf( bline, "Class: %s", class );
	Out_line();
}

/*
 * seebig: calcuate if the big letters can be seen
 * i.e.- printed on the page
 */

void seebig( int *len, int bigletter_height, int *big )
{
	*big = 0;
	if( *len > bigletter_height ){
		*big = bigletter_height;
		*len -= *big;
	}
}

/*
 * banner: does all the actual work
 */

char *isnull( char *s )
{
	if( s == 0 ) s = "";
	return( s );
}

void banner(void)
{
	int len;					/* length of page */
	int i;                      /* ACME integers, INC */
	char jobnumber[1024];

	/* read from the stdin */
	(void)fgets( jobnumber, sizeof(jobnumber), stdin );
	if(debug)fprintf(stdout, "BANNER CMD '%s'\n", jobnumber );
	bigjobnumber = biglogname = bigfromhost = bigjobname = 0;

	/* now calculate the numbers of lines available */
	if(debug)fprintf(stderr, "BANNER: length %d\n", length );
	len = length;
	len -= 4;	/* user information */
	/* now we add a top break and bottom break */
	if( len > 2*breaksize ){
		top_break = breaksize;
		bottom_break = breaksize;
	}  else {
		top_break = 1;
		bottom_break = 1;
	}
	len -= (top_break + bottom_break);

	if( bnrname == 0 ){
		bnrname = login;
	}

	/* see if we can do big letters */
	jobnumber[0] = 0;
	if( controlfile ){
		strncpy( jobnumber, controlfile+3, 3 );
		jobnumber[3] = 0;
	}
	if(jobnumber && *jobnumber ) seebig( &len, Font9x8.height, &bigjobnumber );
	if(bnrname && *bnrname) seebig( &len, Font9x8.height, &biglogname );
	if(host && *host ) seebig( &len, Font9x8.height, &bigfromhost );
	if(job && *job) seebig( &len, Font9x8.height, &bigjobname );

	/* now we see how much space we have left */
	while( length > 0 && len < 0 ){
		len += length;
	}

	/*
	 * we add padding
	 * Note that this produces a banner page exactly PL-1 lines long
	 * This allows a form feed to be added onto the end.
	 */
	if( len > 0 ){
		/* adjust the total page length */
		len = len -1;
		/* check to see if we make breaks a little larger */
		if( len > 16 ){
			top_break += 3;
			bottom_break += 3;
			len -= 6;
		}
		top_sep = len/2;
		bottom_sep = len - top_sep;
	}
	if(debug)fprintf(stderr, "BANNER: length %d, top_break %d, top_sep %d\n",
		length, top_break, top_sep  );
	if(debug)fprintf(stderr, "BANNER: bigjobnumber %d, jobnumber '%s'\n", bigjobnumber,
		isnull(jobnumber) );
	if(debug)fprintf(stderr, "BANNER: biglogname %d, bnrname '%s'\n", biglogname,
		isnull(bnrname) );
	if(debug)fprintf(stderr, "BANNER: bigfromhost %d, host '%s'\n", bigfromhost,
		isnull(host) );
	if(debug)fprintf(stderr, "BANNER: bigjobname %d, jobname '%s'\n", bigjobname,
		isnull(job) );
	if(debug)fprintf(stderr, "BANNER: userinfo %d\n", 4 );
	if(debug)fprintf(stderr, "BANNER: bottom_sep %d, bottom_break %d\n",
		bottom_sep, bottom_break  );

	for( i = 0; i < top_break; ++i ){
		breakline( '*');
	}
	for( i = 0; i < top_sep; ++i ){
		breakline( 0 );
	}

	/*
	 * print the Name, Host and Jobname in BIG letters
	 * allow some of them to be dropped if there isn't enough
	 * room.
	 */

	if( bigjobnumber ) bigprint( &Font9x8, jobnumber );
	if( biglogname ) bigprint( &Font9x8, bnrname );
	if( bigfromhost ) bigprint( &Font9x8, host);
	if( bigjobname ) bigprint( &Font9x8, job );
	userinfo();

	for( i = 0; i < bottom_sep; ++i ){
		breakline( 0 );
	}
	for( i = 0; i < bottom_break; ++i ){
		breakline( '*');
	}
}

void breakline( int c )
{
	int i;

	if( c ){
		for( i = 0; i < width; i++) bline[i] = c;
		bline[i] = '\0';
	} else {
		bline[0] = '\0';
	}
	Out_line();
}

/***************************************************************************
 * bigprint( struct font *font, char * line)
 * print the line in big characters
 * for i = topline to bottomline do
 *  for each character in the line do
 *    get the scan line representation for the character
 *    foreach bit in the string do
 *       if the bit is set, print X, else print ' ';
 *        endfor
 *  endfor
 * endfor
 *
 ***************************************************************************/

void bigprint( struct font *font, char *line )
{
	int i, j, k, len;                   /* ACME Integers, Inc. */

	bline[width] = 0;
	len = strlen(line);
	if(debug)fprintf(stderr,"bigprint: '%s'\n", line );
	for( i = 0; i < font->height; ++i ){
		for( j = 0; j < width; ++j ){
			bline[j] = ' ';
		}
		for( j = 0, k = 0; j < width && k < len; j += font->width, ++k ){
			do_char( font, &font->glyph[line[k]-32], &bline[j], i, width - j );
		}
		Out_line();
	}
}

/***************************************************************************
 * write the buffer out to the file descriptor.
 * don't do if fail is invalid.
 ***************************************************************************/

void Out_line( void )
{
	int i, l;
	char *str;
	bline[sizeof(bline)-1] = 0;
	if( width < sizeof(bline) ) bline[width] = 0;
	for( str = bline, i = strlen(str);
		i > 0 && (l = write( 1, str, i)) > 0;
		i -= l, str += l );
	for( str = "\n", i = strlen(str);
		i > 0 && (l = write( 1, str, i)) > 0;
		i -= l, str += l );
}

/*
 * Time_str: return "cleaned up" ctime() string...
 *
 * Thu Aug 4 12:34:17 BST 1994 -> Aug  4 12:34:17
 * Thu Aug 4 12:34:17 BST 1994 -> 12:34:17
 */

char *Time_str(int shortform, time_t tm)
{
    time_t tvec;
    static char s[99];
	char *t;

	if( tm ){
		tvec = tm;
	} else {
		(void) time (&tvec);
	}
    (void)strcpy( s, ctime(&tvec) );
	t = s;
	s[29] = 0;
	if( shortform > 0 ){
		t = &s[11];
		s[19] = 0;
	} else if( shortform == 0 ){
		t = &s[4];
		s[19] = 0;
	}
	return(t);
}

void do_char( struct font *font, struct glyph *glyph,
	char *str, int line, int width )
{
	int chars, i, j, k;
	char *s;

	/* if(debug)fprintf(stderr,"do_char: '%c', width %d\n", glyph->ch, width ); */
	chars = (font->width+7)/8;	/* calculate the row */
	s = &glyph->bits[line*chars];	/* get start of row */
	for( k = 0, i = 0; k < width && i < chars; ++i ){	/* for each byte in row */
		for( j = 7; k < width && j >= 0; ++k, --j ){	/* from most to least sig bit */
			if( *s & (1<<j) ){		/* get bit value */
				str[k] = 'X';
			}
		}
		++s;
	}
}
