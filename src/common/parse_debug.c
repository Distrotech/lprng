/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: parse_debug.c
 * PURPOSE: Parse the Debug options on the command line
 * Note: modified version of PLP code.
 **************************************************************************/

#include "lp.h"

static char *const _id =
"$Id: parse_debug.c,v 3.0 1996/05/19 04:06:05 papowell Exp $";

/*************************************************************
 * void Get_debug_parm(int argc, char *argv[], struct keywords *list)
 * Scan the command line for -D debugparms
 *  debugparms has the format value,key=value,key,key@...
 *  1. if the value is an integer,  then we treat it as a value for DEBUG
 *  2. if a key is present,  then we scan the list and find the
 *     match for the key.  We then convert according to the type of
 *     option expected.
 *************************************************************/

void Parse_debug (char *dbgstr, struct keywords *list, int interactive );
extern int Debug;	/* debug level */

void Get_debug_parm(int argc, char *argv[], char *optstr,
	struct keywords *list)
{
	int option;
	while ((option = Getopt (argc, argv, optstr )) != EOF) {
		switch (option) {
		case 'D':
		    if( Optarg ){
				Parse_debug (Optarg, list, 1 );
			} else {
				exit( 1 );
			}
		    break;
		default: break;
		}
	}
	Getopt( 0, (void *)0, (void *) 0 );
	if( Debug ) dump_parms( "Get_debug_parms", list );
}

/*

Parse_debug (char *dbgstr, struct keywords *list, int interactive );
Input string:  value,key=value,flag,flag@,...

1. crack the input line at the ','
2. crack each option at = 
3. search for key words
4. assign value to variable

*/

void Parse_debug (char *dbgstr, struct keywords *list, int interactive )
{
	char *s, *buf, *key, *value, *notflag, *convert, *end;
	int i, n, found;

	/* duplicate the string */

	buf = safestrdup( dbgstr );

	/* crack the string at ',' */

	if( (s = strchr( dbgstr, '\n' )) ) *s = 0;
	DEBUG0("Parse_debug '%s'", dbgstr );
	for( s = buf; s && *s; s = end ){
		key = s;
		end = strchr( s, ',' );
		if( end ){
			*end++ = 0;
		}
		/* split option at '=' */
		if( (value = strchr( s, '=' )) ){
			*value++ = 0;
		}
		notflag = 0;
		/* check for not flag at end '@' */
		if( (notflag = strchr( s, '@' )) ){
			*notflag++ = 0;
		}
		found = 0;
		if( isdigit( key[0] ) ){
			/* we should have an integer value */
			convert = key;
			Debug = strtol( key, &convert, 10 );
			found = (convert != key);
		} else {
			/* search the keyword list */
			for (i = 0;
				(convert = list[i].keyword) && strcasecmp( convert, key );
				++i );
			if( convert != 0 ){
				switch( list[i].type ){
				case INTEGER_K:
					if( value == 0 ) break;
					convert = value;
					*(int *)list[i].variable = n =
						strtol( value, &convert, 10 );
					found = (convert != value) && *convert == 0;
					break;
				case STRING_K:
					if( value == 0 ) break;
					*(char **)list[i].variable = value;
					found = 1;
					break;
				case FLAG_K:
					if( value ) break;
					n = list[i].maxval;
					if( notflag ){
						*(int *)list[i].variable &= ~n;
					} else {
						*(int *)list[i].variable |= n;
					}
					found = 1;
					break;
				default:
					break;
				}
			}
		}
		if (found == 0 && interactive ) {
		    fprintf (stderr,
	"debug usage: -D [ num | key=num | key=str | key | key@ ]*\n");
		    fprintf (stderr, "  keys recognized:");
		    for (i = 0; list[i].keyword; i++) {
				if( i ){
					fprintf( stderr, ", " );
					if( !(i % 4) ) fprintf( stderr, "\n   " );
				} else {
					fprintf( stderr, " " );
				}
				switch( list[i].type ){
				case INTEGER_K:
					fprintf (stderr, "%s=num", list[i].keyword);
					break;
				case STRING_K:
					fprintf (stderr, "%s=str", list[i].keyword);
					break;
				case FLAG_K:
					fprintf (stderr, "%s", list[i].keyword );
					break;
				default:
					break;
				}
			}
		    fprintf (stderr, "\n");
		    exit (1);
		}
	}
}
