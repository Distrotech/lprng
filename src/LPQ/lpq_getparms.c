/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpq_getparms.c
 * PURPOSE: extract the parameters from the command line for the
 * LPQ program
 *
 **************************************************************************/

static char *const _id =
"$Id: lpq_getparms.c,v 3.0 1996/05/19 04:05:47 papowell Exp $";

#include "lpq.h"
#include "patchlevel.h"

/***************************************************************************
 * void Get_parms(int argc, char *argv[])
 * 1. Scan the argument list and get the flags
 * 2. Check for duplicate information
 ***************************************************************************/

extern char *next_opt;
void usage();

void Get_parms(int argc, char *argv[] )
{
	int option;

	while ((option = Getopt (argc, argv, LPQ_optstr )) != EOF) {
		switch (option) {
		case 'D': /* debug has already been done */
			break;
		case 'P': if( Optarg == 0 ) usage();
			Printer = Optarg; break;
		case 'V': ++Verbose; break;
		case 'a': Printer = "all"; ++All_printers; break;
		case 'c': Clear_scr = 1; break;
		case 'l': ++Longformat; break;
		case 's': Longformat = 0; break;
		case 't': if( Optarg == 0 ) usage();
			Interval = atoi( Optarg ); break;
		default:
			usage();
		}
	}
	if( Verbose > 0 ) fprintf( stdout, "Version %s\n", PATCHLEVEL );
	if( Verbose > 1 ) Printlist( Copyright, stdout );
}

char *msg[] = {
	"usage: %s [-Ddebuglevel] [-Pprinter] [-V] [-acl] [-tsleeptime]",
	"  -Ddebuglevel - debug level",
	"  -Pprinter    - specify printer",
	"  -V           - print version information",
	"  -a           - all printers",
	"  -c           - clear screen before update",
	"  -l           - increase (lengthen) detailed status information",
	"                 additional l flags add more detail.",
	"  -s           - short (summary) format",
	"  -tsleeptime  - sleeptime between updates",
	0
};

void usage()
{
	Printlist( msg, stderr );
	exit(1);
}
