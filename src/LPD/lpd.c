/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
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
"$Id: lpd.c,v 3.3 1996/08/31 21:11:58 papowell Exp papowell $";

#include "lpd.h"
#include "printcap.h"
#include "lp_config.h"
#include "pr_support.h"
#include "permission.h"
#include "setuid.h"

static void Get_remote_hostbyaddr( struct sockaddr_in *sin );
static void Service_connection( struct sockaddr_in *sin, int listen, int talk );
static void Read_pc();
static void Set_lpd_pid();
static int Get_lpd_pid();
int Setuplog(char *logfile, int sock);
void Service_printer( int listen, int talk );

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
	int status;			/* status of operation */
	int pid;			/* pid */
	fd_set defreadfds, readfds;	/* for select() */
	int max_socks;		/* maximum number of sockets */
	char *s;			/* ACME, for the latest fashions in pointers */
	struct sockaddr_in sin;	/* the connection remote IP address */
	struct timeval delay;	/* timeout if needed */
	pid_t result;		/* process termination information */
	int l;				/* ACME?  Hmmm... well, ok */
	int err;

	/*
	 * This will make things safe even
	 * if not started by root set the UID to user
	 * will make things safe if the program is running SUID and
	 * not started by root.  If it is started by root, it is
	 * irrelevant
	 */
	Initialize();

	/*
		open /dev/null on fd 0, 1, 2 if neccessary
		This must be done before using any other database access
		functions,  as they may open a socket and leave it open.
	*/
	if( (DevNullFD = open( "/dev/null", O_RDWR, Spool_file_perms )) < 0 ){
		logerr_die( LOG_CRIT, "lpd: main cannot open '/dev/null'" );
	}
	while( DevNullFD < 3 ){
		if( (DevNullFD = dup(DevNullFD)) < 0 ){
			logerr_die( LOG_CRIT, "lpd: main cannot dup '/dev/null'" );
		}
	}

	/* scan the argument list for a 'Debug' value */
	Opterr = 0;
	Get_debug_parm( argc, argv, LPD_optstr, debug_vars );
	/* scan the input arguments, setting up values */
	Opterr = 1;
	Get_parms(argc, argv);      /* scan input args */

	/* fix up things if Inetd started the LPD server */
	if( Inetd_started ){
		sock = dup(0);
		if( sock < 3 ){
			logerr_die( LOG_CRIT, "lpd: main - dup result bad value '%d'",
				sock );
		}
		dup2(DevNullFD,0); dup2(DevNullFD,1); dup2(DevNullFD,2);
		Link_setreuse( sock );

#ifdef INETD_DEBUG
		/* set up debugging for INETD startup */
		close(2);
		if( open( "/tmp/logfile", O_WRONLY|O_APPEND|O_CREAT, Spool_file_perms ) != 2 ){
			dup2( DevNullFD,2);
		}
#endif
	}

	/* set signal handlers */
	(void) plp_signal (SIGPIPE, (plp_sigfunc_t)SIG_IGN);
	(void) plp_signal (SIGHUP,  (plp_sigfunc_t)Read_pc);
	(void) plp_signal (SIGINT, cleanup);
	(void) plp_signal (SIGQUIT, cleanup);
	(void) plp_signal (SIGTERM, cleanup);


	/* Get configuration file information */
	Parsebuffer( "default configuration",Default_configuration,
		lpd_all_config, &Config_buffers );

	/* get the configuration file information if there is any */
    if( Allow_getenv ){
		if( UID_root ){
			fprintf( stderr,
			"%s: WARNING- LPD_CONF environment variable option enabled\n"
			"  and running as root!  You have an exposed security breach!\n"
			"  Recompile without -DGETENV\n", Name );
		}
		if( (s = getenv( "LPD_CONF" )) ){
			if( UID_root ){
				log( LOG_ERR, "lpd: ROOT and using LPD_CONF file '%s'", s );
			}
			Server_config_file = s;
		}
    }

	DEBUG0("main: Configuration file '%s'",
		Server_config_file?Server_config_file:"NULL" );

	Getconfig( Server_config_file, lpd_all_config, &Config_buffers );

	if( Debug > 5 ) dump_config_list( "LPD Configuration", lpd_all_config );

	/* 
	 * Testing magic:
	 * if we are running SUID
	 *   We have set our RUID to root and EUID daemon
	 * However,  we may want to run as another UID for testing.
	 * The config file allows us to do this, but we set the SUID values
	 * from the hardwired defaults before we read the configuration file.
	 * After reading the configuration file,  we check the current
	 * DaemonUID and the requested Daemon UID.  If the requested
	 * Daemon UID == 0, then we run as the user which started LPD.
	 */

	Reset_daemonuid();
	Setdaemon_group();
	DEBUG4( "DaemonUID %d", DaemonUID );
	
	/* get the fully qualified domain name of host and the
		short host name as well
		FQDN - fully qualified domain name
		Host - actual one to use in H fields
		ShortHost - short host name
		NOTE: on PCs this will be the IP address
	*/

	Get_local_host();

	/* expand the information in the configuration file */
	Expandconfig( lpd_all_config, &Config_buffers );

	if( Debug > 4 ) dump_config_list( "LPD Configuration After Expansion",
		lpd_all_config );
	if( Debug > 4 ) dump_config_list("LPD Vars", lpd_vars );

	/*
	 * now get the connection
	 */
	if( Lockfile == 0 ){
		logerr_die( LOG_INFO, "No LPD lockfile specified!" );
	}

	if( !Inetd_started ){
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
				Diemsg( "Another print spooler is using TCP printer port, possibly lpd process '%d'",
					pid );
			} else {
				Diemsg( "Another print spooler is using TCP printer port" );
			}
		}
	}

	/*
	 * At this point you are the server for the LPD port
	 * If not started from Inet_d and not in Forground
	 * you need to fork to allow the regular user to continue
	 * you put the child in its separate process group as well
	 */

	if( !Inetd_started && !Foreground ){
		if( (pid = dofork()) < 0 ){
			logerr_die( LOG_ERR, "lpd: main() fork failed" );
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


	/* read the printcap file */
	Read_pc();
	if( Debug > 4 ) dump_printcapfile("Printcap_file", &Printcapfile );

	/* establish the pipes for low level processes to use */
	if( pipe( Lpd_pipe ) ){
		logerr_die( LOG_ERR, "lpd: pipe call failed" );
	}

	/*
	 * If you were asked to start and run a single queue, then
	 * start it up.  This allows you to test a filter by placing
	 * a short job in the queue and then printing it.
	 */
	if( Printer ){
		char b[LINEBUFFER];
		b[0] = '!';
		strncpy(b+1, Printer, sizeof(b)-2);
		Process_jobs( 0, b, sizeof(b) );
		exit(0);
	}
	/* open a connection to logger */
	send_to_logger( 0 );

	/*
	 * clean out the current queues
	 */
	if( !Inetd_started ){
		pid = fork();
		if( pid < 0 ){
			logerr_die( LOG_INFO, "Lpd: fork in main failed" );
		} else if( pid == 0 ){
			static char start[] = "!all\n";

			(void) plp_signal (SIGHUP, cleanup );
			Process_jobs( 0, start, sizeof(start) );
			exit(Errorcode);
		}
	}


	/* set up the wait activity */

	FD_ZERO( &defreadfds );
	if( !Inetd_started ){
		FD_SET( sock, &defreadfds );
	}
	FD_SET( Lpd_pipe[0], &defreadfds );
	max_socks = sock+1;
	if( Lpd_pipe[0] >= sock ){
		max_socks = Lpd_pipe[0]+1;
	}

	/*
	 * start waiting for connections from processes
	 */

	if( Inetd_started ){

		if( getpeername( sock, (struct sockaddr *)&sin, &l ) ){
			logerr_die( LOG_ERR, "lpd: getpeername failed" );
		}
		/* service the connection */
		Service_connection( &sin, -1, sock );

		/* close the connection, wait for process to finish */
		close( sock );
		close( Lpd_pipe[1] );
		while(1){
			delay.tv_sec = 10;	/* 10 seconds wait */
			delay.tv_usec = 0;
			readfds = defreadfds;
			l = select( max_socks, (fd_set *)&readfds,
				(fd_set *)0, (fd_set *)0, &delay );
			err = errno;
			if( l < 0 ){
				if( err != EINTR ){
					logerr_die( LOG_ERR, "lpd: select error!");
				}
			} else if( l == 0 ){
				DEBUG0( "lpd: time out" );
				while( (result = plp_waitpid( -1, &status, WNOHANG )) > 0 ){
					err = errno;
					DEBUG8( "lpd: timeout waitpid %d, '%s'",
						result, Errormsg(err) );
					removepid( result );
				}
				err = errno;
				DEBUG8( "lpd: timeout waitpid %d, '%s'",
					result, Errormsg(err) );
				if( result == -1 && err == ECHILD ){
					DEBUG0( "lpd: exiting" );
					cleanup(0);
					exit(0);
				}
			} else {
				Service_printer( -1, Lpd_pipe[0] );
			}
		}
	}

	/* now the regular server */

	do{
		while( (result = plp_waitpid( -1, &status, WNOHANG )) > 0 ){
			err = errno;
			DEBUG8( "lpd: server process pid %d exited", result );
			removepid( result );
		}
		err = errno;
		DEBUG8( "lpd: waitpid returns %d, '%s'", result,
			result>0? Errormsg(err) : "no error" );
		readfds = defreadfds;

		DEBUG2( "lpd: starting select" );
		l = select( max_socks, (fd_set *)&readfds,
				(fd_set *)0, (fd_set *)0, (void *)0 );
		err = errno;
		if( l < 0 ){
			if( err != EINTR ){
				errno = err;
				logerr_die( LOG_ERR, "lpd: select error!");
			}
		} else if( l == 0 ){
			DEBUG0( "lpd: signal or time out" );
		} else {
			if( FD_ISSET( sock, &readfds ) ){
				l = sizeof( sin );
				DEBUG4("accepting connection on %d", sock );
				newsock = accept( sock, (struct sockaddr *)&sin, &l );
				err = errno;
				DEBUG4("connection on %d", newsock );

				if( newsock >= 0 ){
					Service_connection( &sin, sock, newsock );
				} else {
					errno = err;
					logerr(LOG_INFO, "lpd: accept on listening socket failed" );
				}
			}
			if( FD_ISSET( Lpd_pipe[0], &readfds ) ){
				Service_printer( sock, Lpd_pipe[0] );
			}
		}
	}while( 1 );
	cleanup(0);
	exit(0);
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

	DEBUG8("Setuplog: logfile '%s', sock %d", logfile, sock );
	while( sock < 2 ){
		sock = dup(sock);
	}
	if( sock < 0 ){
		logerr_die( LOG_CRIT, "Setuplog: dup of %d failed", sock );
	}
    /*
     * set stdin, stdout to /dev/null
     */
	if ((fd = open("/dev/null", O_RDWR, Spool_file_perms)) < 0) {
	    logerr_die(LOG_ERR, "Setuplog: open /dev/null failed");
	}
	close( 0 ); close( 1 );
	if( dup2( fd, 0 ) < 0 || dup2( fd, 1 ) < 0 ){
		logerr_die (LOG_CRIT, "Setuplog: dup2 failed");
	}
	close(fd);
	DEBUG4 ("Setuplog: log file '%s'", logfile?logfile:"<NULL>");
    /*
     * open logfile; if it is "-", use stderr; if Foreground is set, use stderr
     */
    if(logfile && *logfile && strcmp(logfile, "-")
		&& !Foreground) {
		DEBUG2 ("Setuplog: opening log file '%s'", logfile);
		if ((fd = Checkwrite(logfile, &statb, O_WRONLY|O_APPEND, 0, 0)) < 0) {
			err = errno;
			DEBUG0 ("cannot open logfile %s - %s", logfile, Errormsg(err));
			if( Debug ) {
				fd = 2;
			} else {
				fd = 1;
			}
		}
		DEBUG4 ("Setuplog: log file '%s' fd '%d'", logfile, fd);
		if (fd != 2 ){
			if( dup2 (fd, 2) < 0) {
				logerr_die (LOG_CRIT, "Setuplog: dup2 of %d failed", fd );
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

static void Service_connection( struct sockaddr_in *sin, int listen, int talk )
{
	int pid;		/* PID of the server process */
	char input[LINEBUFFER];
	int len;
	int status;		/* status of operation */
	char *s;

	pid = fork();
	if( pid < 0 ){
		logerr( LOG_INFO, "Service_connection: fork failed" );
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

	/* run an initial screening permissions check */
	DEBUG3("Service Connection: socket %d, ip '%s' port %d", talk,
		inet_ntoa( sin->sin_addr ), ntohs( sin->sin_port ) );

	/* see if you need to reread the printcap and permissions information */
	if( !Use_info_cache ){
		Read_pc();
	}

	Get_remote_hostbyaddr( sin );
	Perm_check.port =  ntohs(sin->sin_port);
	Perm_check.remoteip  =  Perm_check.ip  =  ntohl(sin->sin_addr.s_addr);
	Perm_check.host = FQDNRemote;
	Perm_check.remotehost = FQDNRemote;
	Perm_check.service = 'X';
	Init_perms_check();
	if( Perms_check( &Perm_file, &Perm_check,
			(struct control_file *)0 ) == REJECT
		|| Last_default_perm == REJECT ){
		char msg[128];
		msg[sizeof(msg)-1] = 0;
		plp_snprintf( msg, sizeof(msg)-1,
			" no connect permissions for IP/port %s/%d\n",
			inet_ntoa( sin->sin_addr ), ntohs( sin->sin_port ) );
		Write_fd_str( talk, msg );
		cleanup(0);
		exit( Errorcode );
	}

	len = sizeof( input ) - 1;
	input[len] = 0;
	DEBUG3( "Starting Read" );
	status = Link_line_read(ShortRemote,&talk,Send_timeout,input,&len);
	if( status ){
		logerr_die( LOG_INFO, "Service_connection: cannot read request" );
	}
	if( len < 3 ){
		fatal( LOG_INFO, "Service_connection: bad request line '\\%d'%s'",
			input[0], input+1 );
	}
	DEBUG1( "Request '\\%d'%s'", input[0], input+1 );
	switch( input[0] ){
		default:
			fatal( LOG_INFO, "Service_connection: bad request line '\\%d'%s'",
				input[0], input+1 );
			break;
		case REQ_START:
			Perm_check.service = 'P';
			Perm_check.printer = input+1;
			if( (s = strchr( input+1, '\n' )) ) *s = 0;
			Init_perms_check();
			if( Perms_check( &Perm_file, &Perm_check,
					(struct control_file *)0 ) == REJECT
				|| Last_default_perm == REJECT ){
				fatal( LOG_ERR,
					"Service_connection: no permission to start printer %s",
					input+1 );
			}
			Process_jobs( &talk, input, sizeof(input) );
			break;
		case REQ_RECV:
			Receive_job( &talk, input, sizeof(input) );
			break;
		case REQ_DSHORT:
		case REQ_DLONG:
			Job_status( &talk, input, sizeof(input) );
			break;
		case REQ_REMOVE:
			Job_remove( &talk, input, sizeof(input) );
			break;
		case REQ_CONTROL:
			Job_control( &talk, input, sizeof(input) );
			break;
	}
	exit(0);
}

/***************************************************************************
 * void Get_remote_hostbyaddr( struct sockaddr_in *sin );
 * 1. Get the remote host IP address using getpeername()
 * 2. look up the address using gethostbyaddr()
 * 3. if not found, use the IP address as the host name
 * 4. set ShortRemote and FQDNRemote
 ***************************************************************************/
 
static void Get_remote_hostbyaddr( struct sockaddr_in *sin )
{
	struct hostent *host_ent;
	char *name, *ip;

	ip = inet_ntoa( sin->sin_addr );
	IPRemote = safestrdup( ip );

	host_ent = gethostbyaddr( (char *)&sin->sin_addr,
		sizeof( sin->sin_addr ), sin->sin_family );

	if( host_ent ){
		name = safestrdup( host_ent->h_name );
		FQDNRemote = Find_fqdn( name, Domain_name );
		FQDNRemote = safestrdup( FQDNRemote );
		ShortRemote = safestrdup( FQDNRemote );
		if( (name = strchr( ShortRemote, '.' )) ) *name = 0;
	} else {
		FQDNRemote = IPRemote;
		ShortRemote = IPRemote;
	}
	DEBUG6( "Get_remote_hostbyaddr: IP address '%s', FQDNRemote '%s', ShortRemote '%s'",
		IPRemote, FQDNRemote, ShortRemote );
}


/***************************************************************************
 * Read_pc()
 * Update the printcap information and permission information
 * 1. free any allocated memory
 * 2. we read the printcap information from files
 * 3. query for any "all" entry
 *    Note: be careful with the 'all' entry, you may need to update it.
 ***************************************************************************/

static void Read_pc()
{
	char error[LINEBUFFER];
	Free_pcf( &Printcapfile );
	if( Printcap_path && *Printcap_path ){
		Getprintcap( &Printcapfile, Printcap_path, 0 );
	}
	if( Lpd_printcap_path && *Lpd_printcap_path ){
		Getprintcap( &Printcapfile, Lpd_printcap_path, 0 );
	}
	Get_printer_vars( "all", error, sizeof(error),
		&Printcapfile, &Pc_var_list, Default_printcap_var,
		(void *)0 );
	if( Printer_perms_path && *Printer_perms_path ){
		Free_perms( &Perm_file );
		Get_perms( "all", &Perm_file, Printer_perms_path );
	}
}


/***************************************************************************
 * Service_printer( int listen, int talk )
 *  Read the printer to be started from the talk socket
 * 1. fork a connection
 * 2. Mother:  close talk and return
 * 2  Child:  close listen
 * 2  Child:  read input line and decide what to do
 *
 ***************************************************************************/

void Service_printer( int listen, int talk )
{
	int pid;		/* PID of the server process */
	char line[LINEBUFFER];
	int len;

	/* get the line */

	line[0] = 0;
	len = read( talk, line, sizeof( line )-1 );
	if( len >= 0 ) line[len] = 0;
	DEBUG4("Service_printer: line len %d, read '%s'", len, line );
	if( len < 0 ){
		return;
	}
	/* NOW we fork */

	pid = fork();
	if( pid < 0 ){
		logerr( LOG_INFO, "Service_printer: fork failed" );
	}
	if( pid < 0 || pid > 0 ){
		return;
	}
	/*
	 * daughter process 
	 * - close the sockets
	 */
	(void) plp_signal (SIGHUP, cleanup );
	close( listen ); listen = -1;
	close( talk ); talk = -1;

	/*
	 * we now service the requested printer
	 */
	Process_jobs( 0, line, len );
	exit(0);
}

/***************************************************************************
 * Get_lpd_pid() and Set_lpd_pid()
 * Get and set the LPD pid into the LPD status file
 ***************************************************************************/

static int Get_lpd_pid()
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

static void Set_lpd_pid()
{
	int lockfd;
	char path[MAXPATHLEN];
	struct stat statb;

	plp_snprintf( path, sizeof(path)-1, "%s.%s", Lockfile, Lpd_port );
	lockfd = Checkwrite( path, &statb, O_WRONLY|O_TRUNC, 1, 0 );
	if( lockfd < 0 ){
		logerr_die( LOG_ERR, "lpd: Cannot open '%s'", Lockfile );
	} else {
		/* we write our PID */
		Server_pid = getpid();
		DEBUG2( "lpd: writing lockfile '%s' with pid '%d'",path,Server_pid );
		Write_pid( lockfd, Server_pid, (char *)0 );
	}
	close( lockfd );
}
