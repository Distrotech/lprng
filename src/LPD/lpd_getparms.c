/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpr_getparms.c
 * PURPOSE: extract the parameters from the command line for the
 * LPR program
 *
 **************************************************************************/

static char *const _id =
"lpd_getparms.c,v 3.5 1997/10/10 03:04:15 papowell Exp";

#include "lp.h"
#include "patchlevel.h"

void usage(void);
/***************************************************************************
 * void Get_parms(int argc, char *argv[])
 * 1. Scan the argument list and get the flags
 * 2. Check for duplicate information
 ***************************************************************************/

char *msg = N_("\
usage: %s [-FV] [-D dbg] [-L log]\n\
 Options\n\
 -D dbg      - set debug level and flags\n\
                 Example: -D10,remote=5\n\
                 set debug level to 10, remote flag = 5\n\
 -F          - run in foreground, log to stderr\n\
               Example: -D10,remote=5\n\
 -L logfile  - append log information to logfile\n\
 -P printer  - start up single printer (test mode)\n\
 -V          - show version info\n");

void Get_parms(int argc, char *argv[] )
{
	int option;

	while ((option = Getopt (argc, argv, LPD_optstr )) != EOF) {
		switch (option) {
		case 'D': /* debug has already been done */ break;
		case 'L': Logfile = Optarg; break;
		case 'F': Foreground = 1; Logfile = "-"; break;
		case 'P': Printer = Optarg; break;
		default:
			usage();
			break;
		case 'V':
			++Verbose;
			break;
		}
	}
	if( Verbose > 0 ) {
		fprintf( stderr, _("Version %s\n"), PATCHLEVEL );
		if( Verbose > 1 ) Printlist( Copyright, stderr );
		exit(1);
		}
}

void usage(void)
{
	fprintf( stderr, _(msg), Name);
	exit(1);
}
