/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: lpq.c,v 5.3 1999/10/12 18:50:33 papowell Exp papowell $";


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
 *      lpq [ -PPrinter_DYN ]
 *    lpq [-Pprinter ]*[-a][-U username][-s][-l][+[n]][-Ddebugopt][job#][user]
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
 * Implementation Notes
 * Patrick Powell Tue May  2 09:58:29 PDT 1995
 * 
 * The LPD server will be returning the formatted status;
 * The format can be the following:
 * 
 * SHORT:
 * Warning: lp is down: lp is ready and printing
 * Warning: no daemon present
 * Rank   Owner      Job  Files                                 Total Size
 * active root       30   standard input                        5 bytes
 * 2nd    root       31   standard input                        5 bytes
 * 
 * LONG:
 * 
 * Warning: lp is down: lp is ready and printing
 * Warning: no daemon present
 * 
 * root: 1st                                [job 030taco]
 *         standard input                   5 bytes
 * 
 * root: 2nd                                [job 031taco]
 *         standard input                   5 bytes
 * 
 */

#include "lp.h"

#include "child.h"
#include "getopt.h"
#include "getprinter.h"
#include "getqueue.h"
#include "initialize.h"
#include "linksupport.h"
#include "patchlevel.h"
#include "sendreq.h"
#include "termclear.h"

/**** ENDINCLUDE ****/


#undef EXTERN
#undef DEFINE
#define EXTERN
#define DEFINE(X) X
#include "lpq.h"
/**** ENDINCLUDE ****/

 struct line_list Lpq_options;
 static char *Username_JOB;

#define MAX_SHORT_STATUS 6

/***************************************************************************
 * main()
 * - top level of LPQ
 *
 ****************************************************************************/

int main(int argc, char *argv[], char *envp[])
{
	int i;
	struct line_list l, options;

	Init_line_list(&l);
	Init_line_list(&options);

	/* set signal handlers */
	(void) plp_signal (SIGHUP, cleanup_HUP);
	(void) plp_signal (SIGINT, cleanup_INT);
	(void) plp_signal (SIGQUIT, cleanup_QUIT);
	(void) plp_signal (SIGTERM, cleanup_TERM);

	/*
	 * set up the user state
	 */

#ifndef NODEBUG
	Debug = 0;
#endif

	Longformat = 1;
	Status_line_count = 0;
	Displayformat = REQ_DLONG;

	Initialize(argc, argv, envp);
	Setup_configuration();
	Get_parms(argc, argv );      /* scan input args */

	if( All_printers || (Printer_DYN && safestrcasecmp(Printer_DYN,ALL) == 0 ) ){
		Get_all_printcap_entries();
		if(DEBUGL1)Dump_line_list("lpq- All_line_list", &All_line_list );
	}
	/* we do the individual printers */
	if( Displayformat == REQ_DLONG && Longformat && Status_line_count <= 0 ){
		Status_line_count = (1 << (Longformat-1));
	}
	do {
		Free_line_list(&Printer_list);
		if( Clear_scr ){
			Term_clear();
			Write_fd_str(1,Time_str(0,0));
			Write_fd_str(1,"\n");
		}
		if( All_printers ){
			DEBUG1("lpq: all printers");
			for( i = 0; i < All_line_list.count; ++i ){
				Set_DYN(&Printer_DYN,All_line_list.list[i] );
				Show_status(argv);
			}
		} else {
			Show_status(argv);
		}
		DEBUG1("lpq: done");
		Remove_tempfiles();
		DEBUG1("lpq: tempfiles removed");
		if( Interval > 0 ){
			plp_sleep( Interval );
		}
		/* we check to make sure that nobody killed the output */
	} while( Interval > 0 );
	DEBUG1("lpq: after loop");
	/* if( Clear_scr ){ Term_finish(); } */
	Errorcode = 0;
	DEBUG1("lpq: cleaning up");
	cleanup(0);
	return(0);
}

void Show_status(char **argv)
{
	int fd;
	char msg[LINEBUFFER];

	DEBUG1("Show_status: start");
	/* set up configuration */
	Get_printer();
	Fix_Rm_Rp_info();

	if( Displayformat != REQ_DSHORT
		&& safestrcasecmp(Printer_DYN, RemotePrinter_DYN) ){
		plp_snprintf( msg, sizeof(msg), _("Printer: %s is %s@%s\n"),
			Printer_DYN, RemotePrinter_DYN, RemoteHost_DYN );
		DEBUG1("Show_status: '%s'",msg);
		if(  Write_fd_str( 1, msg ) < 0 ) cleanup(0);
	}
	if( Check_for_rg_group( Logname_DYN ) ){
		plp_snprintf( msg, sizeof(msg),
			"Printer: %s - cannot use printer, not in privileged group\n" );
		if(  Write_fd_str( 1, msg ) < 0 ) cleanup(0);
		return;
	}
	fd = Send_request( 'Q', Displayformat,
		&argv[Optind], Connect_timeout_DYN,
		Send_query_rw_timeout_DYN, 1 );
	if( fd >= 0 ){
		if( Read_status_info( RemoteHost_DYN, fd,
			1, Send_query_rw_timeout_DYN, Displayformat,
			Status_line_count ) ){
			cleanup(0);
		}
		close(fd);
	}
	DEBUG1("Show_status: end");
}


/***************************************************************************
 *int Read_status_info( int ack, int fd, int timeout );
 * ack = ack character from remote site
 * sock  = fd to read status from
 * char *host = host we are reading from
 * int output = output fd
 *  We read the input in blocks,  split up into lines,
 *  and then pass the lines to a lower level routine for processing.
 *  We run the status through the plp_snprintf() routine,  which will
 *   rip out any unprintable characters.  This will prevent magic escape
 *   string attacks by users putting codes in job names, etc.
 ***************************************************************************/

int Read_status_info( char *host, int sock,
	int output, int timeout, int displayformat,
	int status_line_count )
{
	int i, j, len, n, status, count, index_list, same;
	char buffer[SMALLBUFFER], header[SMALLBUFFER];
	char *s, *t;
	struct line_list l;
	int look_for_pr = 0;

	Init_line_list(&l);

	header[0] = 0;
	status = count = 0;
	/* long status - trim lines */
	DEBUG1("Read_status_info: output %d, timeout %d, dspfmt %d",
		output, timeout, displayformat );
	DEBUG1("Read_status_info: status_line_count %d", status_line_count );
	buffer[0] = 0;
	do {
		DEBUG1("Read_status_info: look_for_pr %d, in buffer already '%s'", look_for_pr, buffer );
		if( DEBUGL2 )Dump_line_list("Read_status_info - starting list", &l );
		count = strlen(buffer);
		n = sizeof(buffer)-count-1;
		status = 1;
		if( n > 0 ){
			status = Link_read( host, &sock, timeout,
				buffer+count, &n );
			DEBUG1("Read_status_info: Link_read status %d, read %d", status, n );
		}
		if( status || n <= 0 ){
			status = 1;
			buffer[count] = 0;
		} else {
			buffer[count+n] = 0;
		}
		DEBUG1("Read_status_info: got '%s'", buffer );
		/* now we have to split line up */
		if( displayformat == REQ_VERBOSE || displayformat == REQ_LPSTAT || Rawformat ){
			if( Write_fd_str( output, buffer ) < 0 ) return(1);
			buffer[0] = 0;
			continue;
		}
		if( (s = safestrrchr(buffer,'\n')) ){
			*s++ = 0;
			/* add the lines */
			Split(&l,buffer,Line_ends,0,0,0,0,0);
			memmove(buffer,s,strlen(s)+1);
		}
		if( DEBUGL2 )Dump_line_list("Read_status_info - status after splitting", &l );
		if( displayformat == REQ_DSHORT ){
			for( i = 0; i < l.count; ++i ){
				t = l.list[i];
				if( t && !Find_exists_value(&Printer_list,t,0) ){
					if( Write_fd_str( output, t ) < 0
						|| Write_fd_str( output, "\n" ) < 0 ) return(1);
					Add_line_list(&Printer_list,t,0,1,0);
				}
			}
			Free_line_list(&l);
			continue;
		}
		if( status ){
			if( buffer[0] ){
				Add_line_list(&l,buffer,0,0,0);
				buffer[0] = 0;
			}
			Check_max(&l,1);
			l.list[l.count++] = 0;
		}
		index_list = 0;
 again:
		DEBUG2("Read_status_info: look_for_pr '%d'", look_for_pr );
		if( DEBUGL2 )Dump_line_list("Read_status_info - starting, Printer_list",
			&Printer_list);
		while( look_for_pr && index_list < l.count ){
			s = l.list[index_list];
			if( s == 0 || isspace(cval(s)) || !(t = strstr(s,"Printer:"))
				|| Find_exists_value(&Printer_list,t,0) ){
				++index_list;
			} else {
				look_for_pr = 0;
			}
		}
		while( index_list < l.count && (s = l.list[index_list]) ){
			DEBUG2("Read_status_info: checking [%d] '%s', total %d", index_list, s, l.count );
			if( s && !isspace(cval(s)) && (t = strstr(s,"Printer:")) ){
				if( Find_exists_value(&Printer_list,t,0) ){
					look_for_pr = 1;
					goto again;
				} else {
					Add_line_list(&Printer_list,t,0,1,0);
					if( Write_fd_str( output, s ) < 0
						|| Write_fd_str( output, "\n" ) < 0 ) return(1);
					++index_list;
					continue;
				}
			}
			if( status_line_count > 0 ){
				/*
				 * starting at line_index, we take this as the header.
				 * then we check to see if there are at least status_line_count there.
				 * if there are,  we will increment status_line_count until we get to
				 * the end of the reading (0 string value) or end of list.
				 */
				header[0] = 0;
				strncpy( header, s, sizeof(header)-1);
				header[sizeof(header)-1] = 0;
				if( (s = strchr(header,':')) ){
					*++s = 0;
				}
				len = strlen(header);
				/* find the last status_line_count lines */
				same = 1;
				for( i = index_list+1; i < l.count ; ++i ){
					same = !safestrncmp(l.list[i],header,len);
					DEBUG2("Read_status_info: header '%s', len %d, to '%s', same %d",
						header, len, l.list[i], same );
					if( !same ){
						break;
					}
				}
				/* if they are all the same,  then we save for next pass */
				/* we print the i - status_line count to i - 1 lines */
				j = i - status_line_count;
				if( index_list < j ) index_list = j;
				DEBUG2("Read_status_info: header '%s', index_list %d, last %d, same %d",
					header, index_list, i, same );
				if( same ) break;
				while( index_list < i ){
					DEBUG2("Read_status_info: writing [%d] '%s'",
						index_list, l.list[index_list]);
					if( Write_fd_str( output, l.list[index_list] ) < 0
						|| Write_fd_str( output, "\n" ) < 0 ) return(1);
					++index_list;
				}
			} else {
				if( Write_fd_str( output, s ) < 0
					|| Write_fd_str( output, "\n" ) < 0 ) return(1);
				++index_list;
			}
		}
		DEBUG2("Read_status_info: at end index_list %d, count %d", index_list, l.count );
		for( i = 0; i < l.count && i < index_list; ++i ){
			if( l.list[i] ) free( l.list[i] ); l.list[i] = 0;
		}
		for( i = 0; index_list < l.count ; ++i, ++index_list ){
			l.list[i] = l.list[index_list];
		}
		l.count = i;
	} while( status == 0 );
	Free_line_list(&l);
	DEBUG1("Read_status_info: done" );
	return(0);
}

/* VARARGS2 */
#ifdef HAVE_STDARGS
 void setstatus (struct job *job,char *fmt,...)
#else
 void setstatus (va_alist) va_dcl
#endif
{
#ifndef HAVE_STDARGS
    struct job *job;
    char *fmt;
#endif
	char msg[LARGEBUFFER];
    VA_LOCAL_DECL

    VA_START (fmt);
    VA_SHIFT (job, struct job * );
    VA_SHIFT (fmt, char *);

	msg[0] = 0;
	if( Verbose ){
		(void) plp_vsnprintf( msg, sizeof(msg)-2, fmt, ap);
		strcat( msg,"\n" );
		if( Write_fd_str( 2, msg ) < 0 ) cleanup(0);
	}
	VA_END;
	return;
}

 void send_to_logger (int sfd, int mfd, struct job *job,const char *header, char *fmt){;}
/* VARARGS2 */
#ifdef HAVE_STDARGS
 void setmessage (struct job *job,const char *header, char *fmt,...)
#else
 void setmessage (va_alist) va_dcl
#endif
{
#ifndef HAVE_STDARGS
    struct job *job;
    char *fmt, *header;
#endif
	char msg[LARGEBUFFER];
    VA_LOCAL_DECL

    VA_START (fmt);
    VA_SHIFT (job, struct job * );
    VA_SHIFT (header, char * );
    VA_SHIFT (fmt, char *);

	msg[0] = 0;
	if( Verbose ){
		(void) plp_vsnprintf( msg, sizeof(msg)-2, fmt, ap);
		strcat( msg,"\n" );
		if( Write_fd_str( 2, msg ) < 0 ) cleanup(0);
	}
	VA_END;
	return;
}

/***************************************************************************
 * void Get_parms(int argc, char *argv[])
 * 1. Scan the argument list and get the flags
 * 2. Check for duplicate information
 ***************************************************************************/

 extern char *next_opt;

 char LPQ_optstr[]    /* LPQ options */
 = "D:P:VacLn:lst:vU:" ;

void Get_parms(int argc, char *argv[] )
{
	int option;
	char *name;

	if( argv[0] && (name = safestrrchr( argv[0], '/' )) ) {
		++name;
	} else {
		name = argv[0];
	}
	/* check to see if we simulate (poorly) the LP options */
	if( name && safestrcmp( name, "lpstat" ) == 0 ){
		fprintf( stderr,"lpq:  please use the LPRng lpstat program\n");
		exit(1);
	} else {
		/* scan the input arguments, setting up values */
		while ((option = Getopt (argc, argv, LPQ_optstr )) != EOF) {
			switch (option) {
			case 'D':
				Parse_debug(Optarg,1);
				break;
			case 'P': if( Optarg == 0 ) usage();
				Set_DYN(&Printer_DYN,Optarg);
				break;
			case 'V': ++Verbose; break;
			case 'a': Set_DYN(&Printer_DYN,"all"); ++All_printers; break;
			case 'c': Clear_scr = 1; break;
			case 'l': ++Longformat; break;
			case 'n': Status_line_count = atoi( Optarg ); break;
			case 'L': Longformat = 0; Rawformat = 1; break;
			case 's': Longformat = 0;
						Displayformat = REQ_DSHORT;
						break;
			case 't': if( Optarg == 0 ) usage();
						Interval = atoi( Optarg );
						break;
			case 'v': Longformat = 0; Displayformat = REQ_VERBOSE; break;
			case 'U': Username_JOB = Optarg; break;
			default:
				usage();
			}
		}
	}
	if( Verbose ) {
		fprintf( stderr, _("Version %s\n"), PATCHLEVEL );
		if( Verbose > 1 ) Printlist( Copyright, stderr );
	}
}

 char *lpq_msg = 
"usage: %s [-aAclV] [-Ddebuglevel] [-Pprinter] [-tsleeptime]\n\
  -a           - all printers\n\
  -c           - clear screen before update\n\
  -l           - increase (lengthen) detailed status information\n\
                 additional l flags add more detail.\n\
  -L           - maximum detailed status information\n\
  -n linecount - linecount lines of detailed status information\n\
  -Ddebuglevel - debug level\n\
  -Pprinter    - specify printer\n\
  -s           - short (summary) format\n\
  -tsleeptime  - sleeptime between updates\n\
  -V           - print version information\n";

void usage(void)
{
	fprintf( stderr, lpq_msg, Name );
	exit(1);
}

 int Start_worker( struct line_list *args, int fd )
{
	return(1);
}

#if 0

#include "permission.h"
#include "lpd.h"
#include "lpd_status.h"
 int Send_request(
	int class,					/* 'Q'= LPQ, 'C'= LPC, M = lprm */
	int format,					/* X for option */
	char **options,				/* options to send */
	int connect_timeout,		/* timeout on connection */
	int transfer_timeout,		/* timeout on transfer */
	int output					/* output on this FD */
	)
{
	int i, n;
	int socket = 1;
	char cmd[SMALLBUFFER];

	cmd[0] = format;
	cmd[1] = 0;
	plp_snprintf(cmd+1, sizeof(cmd)-1, "%s", RemotePrinter_DYN);
	for( i = 0; options[i]; ++i ){
		n = strlen(cmd);
		plp_snprintf(cmd+n,sizeof(cmd)-n," %s",options[i] );
	}
	Perm_check.remoteuser = "papowell";
	Perm_check.user = "papowell";
	Is_server = 1;
	Job_status(&socket,cmd);
	return(-1);
}

#endif
