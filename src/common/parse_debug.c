/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: parse_debug.c
 * PURPOSE: Parse the Debug options on the command line
 * Note: modified version of PLP code.
 **************************************************************************/

#include "lp.h"
#include "killchild.h"
#include "errorcodes.h"
/**** ENDINCLUDE ****/

static char *const _id =
"parse_debug.c,v 3.7 1997/10/04 16:14:09 papowell Exp";

/*************************************************************
 * void Get_debug_parm(int argc, char *argv[], struct keywords *list)
 * Scan the command line for -D debugparms
 *  debugparms has the format value,key=value,key,key@...
 *  1. if the value is an integer,  then we treat it as a value for DEBUG
 *  2. if a key is present,  then we scan the list and find the
 *     match for the key.  We then convert according to the type of
 *     option expected.
 *************************************************************/

struct keywords debug_vars[]		/* debugging variables */
 = {
#if !defined(NODEBUG)
    { "debug",INTEGER_K,(void *)&Debug },
    { "test",INTEGER_K,(void *)&DbgTest },
    { "job",INTEGER_K,(void *)&DbgJob },
/*    { "print",FLAG_K,(void *)&DbgFlag,DPRINTMASK, DPRINTMASK }, */
    { "print",FLAG_K,(void *)&DbgFlag,DBPRINT3, DPRINTMASK },
    { "print+1",FLAG_K,(void *)&DbgFlag,DBPRINT1, DPRINTMASK },
    { "print+2",FLAG_K,(void *)&DbgFlag,DBPRINT2, DPRINTMASK },
    { "print+3",FLAG_K,(void *)&DbgFlag,DBPRINT3, DPRINTMASK },
    { "print+4",FLAG_K,(void *)&DbgFlag,DBPRINT4, DPRINTMASK },
/*    { "network",FLAG_K,(void *)&DbgFlag,DNWMASK, DNWMASK }, */
    { "network",FLAG_K,(void *)&DbgFlag,DBNW3, DNWMASK },
    { "network+1",FLAG_K,(void *)&DbgFlag,DBNW1, DNWMASK },
    { "network+2",FLAG_K,(void *)&DbgFlag,DBNW2, DNWMASK },
    { "network+3",FLAG_K,(void *)&DbgFlag,DBNW3, DNWMASK },
    { "network+4",FLAG_K,(void *)&DbgFlag,DBNW4, DNWMASK },
/*    { "database",FLAG_K,(void *)&DbgFlag,DDBMASK, DDBMASK }, */
    { "database",FLAG_K,(void *)&DbgFlag,DBB3, DDBMASK },
    { "database+1",FLAG_K,(void *)&DbgFlag,DBB1, DDBMASK },
    { "database+2",FLAG_K,(void *)&DbgFlag,DBB2, DDBMASK },
    { "database+3",FLAG_K,(void *)&DbgFlag,DBB3, DDBMASK },
    { "database+4",FLAG_K,(void *)&DbgFlag,DBB4, DDBMASK },
/*    { "receive",FLAG_K,(void *)&DbgFlag,DRECVMASK, DRECVMASK }, */
    { "receive",FLAG_K,(void *)&DbgFlag,DBRECV3, DRECVMASK },
    { "receive+1",FLAG_K,(void *)&DbgFlag,DBRECV1, DRECVMASK },
    { "receive+2",FLAG_K,(void *)&DbgFlag,DBRECV2, DRECVMASK },
    { "receive+3",FLAG_K,(void *)&DbgFlag,DBRECV3, DRECVMASK },
    { "receive+4",FLAG_K,(void *)&DbgFlag,DBRECV4, DRECVMASK },
/*    { "auth",FLAG_K,(void *)&DbgFlag,DAUTHMASK, DAUTHMASK }, */
    { "auth",FLAG_K,(void *)&DbgFlag,DBAUTH3, DAUTHMASK },
    { "auth+1",FLAG_K,(void *)&DbgFlag,DBAUTH1, DAUTHMASK },
    { "auth+2",FLAG_K,(void *)&DbgFlag,DBAUTH2, DAUTHMASK },
    { "auth+3",FLAG_K,(void *)&DbgFlag,DBAUTH3, DAUTHMASK },
    { "auth+4",FLAG_K,(void *)&DbgFlag,DBAUTH4, DAUTHMASK },
/*    { "memory",FLAG_K,(void *)&DbgFlag,DMEMMASK, DMEMMASK }, */
    { "memory",FLAG_K,(void *)&DbgFlag,DBMEM3, DMEMMASK },
    { "memory+1",FLAG_K,(void *)&DbgFlag,DBMEM1, DMEMMASK },
    { "memory+2",FLAG_K,(void *)&DbgFlag,DBMEM2, DMEMMASK },
    { "memory+3",FLAG_K,(void *)&DbgFlag,DBMEM3, DMEMMASK },
    { "memory+4",FLAG_K,(void *)&DbgFlag,DBMEM4, DMEMMASK },
#endif
    { (char *)0 }
};

void Parse_debug (char *dbgstr, struct keywords *list, int interactive );

void Get_debug_parm(int argc, char *argv[], char *optstr,
	struct keywords *list)
{
#if !defined(NODEBUG)
	int option, flag;
	flag = Opterr;
	Opterr = 0;
	
	while ((option = Getopt (argc,argv,optstr?optstr:"D:"))!= EOF) {
		switch (option) {
		case 'D':
		    if( Optarg ){
				Parse_debug (Optarg, list, 1 );
			} else {
				if( Interactive ){
					fprintf(stderr, "-D missing option\n");
				}
				Errorcode = JABORT;
				cleanup(0);
			}
		    break;
		default: break;
		}
	}
	Opterr = flag;
	Getopt( 0, (void *)0, (void *) 0 );
	if( DEBUGL0 ){
		struct keywords *keywords;
		for( keywords = list; keywords->keyword; ++keywords ){
			if( strchr( keywords->keyword, '+' ) ) continue;
			logDebug( "Get_debug_parm: %s = 0x%x",
				keywords->keyword, *((int **)keywords->variable) );
		}
	}
#endif
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
#if !defined(NODEBUG)
	char *s, *key, *value, *notflag, *convert, *end;
	int i, n, found;
	static char buf[LINEBUFFER];

	/* duplicate the string */

	safestrncpy( buf, dbgstr );

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
			int lastflag = 0;
			int nooutput = 0;
		    fprintf (stderr,
	"debug usage: -D [ num | key=num | key=str | flag | flag@ | flag+N ]*\n");
		    fprintf (stderr, "  keys recognized:");
		    for (i = 0; list[i].keyword; i++) {
				if( strchr( list[i].keyword, '+' ) ) continue;
				if( nooutput == 0 ){
					if( i ){
						fprintf( stderr, ", " );
						if( !(i % 4) ) fprintf( stderr, "\n   " );
					} else {
						fprintf( stderr, " " );
					}
				} else {
					nooutput = 0;
				}
				switch( list[i].type ){
				case INTEGER_K:
					fprintf (stderr, "%s=num", list[i].keyword);
					break;
				case STRING_K:
					fprintf (stderr, "%s=str", list[i].keyword);
					break;
				case FLAG_K:
					if( list[i].maxval == 0 || lastflag != list[i].flag ){
						fprintf (stderr, "%s[+N,@]", list[i].keyword );
						lastflag = list[i].maxval;
					} else {
						nooutput = 1;
					}
					break;
				default:
					break;
				}
			}
		    fprintf (stderr, "\n");
			Errorcode = JABORT;
		    cleanup(0);
		}
	}
#endif
}
