/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: debug.c,v 5.1 1999/09/12 21:32:34 papowell Exp papowell $";


/*************************************************************
 * void Get_debug_parm(int argc, char *argv[], struct keywords *list)
 * Scan the command line for -D debugparms
 *  debugparms has the format value,key=value,key,key@...
 *  1. if the value is an integer,  then we treat it as a value for DEBUG
 *  2. if a key is present,  then we scan the list and find the
 *     match for the key.  We then convert according to the type of
 *     option expected.
 *************************************************************/

#include "lp.h"
#include "errorcodes.h"
#include "getopt.h"
#include "child.h"
/**** ENDINCLUDE ****/

struct keywords debug_vars[]		/* debugging variables */
 = {
#if !defined(NODEBUG)
    { "debug",INTEGER_K,(void *)&Debug },
    { "test",INTEGER_K,(void *)&DbgTest },
    { "job",INTEGER_K,(void *)&DbgJob },
    { "log",FLAG_K,(void *)&DbgFlag,DLOG1, DLOGMASK },
    { "log+1",FLAG_K,(void *)&DbgFlag,DLOG1, DLOGMASK },
    { "log+2",FLAG_K,(void *)&DbgFlag,DLOG2|DLOG1, DLOGMASK },
    { "log+3",FLAG_K,(void *)&DbgFlag,DLOG3|DLOG2|DLOG1, DLOGMASK },
    { "log+4",FLAG_K,(void *)&DbgFlag,DLOG4|DLOG3|DLOG2|DLOG1, DLOGMASK },
    { "network",FLAG_K,(void *)&DbgFlag,DNW1, DNWMASK },
    { "network+1",FLAG_K,(void *)&DbgFlag,DNW1, DNWMASK },
    { "network+2",FLAG_K,(void *)&DbgFlag,DNW2|DNW1, DNWMASK },
    { "network+3",FLAG_K,(void *)&DbgFlag,DNW3|DNW2|DNW1, DNWMASK },
    { "network+4",FLAG_K,(void *)&DbgFlag,DNW4|DNW3|DNW2|DNW1, DNWMASK },
    { "database",FLAG_K,(void *)&DbgFlag,DDB1, DDBMASK },
    { "database+1",FLAG_K,(void *)&DbgFlag,DDB1, DDBMASK },
    { "database+2",FLAG_K,(void *)&DbgFlag,DDB2|DDB1, DDBMASK },
    { "database+3",FLAG_K,(void *)&DbgFlag,DDB3|DDB2|DDB1, DDBMASK },
    { "database+4",FLAG_K,(void *)&DbgFlag,DDB4|DDB3|DDB2|DDB1, DDBMASK },
    { "database+4",FLAG_K,(void *)&DbgFlag,DDB4, DDBMASK },
    { "receive",FLAG_K,(void *)&DbgFlag,DRECV1, DRECVMASK },
    { "receive+1",FLAG_K,(void *)&DbgFlag,DRECV1, DRECVMASK },
    { "receive+2",FLAG_K,(void *)&DbgFlag,DRECV2|DRECV1, DRECVMASK },
    { "receive+3",FLAG_K,(void *)&DbgFlag,DRECV3|DRECV2|DRECV1, DRECVMASK },
    { "receive+4",FLAG_K,(void *)&DbgFlag,DRECV4|DRECV3|DRECV2|DRECV1, DRECVMASK },
    { "control",FLAG_K,(void *)&DbgFlag,DCTRL1, DCTRLMASK },
    { "control+1",FLAG_K,(void *)&DbgFlag,DCTRL1, DCTRLMASK },
    { "control+2",FLAG_K,(void *)&DbgFlag,DCTRL2|DCTRL1, DCTRLMASK },
    { "control+3",FLAG_K,(void *)&DbgFlag,DCTRL3|DCTRL2|DCTRL1, DCTRLMASK },
    { "control+4",FLAG_K,(void *)&DbgFlag,DCTRL4|DCTRL3|DCTRL2|DCTRL1, DCTRLMASK },
    { "lprm",FLAG_K,(void *)&DbgFlag,DLPRM1, DLPRMMASK },
    { "lprm+1",FLAG_K,(void *)&DbgFlag,DLPRM1, DLPRMMASK },
    { "lprm+2",FLAG_K,(void *)&DbgFlag,DLPRM2|DLPRM1, DLPRMMASK },
    { "lprm+3",FLAG_K,(void *)&DbgFlag,DLPRM3|DLPRM2|DLPRM1, DLPRMMASK },
    { "lprm+4",FLAG_K,(void *)&DbgFlag,DLPRM4|DLPRM3|DLPRM2|DLPRM1, DLPRMMASK },
    { "lpq",FLAG_K,(void *)&DbgFlag,DLPQ1, DLPQMASK },
    { "lpq+1",FLAG_K,(void *)&DbgFlag,DLPQ1, DLPQMASK },
    { "lpq+2",FLAG_K,(void *)&DbgFlag,DLPQ2|DLPQ1, DLPQMASK },
    { "lpq+3",FLAG_K,(void *)&DbgFlag,DLPQ3|DLPQ2|DLPQ1, DLPQMASK },
    { "lpq+4",FLAG_K,(void *)&DbgFlag,DLPQ4|DLPQ3|DLPQ2|DLPQ1, DLPQMASK },
#endif
    { (char *)0 }
};

/*

Parse_debug (char *dbgstr, struct keywords *list, int interactive );
Input string:  value,key=value,flag,flag@,...

1. crack the input line at the ','
2. crack each option at = 
3. search for key words
4. assign value to variable

*/

void Parse_debug (char *dbgstr, int interactive )
{
#if !defined(NODEBUG)
	char *key, *convert, *end;
	int i, n, found, count;
	struct keywords *list = debug_vars;
	struct line_list l;

	Init_line_list(&l);
	Split(&l,dbgstr,File_sep,0,0,0,0,0);

	for( count = 0; count < l.count; ++count ){
		found = 0;
		end = key = l.list[count];
		n = strtol(key,&end,0);
		if( *end == 0 ){
			Debug = n;
			if( n == 0 )DbgFlag = 0;
			found = 1;
		} else {
			if( (end = safestrchr(key,'=')) ){
				*end++ = 0;
				n = strtol(end,0,0);
			}
			/* search the keyword list */
			for (i = 0;
				(convert = list[i].keyword) && safestrcasecmp( convert, key );
				++i );
			if( convert != 0 ){
				switch( list[i].type ){
				case INTEGER_K:
					*(int *)list[i].variable = n;
					found = 1;
					break;
				case FLAG_K:
					*(int *)list[i].variable |= list[i].maxval;
					/*
						DEBUG1("Parse_debug: key '%s', val 0x%x, DbgFlag 0x%x",
						key, list[i].maxval, DbgFlag );
					 */
					found = 1;
					break;
				default:
					break;
				}
			}
		}
		if(!found && interactive ){
			int lastflag = 0;
			int nooutput = 0;
		    fprintf (stderr,
	"debug usage: -D [ num | key=num | key=str | flag | flag@ | flag+N ]*\n");
		    fprintf (stderr, "  keys recognized:");
		    for (i = 0; list[i].keyword; i++) {
				if( safestrchr( list[i].keyword, '+' ) ) continue;
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
	Free_line_list(&l);
#endif
	/* logDebug("Parse_debug: Debug %d, DbgFlag 0x%x", Debug, DbgFlag ); */
}
