/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lprm_getparms.c
 * PURPOSE: extract the parameters from the command line for the
 * LPRM program
 *
 **************************************************************************/

static char *const _id =
"$Id: lprm_getparm.c,v 3.0 1996/05/19 04:05:53 papowell Exp $";

#include "lprm.h"

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

	while ((option = Getopt (argc, argv, LPRM_optstr )) != EOF) {
		switch (option) {
		case 'D': /* debug has already been done */
			break;
		case 'V': ++Verbose; break;
		case 'P': Printer = Optarg; break;
		case 'a': All_printers = 1; break;
		default:
			usage();
		}
	}
	if( Verbose > 1 ) Printlist( Copyright, stderr );
}

char *msg[] = {
	"usage: %s [-a | -Pprinter] [-Ddebuglevel] (jobid|user|'all')*",
	"  -a           - all printers",
	"  -Pprinter    - printer (default PRINTER environment variable)",
	"  -Ddebuglevel - debug level",
	"  -V           - show version information",
	"  user           removes user jobs",
	"  all            removes all jobs",
	"  jobid          removes job number jobid",
	" Example:",
    "    'lprm -Plp 30' removes job 30 on printer lp",
	"    'lprm -a'      removes all your jobs on all printers",
	"    'lprm -a all'  removes all jobs on all printers",
	"  Note: lprm removes only jobs for which you have removal permission",
	(char *)0
};

void usage()
{
	Printlist(msg, stderr);
	exit(1);
}
