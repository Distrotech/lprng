/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: pathname.c
 * PURPOSE: expand and manage absolute pathnames
 **************************************************************************/

static char *const _id =
"pathname.c,v 3.2 1997/09/18 19:46:01 papowell Exp";

#include "lp.h"
#include "pathname.h"
/**** ENDINCLUDE ****/


/***************************************************************************
 * Fix_dir( pathname, size, s )
 * Add a trailing / to directory name s
 * if s is null or empty string, set it to /tmp/
 ***************************************************************************/
void Fix_dir( char *p, int size, char *s )
{
	int i;

	p[0] = 0;
	if( s && *s ){
		i = strlen( s );
		if( i >= size - 2 ){
			fatal( LOG_ERR, "Fix_dir: len '%d` too small for '%s'", size, s );
		}
		strcpy( p, s );
		s = &p[i];
		if( i == 0 || s[-1] != '/' ){
			*s++ = '/';
			*s = 0;
		}
	}
}

/***************************************************************************
 * Init_path( struc dpathname *p, char *s )
 *  - initialize the pathname and length fields of the
 *    dpathname data structure
 *
 * Expand_path( struc dpathname *p, char *s )
 *  - initialize the pathname and length fields of the
 *    dpathname data structure
 *  - expands $H, $h, and $P
 *
 * Clear_path( struc dpathname *p )
 *  - set the pathname field to pathlen bytes long
 *
 * Add_path( struc dpathname *p, char *s )
 *  - call Clear_path and then append  s to the pathname
 *
 * Add2_path( struc dpathname *p, char *s1, char *s2 )
 *  - call Clear_path and then append  s1 and s2 to the pathname
 *
 ***************************************************************************/

void Init_path( struct dpathname *p, char *s )
{
	if( s && *s ){
		Fix_dir( p->pathname, MAXPATHLEN, s );
		p->pathlen = strlen( p->pathname );
	} else {
		p->pathname[0] = 0;
		p->pathlen = 0;
	}
}


char *Expand_path( struct dpathname *p, char *s )
{
	int c, j;
	char *t;

	if( p ){
		j = 0;
		while( s && j<sizeof(p->pathname)-1 && (c = p->pathname[j] = *s++) ){
			if( c == '$' ){
				c = *s++;
				p->pathname[j] = 0;
				t = 0;
				switch( c ){
				case 'P': t = Printer; break;
				case 'H': t = FQDNHost; break;
				case 'h': t = ShortHost; break;
				}
				while( t && j < sizeof(p->pathname)-1 
						&& (p->pathname[j] = *t++) ) ++j;
			} else {
				++j;
			}
		}
		if( j > 0 && j<sizeof(p->pathname)-1 && p->pathname[j-1] != '/' ){
			p->pathname[j++] = '/';
		}
		if( j < sizeof(p->pathname) ) p->pathname[j] = 0;
		p->pathlen = strlen( p->pathname );
		return( p->pathname );
	}
	return( (char *)0 );
}

char *Clear_path( struct dpathname *p )
{
	if( p ){
		p->pathname[p->pathlen] = 0;
		return( p->pathname );
	}
	return( (char *)0 );
}

char *Add_path( struct dpathname *p, char *s )
{
	if( p ){
		p->pathname[p->pathlen] = 0;
		if( s ) strncat( p->pathname, s,
			sizeof( p->pathname ) - strlen(p->pathname) );
		return( p->pathname );
	}
	return( (char *)0 );
}

char *Add2_path( struct dpathname *p, char *s1, char *s2 )
{
	if( p ){
		p->pathname[p->pathlen] = 0;
		if( s1 ) strncat( p->pathname, s1,
			sizeof(p->pathname) - strlen(p->pathname) );
		if( s2 ) strncat( p->pathname, s2,
			sizeof(p->pathname) - strlen(p->pathname) );
		return( p->pathname );
	}
	return( (char *)0 );
}
