/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: globmatch.c
 * PURPOSE: permform a glob type of match
 **************************************************************************/

static char *const _id = "globmatch.c,v 3.5 1997/09/18 19:45:59 papowell Exp";

#include "lp.h"
#include "globmatch.h"
/**** ENDINCLUDE ****/

static int glob_pattern( char *pattern, char *str )
{
	int result;
	int len;
	char *glob;
	char pairs[3];

	DEBUG4("glob_pattern: pattern '%s' to '%s'", pattern, str );
	result =  strcasecmp( pattern, str );
	/* now we do walk through pattern */ 
	if( result && (glob = strchr( pattern, '*' )) ){
		/* check the characters up to the glob length for a match */
		len = glob - pattern;
		if( len == 0 || strncasecmp( pattern, str, len ) == 0 ){
			/* matched: pattern xxxx*  to xxxx
			 * now we have to do the remainder. We simply check for rest
			 */
			pattern += len+1;
			str += len;
			/* check for trailing glob */
			if( pattern[0] ){
				/* find the first character in pattern */
				/* note: if first char is glob, you are done */
				if( pattern[0] != '*' ){
					glob = pairs;
					*glob = len = pattern[0];
					if( isalpha( len ) ){
						*glob++ = tolower( len );
						*glob = toupper( len );
					}
					glob[1] = 0;
					while( result && (str = strpbrk( str, pairs)) ){
						result = glob_pattern( pattern, str ); 
						++str;
					}
				} else {
					result = glob_pattern( pattern, str );
				}
			} else {
				/* trailing glob pattern */
				result = 0;
			}
		}
	}
	return( result != 0 );
}

#define ISNULL(X) (X)?(X):"<NULL>"

int Globmatch( char *pattern, char *str )
{
	int result;

	/* try simple test first: string compare */
	DEBUG4("Globmatch: pattern '%s' to '%s'", pattern, str );
	if( pattern && strcasecmp( pattern, "NULL" ) == 0 ) pattern = "";
	if( pattern == 0 ) pattern = "";
	if( str == 0 ) str = "";
	result = glob_pattern( pattern, str );
	DEBUG4("Globmatch: '%s' to '%s' result %d", pattern, str, result );
	return( result );
}
