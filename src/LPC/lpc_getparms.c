/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpc_getparms.c
 * PURPOSE: extract the parameters from the command line for the
 * LPC program
 *
 **************************************************************************/

static char *const _id =
"lpc_getparms.c,v 3.9 1997/12/24 20:10:12 papowell Exp";

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
	if( Verbose > 0 ) {
		fprintf( stderr, _("Version %s\n"), PATCHLEVEL );
		if( Verbose > 1 ) Printlist( Copyright, stderr );
		}
}

char *msg = N_("\
usage: %s [-A] [-Ddebuglevel] [-Pprinter] [-V] [command]\n\
 with no commands, reads from stdin\n\
  -A           - use authentication\n\
  -Pprinter    - specify printer\n\
  -V           - increase information verbosity\n\
  -Ddebuglevel - debug level\n\
 commands:\n\
 active    (printer[@host])        - check for active server\n\
 abort     (printer[@host] | all)  - stop server\n\
 class     printer[@host] (class | off)      - show/set class printing\n\
 disable   (printer[@host] | all)  - disable queueing\n\
 debug     (printer[@host] | all) debugparms - set debug level for printer\n\
 enable    (printer[@host] | all)  - enable  queueing\n\
 hold      (printer[@host] | all) (name[@host] | job | all)*   - hold job\n\
 holdall   (printer[@host] | all)  - hold all jobs on\n\
 kill      (printer[@host] | all)  - stop and restart server\n\
 lpd       (printer[@host]) - get LPD PID \n\
 lpq       (printer[@host] | all) (name[@host] | job | all)*   - invoke LPQ\n\
 lprm      (printer[@host] | all) (name[@host]|host|job| all)* - invoke LPRM\n\
 move printer (user|jobid)* target - move jobs to new queue\n\
 noholdall (printer[@host] | all)  - hold all jobs off\n\
 printcap  (printer[@host] | all)  - report printcap values\n\
 quit                              - exit LPC\n\
 redirect  (printer[@host] | all) (printer@host | off )*       - redirect jobs\n\
 redo      (printer[@host] | all) (name[@host] | job | all)*   - release job\n\
 release   (printer[@host] | all) (name[@host] | job | all)*   - release job\n\
 reread    (printer[@host])        - LPD reread database information\n\
 start     (printer[@host] | all)  - start printing\n\
 status    (printer[@host] | all)  - status of printers\n\
 stop      (printer[@host] | all)  - stop  printing\n\
 topq      (printer[@host] | all) (name[@host] | job | all)*   - reorder job\n\
 defaultq                         - default queue for LPD server\n");

void usage(void)
{
	fprintf( stderr, _(msg), Name );
	exit(1);
}

void use_msg(void)
{
	fputs (_(msg), stdout);
	fflush(stdout);
}
