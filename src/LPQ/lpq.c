/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpq.c
 * PURPOSE:
 **************************************************************************/

/***************************************************************************
 * SYNOPSIS
 *      lpq [ -PPrinter ]
 *    lpq [-Pprinter ]*[-a][-s][-l][+[n]][-Ddebugopt][job#][user]
 * DESCRIPTION
 *   lpq sends a status request to lpd(8)
 *   and reports the status of the
 *   specified jobs or all  jobs  associated  with  a  user.  lpq
 *   invoked  without  any arguments reports on the printer given
 *   by the default printer (see -P option).  For each  job  sub-
 *   mitted  (i.e.  invocation  of lpr(1)) lpq reports the user's
 *   name, current rank in the queue, the names of files compris-
 *   ing  the job, the job identifier (a number which may be sup-
 *   plied to lprm(1) for removing a specific job), and the total
 *   size  in  bytes.  Job ordering is dependent on the algorithm
 *   used to scan the spooling directory and is  FIFO  (First  in
 *   First Out), in order of priority level.  File names compris-
 *   ing a job may be unavailable (when lpr(1) is used as a  sink
 *   in  a  pipeline)  in  which  case  the  file is indicated as
 *   ``(stdin)''.
 *    -P printer
 *         Specifies a particular printer, otherwise  the  default
 *         line printer is used (or the value of the PRINTER vari-
 *         able in the environment).  If PRINTER is  not  defined,
 *         then  the  first  entry in the /etc/printcap(5) file is
 *         reported.  Multiple printers can be displayed by speci-
 *         fying more than one -P option.
 *
 *   -a   All printers listed in the  /etc/printcap(5)  file  are
 *        reported.
 *
 *   -l   An alternate  display  format  is  used,  which  simply
 *        reports the user, jobnumber, and originating host.
 *
 *   [+[n]]
 *        Forces lpq to periodically display  the  spool  queues.
 *        Supplying  a  number immediately after the + sign indi-
 *        cates that lpq should sleep n seconds in between  scans
 *        of the queue.
 *        Note: the screen will be cleared at the start of each
 *        display using the 'curses.h' package.
 ****************************************************************************
 *
Implementation Notes
Patrick Powell Tue May  2 09:58:29 PDT 1995

The LPD server will be returning the formatted status;
The format can be the following:

SHORT:
Warning: lp is down: lp is ready and printing
Warning: no daemon present
Rank   Owner      Job  Files                                 Total Size
active root       30   standard input                        5 bytes
2nd    root       31   standard input                        5 bytes

LONG:

Warning: lp is down: lp is ready and printing
Warning: no daemon present

root: 1st                                [job 030taco]
        standard input                   5 bytes

root: 2nd                                [job 031taco]
        standard input                   5 bytes

*/

#include "lp.h"
#include "printcap.h"
#include "checkremote.h"
#include "initialize.h"
#include "killchild.h"
#include "termclear.h"
#include "sendlpq.h"
#include "getprinter.h"
#include "fileopen.h"
#include "malloclist.h"
#include "readstatus.h"
/**** ENDINCLUDE ****/

static char *const _id =
"$Id: lpq.c,v 3.6 1997/01/30 21:15:20 papowell Exp $";


char LPQ_optstr[]    /* LPQ options */
 = "AD:P:Vaclst:v" ;

static void Extract_pr( struct malloc_list *list, struct malloc_list *all );

#define MAX_SHORT_STATUS 6

/***************************************************************************
 * main()
 * - top level of LPQ
 *
 ****************************************************************************/

int main(int argc, char *argv[], char *envp[])
{
	char msg[LINEBUFFER];
	int i;
	struct stat statb;
	char **list;
	char orig_name[LINEBUFFER];
	struct printcap_entry *printcap_entry = 0;

	/*
	 * set up the user state
	 */
	Interactive = 1;
	Longformat = 1;
	Displayformat = REQ_DLONG;
	orig_name[0] = 0;
	Initialize();


	/* set signal handlers */
	(void) plp_signal (SIGPIPE, cleanup);
	(void) plp_signal (SIGHUP, cleanup);
	(void) plp_signal (SIGINT, cleanup);
	(void) plp_signal (SIGQUIT, cleanup);
	(void) plp_signal (SIGTERM, cleanup);



	/* scan the argument list for a 'Debug' value */
	Get_debug_parm( argc, argv, 0, debug_vars );
	/* scan the input arguments, setting up values */
	Get_parms(argc, argv);      /* scan input args */
	if( Printer ){
		safestrncpy( orig_name, Printer );
	}

	/* set up configuration */
	Setup_configuration();

	/* fake the lpstat information */
	if( Lp_sched ){
		if( Write_fd_str( 1, "scheduler is running\n" ) < 0 ) cleanup(0);
	}
	/* check to see if you have the default */
	if( Lp_default ){
		Get_printer( &printcap_entry );
		plp_snprintf( msg, sizeof(msg),
		"system default destination: %s\n", Printer );
		if(  Write_fd_str( 1, msg ) < 0 ) cleanup(0);
	}
	if( LP_mode && (Lp_accepting == 0)
		&& (Lp_status == 0) && (Lp_getlist == 0) ){
		DEBUG0( "LPmode %d, Lp_accepting %d, Lp_status %d",
			LP_mode, Lp_accepting, Lp_status );
		cleanup(0);
	}

	if( Lp_pr_list.count ){
		Extract_pr( &Lp_pr_list, &All_list );
		if( All_list.count ){
			All_printers = 1;
		}
		Optind = 0;
		argv = Lp_pr_list.list;
	}
	/* now force the printer list */
	if( All_printers ){
		if( All_list.count == 0 ){
			Printer = "all";
			Get_printer(&printcap_entry);
		}
	} else {
		if( *orig_name ) Printer = orig_name;
		Get_printer(&printcap_entry);
		if( *orig_name == 0 ) safestrncpy( orig_name, Printer );
	}
	if( RemoteHost == 0 || *RemoteHost == 0 ){
		RemoteHost = 0;
		if( Default_remote_host && *Default_remote_host ){
			RemoteHost = Default_remote_host;
		} else if( FQDNHost && *FQDNHost ){
			RemoteHost = FQDNHost;
		}
	}
	if( RemoteHost == 0 ){
		Diemsg( "No remote host specified" );
	}
	if( RemotePrinter == 0 || *RemotePrinter == 0 ){
		RemotePrinter = Printer;
	}

	DEBUG0("lpq: Printer '%s', All_printers %d, All_list.count %d",
		Printer, All_printers, All_list.count );
	/* we do the individual printers */
	if( LP_mode ){
		Clear_scr = 0;
		Interval = 0;
	}
	if( Longformat && (Longformat < MAX_SHORT_STATUS )
		&& Displayformat != REQ_VERBOSE ){
		Max_status_lines = (1 << (Longformat-1));
	}
	do {
		if( Clear_scr ) Term_clear();
		Pr_status_check( 0 );
		if( All_printers && All_list.count ){
			list = All_list.list;
			for( i = 0; i < All_list.count; ++i ){
				RemoteHost = RemotePrinter = 0;
				Printer = Lp_device = list[i];
				DEBUG0("LPQ: Printer [%d of %d] '%s'",
					i, All_list.count, Printer );
				safestrncpy( orig_name, Printer );
				Check_remotehost(1);
				if( RemoteHost == 0 || *RemoteHost == 0 ){
					if( Default_remote_host && *Default_remote_host ){
						RemoteHost = Default_remote_host;
					} else if( FQDNHost && *FQDNHost ){
						RemoteHost = FQDNHost;
					}
				}
				if( LP_mode == 0 && Displayformat != REQ_DSHORT
					&& RemotePrinter && strcmp(RemotePrinter,orig_name)){
					plp_snprintf( msg, sizeof(msg), "Printer: %s is %s@%s\n",
						Printer, RemotePrinter, RemoteHost );
					DEBUG0("Do_all_printers: '%s'",msg);
					/* write as long as you can */
					if(  Write_fd_str( 1, msg ) < 0 ) cleanup(0);
				}
				Send_lpqrequest(
					RemotePrinter?RemotePrinter:Printer,
					RemoteHost, Displayformat, &argv[Optind],
					Connect_timeout, Send_timeout, 1 );
				/* if(  Write_fd_str( 1, "\n" ) < 0 ) cleanup(0); */
			}
		} else {
			DEBUG0("lpq: remoteprinter '%s', remote host '%s'",
				RemotePrinter?RemotePrinter:"NULL STRING", RemoteHost );
			if( LP_mode == 0 && Displayformat != REQ_DSHORT
				&& RemotePrinter && strcmp(RemotePrinter,orig_name)){
				plp_snprintf( msg, sizeof(msg), "Printer: %s is %s@%s\n",
					Printer, RemotePrinter, RemoteHost );
				if(  Write_fd_str( 1, msg ) < 0 ) cleanup(0);
			}
			Send_lpqrequest(
				RemotePrinter?RemotePrinter:Printer,
				RemoteHost, Displayformat, &argv[Optind],
				Connect_timeout, Send_timeout, 1 );
			/* if(  Write_fd_str( 1, "\n" ) < 0 ) cleanup(0); */
		}
		Remove_tempfiles();
		if( Interval > 0 ){
			sleep( Interval );
		}
		/* we check to make sure that nobody killed the output */
	} while( Interval > 0 && (fstat( 1, &statb ) == 0) );
	if( Clear_scr ){
		Term_finish();
	}
	Errorcode = 0;
	cleanup(0);
	return(0);
}


/***************************************************************************
 * Extract_pr - extract the list of printers out of the list
 * or arguments. Put the non-printer values back in the list
 * and put the printers in the all list.
 ***************************************************************************/
static void Extract_pr( struct malloc_list *list, struct malloc_list *all )
{
	char **arg_list;
	char **all_list;
	int i, j, k;
	char *name;
	int allflag = 0;

	if( list->count+1 >= all->max ){
		extend_malloc_list( all, sizeof( arg_list[0] ), list->count+1 );
	}
	arg_list = list->list;
	all_list = all->list;
	for( i = j = k = 0; i < list->count; ++i ){
		arg_list[j] = name = arg_list[i];
		if( strcmp( name, "all" ) == 0 ) allflag = 1;
		if( Find_printcap_entry( name, 0 ) ){
			all_list[all->count++] = name;
		} else {
			++j;
		}
	}
	list->count = j;
	arg_list[j] = 0;
	if( allflag ){
		All_printers = 1;
		all->count = 0;
	}
	all_list[all->count] = 0;
	if(DEBUGL0 ){
		logDebug("Extract_pr: allflag %d, arg_list %d entries",
			allflag, list->count );
		arg_list = list->list;
		for( i = 0; arg_list[i]; ++i ){
			logDebug( " [%d] '%s'", i, arg_list[i] );
		}
		logDebug("Extract_pr: all_list 0x%x %d entries",
			all->list, all->count );
		arg_list = all->list;
		for( i = 0; arg_list[i]; ++i ){
			logDebug( " [%d] '%s'", i, arg_list[i] );
		}
	}
	DEBUG0( "Extract_pr: done" );
}
