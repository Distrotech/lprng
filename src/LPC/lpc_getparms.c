/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpc_getparms.c
 * PURPOSE: extract the parameters from the command line for the
 * LPC program
 *
 **************************************************************************/

static char *const _id =
"$Id: lpc_getparms.c,v 3.1 1996/12/28 21:40:00 papowell Exp $";

#include "lp.h"
#include "patchlevel.h"

/***************************************************************************
 * void Get_parms(int argc, char *argv[])
 * 1. Scan the argument list and get the flags
 * 2. Check for duplicate information
 ***************************************************************************/

extern char *next_opt;
void usage(void);

void Get_parms(int argc, char *argv[] )
{
	int option;

	while ((option = Getopt (argc, argv, LPC_optstr )) != EOF) {
		switch (option) {
		case 'A': Use_auth_flag = 1; /* use authentication */
			break;
		case 'D': /* debug has already been done */
			break;
		case 'P': if( Optarg == 0 ) usage();
			Printer = Optarg; break;
		case 'V':
			++Verbose;
			break;
		default:
			usage();
		}
	}
	if( Verbose > 0 ) fprintf( stdout, "Version %s\n", PATCHLEVEL );
	if( Verbose > 1 ) Printlist( Copyright, stdout );
}

char *msg[] = {
	"usage: %s [-A] [-Ddebuglevel] [-Pprinter] [-V] [command]",
	" with no commands, reads from stdin"
	"  -A           - use authentication",
	"  -Pprinter    - specify printer",
	"  -V           - increase information verbosity",
	"  -Ddebuglevel - debug level",
	" commands:",
	" abort   (printer[@host] | all)  - stop server",
	" disable (printer[@host] | all)  - disable queueing",
	" debug   (printer[@host] | all) debugparms - set debug level for printer",
	" enable  (printer[@host] | all)  - enable  queueing",
	" hold    (printer[@host] | all) (name[@host] | job | all)* - hold job",
	" holdall (printer[@host] | all)  - hold all jobs on",
	" kill    (printer[@host] | all)  - stop and restart server",
	" lpd [HUP]  - get LPD PID, signal it to reread printcap and configuration",
	" lpq (printer[@host] | all) (name[@host] | job | all)*     - invoke LPQ",
	" lprm (printer[@host] | all) (name[@host]|host|job| all)*  - invoke LPRM",
	" move printer (user|jobid)* target - move jobs to new queue",
	" noholdall (printer[@host] | all)  - hold all jobs off",
	" printcap (printer[@host] | all) - report printcap values",
	" quit                            - exit LPC",
	" redirect (printer[@host] | all) (printer@host | off )*    - redirect jobs",
	" release  (printer[@host] | all) (name[@host] | job | all)* - release job",
	" reread                          - LPD reread database information",
	" start   (printer[@host] | all)  - start printing",
	" status  (printer[@host] | all)  - status of printers",
	" stop    (printer[@host] | all)  - stop  printing",
	" topq    (printer[@host] | all) (name[@host] | job | all)* - reorder job",
	0
};

void usage(void)
{
	Printlist(msg, stderr);
	exit(1);
}

void use_msg(void)
{
	Printlist(msg, stdout);
	fflush(stdout);
}
