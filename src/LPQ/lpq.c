/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
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
#include "permission.h"
#include "linksupport.h"
/**** ENDINCLUDE ****/

static char *const _id =
"lpq.c,v 3.20 1998/03/24 02:43:22 papowell Exp";



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
	int i, lenp, lenr;
	struct stat statb;
	char **list;
	char orig_name[LINEBUFFER];

	/*
	 * set up the user state
	 */
	Interactive = 1;
	Longformat = 1;
	Displayformat = REQ_DLONG;
	orig_name[0] = 0;
	Initialize(argc, argv, envp);


	/* set signal handlers */
	(void) plp_signal (SIGHUP, cleanup_HUP);
	(void) plp_signal (SIGINT, cleanup_INT);
	(void) plp_signal (SIGQUIT, cleanup_QUIT);
	(void) plp_signal (SIGTERM, cleanup_TERM);



	Get_parms(argc, argv);      /* scan input args */
	if( Printer ){
		safestrncpy( orig_name, Printer );
	}

	/* set up configuration */
	Setup_configuration();

	/* fake the lpstat information */
	if( Lp_sched ){
		if( Write_fd_str( 1, _("scheduler is running\n") ) < 0 ) cleanup(0);
	}
	/* check to see if you have the default */
	if( Lp_default ){
		Get_printer(0);
		plp_snprintf( msg, sizeof(msg),
		_("system default destination: %s\n"), Printer );
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
	if( All_printers || (Printer && strcmp(Printer,"all") == 0 ) ){
		if( All_list.count == 0 ){
			Get_all_printcap_entries();
			Printer = "all";
			Get_printer(0);
		}
		if(DEBUGL0){
			logDebug("lpq: All_list.count %d", All_list.count );
			list = All_list.list;
			for( i = 0; i < All_list.count; ++i ){
				DEBUG0("lpq: printer[%d] = '%s'", i, list[i] );
			}
		}
	} else {
		if( *orig_name ) Printer = orig_name;
		DEBUG0(
"lpq: before Get_printer: Printer '%s', RemotePrinter '%s', RemoteHost '%s'",
			Printer, RemotePrinter, RemoteHost );
		Get_printer(0);
		if( *orig_name == 0 ) safestrncpy( orig_name, Printer );
	}
	DEBUG0("lpq: Printer %s, RemotePrinter %s, RemoteHost %s",
		Printer, RemotePrinter, RemoteHost );
	if( RemoteHost == 0 ){
		RemoteHost = Default_remote_host;
		if( RemoteHost == 0 ){
			RemoteHost = FQDNHost;
		}
	}
	if( RemotePrinter == 0 ){
		RemotePrinter = Printer;
	}
	if( RemoteHost == 0 ){
		Diemsg( _("No remote host specified") );
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
		Status_line_count = (1 << (Longformat-1));
	}
	do {
		if( Clear_scr ) Term_clear();
		Pr_status_check( 0 );
		if( All_printers && All_list.count ){
			list = All_list.list;
			for( i = 0; i < All_list.count; ++i ){
				RemoteHost = RemotePrinter = Lp_device = 0;
				Printer = list[i];
				DEBUG0("LPQ: Printer [%d of %d] '%s'",
					i, All_list.count, Printer );
				safestrncpy( orig_name, Printer );
				if( strchr( Printer, '@' ) ){
					Lp_device = Printer;
					Check_remotehost();
				} else if( Force_localhost ){
					RemoteHost = Localhost;
				}
				if( RemoteHost == 0 || *RemoteHost == 0){
					if( Default_remote_host && *Default_remote_host ){
						RemoteHost = Default_remote_host;
					} else if( FQDNHost && *FQDNHost ){
						RemoteHost = FQDNHost;
					}
				}
				if( LP_mode == 0 && Displayformat != REQ_DSHORT
					&& RemotePrinter ){
					plp_snprintf(msg,sizeof(msg),"%s@%s",
						RemotePrinter,RemoteHost);
					lenp = strlen( Printer );
					lenr = strlen( RemoteHost );
					if( lenp > lenr ) lenp = lenr;
					if( strncmp(Printer, msg, lenp) ){
						plp_snprintf( msg, sizeof(msg), _("Printer: %s is %s@%s\n"),
							Printer, RemotePrinter, RemoteHost );
						DEBUG0("lpq: '%s'",msg);
						if(  Write_fd_str( 1, msg ) < 0 ) cleanup(0);
					}
				}
				if( RemotePrinter && RemotePrinter[0] == 0 ) RemotePrinter = 0;
				if( Check_for_rg_group( Logname ) ){
					fprintf( stderr,
						"cannot use printer - not in privileged group\n" );
					continue;
				}
				if (LP_mode && Lp_summary && Longformat) { /* lpstat -v */
					if (RemotePrinter)
					  plp_snprintf(msg,sizeof(msg),_("system for %s: %s\n"), Printer, RemoteHost);
					else
					  plp_snprintf(msg,sizeof(msg),_("device for %s: /dev/null\n"), Printer);
					if(  Write_fd_str( 1, msg ) < 0 ) cleanup(0);
				} else
				Send_lpqrequest(
					RemotePrinter?RemotePrinter:Printer,
					RemoteHost, Displayformat, &argv[Optind],
					Connect_timeout, Send_query_rw_timeout, 1 );
				/* if(  Write_fd_str( 1, "\n" ) < 0 ) cleanup(0); */
			}
		} else {
			DEBUG0("lpq: remoteprinter '%s', remote host '%s'",
				RemotePrinter, RemoteHost );
			if( Check_for_rg_group( Logname ) ){
				fprintf( stderr,
					"cannot use printer - not in privileged group\n" );
				break;
			}
			if( LP_mode == 0 && Displayformat != REQ_DSHORT
				&& RemotePrinter){
				lenp = strlen( RemotePrinter );
				lenr = strlen( orig_name );
				if( lenp > lenr ) lenp = lenr;
				if( strncmp(RemotePrinter,orig_name,lenp)){
				plp_snprintf( msg, sizeof(msg), _("Printer: %s is %s@%s\n"),
					Printer, RemotePrinter, RemoteHost );
				if(  Write_fd_str( 1, msg ) < 0 ) cleanup(0);
				}
			}
			if (LP_mode && Lp_summary && Longformat) { /* lpstat -v */
				lenp = strlen( RemotePrinter );
				lenr = strlen( orig_name );
				if( lenp > lenr ) lenp = lenr;
				if (RemotePrinter && strncmp(RemotePrinter,orig_name, lenp))
				  plp_snprintf(msg,sizeof(msg),_("system for %s: %s\n"), orig_name, RemoteHost);
				else
				  plp_snprintf(msg,sizeof(msg),_("device for %s: /dev/null\n"), Printer);
				if(  Write_fd_str( 1, msg ) < 0 ) cleanup(0);
			} else
			Send_lpqrequest(
				RemotePrinter?RemotePrinter:Printer,
				RemoteHost, Displayformat, &argv[Optind],
				Connect_timeout, Send_query_rw_timeout, 1 );
			/* if(  Write_fd_str( 1, "\n" ) < 0 ) cleanup(0); */
		}
		Remove_tempfiles();
		if( Interval > 0 ){
			plp_sleep( Interval );
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
	char *host;
	char *name;
	int allflag = 0;

	DEBUG0("Extract_pr: count %d", list->count );
	if( list->count+1 >= all->max ){
		extend_malloc_list( all, sizeof( arg_list[0] ), list->count+1,__FILE__,__LINE__ );
	}
	arg_list = list->list;
	all_list = all->list;
	for( i = j = k = 0; i < list->count; ++i ){
		arg_list[j] = name = arg_list[i];
		DEBUG0("Extract_pr: checking '%s'", name );
		if( (host = strchr(name,'@')) ){
			if( strchr(host,'+') ){
				host = 0;
			}
		}
		if( strcmp( name, "all" ) == 0 ){
			allflag = 1;
		} else if( host || Find_printcap_entry( name, 0 ) ){
			DEBUG0("Extract_pr: printer '%s'", name );
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
	if(DEBUGL0){
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
