/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: lprm.c,v 5.1 1999/09/12 21:32:48 papowell Exp papowell $";


/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lprm.c
 * PURPOSE:
 **************************************************************************/

/***************************************************************************
 * SYNOPSIS
 *      lprm [ -PPrinter_DYN ]
 *    lprm [-Pprinter ]*[-a][-s][-l][+[n]][-Ddebugopt][job#][user]
 * DESCRIPTION
 *   lprm sends a status request to lpd(8)
 *   and reports the status of the
 *   specified jobs or all  jobs  associated  with  a  user.  lprm
 *   invoked  without  any arguments reports on the printer given
 *   by the default printer (see -P option).  For each  job  sub-
 *   mitted  (i.e.  invocation  of lpr(1)) lprm reports the user's
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
 *        Forces lprm to periodically display  the  spool  queues.
 *        Supplying  a  number immediately after the + sign indi-
 *        cates that lprm should sleep n seconds in between  scans
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
#include "sendauth.h"
#include "sendreq.h"

/**** ENDINCLUDE ****/

#undef EXTERN
#undef DEFINE
#define EXTERN
#define DEFINE(X) X
#include "lprm.h"
/**** ENDINCLUDE ****/

void Do_removal(char **argv);
static char *User_name_JOB;

/***************************************************************************
 * main()
 * - top level of LPRM
 *
 ****************************************************************************/

int main(int argc, char *argv[], char *envp[])
{
	int i;
	struct line_list args;

	Init_line_list(&args);

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


	Initialize(argc, argv, envp);
	Setup_configuration();
	Get_parms(argc, argv);      /* scan input args */

	Add_line_list(&args,Logname_DYN,0,0,0);
	for( i = Optind; argv[i]; ++i ){
		Add_line_list(&args,argv[i],0,0,0);
	}
	Check_max(&args,2);
	args.list[args.count] = 0;

	/* now force the printer list */
	if( All_printers || (Printer_DYN && safestrcasecmp(Printer_DYN,ALL) == 0 ) ){
		All_printers = 1;
		Get_all_printcap_entries();
		if(DEBUGL1)Dump_line_list("lprm - final All_line_list", &All_line_list);
	}
	DEBUG1("lprm: Printer_DYN '%s', All_printers %d, All_line_list.count %d",
		Printer_DYN, All_printers, All_line_list.count );
	if( User_name_JOB ){
		struct line_list user_list;
		char *str, *t;
		struct passwd *pw;
		int found, uid;

		DEBUG2("lprm: checking '%s' for -U perms",
			Allow_user_setting_DYN );
		Init_line_list(&user_list);
		Split( &user_list, Allow_user_setting_DYN,File_sep,0,0,0,0,0);
		
		found = 0;
		for( i = 0; !found && i < user_list.count; ++i ){
			str = user_list.list[i];
			DEBUG2("lprm: checking '%s'", str );
			uid = strtol( str, &t, 10 );
			if( str == t || *t ){
				/* try getpasswd */
				pw = getpwnam( str );
				if( pw ){
					uid = pw->pw_uid;
				}
			}
			DEBUG2( "lprm: uid '%d'", uid );
			found = ( uid == OriginalRUID );
			DEBUG2( "lprm: found '%d'", found );
		}
		if( !found ){
			Diemsg( _("-U (username) can only be used by ROOT or authorized users") );
		}
		Set_DYN( &Logname_DYN, User_name_JOB );
	}
	if( All_printers ){
		if( All_line_list.count == 0 ){
			fprintf(stderr,"no printers\n");
			cleanup(0);
		}
		for( i = 0; i < All_line_list.count; ++i ){
			Set_DYN(&Printer_DYN,All_line_list.list[i] );
			Do_removal(args.list);
		}
	} else {
		Do_removal(args.list);
	}
	Free_line_list(&args);
	DEBUG1("lprm: done");
	Remove_tempfiles();
	DEBUG1("lprm: tempfiles removed");
	Errorcode = 0;
	DEBUG1("lprm: cleaning up");
	cleanup(0);
	return(0);
}


void Do_removal(char **argv)
{
	int fd, n;
	char msg[LINEBUFFER];

	DEBUG1("Do_removal: start");
	Get_printer();
	Fix_Rm_Rp_info();
	/* fix up authentication */
	if( Check_for_rg_group( Logname_DYN ) ){
		fprintf( stderr,
			"cannot use printer - not in privileged group\n" );
		return;
	}
	fd = Send_request( 'M', REQ_REMOVE,
		argv, Connect_timeout_DYN, Send_query_rw_timeout_DYN, 1 );
	while( (n = read(fd, msg, sizeof(msg)) ) > 0 ){
		if( write(1,msg,n) < 0 ) cleanup(0);
	}
	close(fd);
	DEBUG1("Do_removal: end");
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
    VA_SHIFT (header, char *);
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
void usage(void);
char LPRM_optstr[]   /* LPRM options */
 = "aD:P:U:V" ;
char CLEAN_optstr[]   /* CLEAN options */
 = "D:" ;

void Get_parms(int argc, char *argv[] )
{
	int option;
	char *name;


	if( argv[0] && (Name = strrchr( argv[0], '/' )) ) {
		++Name;
	} else {
		Name = argv[0];
	}
	/* check to see if we simulate (poorly) the LP options */
	if( Name && safestrcmp( Name, "clean" ) == 0 ){
		LP_mode = 1;
		while ((option = Getopt (argc, argv, CLEAN_optstr )) != EOF)
		switch (option) {
		case 'D':
			Parse_debug( Optarg, 1 );
			break;
		default: usage(); break;
		}
		if( Optind < argc ){
			name = argv[argc-1];
			Get_all_printcap_entries();
			if( safestrcasecmp(name,ALL) ){
				if( Find_exists_value( &All_line_list, name, Value_sep ) ){
					Set_DYN(&Printer_DYN,name);
					argv[argc-1] = 0;
				}
			} else {
				All_printers = 1;
				Set_DYN(&Printer_DYN,"all");
			}
		}
	} else {
		while ((option = Getopt (argc, argv, LPRM_optstr )) != EOF)
		switch (option) {
		case 'a': All_printers = 1; Set_DYN(&Printer_DYN,"all"); break;
		case 'D': Parse_debug(Optarg, 1); break;
		case 'V': ++Verbose; break;
		case 'U': User_name_JOB = Optarg; break;
		case 'P': Set_DYN(&Printer_DYN, Optarg); break;
		default: usage(); break;
		}
	}
	if( Verbose ){
		fprintf( stderr, _("Version %s\n"), PATCHLEVEL );
		if( Verbose > 1 ) Printlist( Copyright, stderr );
	}
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
  -Uuser       - impersonate this user (root or privileged user only)\n\
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
	if( !LP_mode ){
		fprintf( stderr, _(lprm_msg), Name );
	} else {
		fprintf( stderr, _(clean_msg), Name );
	}
	exit(1);
}

int Start_worker( struct line_list *l, int fd )
{
	return(1);
}

#if TEST

#include "permission.h"
#include "lpd.h"
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
	plp_snprintf(cmd+1, sizeof(cmd)-1, RemotePrinter_DYN);
	for( i = 0; options[i]; ++i ){
		n = strlen(cmd);
		plp_snprintf(cmd+n,sizeof(cmd)-n," %s",options[i] );
	}
	Perm_check.remoteuser = "papowell";
	Perm_check.user = "papowell";
	Is_server = 1;
	Job_remove(&socket,cmd);
	return(-1);
}

#endif
