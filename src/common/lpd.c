/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: lpd.c,v 5.2 1999/10/04 17:00:54 papowell Exp papowell $";


#include "lp.h"
#include "child.h"
#include "fileopen.h"
#include "gethostinfo.h"
#include "getopt.h"
#include "getprinter.h"
#include "getqueue.h"
#include "initialize.h"
#include "linksupport.h"
#include "patchlevel.h"
#include "permission.h"
#include "proctitle.h"
#include "errorcodes.h"
#include "krb5_auth.h"
#include "lpd_secure.h"

/* force local definitions */
#undef EXTERN
#undef DEFINE
#undef DEFS

#define EXTERN
#define DEFINE(X) X
#define DEFS

#include "lpd.h"
#include "lpd_remove.h"
#include "lpd_logger.h"
#include "lpd_jobs.h"
#include "lpd_rcvjob.h"
#include "lpd_control.h"
#include "lpd_status.h"


/**** ENDINCLUDE ****/

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
	int pid;			/* pid */
	fd_set defreadfds, readfds;	/* for select() */
	struct timeval timeval, *timeout;
	int timeout_encountered = 0;	/* we have a timeout */
	int max_socks;		/* maximum number of sockets */
	int n, m;	/* ACME?  Hmmm... well, ok */
	int err;
 	time_t last_time;	/* time that last Start_all was done */
 	time_t this_time;	/* current time */
	plp_status_t status;
	int max_servers = 0;
	int start_fd = 0;
	int status_pid = 0;
	int request_pipe[2], status_pipe[2];
	struct line_list args;
	char *s;

	Init_line_list( &args );
	Is_server = 1;	/* we are the LPD server */
	Logger_fd = -1;

#ifndef NODEBUG
	Debug = 0;
#endif
	if(DEBUGL3){
		logDebug("lpd: argc %d", argc );
		for( n = 0; n < argc; ++n ){
			logDebug(" [%d] '%s'", n, argv[n] );
		}
		logDebug("lpd: env" );
		for( n = 0; envp[n]; ++n ){
			logDebug(" [%d] '%s'", n, envp[n] );
		}
	}

	/* set signal handlers */
	(void) plp_signal (SIGHUP,  (plp_sigfunc_t)Reinit);
	(void) plp_signal (SIGINT, cleanup_INT);
	(void) plp_signal (SIGQUIT, cleanup_QUIT);
	(void) plp_signal (SIGTERM, cleanup_TERM);
	(void) plp_signal (SIGUSR1, (plp_sigfunc_t)SIG_IGN);
	(void) plp_signal (SIGUSR2, (plp_sigfunc_t)SIG_IGN);

	Get_parms(argc, argv);      /* scan input args */

	Initialize(argc, argv, envp);
	Setup_configuration();

	if( Worker_LPD ){
		Lpd_worker(argv,argc,Optind);
		cleanup(0);
	}


	if( Lockfile_DYN == 0 ){
		logerr_die( LOG_INFO, _("No LPD lockfile specified!") );
	}

	/* set up the log file and standard environment - do not
	   fool around with anything but fd 0,1,2 which should be safe
		as we made sure that the fd 0,1,2 existed.
    */

	Setup_log( Logfile_LPD );

#if defined(HAVE_KRB5_H)
	if(DEBUGL3){
		char buffer[LINEBUFFER];
		remote_principal_krb5( Kerberos_service_DYN, 0, buffer, sizeof(buffer) );
		logDebug("lpd: kerberos principle '%s'", buffer );
	}
#endif

	/* chdir to the root directory */
	if( chdir( "/" ) == -1 ){
		Errorcode = JABORT;
		logerr_die( LOG_ERR, "cannot chdir to /");
	}

	/*
	 * This is the one of the two places where we need to be
	 * root in order to open a socket
	 */

	sock = Link_listen();
	DEBUG1("lpd: listening socket fd %d",sock);
	if( sock < 0 ){
		/*
		 * try reading the lockfile
		 */
  		pid = Get_lpd_pid();
 		if( pid > 0 && kill(pid,0) ) pid = 0;
		Errorcode = JACTIVE;
  		if( pid > 0 ){
  			Diemsg( _("Another print spooler is using TCP printer port, possibly lpd process '%d'"),
  				pid );
  		} else {
 			if( !UID_root ){
 				Diemsg("Not running with ROOT perms and trying to open port '%s'", Lpd_port_DYN );
 			}
 			Diemsg( _("Another print spooler is using TCP printer port - not LPRng") );
  		}
		Errorcode = JSUCC;
	}

	/* setting nonblocking on the listening fd
	 * will prevent a problem with terminations of connections
	 * before ACCEPT has completed
	 *  1. user connects, does the 3 Way Handshake
	 *  2. before accept() is done,  a RST packet is sent
	 *  3. a select() will succeed, but the accept() will hang
	 *  4. if the non-blocking mode is used, then the select will
	 *     succeed and the accept() will fail
	 */
	Set_nonblock_io(sock);

	/*
	 * At this point you are the server for the LPD port
	 * you need to fork to allow the regular user to continue
	 * you put the child in its separate process group as well
	 */

	Name = "MAIN";
	setproctitle( "lpd %s", Name  );

	if( (pid = dofork(1)) < 0 ){
		logerr_die( LOG_ERR, _("lpd: main() dofork failed") );
	} else if( pid ){
		if( Foreground_LPD ){
			while( (pid = plp_waitpid( pid, &status, 0)) > 0 ){
				DEBUG1( "lpd: process %d, status '%s'",
					pid, Decode_status(&status));
			}
		}
		Errorcode = 0;
		exit(0);
	}
	Name = "Waiting";
	setproctitle( "lpd %s", Name  );

	/*
	 * Write the PID into the lockfile
	 */

	Set_lpd_pid();

	/* establish the pipes for low level processes to use */
	if( pipe( request_pipe ) == -1 ){
		logerr_die( LOG_ERR, _("lpd: pipe call failed") );
	}
	DEBUG2( "lpd: fd request_pipe(%d,%d)",request_pipe[0],request_pipe[1]);
	Lpd_request = request_pipe[1];

	Logger_fd = -1;
	if( Logger_destination_DYN ){
		if( pipe( status_pipe ) == -1 ){
			logerr_die( LOG_ERR, _("lpd: pipe call failed") );
		}
		Logger_fd = status_pipe[1];
		DEBUG2( "lpd: fd status_pipe(%d,%d)",status_pipe[0],status_pipe[1]);
		status_pid = Start_logger( status_pipe[0] );
		if( status_pid <= 0 ){
			logerr_die( LOG_ERR, "lpd: cannot fork logger process");
		}
		DEBUG1("lpd: status_pid %d", status_pid );
	}

	/* open a connection to logger */
	setmessage(0,LPD,"Starting");

	/* get the maximum number of servers allowed */
	max_servers = Get_max_servers();
	DEBUG3( "lpd: maximum servers %d", max_servers );

	/*
	 * clean out the current queues
	 */
 	last_time = time( (void *)0 );

	/* set up the wait activity */

	FD_ZERO( &defreadfds );
	FD_SET( sock, &defreadfds );
	FD_SET( request_pipe[0], &defreadfds );

	/*
	 * start waiting for connections from processes
	 */

	last_time = time( (void *)0 );
	start_fd = Start_all();


	do{
		DEBUG1("lpd: LOOP START");
		if(DEBUGL3){ int fd; fd = dup(0); logDebug("lpd: next fd %d",fd); close(fd); };

		timeout = 0;
		DEBUG2( "lpd: Poll_time %d, Force_poll %d, start_fd %d, Started_server %d",
			Poll_time_DYN, Force_poll_DYN, start_fd, Started_server );
		if(DEBUGL2)Dump_line_list("lpd - Servers_line_list",&Servers_line_list );
		if( Poll_time_DYN > 0 && start_fd <= 0 && Servers_line_list.count == 0 ){
			memset(&timeval, 0, sizeof(timeval));
			this_time = time( (void *)0 );
			m = (this_time - last_time);
			timeval.tv_sec = Poll_time_DYN - m;
			timeout = &timeval;
			DEBUG2("lpd: from last poll %d, to next poll %d",
				m, (int)timeval.tv_sec );
			if( m >= Poll_time_DYN ){
				if( Started_server || Force_poll_DYN ){
					start_fd = Start_all();
					DEBUG1( "lpd: restarting poll, start_fd %d", start_fd);
					last_time = this_time;
					timeval.tv_sec = Poll_time_DYN;
				} else {
					DEBUG1( "lpd: no poll" );
					timeout = 0;
				}
			}
		}

		/*
		 * collect zombies 
		 */

		while( (pid = plp_waitpid( -1, &status, WNOHANG)) > 0 ){
			DEBUG1( "lpd: process %d, status '%s'",
				pid, Decode_status(&status));
			if( status_pid > 0 && pid == status_pid ){
				DEBUG1( "lpd: restaring logger process");
				while( (status_pid = Start_logger( status_pipe[0] )) < 0 ){
					DEBUG1("lpd: could not start process - %s",
						Errormsg(errno) );
					status_pid = plp_waitpid( -1, &status, 0);
					DEBUG1( "lpd: process %d, status '%s'",
						status_pid, Decode_status(&status));
				}
				DEBUG1("lpd: status_pid %d", status_pid );
			}
		}
		n = Countpid();
		max_servers = Get_max_servers();
		DEBUG1("lpd: max_servers %d, active %d", max_servers, n );
		while( Servers_line_list.count > 0 && n < max_servers ){ 
			s = Servers_line_list.list[0];
			DEBUG1("lpd: starting server '%s'", s );
			Free_line_list( &args );
			Set_str_value(&args,PRINTER,s);
			Set_str_value(&args,CALL,QUEUE);
			pid = Start_worker( &args, 0 );
			Free_line_list(&args);
			if( pid > 0 ){
				Remove_line_list( &Servers_line_list, 0 );
				++Started_server;
				++n;
			} else {
				break;
			}
		}

		/* do not accept incoming call if no worker available */
		readfds = defreadfds;
		if( n >= max_servers ){
			DEBUG1( "lpd: not accepting requests", sock );
			FD_CLR( sock, &readfds );
		}

		max_socks = sock+1;
		if( request_pipe[0] >= max_socks ){
			max_socks = request_pipe[0]+1;
		}
		if( start_fd > 0 ){
			FD_SET( start_fd, &readfds );
			if( start_fd >= max_socks ){
				max_socks = start_fd+1;
			}
		}

		DEBUG1( "lpd: starting select timeout '%s', %d sec, max_socks %d",
		timeout?"yes":"no", (int)(timeout?timeout->tv_sec:0), max_socks );
		if(DEBUGL2){
			int i;
			for(i=0; i < max_socks; ++i ){
				if( FD_ISSET( i, &readfds ) ){
					logDebug( "lpd: waiting for fd %d to be readable", i );
				}
			}
		}
		Setup_waitpid_break();
		errno = 0;
		m = select( max_socks,
			FD_SET_FIX((fd_set *))&readfds,
			FD_SET_FIX((fd_set *))0,
			FD_SET_FIX((fd_set *))0, timeout );
		err = errno;
		Setup_waitpid();
		if(DEBUGL1){
			int i;
			logDebug( "lpd: select returned %d, error '%s'",
				m, Errormsg(err) );
			for(i=0; i < max_socks; ++i ){
				if( FD_ISSET( i, &readfds ) ){
					logDebug( "lpd: fd %d readable", i );
				}
			}
		}
		/* if we got a SIGHUP then we reread configuration */
		if( Reread_config || !Use_info_cache_DYN ){
			DEBUG1( "lpd: rereading configuration" );
			/* we need to force the LPD logger to use new printcap information */
			if( Reread_config ){
				if( status_pid > 0 ) kill( status_pid, SIGINT );
				setmessage(0,LPD,"Restart");
				Reread_config = 0;
			}
			Setup_configuration();
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
			DEBUG1( "lpd: signal or time out" );
			continue;
		}
		if( FD_ISSET( sock, &readfds ) ){
			int p[2];
			char b[32];
			if( pipe(p) == -1 ){
				logerr(LOG_INFO, _("lpd: pipe() failed") );
			}
			Free_line_list( &args );
			Lpd_ack_fd = p[1];
			Set_str_value(&args,CALL,SERVER);
			pid = Start_worker( &args, sock );
			if( pid < 0 ){
				logerr(LOG_INFO, _("lpd: fork() failed") );
			}
			Free_line_list(&args);
			Lpd_ack_fd = 0;
			close( p[1] );
			while( read( p[0], b, sizeof(b) ) > 0 );
			DEBUG1( "lpd: listener %d running", pid );
			close( p[0] );
		}
		if( FD_ISSET( request_pipe[0], &readfds ) 
			&& Read_server_status( request_pipe[0] ) == 0 ){
			Errorcode = JABORT;
			logerr_die(LOG_INFO, _("lpd: Lpd_request pipe EOF! cannot happen") );
		}
		if( start_fd > 0 && FD_ISSET( start_fd, &readfds ) ){
			start_fd = Read_server_status( start_fd );
		}
	}while( 1 );
	Free_line_list(&args);
	cleanup(0);
	return(0);
}

/***************************************************************************
 * Setup_log( char *logfile, int sock )
 * Purpose: to set up a standard error logging environment
 * saveme will prevent stdin from being clobbered
 *   1.  dup 'sock' to fd 0, close sock
 *   2.  opens /dev/null on fd 1
 *   3.  If logfile is "-" or NULL, output file is alread opened
 *   4.  Open logfile; if unable to, then open /dev/null for output
 ***************************************************************************/
void Setup_log(char *logfile )
{
	struct stat statb;

	close(0); close(1);
	if (open("/dev/null", O_RDONLY, 0) != 0) {
	    logerr_die(LOG_ERR, _("Setup_log: open /dev/null failed"));
	}
	if (open("/dev/null", O_WRONLY, 0) != 1) {
	    logerr_die(LOG_ERR, _("Setup_log: open /dev/null failed"));
	}

    /*
     * open logfile; if it is "-", use stderr; if Foreground is set, use stderr
     */
	if( fstat(2,&statb) == -1 && dup2(1,2) == -1 ){
		logerr_die(LOG_ERR, _("Setup_log: dup2(%d,%d) failed"), 1, 2);
	}
    if( logfile == 0 ){
		if( !Foreground_LPD && dup2(1,2) == -1 ){
			logerr_die(LOG_ERR, _("Setup_log: dup2(%d,%d) failed"), 1, 2);
		}
	} else if( safestrcmp(logfile, "-") ){
		close(2);
		if( Checkwrite(logfile, &statb, O_WRONLY|O_APPEND, 0, 0) != 2) {
			logerr_die(LOG_ERR, _("Setup_log: open %s failed"), logfile );
		}
	}
}

/***************************************************************************
 * Service_connection( struct line_list *args )
 *  Service the connection on the talk socket
 * 1. fork a connection
 * 2. Mother:  close talk and return
 * 2  Child:  close listen
 * 2  Child:  read input line and decide what to do
 *
 ***************************************************************************/

void Service_connection( struct line_list *args )
{
	char input[SMALLBUFFER];
	char buffer[LINEBUFFER];	/* for messages */
	int len, talk;
	int status;		/* status of operation */
	int permission, newsock, err;
	int port = 0;
	struct sockaddr sinaddr;

	Name = "SERVER";
	setproctitle( "lpd %s", Name );
	(void) plp_signal (SIGHUP, cleanup );

	if( !(talk = Find_flag_value(args,INPUT,Value_sep)) ){
		Errorcode = JABORT;
		fatal(LOG_ERR,"Service_connection: no talk fd"); 
	}

	Free_line_list(args);
	len = sizeof( sinaddr );
	DEBUG1("Service_connection: listening fd %d", talk );
	newsock = accept( talk, &sinaddr, &len );
	err = errno;
	DEBUG1("Service_connection: connection fd %d", newsock );

	if( newsock > 0 ){
		if( dup2( newsock, talk ) == -1 ){
			logerr_die( LOG_INFO, "Service_connection: dup2() failed!" );
		}
		if( newsock != talk ){
			close( newsock );
		}
	} else {
		errno = err;
		logerr_die(LOG_INFO, _("Service_connection: accept on listening socket failed") );
	}

	if( Lpd_ack_fd ){
		close( Lpd_ack_fd );
		Lpd_ack_fd = -1;
	}

	Set_block_io(talk);

	/* get the remote name and set up the various checks */
	Perm_check.addr = &sinaddr;
	Get_remote_hostbyaddr( &RemoteHost_IP, &sinaddr );
	Perm_check.remotehost  =  &RemoteHost_IP;
	Perm_check.host = &RemoteHost_IP;

	if( sinaddr.sa_family == AF_INET ){
		port = ((struct sockaddr_in *)&sinaddr)->sin_port;
#if defined(IN6_ADDR)
	} else if( sinaddr.sa_family == AF_INET6 ){
		port = ((struct sockaddr_in6 * )&sinaddr)->sin6_port;
#endif
	} else {
		fatal( LOG_INFO, _("Service_connection: bad protocol family '%d'"), sinaddr.sa_family );
	}
	Perm_check.port =  ntohs(port);
	DEBUG2("Service_connection: socket %d, ip '%s' port %d", talk,
		inet_ntop_sockaddr( &sinaddr, buffer, sizeof(buffer) ), ntohs( port ) );

	len = sizeof( input ) - 1;
	memset(input,0,sizeof(input));
	DEBUG1( "Service_connection: starting read on fd %d", talk );

	status = Link_line_read(ShortRemote_FQDN,&talk,
		Send_job_rw_timeout_DYN,input,&len);
	if( len >= 0 ) input[len] = 0;
	DEBUG1( "Service_connection: read status %d, len %d, '%s'",
		status, len, input );
	if( len == 0 ){
		DEBUG3( "Service_connection: zero length read" );
		cleanup(0);
	}
	if( status ){
		logerr_die( LOG_DEBUG, _("Service_connection: cannot read request") );
	}
	if( len < 2 ){
		fatal( LOG_INFO, _("Service_connection: bad request line '%s'"), input );
	}

	/* see if you need to reread the printcap and permissions information */

	Free_line_list(&Perm_line_list);
	Merge_line_list(&Perm_line_list,&RawPerm_line_list,0,0,0);

	Perm_check.service = 'X';

	permission = Perms_check( &Perm_line_list, &Perm_check, 0, 0 );
	if( permission == P_REJECT ){
		DEBUG1("Service_connection: talk socket '%d' no connect perms", talk );
		Write_fd_str( talk, _("\001no connect permissions\n") );
		cleanup(0);
	}
	Dispatch_input(&talk,input);
	cleanup(0);
}

void Dispatch_input(int *talk, char *input )
{
	switch( input[0] ){
		default:
			fatal( LOG_INFO,
				_("Dispatch_input: bad request line '%s'"), input );
			break;
		case REQ_START:
			/* simply send a 0 ACK and close connection - NOOP */
			Write_fd_len( *talk, "", 1 );
			break;
		case REQ_RECV:
			Receive_job( talk, input );
			break;
		case REQ_DSHORT:
		case REQ_DLONG:
		case REQ_VERBOSE:
			Job_status( talk, input );
			break;
		case REQ_REMOVE:
			Job_remove( talk, input );
			break;
		case REQ_CONTROL:
			Job_control( talk, input );
			break;
		case REQ_BLOCK:
			Receive_block_job( talk, input );
			break;
		case REQ_SECURE:
			Receive_secure( talk, input );
			break;
		case REQ_K4AUTH:
			Receive_k4auth( talk, input );
			break;
	}
}

/***************************************************************************
 * Reinit()
 * Reinitialize the database/printcap/permissions information
 * 1. free any allocated memory
 ***************************************************************************/

void Reinit(void)
{
	Reread_config = 1;
	(void) plp_signal (SIGHUP,  (plp_sigfunc_t)Reinit);
}


/***************************************************************************
 * Get_lpd_pid() and Set_lpd_pid()
 * Get and set the LPD pid into the LPD status file
 ***************************************************************************/

int Get_lpd_pid(void)
{
	int pid;
	int lockfd;
	char *path;
	struct stat statb;

	path = safestrdup3( Lockfile_DYN,".", Lpd_port_DYN, __FILE__, __LINE__ );
	pid = -1;
	lockfd = Checkread( path, &statb );
	if( lockfd >= 0 ){
		pid = Read_pid( lockfd, (char *)0, 0  ); 
	}
	if( path ) free(path); path = 0;
	return(pid);
}

void Set_lpd_pid(void)
{
	int lockfd, err;
	char *path;
	struct stat statb;

	path = safestrdup3( Lockfile_DYN,".", Lpd_port_DYN, __FILE__, __LINE__ );
	To_root();
	lockfd = Checkwrite( path, &statb, O_WRONLY|O_TRUNC, 1, 0 );
	To_daemon();
	if( lockfd < 0 ){
		logerr_die( LOG_ERR, _("lpd: Cannot open '%s'"), path );
	} else {
		/* we write our PID */
		Server_pid = getpid();
		DEBUG1( "lpd: writing lockfile '%s' fd %d with pid '%d'",path,lockfd,Server_pid );
		Write_pid( lockfd, Server_pid, (char *)0 );
		To_root();
		err = fchmod( lockfd, (statb.st_mode | 0644) );
		To_daemon();
		if( err == -1 ){
			logerr_die( LOG_ERR, _("lpd: Cannot change mode '%s'"), path );
		}
	}
	if(path) free(path); path = 0;
	close( lockfd );
}

int Read_server_status( int fd )
{
	int status, count, found, n;
	char buffer[LINEBUFFER];
	char *name;
	fd_set readfds;	/* for select() */
	struct timeval timeval;
	struct line_list l;

	buffer[0] = 0;
	errno = 0;

	DEBUG1( "Read_server_status: starting" );

	Init_line_list(&l);
	while(1){
		FD_ZERO( &readfds );
		FD_SET( fd, &readfds );
		memset(&timeval,0, sizeof(timeval));
		status = select( fd+1,
			FD_SET_FIX((fd_set *))&readfds,
			FD_SET_FIX((fd_set *))0,
			FD_SET_FIX((fd_set *))0, &timeval );
		DEBUG1( "Read_server_status: select status %d", status);
		if( status == 0 ){
			break;
		} else if( status < 0 ){
			close(fd);
			fd = 0;
			break;
		}
		status = read(fd,buffer,sizeof(buffer)-1);
		DEBUG1( "Read_server_status: read status %d", status );
		if( status <= 0 ){
			close(fd);
			fd = 0;
			break;
		}
		buffer[status] = 0;
		/* we split up read line and record information */
		Split(&l,buffer,Whitespace,0,0,0,0,0);
		if(DEBUGL1)Dump_line_list("Read_server_status - input", &l );
		for( count = 0; count < l.count; ++count ){ 
			name = l.list[count];
			found = 0;
			for( n = 0;!found && n < Servers_line_list.count; ++n ){
				found = !safestrcasecmp( Servers_line_list.list[n], name);
			}
			if( !found ){
				Add_line_list(&Servers_line_list,name,0,0,0);
			}
		}
	}
	if(DEBUGL2)Dump_line_list("Read_server_status - waiting for start",
			&Servers_line_list );
	return(fd);
}

/***************************************************************************
 * void Get_parms(int argc, char *argv[])
 * 1. Scan the argument list and get the flags
 * 2. Check for duplicate information
 ***************************************************************************/

 char *usagemsg = N_("\
 usage: %s [-FV] [-D dbg] [-L log]\n\
 Options\n\
 -D dbg      - set debug level and flags\n\
                 Example: -D10,remote=5\n\
                 set debug level to 10, remote flag = 5\n\
 -F          - run in foreground, log to stderr\n\
               Example: -D10,remote=5\n\
 -L logfile  - append log information to logfile\n\
 -V          - show version info\n");

void usage(void)
{
	fprintf( stderr, _(usagemsg), Name);
	exit(1);
}

 char LPD_optstr[] 	/* LPD options */
 = "D:FL:VX" ;

void Get_parms(int argc, char *argv[] )
{
	int option, verbose = 0;

	while ((option = Getopt (argc, argv, LPD_optstr )) != EOF) {
		switch (option) {
		case 'D': Parse_debug(Optarg, 1); break;
		case 'L': Logfile_LPD = Optarg; break;
		case 'F': Foreground_LPD = 1; break;
		case 'X': Worker_LPD = 1; break;
		default:
			usage();
			break;
		case 'V':
			verbose = !verbose;
			break;
		}
	}
	if( Optind != argc ){
		usage();
	}
	if( verbose > 0 ) {
		fprintf( stderr, _("Version %s\n"), PATCHLEVEL );
		if( verbose > 1 ) Printlist( Copyright, stderr );
		exit(1);
	}
}


/*
 * Error status on STDERR
 */
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
	char *path = 0;
	char msg_b[SMALLBUFFER];
	static int insetstatus;
    VA_LOCAL_DECL

    VA_START (fmt);
    VA_SHIFT (job, struct job * );
    VA_SHIFT (fmt, char *);

	if( Doing_cleanup || fmt == 0 || *fmt == 0 || insetstatus ) return;

	insetstatus = 1;
	(void) plp_vsnprintf( msg_b, sizeof(msg_b)-4, fmt, ap);

	DEBUG1("setstatus: Status_fd %d, Mail_fd %d, msg '%s'", Status_fd, Mail_fd, msg_b);

	if( Status_fd <= 0 && Spool_dir_DYN && Printer_DYN ){
		path = Make_pathname( Spool_dir_DYN, Queue_status_file_DYN);
		Status_fd = Trim_status_file( path,
			Max_status_size_DYN, Min_status_size_DYN );
		if( path ) free(path); path = 0;
	}

	send_to_logger( Status_fd, Mail_fd, job, PRSTATUS, msg_b );
	insetstatus = 0;
	VA_END;
}


/***************************************************************************
 * void setmessage (struct job *job,char *header, char *fmt,...)
 * put the message out (if necessary) to the logger
 ***************************************************************************/

/* VARARGS2 */
#ifdef HAVE_STDARGS
 void setmessage (struct job *job,const char *header, char *fmt,...)
#else
 void setmessage (va_alist) va_dcl
#endif
{
#ifndef HAVE_STDARGS
    struct job *job;
    char *header;
    char *fmt;
#endif
	char msg_b[SMALLBUFFER];

    VA_LOCAL_DECL

    VA_START (fmt);
    VA_SHIFT (job, struct job * );
    VA_SHIFT (header, char *);
    VA_SHIFT (fmt, char *);

	if( Doing_cleanup ) return;
	(void) plp_vsnprintf( msg_b, sizeof(msg_b)-4, fmt, ap);
	DEBUG1("setmessage: msg '%s'", msg_b);
	send_to_logger( -1, -1, job, header, msg_b );
	VA_END;
}


/***************************************************************************
 * send_to_logger( struct job *job, char *msg )
 *  This will try and send to the logger.
 ***************************************************************************/

 void send_to_logger( int send_to_status_fd, int send_to_mail_fd,
	struct job *job, const char *header, char *msg_b )
{
	char *s, *t;
	char *id, *tstr;
	int num,pid;
	char out_b[4*SMALLBUFFER];
	struct line_list l;

	if( Doing_cleanup ) return;
	Init_line_list(&l);
	if(DEBUGL4){
		char buffer[32];
		plp_snprintf(buffer,sizeof(buffer)-5,"%s", msg_b );
		if( msg_b ) safestrncat( buffer,"...");
		logDebug("send_to_logger: Logger_fd fd %d, Status_fd fd %d, Mail_fd fd %d, header '%s', body '%s'",
			Logger_fd, Status_fd, Mail_fd, header, buffer );
	}
	s = t = id = tstr = 0;
	num = 0;
	if( job ){
		Set_str_value(&l,IDENTIFIER,
			(id = Find_str_value(&job->info,IDENTIFIER,Value_sep)) );
		Set_decimal_value(&l,NUMBER,
			(num = Find_decimal_value(&job->info,NUMBER,Value_sep)) );
	}
	Set_str_value(&l,UPDATE_TIME,(tstr=Time_str(0,0)));
	Set_decimal_value(&l,PROCESS,(pid=getpid()));

	plp_snprintf( out_b, sizeof(out_b), "%s at %s ## %s=%s %s=%d %s=%d\n",
		msg_b, tstr, IDENTIFIER, id, NUMBER, num, PROCESS, pid );

	if( send_to_status_fd > 0 && Status_fd > 0 && Write_fd_str( Status_fd, out_b ) < 0 ){
		DEBUG4("send_to_logger: write to fd %d failed - %s",
			Status_fd, Errormsg(errno) );
		close( Status_fd );
		Status_fd = -1;
	}
	if( send_to_mail_fd > 0 && Mail_fd > 0 && Write_fd_str( Mail_fd, out_b ) < 0 ){
		DEBUG4("send_to_logger: write to fd %d failed - %s",
			Mail_fd, Errormsg(errno) );
		close(Mail_fd);
		Mail_fd = -1;
	}
	if( Logger_fd > 0 ){
		Set_str_value(&l,PRINTER,Printer_DYN);
		Set_str_value(&l,HOST,FQDNHost_FQDN);
		s = Escape(msg_b,0,1);
		Set_str_value(&l,VALUE,s);
		if(s) free(s); s = 0;
		t = Join_line_list(&l,"\n");
		s = Escape(t,0,1); 
		if(t) free(t); t = 0;
		t = safestrdup4(header,"=",s,"\n",__FILE__,__LINE__);
		Write_fd_str( Logger_fd, t );
		if( s ) free(s); s = 0;
		if( t ) free(t); t = 0;
	}
	Free_line_list(&l);
}

/*
 *  Support for non-copy on write fork as for NT
 *   1. Preparation for the fork is done by calling 'Setup_lpd_call'
 *      This establishes a default setup for the new process by setting
 *      up a list of parameters and file descriptors to be passed.
 *   2. The user then adds fork/process specific options
 *   3. The fork is done by calling Make_lpd_call which actually
 *      does the fork() operation.  If the lpd_path option is set,
 *      then a -X command line flag is added and an execv() of the program
 *      is done.
 *   4.A - fork()
 *        Make_lpd_call (child) will call Do_work(),  which dispatches
 *         a call to the required function.
 *   4.B - execv()
 *        The execv'd process checks the command line parameters for -X
 *         flag and when it finds it calls Do_work() with the same parameters
 *         as would be done for the fork() version.
 */

void Setup_lpd_call( struct line_list *passfd, struct line_list *args )
{
	Free_line_list( args );
	Check_max(passfd, 10 );
	passfd->count = 0;
	passfd->list[passfd->count++] = Cast_int_to_voidstar(0);
	passfd->list[passfd->count++] = Cast_int_to_voidstar(1);
	passfd->list[passfd->count++] = Cast_int_to_voidstar(2);
	if( Mail_fd > 0 ){
		Set_decimal_value(args,MAIL_FD,passfd->count);
		passfd->list[passfd->count++] = Cast_int_to_voidstar(Mail_fd);
	}
	if( Status_fd > 0 ){
		Set_decimal_value(args,STATUS_FD,passfd->count);
		passfd->list[passfd->count++] = Cast_int_to_voidstar(Status_fd);
	}
	if( Logger_fd > 0 ){
		Set_decimal_value(args,LOGGER,passfd->count);
		passfd->list[passfd->count++] = Cast_int_to_voidstar(Logger_fd);
	}
	if( Lpd_request > 0 ){
		Set_decimal_value(args,LPD_REQUEST,passfd->count);
		passfd->list[passfd->count++] = Cast_int_to_voidstar(Lpd_request);
	}
	if( Lpd_ack_fd > 0 ){
		Set_decimal_value(args,LPD_ACK_FD,passfd->count);
		passfd->list[passfd->count++] = Cast_int_to_voidstar(Lpd_ack_fd);
	}
	Set_flag_value(args,DEBUG,Debug);
	Set_flag_value(args,DEBUGFV,DbgFlag);
#ifdef DMALLOC
	{
		extern int _dmalloc_outfile;
		if( _dmalloc_outfile > 0 ){
			Set_decimal_value(args,DMALLOC_OUTFILE,passfd->count);
			passfd->list[passfd->count++] = Cast_int_to_voidstar(_dmalloc_outfile);
		}
	}
#endif
}

/*
 * Calls[] = list of dispatch functions 
 */

 struct call_list Calls[] = {
	{&LOGGER,Logger},		/* used by LPD to do logging for remote sites */
	{&ALL,Service_all},			/* used by LPD for Start_all() operation */
	{&SERVER,Service_connection},	/* used by LPD to handle a connection */
	{&QUEUE,Service_queue},		/* used by LPD to handle a queue */
	{&PRINTER,Service_worker},	/* used by LPD queue manager to do actual printing */
	{&LOG,Service_log},			/* used by LPD to create logger process for filter */
	{0,0}
};

/*
 * Make_lpd_call - does the actual forking operation
 *  - sets up file descriptor for child, can close_on_exec()
 *  - does fork() or execve() as appropriate
 *
 *  returns: pid of child or -1 if fork failed.
 */

int Make_lpd_call( struct line_list *passfd, struct line_list *args )
{
	int pid, fd, i, n, newfd;
	struct line_list env;

	Init_line_list(&env);
	pid = dofork(1);
	if( pid ){
		return(pid);
	}
	Name = "LPD_CALL";

	if(DEBUGL2){
		logDebug("Make_lpd_call: lpd path '%s'", Lpd_path_DYN );
		logDebug("Make_lpd_call: passfd count %d", passfd->count );
		for( i = 0; i < passfd->count; ++i ){
			logDebug(" [%d] %d", i, Cast_ptr_to_int(passfd->list[i]));
		}
		Dump_line_list("Make_lpd_call - args", args );
	}
	for( i = 0; i < passfd->count; ++i ){
		fd = Cast_ptr_to_int(passfd->list[i]);
		if( fd < i  ){
			/* we have fd 3 -> 4, but 3 gets wiped out */
			do{
				newfd = dup(fd);
				if( newfd < 0 ){
					logerr_die(LOG_INFO,"Make_lpd_call: dup failed");
				}
				DEBUG4("Make_lpd_call: fd [%d] = %d, dup2 -> %d",
					i, fd, newfd );
				passfd->list[i] = Cast_int_to_voidstar(newfd);
			} while( newfd < i );
		}
	}
	if(DEBUGL2){
		logDebug("Make_lpd_call: after fixing fd count %d", passfd->count);
		for( i = 0 ; i < passfd->count; ++i ){
			fd = Cast_ptr_to_int(passfd->list[i]);
			logDebug("  [%d]=%d",i,fd);
		}
	}
	for( i = 0; i < passfd->count; ++i ){
		fd = Cast_ptr_to_int(passfd->list[i]);
		DEBUG2("Make_lpd_call: fd %d -> %d",fd, i );
		if( dup2( fd, i ) == -1 ){
			Errorcode = JABORT;
			logerr_die(LOG_INFO,"Make_lpd_call: dup2(%d,%d) failed",
				fd, i );
		}
	}
	if( Lpd_path_DYN ){
		/* we really do the execv */
		Setup_env_for_process(&env,0);
#ifdef DMALLOC
		Set_str_value(&env,DMALLOC_OPTIONS,getenv(DMALLOC_OPTIONS));
#endif
		Set_str_value(&env,LPD_CONF,getenv(LPD_CONF));
		Check_max(args,10);
		args->list[args->count] = 0;
		for( i = args->count; i >= 0; --i ){
			args->list[i+2] = args->list[i];
		}
		args->list[0] = safestrdup(Lpd_path_DYN,__FILE__,__LINE__);
		args->list[1] = safestrdup("-X",__FILE__,__LINE__);
		args->count += 2;
		if(DEBUGL2)Dump_line_list("Make_lpd_call: args", args );
		close_on_exec(passfd->count);
		execve(args->list[0],args->list,env.list);
		logerr_die(LOG_ERR,"Make_lpd_call: execve '%s' failed",
			Lpd_path_DYN );
	}
	/* close other ones to simulate close_on_exec() */
	n = Get_max_fd();
	for( i = passfd->count ; i < n; ++i ){
		close(i);
	}
	passfd->count = 0;
	Free_line_list( passfd );
	Do_work( args );
	return(0);
}

/*
 *  Do_work- called to dispatch process to the appropriate function
 */

void Do_work( struct line_list *args )
{
	const char **ps;
	char *name;
	int i;

	Logger_fd = Find_flag_value(args, LOGGER,Value_sep);
	Status_fd = Find_flag_value(args, STATUS_FD,Value_sep);
	Mail_fd = Find_flag_value(args, MAIL_FD,Value_sep);
	Lpd_ack_fd = Find_flag_value(args, LPD_ACK_FD,Value_sep);
	Lpd_request = Find_flag_value(args, LPD_REQUEST,Value_sep);
	Debug= Find_flag_value( args, DEBUG, Value_sep);
	DbgFlag= Find_flag_value( args, DEBUGFV, Value_sep);
#ifdef DMALLOC
	{
		extern int _dmalloc_outfile;
		_dmalloc_outfile = Find_flag_value(args, DMALLOC_OUTFILE,Value_sep);
	}
#endif
	name = Find_str_value(args,CALL,Value_sep);
	DEBUG3("Do_work: calling '%s'", name );
	ps = 0;
	for( i = 0; name && (ps = Calls[i].id) && safestrcasecmp(*ps,name); ++i);
	if( ps ){
		DEBUG3("Do_work: found '%s'", name );
		(Calls[i].p)(args);
	} else {
		Errorcode = JABORT;
		DEBUG3("Do_work: did not find '%s'", name );
	}
	cleanup(0);
}

/*
 * Lpd_worker - called by LPD on startup when it discovers
 *  the -X flag on the command line.
 */

void Lpd_worker( char **argv, int argc, int optindv  )
{
	struct line_list args;

	Name = "LPD_WORKER";
	DEBUG1("Lpd_worker: argc %d, optind %d", argc, optindv );
	Init_line_list( &args );
	while( optindv < argc ){
		Add_line_list(&args,argv[optindv++],Value_sep,1,1);
	}
	if(DEBUGL1)Dump_line_list("Lpd_worker - args", &args );
	Do_work( &args );
	cleanup(0);
}

/*
 * Start_logger - helper function to setup logger process
 */

int Start_logger( int log_fd )
{
	struct line_list args, passfd;
	int fd = Logger_fd;
	int pid;

	Init_line_list(&passfd);
	Init_line_list(&args);

	Logger_fd = -1;
	Setup_lpd_call( &passfd, &args );
	Logger_fd = fd;

	Set_str_value(&args,CALL,LOGGER);

	Check_max(&passfd,2);
	Set_decimal_value(&args,INPUT,passfd.count);
	passfd.list[passfd.count++] = Cast_int_to_voidstar(log_fd);

	pid = Make_lpd_call( &passfd, &args );
	passfd.count = 0;
	Free_line_list( &args );
	Free_line_list( &passfd );
	DEBUG1("Start_logger: log_fd %d, status_pid %d", log_fd, pid );
	return(pid);
}

/*
 * Start_worker - general purpose dispatch function
 *   - adds an input FD
 */

int Start_worker( struct line_list *parms, int fd  )
{
	struct line_list args, passfd;
	int pid;

	Init_line_list(&passfd);
	Init_line_list(&args);
	if(DEBUGL1){
		DEBUG1("Start_worker: fd %d", fd );
		Dump_line_list("Start_worker - parms", parms );
	}
	Setup_lpd_call( &passfd, &args );
	Merge_line_list( &args, parms, Value_sep,1,1);
	Free_line_list( parms );
	if( fd ){
		Check_max(&passfd,2);
		Set_decimal_value(&args,INPUT,passfd.count);
		passfd.list[passfd.count++] = Cast_int_to_voidstar(fd);
	}

	pid = Make_lpd_call( &passfd, &args );
	Free_line_list( &args );
	passfd.count = 0;
	Free_line_list( &passfd );
	DEBUG1("Start_worker: pid %d", pid );
	return(pid);
}

int Start_all( void )
{
	struct line_list args, passfd;
	int pid, p[2];

	Init_line_list(&passfd);
	Init_line_list(&args);

	DEBUG1( "Start_all: begin" );
	Started_server = 0;
	if( pipe(p) == -1 ){
		logerr_die( LOG_INFO, _("Start_all: pipe failed!") );
	}
	DEBUG1( "Start_all: fd p(%d,%d)",p[0],p[1]);

	Setup_lpd_call( &passfd, &args );
	Set_str_value(&args,CALL,ALL);

	Check_max(&passfd,2);
	Set_decimal_value(&args,INPUT,passfd.count);
	passfd.list[passfd.count++] = Cast_int_to_voidstar(p[1]);

	pid = Make_lpd_call( &passfd, &args );

	Free_line_list( &args );
	passfd.count = 0;
	Free_line_list(&passfd);
	close(p[1]);
	if( pid < 0 ){
		close( p[0] );
		p[0] = -1;
	}
	DEBUG1("Start_all: pid %d, fd %d", pid, p[0] );
	return(p[0]);
}

void Service_all( struct line_list *args )
{
	int i, reportfd, fd, printable, held, move, printing_enabled,
		server_pid;
	char buffer[SMALLBUFFER], *pr, *path = 0;
	struct stat statb;
	
	/* we start up servers while we can */
	Name = "STARTALL";
	setproctitle( "lpd %s", Name );

	reportfd = Find_flag_value(args,INPUT,Value_sep);
	Free_line_list(args);

	if(All_line_list.count == 0 ){
		Get_all_printcap_entries();
	}
	for( i = 0; i < All_line_list.count; ++i ){
		Set_DYN(&Printer_DYN,0);
		if( path ) free(path); path = 0;
		pr = All_line_list.list[i];
		DEBUG1("Service_all: checking '%s'", pr );
		if( Setup_printer( pr, buffer, sizeof(buffer)) ) continue;
		/* now check to see if there is a server and unspooler process active */
		path = Make_pathname( Spool_dir_DYN, Printer_DYN );
		server_pid = 0;
		if( (fd = Checkread( path, &statb ) ) >= 0 ){
			server_pid = Read_pid( fd, (char *)0, 0 );
			close( fd );
		}
		if( path ) free(path); path = 0;
		DEBUG3("Service_all: printer '%s' checking server pid %d", Printer_DYN, server_pid );
		if( server_pid > 0 && kill( server_pid, 0 ) == 0 ){
			DEBUG3("Get_queue_status: server %d active", server_pid );
			continue;
		}
		printing_enabled = !(Pr_disabled(&Spool_control) || Pr_aborted(&Spool_control));

		Free_line_list( &Sort_order );
		if( Scan_queue( Spool_dir_DYN, &Spool_control, &Sort_order,
				&printable,&held,&move, 1 ) ){
			continue;
		}
		if( move || (printable && printing_enabled) ){
			if( Server_queue_name_DYN ){
				pr = Server_queue_name_DYN;
			} else {
				pr = Printer_DYN;;
			}
			DEBUG1("Service_all: starting '%s'", pr );
			plp_snprintf(buffer,sizeof(buffer),"%s\n",pr );
			if( Write_fd_str(reportfd,buffer) < 0 ) cleanup(0);
		}
	}
	Free_line_list( &Sort_order );
	if( path ) free(path); path = 0;
	Errorcode = 0;
	cleanup(0);
}

void Service_queue( struct line_list *args )
{
	int subserver, idle;

	Set_DYN(&Printer_DYN, Find_str_value(args, PRINTER,Value_sep) );
	subserver = Find_flag_value( args, SUBSERVER, Value_sep );
	idle = Find_flag_value( args, IDLE, Value_sep );

	Free_line_list(args);
	Do_queue_jobs( Printer_DYN, subserver, idle );
	cleanup(0);
}

void Service_log( struct line_list *args )
{
	char *s, *name;
	int error, n;
	struct job job, *j;
	char buffer[SMALLBUFFER];

	Init_job(&job);
	j = 0;

	Set_DYN(&Printer_DYN, Find_str_value(args, PRINTER,Value_sep) );
	Set_str_value(&job.info,PRINTER,Printer_DYN);

	error = Find_flag_value(args,INPUT,Value_sep);
	Set_str_value(&job.info,IDENTIFIER,
		(s = Find_str_value(args, IDENTIFIER,Value_sep)) );

	if(s) j = &job;
	name = Find_str_value(args,NAME,Value_sep);
	if( name ) Name = name;
	
	DEBUG2("Service_log: name '%s' id '%s' ", name, s);
	Init_buf(&Outbuf, &Outmax, &Outlen );
	while( Outlen < Outmax
		&& (n = read(error, Outbuf+Outlen, Outmax-Outlen)) > 0 ){
		Outbuf[Outlen+n] = 0;
		while( (s = safestrchr(Outbuf,'\n')) ){
			*s++ = 0;
			DEBUG2("Service_log: %s '%s'", name, Outbuf);
			plp_snprintf(buffer,sizeof(buffer),
				"%s error '%s' at %s\n", name, Outbuf,Time_str(0,0)); 
			if( Status_fd > 0 && Write_fd_str( Status_fd, buffer ) < 0 ){
				close( Status_fd );
				Status_fd = -1;
			}
			if( Mail_fd > 0 && Write_fd_str( Mail_fd, buffer ) < 0 ){
				DEBUG4("Service_log: write to fd %d failed - %s",
					Mail_fd, Errormsg(errno) );
				close(Mail_fd);
				Mail_fd = -1;
			}
			memmove(Outbuf,s,strlen(s)+1);
		}
		Outlen = strlen(Outbuf);
	}
	if( Outlen ){
		DEBUG2("Service_log: %s '%s'", name, Outbuf);
		plp_snprintf(buffer,sizeof(buffer),
			"%s error '%s' at %s", name, Outbuf,Time_str(0,0)); 
		if( Status_fd > 0 && Write_fd_str( Status_fd, buffer ) < 0 ){
			close( Status_fd );
			Status_fd = -1;
		}
		if( Mail_fd > 0 && Write_fd_str( Mail_fd, buffer ) < 0 ){
			DEBUG4("Service_log: write to fd %d failed - %s",
				Mail_fd, Errormsg(errno) );
			close(Mail_fd);
			Mail_fd = -1;
		}
	}
	Free_job(&job);
	DEBUG2("Service_log: '%s' stderr exiting", name);
	Errorcode = 0;
	cleanup(0);
}

plp_signal_t sigchld_handler (int signo)
{
	int nonblock;
	DEBUG6 ("sigchld_handler: caught SIGCHLD");
	signal( SIGCHLD, SIG_DFL );
	if( !(nonblock = Get_nonblock_io( Lpd_request )) ){
		Set_nonblock_io( Lpd_request );
	}
	Write_fd_str(Lpd_request,"\n");
	if( nonblock ){
		Set_block_io( Lpd_request );
	}
}

void Setup_waitpid (void)
{
	signal( SIGCHLD, SIG_DFL );
}

void Setup_waitpid_break (void)
{
	(void) plp_signal_break(SIGCHLD, sigchld_handler);
}

