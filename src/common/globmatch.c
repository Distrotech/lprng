/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: globmatch.c
 * PURPOSE: permform a glob type of match
 **************************************************************************/

static char *const _id = "$Id: globmatch.c,v 3.1 1996/06/30 17:12:44 papowell Exp $";

#include "lp.h"
#include "globmatch.h"

static int glob_pattern( char *pattern, char *str )
{
	int result;
	int len;
	char *glob;
	char pairs[3];

	DEBUG9("glob_pattern: pattern '%s' to '%s'\n", pattern, str );
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
	DEBUG9("Globmatch: pattern '%s' to '%s'\n",
		ISNULL(pattern), ISNULL(str)
		);
	if( pattern && strcasecmp( pattern, "NULL" ) == 0 ) pattern = 0;
	if( pattern == 0 ) pattern = "";
	if( str == 0 ) str = "";
	result = (strcasecmp( str , pattern ) != 0);
	if( result ){
		result = glob_pattern( pattern, str );
	}
	DEBUG9("Globmatch: '%s' to '%s' result %d\n",
		ISNULL(pattern), ISNULL(str),
		result );
	return( result );
}
