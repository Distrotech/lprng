/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lprm_getparms.c
 * PURPOSE: extract the parameters from the command line for the
 * LPRM program
 *
 **************************************************************************/

static char *const _id =
"lprm_getparm.c,v 3.6 1997/10/10 03:04:15 papowell Exp";

#include "lp.h"
#include "patchlevel.h"
#include "printcap.h"
/**** ENDINCLUDE ****/

/***************************************************************************
 * void Get_parms(int argc, char *argv[])
 * 1. Scan the argument list and get the flags
 * 2. Check for duplicate information
 ***************************************************************************/

extern char *next_opt;
void usage(void);
char LPRM_optstr[]   /* LPRM options */
 = "aAD:P:V" ;
char CLEAN_optstr[]   /* CLEAN options */
 = "AD:" ;

void Get_parms(int argc, char *argv[] )
{
	int option;
	char *name;


	if( argv[0] && (name = strrchr( argv[0], '/' )) ) {
		++name;
	} else {
		name = argv[0];
	}
	/* check to see if we simulate (poorly) the LP options */
	if( name && strcmp( name, "clean" ) == 0 ){
		Get_debug_parm( argc, argv, CLEAN_optstr, debug_vars );
		LP_mode = 1;
		while ((option = Getopt (argc, argv, CLEAN_optstr )) != EOF)
		switch (option) {
		case 'A': Use_auth_flag = 1; break; /* use authentication */
		case 'D': break; /* debug has already been done */
		default: usage(); break;
		}
		if( argc - Optind > 0 ){
			/* last entry might be printer name */
			name = argv[argc-1];
			if( strcmp( name, "all" ) == 0
				|| Find_printcap_entry( name, 0 ) ){
				Printer = name;
				argv[argc-1] = 0;
				--argc;
			}
		}
	} else {
		Get_debug_parm( argc, argv, LPRM_optstr, debug_vars );
		while ((option = Getopt (argc, argv, LPRM_optstr )) != EOF)
		switch (option) {
		case 'a': All_printers = 1; Printer = "all"; break;
		case 'A': Use_auth_flag = 1; break; /* use authentication */
		case 'D': break; /* debug has already been done */
		case 'V': ++Verbose; break;
		case 'P': Printer = Optarg; break;
		default: usage(); break;
		}
	}
	if( Verbose > 0 ) fprintf( stderr, _("Version %s\n"), PATCHLEVEL );
	if( Verbose > 1 ) Printlist( Copyright, stderr );
}

char *clean_msg = N_("\
usage: %s [-A] [-Ddebuglevel] (jobid|user|'all')* [printer]\n\
  -A           - use authentication\n\
  -Ddebuglevel - debug level\n\
  user           removes user jobs\n\
  all            removes all jobs\n\
  jobid          removes job number jobid\n\
 Example:\n\
    'clean 30 lp' removes job 30 on printer lp\n\
    'clean'       removes first job on default printer\n\
    'clean all'      removes all your jobs on default printer\n\
    'clean all all'  removes all your jobs on all printers\n\
  Note: lprm removes only jobs for which you have removal permission\n");

char *lprm_msg = N_("\
usage: %s [-A] [-a | -Pprinter] [-Ddebuglevel] (jobid|user|'all')*\n\
  -a           - all printers\n\
  -A           - use authentication\n\
  -Pprinter    - printer (default PRINTER environment variable)\n\
  -Ddebuglevel - debug level\n\
  -V           - show version information\n\
  user           removes user jobs\n\
  all            removes all jobs\n\
  jobid          removes job number jobid\n\
 Example:\n\
    'lprm -Plp 30' removes job 30 on printer lp\n\
    'lprm -a'      removes all your jobs on all printers\n\
    'lprm -a all'  removes all jobs on all printers\n\
  Note: lprm removes only jobs for which you have removal permission\n");

void usage(void)
{
	if( LP_mode ){
		fprintf( stderr, _(lprm_msg), Name );
	} else {
		fprintf( stderr, _(clean_msg), Name );
	}
	exit(1);
}
