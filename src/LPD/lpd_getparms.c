/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpr_getparms.c
 * PURPOSE: extract the parameters from the command line for the
 * LPR program
 *
 **************************************************************************/

static char *const _id =
"$Id: lpd_getparms.c,v 3.1 1996/12/28 21:40:01 papowell Exp $";

#include "lp.h"
#include "patchlevel.h"

/***************************************************************************
 * void Get_parms(int argc, char *argv[])
 * 1. Scan the argument list and get the flags
 * 2. Check for duplicate information
 ***************************************************************************/

char *msg[] = {
"usage: %s [-FVci] [-D dbg] [-L log]",
" Options",
" -D dbg      - set debug level and flags",
"                 Example: -D10,remote=5",
"                 set debug level to 10, remote flag = 5",
" -F          - run in foreground, log to stderr",
"               Example: -D10,remote=5",
" -L logfile  - append log information to logfile",
" -P printer  - start up single printer (test mode)",
" -i          - specify if started by INETD",
" -V          - show version info",
0
};


void Get_parms(int argc, char *argv[] )
{
	int option;

	while ((option = Getopt (argc, argv, LPD_optstr )) != EOF) {
		switch (option) {
		case 'D': /* debug has already been done */ break;
		case 'L': Logfile = Optarg; break;
		case 'F': Foreground = 1; Logfile = "-"; break;
		case 'P': Printer = Optarg; break;
		case 'c': Clean = 1; break;
		default:
			Printlist(msg, stderr);
			exit(1);
			break;
		case 'V':
			++Verbose;
			fprintf( stdout, "Version %s\n", PATCHLEVEL );
			if( Verbose > 1 ) Printlist( Copyright, stdout );
			exit(1);
			break;
		}
	}
}
