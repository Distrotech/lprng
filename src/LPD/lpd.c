/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpd.c
 * PURPOSE: simulate the LPD daemon
 **************************************************************************/

/*
Additional Implementation Notes
Sun Apr 16 08:52:20 PDT 1995

We are going to simulate the LPD daemon using the basic facilities.

1. read all of the configuration stuff.
2. open a listening socket.
3. wait for a connection
4. print the connection information

*/
static char *const _id =
"lpd.c,v 3.23 1998/03/24 02:43:22 papowell Exp";

#include "lp.h"
#include "printcap.h"
#include "fileopen.h"
#include "gethostinfo.h"
#include "decodestatus.h"
#include "initialize.h"
#include "killchild.h"
#include "linksupport.h"
#include "permission.h"
#include "serverpid.h"
#include "setstatus.h"
#include "waitchild.h"
#include "krb5_auth.h"
#include "cleantext.h"
/**** ENDINCLUDE ****/

static void Service_connection( struct sockaddr *sin, int listen, int talk );
static void Read_pc(void), Reinit(void);
static void Set_lpd_pid(void);
static int Get_lpd_pid(void);
int Setuplog(char *logfile, int sock);
void Service_printer( int talk );
static int Reread_config;


char LPD_optstr[] 	/* LPD options */
 = "D:FL:P:Vic" ;

/***************************************************************************
 * main()
 * - top level of LPD Lite.  This is a cannonical method of handling
 *   input.  Note that we assume that the LPD daemon will handle all
 *   of the dirty work associated with formatting, printing, etc.
 * 
 * 1. get the debug level from command line arguments
 * 2. set signal handlers for cleanup
 * 3. get the Host computer Name and user Name
 * 4. scan command line arguments
 * 5. check command line arguments for consistency
 *
 ****************************************************************************/


int main(int argc, char *argv[], char *envp[])
{
	int sock = 0;		/* socket for listen */
	int newsock;		/* new socket */
	int pid;			/* pid */
	fd_set defreadfds, readfds;	/* for select() */
	struct timeval timeval, *timeout;
	int timeout_encountered = 0;	/* we have a timeout */
	int max_socks;		/* maximum number of sockets */
	struct sockaddr sin;	/* the connection remote IP address */
	int n, m, started_process;	/* ACME?  Hmmm... well, ok */
	int err;
 	time_t last_time;	/* time that last Start_all was done */
 	time_t this_time;	/* current time */
	plp_status_t status;

	Is_server = 1;	/* we are the LPD server */

	/*
	 * This will make things safe even
	 * if not started by root set the UID to user
	 * will make things safe if the program is running SUID and
	 * not started by root.  If it is started by root, it is
	 * irrelevant
	 */
	Initialize(argc, argv, envp);


	/* scan the argument list for a 'Debug' value */
	Get_debug_parm( argc, argv, LPD_optstr, debug_vars );
	/* scan the input arguments, setting up values */

	Get_parms(argc, argv);      /* scan input args */

	/* set signal handlers */
	(void) plp_signal (SIGHUP,  (plp_sigfunc_t)Reinit);
	(void) plp_signal (SIGINT, cleanup_INT);
	(void) plp_signal (SIGQUIT, cleanup_QUIT);
	(void) plp_signal (SIGTERM, cleanup_TERM);
	(void) plp_signal (SIGUSR1, (plp_sigfunc_t)SIG_IGN);
	(void) plp_signal (SIGUSR2, (plp_sigfunc_t)SIG_IGN);

	/*
	 * set up the configuration
	 */
	Setup_configuration();

#if defined(HAVE_KRB5_H)
	if(DEBUGL0){
		char buffer[LINEBUFFER];
		remote_principal_krb5( Kerberos_service, 0, buffer, sizeof(buffer) );
		logDebug("lpd: kerberos principle '%s'", buffer );
	}
#endif

	/*
	 * now get the connection
	 */

	if( Lockfile == 0 ){
		logerr_die( LOG_INFO, _("No LPD lockfile specified!") );
	}

	/*
	 * This is the one of the two places where we need to be
	 * root in order to open a socket
	 */
	sock = Link_listen();
	if( sock < 0 ){
		/*
		 * try reading the lockfile
		 */
		pid = Get_lpd_pid();
		if( pid > 0 ){
			Diemsg( _("Another print spooler is using TCP printer port, possibly lpd process '%d'"),
				pid );
		} else {
			Diemsg( _("Another print spooler is using TCP printer port") );
		}
	}

	/*
	 * At this point you are the server for the LPD port
	 * you need to fork to allow the regular user to continue
	 * you put the child in its separate process group as well
	 */

	if( !Foreground ){
		if( (pid = dofork(1)) < 0 ){
			logerr_die( LOG_ERR, _("lpd: main() dofork failed") );
		} else if( pid ){
			exit(0);
		}
	}

	/* set up the log file and standard environment - do not
	   fool around with anything but fd 0,1,2 which should be safe
		as we made sure that the fd 0,1,2 existed.
    */

	sock = Setuplog( Logfile, sock );/**/

	/*
	 * Write the PID into the lockfile
	 */

	Set_lpd_pid();

	/* open a connection to logger */
	setmessage(0,"LPD","Starting");

	/* read the printcap file */
	Read_pc();

	/* establish the pipes for low level processes to use */
	if( pipe( Lpd_pipe ) ){
		logerr_die( LOG_ERR, _("lpd: pipe call failed") );
	}

	/* get the maximum number of servers allowed */
	Max_servers = Get_max_servers();
	DEBUG0( "lpd: maximum servers %d", Max_servers );

	DEBUGF(DMEM1)("lpd: memory allocation now %s",  Brk_check_size() );
	/*
	 * clean out the current queues
	 */
 	last_time = time( (void *)0 );

	Printer = 0;
	Name = "MAIN";
	proctitle( "lpd %s", Name );

	Start_all();

	/* set up the wait activity */

	FD_ZERO( &defreadfds );
	FD_SET( sock, &defreadfds );
	FD_SET( Lpd_pipe[0], &defreadfds );
	max_socks = sock+1;
	if( Lpd_pipe[0] >= sock ){
		max_socks = Lpd_pipe[0]+1;
	}

	/*
	 * start waiting for connections from processes
	 */

#if 0 && defined(DMALLOC)
	Write_fd_str(dmalloc_outfile, "**** Before Main Loop\n");
	dmalloc_log_unfreed();
#endif
	started_process = 0;
	last_time = time( (void *)0 );
	do{
		while( plp_waitpid( -1, &status, WNOHANG) > 0 );
		/* we better see if we have enough servers left */
		n = Countpid( 1 );
		DEBUG0( "lpd: %d servers active of %d", n, Max_servers );
		DEBUGF(DMEM1)( "lpd: memory allocation now %s",  Brk_check_size() );
		timeout = 0;
		/* handle case when we are starting up a lot of servers */
		if( n < Max_servers ){
			started_process = Start_idle_server( Max_servers-n );
		}
		/* handle case when we need to poll the queues again */
		if( Poll_time > 0 ){
			this_time = time( (void *)0 );
			m = (this_time - last_time);
			timeout_encountered = 0;
			if( m >= Poll_time ){
				timeout_encountered = 1;
			}
			memset(&timeval,0, sizeof(timeval));
			timeval.tv_sec = Poll_time;
			if( timeout_encountered && !started_process ){
				/* start them all again */
				Start_all();
				last_time = this_time;
				n = Countpid( 1 );
				started_process = Start_idle_server( Max_servers-n );
			}
			if( started_process || Force_poll ){
				DEBUG0( "lpd: setting timeout" );
				timeout = &timeval;
			} else {
				DEBUG0( "lpd: no work, suppressing timeout" );
			}
		}

		/* deal with a problem where processes that have died
		 * before the signal handler was installed will not be collected
		 */
		while( plp_waitpid( -1, &status, WNOHANG) > 0 );
		readfds = defreadfds;
		n = Countpid( 1 );
		/* do not accept incoming call if no worker available */
		if( n >= Max_servers ){
			FD_CLR( sock, &defreadfds );
		}

		started_process = 0;
		DEBUG1( "lpd: starting select with timeout 0x%x, %d sec", timeout,
			timeout?timeout->tv_sec:0);
		Setup_waitpid_break(1);
		m = select( max_socks,
			FD_SET_FIX((fd_set *))&readfds,
			FD_SET_FIX((fd_set *))0,
			FD_SET_FIX((fd_set *))0, timeout );
		err = errno;
		Setup_waitpid();
		DEBUG1( "lpd: select returned %d, error '%s'", m, Errormsg(err) );
		/* if we got a SIGHUP then we reread configuration */
		if( Reread_config ){
			DEBUG1( "lpd: rereading configuration" );
			Reread_config = 0;
			Setup_configuration();
			setmessage(0,"LPD","Restart");
			Read_pc();
		}
		/* mark this as a timeout */
		timeout_encountered = (m == 0);
		if( m < 0 ){
			if( err != EINTR ){
				errno = err;
				logerr_die( LOG_ERR, _("lpd: select error!"));
				break;
			}
			continue;
		} else if( m == 0 ){
			DEBUG0( "lpd: signal or time out" );
			continue;
		}
		if( FD_ISSET( sock, &readfds ) ){
			m = sizeof( sin );
			DEBUG3("accepting connection on %d", sock );
			newsock = accept( sock, &sin, &m );
			err = errno;
			DEBUG3("connection on %d", newsock );

			if( newsock >= 0 ){
				Service_connection( &sin, sock, newsock );
				started_process = 1;
			} else {
				errno = err;
				logerr(LOG_INFO, _("lpd: accept on listening socket failed") );
			}
		}
		if( FD_ISSET( Lpd_pipe[0], &readfds ) ){
			Service_printer( Lpd_pipe[0] );
			started_process = 1;
		}
	}while( 1 );
	cleanup(0);
	return(0);
}

/***************************************************************************
 * Setuplog( char *logfile, int sock )
 * Purpose: to set up a standard error logging environment
 * saveme will prevent stdin from being clobbered
 *   1.  dup 'sock' to fd 0, close sock
 *   2.  opens /dev/null on fd 1
 *   3.  If logfile is "-" or NULL, output file is alread opened
 *   4.  Open logfile; if unable to, then open /dev/null for output
 ***************************************************************************/
int Setuplog(char *logfile, int sock)
{
    int fd;
	struct stat statb;
	int err;

	DEBUG4("Setuplog: logfile '%s', sock %d", logfile, sock );
	while( sock < 2 ){
		sock = dup(sock);
	}
	if( sock < 0 ){
		logerr_die( LOG_CRIT, _("Setuplog: dup of %d failed"), sock );
	}
    /*
     * set stdin, stdout to /dev/null
     */
	if ((fd = open("/dev/null", O_RDWR, Spool_file_perms)) < 0) {
	    logerr_die(LOG_ERR, _("Setuplog: open /dev/null failed"));
	}
	close( 0 ); close( 1 );
	if( dup2( fd, 0 ) < 0 || dup2( fd, 1 ) < 0 ){
		logerr_die (LOG_CRIT, _("Setuplog: dup2 failed"));
	}
	close(fd);
	DEBUG3 ("Setuplog: log file '%s'", logfile?logfile:"<NULL>");
    /*
     * open logfile; if it is "-", use stderr; if Foreground is set, use stderr
     */
    if(logfile && *logfile && strcmp(logfile, "-")
		&& !Foreground) {
		DEBUG1 ("Setuplog: opening log file '%s'", logfile);
		if ((fd = Checkwrite(logfile, &statb, O_WRONLY|O_APPEND, 0, 0)) < 0) {
			err = errno;
			DEBUG0 ("cannot open logfile %s - %s", logfile, Errormsg(err));
			if(DEBUGL0) {
				fd = 2;
			} else {
				fd = 1;
			}
		}
		DEBUG3 ("Setuplog: log file '%s' fd '%d'", logfile, fd);
		if (fd != 2 ){
			if( dup2 (fd, 2) < 0) {
				logerr_die (LOG_CRIT, _("Setuplog: dup2 of %d failed"), fd );
			}
			(void) close (fd);
		}
	}
	return( sock );
}

/***************************************************************************
 * Service_connection( int listen, int talk )
 *  Service the connection on the talk socket
 * 1. fork a connection
 * 2. Mother:  close talk and return
 * 2  Child:  close listen
 * 2  Child:  read input line and decide what to do
 *
 ***************************************************************************/

static void Service_connection( struct sockaddr *sin, int listen, int talk )
{
	int pid;		/* PID of the server process */
	char input[LINEBUFFER];
	char buffer[LINEBUFFER];	/* for messages */
	int len;
	int status;		/* status of operation */
	int permission;
	int port = 0;

	pid = dofork(1);
	if( pid < 0 ){
		logerr( LOG_INFO, _("Service_connection: dofork failed") );
	}
	if( pid ){
		/*
		 * mother process - close the talk socket
		 */
		close( talk );
		return;
	}
	/*
	 * daughter process 
	 * - close the listen socket
	 */
	(void) plp_signal (SIGHUP, cleanup );
	close( listen ); listen = -1;

	/* get the remote name and set up the various checks */
	Perm_check.addr = sin;
	Get_remote_hostbyaddr( &RemoteHostIP, sin );
	Perm_check.remotehost  =  &RemoteHostIP;
	Perm_check.host = &RemoteHostIP;

	len = sizeof( input ) - 1;
	memset(input,0,sizeof(input));
	DEBUG0( "LPD: Starting Read" );
	status = Link_line_read(ShortRemote,&talk,
		Send_job_rw_timeout,input,&len);
	DEBUG0( "Request '%s'", input );
	if( len == 0 ){
		DEBUG3( "LPD: zero length read" );
		cleanup(0);
	}
	if( status ){
		logerr_die( LOG_DEBUG, _("Service_connection: cannot read request") );
	}
	if( len < 3 ){
		fatal( LOG_INFO, _("Service_connection: bad request line '%s'"), input );
	}

	if( sin->sa_family == AF_INET ){
		port = ((struct sockaddr_in *)sin)->sin_port;
#if defined(IN6_ADDR)
	} else if( sin->sa_family == AF_INET6 ){
		port = ((struct sockaddr_in6 *)sin)->sin6_port;
#endif
	} else {
		fatal( LOG_INFO, _("Service_connection: bad protocol family '%d'"), sin->sa_family );
	}
	DEBUG2("Service_connection: socket %d, ip '%s' port %d", talk,
		inet_ntop_sockaddr( sin, buffer, sizeof(buffer) ), ntohs( port ) );
	Perm_check.port =  ntohs(port);

	/* see if you need to reread the printcap and permissions information */
	if( !Use_info_cache ){
		Read_pc();
	}

	Perm_check.service = 'X';
	Init_perms_check();
	if( (permission = Perms_check( &Perm_file, &Perm_check,
			Cfp_static )) == REJECT
		|| (permission == 0 && Last_default_perm == REJECT) ){
		DEBUG2("Service_connection: talk socket '%d' no connect perms", talk );
		Write_fd_str( talk, _("\001no connect permissions\n") );
		cleanup(0);
	}
	switch( input[0] ){
		default:
			fatal( LOG_INFO, _("Service_connection: bad request line '\\%d'%s'"),
				input[0], input+1 );
			break;
		case REQ_START:
			/* simply send a 0 ACK and close connection - NOOP */
			Write_fd_len( talk, "", 1 );
			break;
		case REQ_RECV:
			Receive_job( &talk, input, sizeof(input),
				Send_job_rw_timeout );
			break;
		case REQ_DSHORT:
		case REQ_DLONG:
		case REQ_VERBOSE:
			Job_status( &talk, input, sizeof(input) );
			break;
		case REQ_REMOVE:
			Job_remove( &talk, input, sizeof(input) );
			break;
		case REQ_CONTROL:
			Job_control( &talk, input, sizeof(input) );
			break;
		case REQ_BLOCK:
			Receive_block_job( &talk, input, sizeof(input),
				Send_job_rw_timeout );
			break;
		case REQ_SECURE:
			Receive_secure( &talk, input, sizeof(input),
				Send_job_rw_timeout );
			break;
	}
	cleanup(0);
}

/***************************************************************************
 * Reinit()
 * Reinitialize the database/printcap/permissions information
 * 1. free any allocated memory
 ***************************************************************************/

static void Reinit(void)
{
	Reread_config = 1;
	(void) plp_signal (SIGHUP,  (plp_sigfunc_t)Reinit);
}



/***************************************************************************
 * Read_pc()
 * Update the printcap information and permission information
 * 1. free any allocated memory
 * 2. we read the printcap information from files
 * 3. query for any "all" entry
 *    Note: be careful with the 'all' entry, you may need to update it.
 ***************************************************************************/

static void Read_pc(void)
{
	/* you might as well reset all the logging information as well */
	DEBUG1( "Read_pc: starting" );
	Free_printcap_information();
	Free_perms( &Perm_file );
	Free_perms( &Local_perm_file );
#if defined(DMALLOC)
	Write_fd_str(dmalloc_outfile, "**** After Freeing Memory in Read_pc\n");
	dmalloc_log_unfreed();
#endif
	Get_all_printcap_entries();
	Get_perms( "all", &Perm_file, Printer_perms_path );
	DEBUG1( "Read_pc: done" );
}


/***************************************************************************
 * Service_printer( int talk )
 *  Read the printer to be started from the talk socket
 * 1. fork a connection
 * 2. Mother:  close talk and return
 * 2  Child:  close listen
 * 2  Child:  read input line and decide what to do
 *
 ***************************************************************************/

void Service_printer( int talk )
{
	char *name, *s, *end;
	char line[LINEBUFFER];
	int n, len, pid;

	/* get the line */

	for( len = 0; len < sizeof(line)-1 && (n = read(talk, &line[len],1)) > 0;++len ){
		if( line[len] == '\n' ) break;
	}
	line[len] = 0;
	DEBUG0("Service_printer: line len %d, read '%s'", len, line );
	if( len <= 0 ){
		return;
	}
	/* process the list of names on the command line */
	for( name = line+1; name && *name; name = end ){
		end = strpbrk( name, ",; \t" );
		if( end ) *end++ = 0;
		if( (s = Clean_name( name )) ){
			DEBUG0( "Service_printer: bad character '%c' in printer name '%s'",
				*s, name );
			continue;
		}
		proctitle( "lpd %s '%s'", "STARTING", name );
		/*
		 * if we are starting 'all' then we need to start subprocesses
		 */
		if( strcmp( name, "all" ) == 0){
			/* we start all of them */
			Start_all();
		} else {
			pid = dofork(1);
			if( pid < 0 ){
				logerr( LOG_INFO, _("Service_printer: dofork failed") );
				break;
			} else if( pid == 0 ){
				/* child */
				Do_queue_jobs( name );
				cleanup(0);
			}
		}
	}
}

/***************************************************************************
 * Get_lpd_pid() and Set_lpd_pid()
 * Get and set the LPD pid into the LPD status file
 ***************************************************************************/

static int Get_lpd_pid(void)
{
	int pid;
	int lockfd;
	char path[MAXPATHLEN];
	struct stat statb;

	plp_snprintf( path, sizeof(path)-1, "%s.%s", Lockfile, Lpd_port );
	pid = -1;
	lockfd = Checkread( path, &statb );
	if( lockfd >= 0 ){
		pid = Read_pid( lockfd, (char *)0, 0  ); 
	}
	return(pid);
}

static void Set_lpd_pid(void)
{
	int lockfd;
	char path[MAXPATHLEN];
	struct stat statb;

	plp_snprintf( path, sizeof(path)-1, "%s.%s", Lockfile, Lpd_port );
	lockfd = Checkwrite( path, &statb, O_WRONLY|O_TRUNC, 1, 0 );
	if( lockfd < 0 ){
		logerr_die( LOG_ERR, _("lpd: Cannot open '%s'"), Lockfile );
	} else {
		/* we write our PID */
		Server_pid = getpid();
		DEBUG1( "lpd: writing lockfile '%s' with pid '%d'",path,Server_pid );
		Write_pid( lockfd, Server_pid, (char *)0 );
	}
	close( lockfd );
}
