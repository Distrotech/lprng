/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
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
"$Id: lpq_getparms.c,v 3.5 1997/01/22 15:13:27 papowell Exp $";

#include "lp.h"
#include "patchlevel.h"
#include "malloclist.h"
/**** ENDINCLUDE ****/

static void Add_to( struct malloc_list *val, char *arg )
{
	char *s, *end;
	char **list;

	if( val->count + 2 >= val->max ){
		extend_malloc_list( val, sizeof( list[0] ), 100 );
	}
	list = val->list;
	if( arg && *arg ){
		arg = safestrdup( arg );
		for( s = arg; s && *s; s = end ){
			while( isspace( *s ) ) *s++ = 0;
			if( *s == 0 ) break;
			end = strpbrk( s, " \t," );
			if( end ){
				*end++ = 0;
			}
			if( val->count + 2 >= val->max ){
				extend_malloc_list( val, sizeof( list[0] ), 100 );
				list = val->list;
			}
			list[val->count++] = s;
		}
	}
	list[val->count] = 0;
}

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
	char *name;

	if( argv[0] && (name = strrchr( argv[0], '/' )) ) {
		++name;
	} else {
		name = argv[0];
	}
	/* check to see if we simulate (poorly) the LP options */
	if( name && strcmp( name, "lpstat" ) == 0 ){
		LP_mode = 1;
		while ((option = Getopt (argc, argv,
			"AdrRsta:c:f:lo:p:D:P:S:u:v" )) != EOF)
		switch(option){
		case 'a': /* option a */
			Lp_status = 1;
			Lp_accepting = 1;
			if( Optarg == 0 ){
				Add_to( &Lp_pr_list, "all" );
			} else {
				Add_to( &Lp_pr_list, Optarg );
			}
			break;
		case 'A': /* option A */
			Use_auth_flag = 1;
			break;
		case 'c': /* option c */
			Lp_status = 1;
			if( Optarg == 0 ){
				Add_to( &Lp_pr_list, "all" );
			} else {
				Add_to( &Lp_pr_list, Optarg );
			}
			break;
		case 'd': /* option d */
			Lp_default = 1;
			break;
		case 'D': /* option D */
			Lp_getlist = 0;
			break;
		case 'f': /* option f - ignored */
			break;
		case 'l': /* option l - ignored */
			break;
		case 'o': /* option o */
			Lp_getlist = 1;
			Lp_showjobs = 1;
			Lp_status = 1;
			if( Optarg == 0 ){
				Add_to( &Lp_pr_list, "all" );
			} else {
				Add_to( &Lp_pr_list, Optarg );
			}
			break;
		case 'p': /* option p */
			Lp_getlist = 1;
			Lp_status = 1;
			if( Optarg == 0 ){
				Add_to( &Lp_pr_list, "all" );
			} else {
				Add_to( &Lp_pr_list, Optarg );
			}
			break;
		case 'P': /* option P */
			Lp_getlist = 0;
			break;
		case 'r': /* option r */
			Lp_sched = 1;
			Lp_getlist = 0;
			break;
		case 'R': /* option R */
			Lp_showjobs = 1;
			break;
		case 's': /* option s */
			All_printers = 1;
			Lp_summary = 1;
			Lp_status = 1;
			Longformat = 0;
			break;
		case 't': /* option t */
			All_printers = 1;
			Lp_sched = 1;
			Lp_default = 1;
			Lp_status = 1;
			Lp_showjobs = 1;
			break;
		case 'S': /* option S - ignored */
			Lp_getlist = 1;
			break;
		case 'u': /* option u */
			Lp_getlist = 1;
			Lp_status = 1;
			if( Optarg == 0 ){
				Add_to( &Lp_pr_list, "all" );
			} else {
				Add_to( &Lp_pr_list, Optarg );
			}
			break;
		case 'v': /* option v */
			Lp_summary = 1;
			Lp_status = 1;
			break;
		default: usage(); break;
		}
		if( Optind == argc ){
			Lp_status = 1;
			name = getenv( "USER" );
			if( name == 0 ){
				Diemsg( "USER environment variable not set");
			}
			Add_to( &Lp_pr_list, name );
		}
		while( Optind < argc ){
			Lp_status = 1;
			Add_to( &Lp_pr_list, argv[Optind++] );
		}
		if( DEBUGL0 ){
			int i;
			char **list = Lp_pr_list.list;
		
			logDebug( "LP_mode %d", LP_mode );
			logDebug(
"Lp_sched %d, Lp_default %d, Lp_status %d, Lp_showjobs %d, Lp_accepting %d",
			Lp_sched, Lp_default, Lp_status, Lp_showjobs, Lp_accepting );
			logDebug( "Lp_pr_list.count %d", Lp_pr_list.count );
			for( i = 0; i < Lp_pr_list.count; ++i ){
				logDebug( "[%d] '%s'\n", i, list[i] );
			}
		}
	} else while ((option = Getopt (argc, argv, LPQ_optstr )) != EOF) {
		switch (option) {
		case 'A': Use_auth_flag = 1; /* use authentication */
			break;
		case 'D': /* debug has already been done */
			break;
		case 'P': if( Optarg == 0 ) usage();
			Printer = Optarg;
			Orig_printer = Optarg;
			break;
		case 'V': ++Verbose; break;
		case 'a': Printer = "all"; ++All_printers; break;
		case 'c': Clear_scr = 1; break;
		case 'l': ++Longformat; break;
		case 's': Longformat = 0;
					Displayformat = REQ_DSHORT;
					break;
		case 't': if( Optarg == 0 ) usage();
					Interval = atoi( Optarg );
					break;
		case 'v': Longformat = 0; Displayformat = REQ_VERBOSE; break;
		default:
			usage();
		}
	}
	if( Verbose > 0 ) fprintf( stdout, "Version %s\n", PATCHLEVEL );
	if( Verbose > 1 ) Printlist( Copyright, stdout );
}

char *lpq_msg[] = {
	"usage: %s [-aAclV] [-Ddebuglevel] [-Pprinter] [-tsleeptime]",
	"  -a           - all printers",
	"  -A           - use authentication",
	"  -c           - clear screen before update",
	"  -l           - increase (lengthen) detailed status information",
	"                 additional l flags add more detail.",
	"  -Ddebuglevel - debug level",
	"  -Pprinter    - specify printer",
	"  -s           - short (summary) format",
	"  -tsleeptime  - sleeptime between updates",
	"  -V           - print version information",
	0
};

char *lpstat_msg[] = {
"usage: %s [-d] [-r] [-R] [-s] [-t] [-a [list]]",
"  [-c [list]] [-f [list] [-l]] [-o [list]]",
"  [-p [list]] [-P] [-S [list]] [list]",
"  [-u [login-ID-list]] [-v [list]]",
" list is a list of print queues",
" -a [list] destination status *",
" -c [list] class status *",
" -f [list] forms status *",
" -o [list] job or printer status *",
" -p [list] printer status *",
" -P        paper types - ignored",
" -r        scheduler status",
" -s        summary status information - short format",
" -S [list] character set - ignored",
" -t        all status information - long format",
" -u [joblist] job status information",
" -v [list] printer mapping *",
" * - long status format produced",
0
};

void usage(void)
{
	if( LP_mode ){
		Printlist( lpstat_msg, stderr );
	} else {
		Printlist( lpq_msg, stderr );
	}
	exit(1);
}
