/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
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

#include "lpq.h"
#include "printcap.h"
#include "setuid.h"
#include "lp_config.h"
#include "termclear.h"
#include "getprinter.h"

static char *const _id =
"$Id: lpq.c,v 3.2 1996/08/31 21:11:58 papowell Exp papowell $";

/***************************************************************************
 * main()
 * - top level of LPP Lite.
 *
 ****************************************************************************/

static struct malloc_list list;

int main(int argc, char *argv[], char *envp[])
{
	char *s, *end;
	char msg[LINEBUFFER];
	int i, c;
	struct stat statb;
	static char *alist;

	/*
	 * set up the user state
	 */
	Interactive = 1;
	Longformat = 1;
	Initialize();


	/* set signal handlers */
	(void) plp_signal (SIGPIPE, cleanup);
	(void) plp_signal (SIGHUP, cleanup);
	(void) plp_signal (SIGINT, cleanup);
	(void) plp_signal (SIGQUIT, cleanup);
	(void) plp_signal (SIGTERM, cleanup);


	/* scan the argument list for a 'Debug' value */

	Opterr = 0;
	Get_debug_parm( argc, argv, LPQ_optstr, debug_vars );
	Opterr = 1;

	/* get the configuration file information if there is any */
	Parsebuffer( "default configuration", Default_configuration,
		lpq_config, &Config_buffers );

    if( Allow_getenv ){
		if( UID_root ){
			fprintf( stderr,
			"%s: WARNING- LPD_CONF environment variable option enabled\n"
			"  and running as root!  You have an exposed security breach!\n"
			"  Recompile without -DGETENV or do not run clients as ROOT\n",
			Name);
		}
		if( (s = getenv( "LPD_CONF" )) ){
			Client_config_file = s;
		}
    }

	DEBUG0("main: Configuration file '%s'", Client_config_file?Client_config_file:"NULL" );

	/* Get configuration file information */
	Getconfig( Client_config_file, lpq_config, &Config_buffers );

	if( Debug > 5 ) dump_config_list( "LPQ Configuration", lpq_config );


	/* get the fully qualified domain name of host and the
		short host name as well
		FQDN - fully qualified domain name
		Host - actual one to use in H fields
		ShortHost - short host name
		NOTE: on PCs this will be the IP address
	*/

	Get_local_host();

	/* expand the information in the configuration file */
	Expandconfig( lpq_config, &Config_buffers );

	if( Debug > 4 ) dump_config_list( "LPQ Configuration After Expansion",
		lpq_config );

	/* scan the input arguments, setting up values */
	Get_parms(argc, argv);      /* scan input args */

	/* now look for the printcap entry */
	if( All_printers ){
		Printer = "all";
	}
	Get_printer();
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

	if( All_list && *All_list ){
		alist = safestrdup( All_list );
		for( s = alist; s && *s; s = end ){
			end = strpbrk( s, ", \t" );
			if( end ){
				*end++ = 0;
			}
			while( (c = *s) && isspace(c) ) ++s;
			if( c == 0 ) continue;
			if( list.count >= list.max ){
				extend_malloc_list( &list, sizeof( s ), 100 );
			}
			list.list[list.count++] = s;
		}
		if( list.count >= list.max ){
			extend_malloc_list( &list, sizeof( s ), 100 );
		}
		list.list[list.count] = 0;
	}

	/* we do the individual printers */
	do {
		if( Clear_scr ) Term_clear();
		if( alist ){
			if(Debug>2){
				logDebug("Do_all_printers: all = '%s'", All_list );
				for( i = 0; i < list.count; ++i ){
					logDebug(" [%d] %s", i, list.list[i]);
				}
			}
			for( i = 0; i < list.count; ++i ){
				RemoteHost = RemotePrinter = 0;
				Printer = Lp_device = list.list[i];
				DEBUG4("Do_all_printers: Printer [%d of %d] '%s'",
					i, list.count, Printer );
				Check_remotehost(1);
				if( RemoteHost == 0 || *RemoteHost == 0 ){
					if( Default_remote_host && *Default_remote_host ){
						RemoteHost = Default_remote_host;
					} else if( FQDNHost && *FQDNHost ){
						RemoteHost = FQDNHost;
					}
				}
				if( RemotePrinter && strcmp( RemotePrinter, Printer ) ){
					plp_snprintf( msg, sizeof(msg), "Printer: %s is %s@%s\n",
						Printer, RemotePrinter, RemoteHost );
					DEBUG4("Do_all_printers: '%s'",msg);
					/* write as long as you can */
					if(  Write_fd_str( 1, msg ) < 0 ) exit(0);
				}
				Send_statusrequest(
					RemotePrinter?RemotePrinter:Printer,
					RemoteHost, Longformat, &argv[Optind],
					Connect_timeout, Send_timeout, 1 );
				/* if(  Write_fd_str( 1, "\n" ) < 0 ) exit(0); */
			}
		} else {
			DEBUG4("lpq: remoteprinter '%s', remote host '%s'",
				RemotePrinter, RemoteHost );
			if( RemotePrinter && strcmp( RemotePrinter, Printer ) ){
				plp_snprintf( msg, sizeof(msg), "Printer: %s is %s@%s\n",
					Printer, RemotePrinter, RemoteHost );
				if(  Write_fd_str( 1, msg ) < 0 ) exit(0);
			}
			Send_statusrequest(
				RemotePrinter?RemotePrinter:Printer,
				RemoteHost, Longformat, &argv[Optind],
				Connect_timeout, Send_timeout, 1 );
			/* if(  Write_fd_str( 1, "\n" ) < 0 ) exit(0); */
		}
		if( Interval > 0 ){
			sleep( Interval );
		}
		/* we check to make sure that nobody killed the output */
	} while( Interval > 0 && (fstat( 1, &statb ) == 0) );
	if( Clear_scr ){
		Term_finish();
	}
	exit(0);
}
