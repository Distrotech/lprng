/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2000, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: lpd.c,v 5.22 2000/11/07 18:14:24 papowell Exp papowell $";


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
	int max_socks;		/* maximum number of sockets */
	int n, m;	/* ACME?  Hmmm... well, ok */
	int err, newsock;
 	time_t last_time;	/* time that last Start_all was done */
 	time_t this_time;	/* current time */
	plp_status_t status;
	int max_servers = 0;
	int start_fd = 0;
	int status_pid = 0;
	int request_pipe[2], status_pipe[2];
	int fork_failed;
	struct line_list args;
	char *s;
	int first_scan = 1;

	Init_line_list( &args );
	Is_server = 1;	/* we are the LPD server */
	Logger_fd = -1;

#ifndef NODEBUG
	Debug = 0;
#endif
	if(DEBUGL3){
		LOGDEBUG("lpd: argc %d", argc );
		for( n = 0; n < argc; ++n ){
			LOGDEBUG(" [%d] '%s'", n, argv[n] );
		}
		LOGDEBUG("lpd: env" );
		for( n = 0; envp[n]; ++n ){
			LOGDEBUG(" [%d] '%s'", n, envp[n] );
		}
	}

	/* set signal handlers */
	(void) plp_signal(SIGHUP,  (plp_sigfunc_t)Reinit);
	(void) plp_signal(SIGINT, cleanup_INT);
	(void) plp_signal(SIGQUIT, cleanup_QUIT);
	(void) plp_signal(SIGTERM, cleanup_TERM);
	(void) signal(SIGUSR1, SIG_IGN);
	(void) signal(SIGUSR2, SIG_IGN);
	(void) signal(SIGCHLD, SIG_DFL);
	(void) signal(SIGPIPE, SIG_IGN);

	/*
	the next bit of insanity is caused by the interaction of signal(2) and execve(2)
	man signal(2):

	 When a process which has installed signal handlers forks, the child pro-
	 cess inherits the signals.  All caught signals may be reset to their de-
	 fault action by a call to the execve(2) function; ignored signals remain
	 ignored.


	man execve(2):
	 
	 Signals set to be ignored in the calling process are set to be ignored in
					   ^^^^^^^
					   signal(SIGCHLD, SIG_IGN)  <- in the acroread code???
	 
	 the new process. Signals which are set to be caught in the calling pro-  
	 cess image are set to default action in the new process image.  Blocked  
	 signals remain blocked regardless of changes to the signal action.  The  
	 signal stack is reset to be undefined (see sigaction(2) for more informa-
	 tion).


	^&*(*&^!!! &*())&*&*!!!  and again, I say, &*()(&*!!!

	This means that if you fork/execve a child,  then you better make sure
	that you set up its signal/mask stuff correctly.

    So if somebody blocks all signals and then starts up LPD,  it will not work
	correctly.

	*/

	{ plp_block_mask oblock; plp_unblock_all_signals( &oblock ); }


	Get_parms(argc, argv);      /* scan input args */

	Initialize(argc, argv, envp, 'D' );
	DEBUG1("Get_parms: UID_root %d, OriginalRUID %d", UID_root, OriginalRUID);

	if( UID_root && OriginalRUID ){
		FATAL(LOG_ERR) "lpd installed SETUID root and started by user %d! Possible hacker attack", OriginalRUID);
	}

	Setup_configuration();

	if( Worker_LPD ){
		Lpd_worker(argv,argc,Optind);
		cleanup(0);
	}


	/* get the maximum number of servers allowed */
	max_servers = Get_max_servers();
	n = Get_max_fd();
	DEBUG1( "lpd: maximum servers %d, maximum file descriptors %d ",
		max_servers, n );

	if( Lockfile_DYN == 0 ){
		LOGERR_DIE(LOG_INFO) _("No LPD lockfile specified!") );
	}

	/* chdir to the root directory */
	if( chdir( "/" ) == -1 ){
		Errorcode = JABORT;
		LOGERR_DIE(LOG_ERR) "cannot chdir to /");
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
		Errorcode = 1;
  		if( pid > 0 ){
  			DIEMSG( _("Another print spooler is using TCP printer port, possibly lpd process '%d'"),
  				pid );
  		} else {
 			if( !UID_root ){
 				DIEMSG("Not running with ROOT perms and trying to open port '%s'", Lpd_port_DYN );
 			}
 			DIEMSG( _("Another print spooler is using TCP printer port - not LPRng") );
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
		LOGERR_DIE(LOG_ERR) _("lpd: main() dofork failed") );
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

	/* set up the log file and standard environment - do not
	   fool around with anything but fd 0,1,2 which should be safe
		as we made sure that the fd 0,1,2 existed.
    */

	Setup_log( Logfile_LPD );

	Name = "Waiting";
	setproctitle( "lpd %s", Name  );

	/*
	 * Write the PID into the lockfile
	 */

	Set_lpd_pid();

	if( Drop_root_DYN ){
		Full_daemon_perms();
	}

	/* establish the pipes for low level processes to use */
	if( pipe( request_pipe ) == -1 ){
		LOGERR_DIE(LOG_ERR) _("lpd: pipe call failed") );
	}
	Max_open(request_pipe[0]); Max_open(request_pipe[1]);
	DEBUG2( "lpd: fd request_pipe(%d,%d)",request_pipe[0],request_pipe[1]);
	Lpd_request = request_pipe[1];
	Set_nonblock_io( Lpd_request );

	Logger_fd = -1;
	status_pid = -1;
	if( Logger_destination_DYN ){
		if( pipe( status_pipe ) == -1 ){
			LOGERR_DIE(LOG_ERR) _("lpd: pipe call failed") );
		}
		Max_open(status_pipe[0]); Max_open(status_pipe[1]);
		Logger_fd = status_pipe[1];
		DEBUG2( "lpd: fd status_pipe(%d,%d)",status_pipe[0],status_pipe[1]);
	}

	/* open a connection to logger */
	setmessage(0,LPD,"Starting");

	/* get the maximum number of servers allowed */
	max_servers = Get_max_servers();
	DEBUG1( "lpd: maximum servers %d", max_servers );

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
	fork_failed = start_fd = Start_all(first_scan);
	Fork_error( fork_failed );
	if( start_fd > 0 ){
		first_scan = 0;
	}

	do{
		DEBUG1("lpd: LOOP START");
		if(DEBUGL3){ int fd; fd = dup(0); LOGDEBUG("lpd: next fd %d",fd); close(fd); };

		timeout = 0;
		DEBUG2( "lpd: Poll_time %d, Force_poll %d, start_fd %d, Started_server %d",
			Poll_time_DYN, Force_poll_DYN, start_fd, Started_server );
		if(DEBUGL2)Dump_line_list("lpd - Servers_line_list",&Servers_line_list );

		/*
		 * collect zombies 
		 */

		while( (pid = plp_waitpid( -1, &status, WNOHANG)) > 0 ){
			DEBUG1( "lpd: process %d, status '%s'",
				pid, Decode_status(&status));
			if( pid == status_pid ){
				status_pid = -1;
			}
			fork_failed = 1;
		}
		if( fork_failed > 0 && Logger_fd > 0 && status_pid < 0 ){
			DEBUG1( "lpd: restarting logger process");
			fork_failed = status_pid = Start_logger( status_pipe[0] );
			Fork_error( fork_failed );
			DEBUG1("lpd: status_pid %d", status_pid );
		}
		if( fork_failed < 0 ){
			/* wait for 10 seconds then go in a loop */
			memset(&timeval, 0, sizeof(timeval));
			timeval.tv_sec = 10;
			timeout = &timeval;
		} else if( Poll_time_DYN > 0
			&& start_fd <= 0 && Servers_line_list.count == 0 ){
			memset(&timeval, 0, sizeof(timeval));
			this_time = time( (void *)0 );
			m = (this_time - last_time);
			timeval.tv_sec = Poll_time_DYN - m;
			timeout = &timeval;
			DEBUG2("lpd: from last poll %d, to next poll %d",
				m, (int)timeval.tv_sec );
			if( m >= Poll_time_DYN ){
				if( Started_server || Force_poll_DYN ){
					fork_failed = start_fd = Start_all(first_scan);
					Fork_error( fork_failed );
					DEBUG1( "lpd: restarting poll, start_fd %d", start_fd);
					if( start_fd > 0 ){
						if( first_scan ) first_scan = 0;
						last_time = this_time;
						timeval.tv_sec = Poll_time_DYN;
					}
				} else {
					DEBUG1( "lpd: no poll" );
					timeout = 0;
				}
			}
		}

		n = Countpid();
		max_servers = Get_max_servers();
		DEBUG1("lpd: max_servers %d, active %d", max_servers, n );

		/* allow a little space for people to send commands */
		while( fork_failed > 0 && Servers_line_list.count > 0 && n < max_servers-4 ){ 
			s = Servers_line_list.list[0];
			DEBUG1("lpd: starting server '%s'", s );
			Set_str_value(&args,PRINTER,s);
			Set_str_value(&args,CALL,QUEUE);
			fork_failed = pid = Start_worker( &args, 0 );
			Fork_error( fork_failed );
			Free_line_list(&args);
			if( pid > 0 ){
				Remove_line_list( &Servers_line_list, 0 );
				++Started_server;
				++n;
			}
		}

		DEBUG1("lpd: fork_failed %d, processes %d active, max %d",
			fork_failed, n, max_servers );
		/* do not accept incoming call if no worker available */
		readfds = defreadfds;
		if( n >= max_servers || fork_failed < 0 ){
			DEBUG1( "lpd: not accepting requests" );
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
					LOGDEBUG( "lpd: waiting for fd %d to be readable", i );
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
			LOGDEBUG( "lpd: select returned %d, error '%s'",
				m, Errormsg(err) );
			for(i=0; i < max_socks; ++i ){
				if( FD_ISSET( i, &readfds ) ){
					LOGDEBUG( "lpd: fd %d readable", i );
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
		if( m < 0 ){
			if( err != EINTR ){
				errno = err;
				LOGERR_DIE(LOG_ERR) _("lpd: select error!"));
				break;
			}
			continue;
		} else if( m == 0 ){
			DEBUG1( "lpd: signal or time out, fork_failed %d", fork_failed );
			/* we try to fork now */
			if( fork_failed < 0 ) fork_failed = 1;
			continue;
		}
		if( FD_ISSET( sock, &readfds ) ){
			struct sockaddr sinaddr;
			int len;
			len = sizeof( sinaddr );
			newsock = accept( sock, &sinaddr, &len );
			err = errno;
			DEBUG1("lpd: connection fd %d", newsock );
			if( newsock > 0 ){
				Set_str_value(&args,CALL,SERVER);
				pid = Start_worker( &args, newsock );
				if( pid < 0 ){
					LOGERR(LOG_INFO) _("lpd: fork() failed") );
					Write_fd_str( newsock, "\002Server load too high\n");
				} else {
					DEBUG1( "lpd: listener pid %d running", pid );
				}
				close( newsock );
				Free_line_list(&args);
			} else {
				errno = err;
				LOGERR(LOG_INFO) _("Service_connection: accept on listening socket failed") );
			}
		}
		if( FD_ISSET( request_pipe[0], &readfds ) 
			&& Read_server_status( request_pipe[0] ) == 0 ){
			Errorcode = JABORT;
			LOGERR_DIE(LOG_ERR) _("lpd: Lpd_request pipe EOF! cannot happen") );
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
 * saveme will prevent STDIN from being clobbered
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
	    LOGERR_DIE(LOG_ERR) _("Setup_log: open /dev/null failed"));
	}
	if (open("/dev/null", O_WRONLY, 0) != 1) {
	    LOGERR_DIE(LOG_ERR) _("Setup_log: open /dev/null failed"));
	}

    /*
     * open logfile; if it is "-", use STDERR; if Foreground is set, use stderr
     */
	if( fstat(2,&statb) == -1 && dup2(1,2) == -1 ){
		LOGERR_DIE(LOG_ERR) _("Setup_log: dup2(%d,%d) failed"), 1, 2);
	}
    if( logfile == 0 ){
		if( !Foreground_LPD && dup2(1,2) == -1 ){
			LOGERR_DIE(LOG_ERR) _("Setup_log: dup2(%d,%d) failed"), 1, 2);
		}
	} else if( safestrcmp(logfile, "-") ){
		close(2);
		if( Checkwrite(logfile, &statb, O_WRONLY|O_APPEND, 0, 0) != 2) {
			LOGERR_DIE(LOG_ERR) _("Setup_log: open %s failed"), logfile );
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
	int permission;
	int port = 0;
	struct sockaddr sinaddr;

	Name = "SERVER";
	setproctitle( "lpd %s", Name );
	(void) plp_signal (SIGHUP, cleanup );

	if( !(talk = Find_flag_value(args,INPUT,Value_sep)) ){
		Errorcode = JABORT;
		FATAL(LOG_ERR)"Service_connection: no talk fd"); 
	}

	DEBUG1("Service_connection: listening fd %d", talk );

	Free_line_list(args);

	/* make sure you use blocking IO */
	Set_block_io(talk);

	len = sizeof( sinaddr );
	if( getpeername( talk, &sinaddr, &len ) ){
		LOGERR_DIE(LOG_DEBUG) _("Service_connection: getpeername failed") );
	}

	if( sinaddr.sa_family == AF_INET ){
		port = ((struct sockaddr_in *)&sinaddr)->sin_port;
#if defined(IPV6)
	} else if( sinaddr.sa_family == AF_INET6 ){
		port = ((struct sockaddr_in6 * )&sinaddr)->sin6_port;
#endif
	} else {
		FATAL(LOG_INFO) _("Service_connection: bad protocol family '%d'"), sinaddr.sa_family );
	}

	DEBUG2("Service_connection: socket %d, ip '%s' port %d", talk,
		inet_ntop_sockaddr( &sinaddr, buffer, sizeof(buffer) ), ntohs( port ) );

	/* get the remote name and set up the various checks */
	Perm_check.addr = &sinaddr;

	Get_remote_hostbyaddr( &RemoteHost_IP, &sinaddr );
	Perm_check.remotehost  =  &RemoteHost_IP;
	Perm_check.host = &RemoteHost_IP;
	Perm_check.port =  ntohs(port);

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
		LOGERR_DIE(LOG_DEBUG) _("Service_connection: cannot read request") );
	}
	if( len < 2 ){
		FATAL(LOG_INFO) _("Service_connection: bad request line '%s'"), input );
	}

	/* read the permissions information */

	if( Perm_filters_line_list.count ){
		Free_line_list(&Perm_line_list);
		Merge_line_list(&Perm_line_list,&RawPerm_line_list,0,0,0);
		Filterprintcap( &Perm_line_list, &Perm_filters_line_list, "");
	}
   
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
			FATAL(LOG_INFO)
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
#if defined(MIT_KERBEROS4)
		case REQ_K4AUTH:
			Receive_k4auth( talk, input );
			break;
#endif
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
	int lockfd;
	char *path;
	struct stat statb;

	path = safestrdup3( Lockfile_DYN,".", Lpd_port_DYN, __FILE__, __LINE__ );
	lockfd = Checkwrite( path, &statb, O_WRONLY|O_TRUNC, 1, 0 );
	fchmod( lockfd, (statb.st_mode & ~0777) | 0644 );
	if( lockfd < 0 ){
		To_root();
		lockfd = Checkwrite( path, &statb, O_WRONLY|O_TRUNC, 1, 0 );
		if( lockfd > 0 ){
			fchown( lockfd, DaemonUID, DaemonGID );
			fchmod( lockfd, (statb.st_mode & ~0777) | 0644 );
		}
		To_daemon();
	}
	if( lockfd < 0 ){
		LOGERR_DIE(LOG_ERR) _("lpd: Cannot open '%s'"), path );
	} else {
		/* we write our PID */
		Server_pid = getpid();
		DEBUG1( "lpd: writing lockfile '%s' fd %d with pid '%d'",path,lockfd,Server_pid );
		Write_pid( lockfd, Server_pid, (char *)0 );
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

 static char *msg[] = {
	N_("usage: %s [-FV] [-D dbg] [-L log]\n"),
	N_(" Options\n"),
	N_(" -D dbg      - set debug level and flags\n"),
	N_("                 Example: -D10,remote=5\n"),
	N_("                 set debug level to 10, remote flag = 5\n"),
	N_(" -F          - run in foreground, log to STDERR\n"),
	N_("               Example: -D10,remote=5\n"),
	N_(" -L logfile  - append log information to logfile\n"),
	N_(" -V          - show version info\n"),
	0,
};

void usage(void)
{
	int i;
	char *s;
	for( i = 0; (s = msg[i]); ++i ){
		if( i == 0 ){
			FPRINTF( STDERR, _(s), Name);
		} else {
			FPRINTF( STDERR, "%s", _(s) );
		}
	}
	exit(1);
}

 char LPD_optstr[] 	/* LPD options */
 = "D:FL:VX" ;

void Get_parms(int argc, char *argv[] )
{
	int option, verbose = 0;

	while ((option = Getopt (argc, argv, LPD_optstr )) != EOF){
		switch (option) {
		case 'D': Parse_debug(Optarg, 1); break;
		case 'F': Foreground_LPD = 1; break;
		case 'L': Logfile_LPD = Optarg; break;
		case 'V': ++verbose; continue;
		case 'X': Worker_LPD = 1; break;
		default: usage(); break;
		}
	}
	if( Optind != argc ){
		usage();
	}
	if( verbose ) {
		FPRINTF( STDERR, "%s\n", Version );
		if( verbose > 1 ) Printlist( Copyright, 1 );
		exit(0);
	}
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
	{0,0}
};

int Start_all( int first_scan )
{
	struct line_list args, passfd;
	int pid, p[2];

	Init_line_list(&passfd);
	Init_line_list(&args);

	DEBUG1( "Start_all: begin" );
	Started_server = 0;
	if( pipe(p) == -1 ){
		LOGERR_DIE(LOG_INFO) _("Start_all: pipe failed!") );
	}
	Max_open(p[0]); Max_open(p[1]);
	DEBUG1( "Start_all: fd p(%d,%d)",p[0],p[1]);

	Setup_lpd_call( &passfd, &args );
	Set_str_value(&args,CALL,ALL);

	Check_max(&passfd,2);
	Set_decimal_value(&args,INPUT,passfd.count);
	passfd.list[passfd.count++] = Cast_int_to_voidstar(p[1]);
	Set_decimal_value(&args,FIRST_SCAN,first_scan);

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
		server_pid, change;
	char buffer[SMALLBUFFER], *pr, *forwarding;
	struct stat statb;
	int first_scan;
	
	/* we start up servers while we can */
	Name = "STARTALL";
	setproctitle( "lpd %s", Name );

	first_scan = Find_flag_value(args,FIRST_SCAN,Value_sep);
	reportfd = Find_flag_value(args,INPUT,Value_sep);
	Free_line_list(args);

	if(All_line_list.count == 0 ){
		Get_all_printcap_entries();
	}
	for( i = 0; i < All_line_list.count; ++i ){
		Set_DYN(&Printer_DYN,0);
		Set_DYN(&Spool_dir_DYN,0);
		pr = All_line_list.list[i];
		DEBUG1("Service_all: checking '%s'", pr );
		if( Setup_printer( pr, buffer, sizeof(buffer), 0) ) continue;
		/* now check to see if there is a server and unspooler process active */
		server_pid = 0;
		if( (fd = Checkread( Printer_DYN, &statb ) ) > 0 ){
			server_pid = Read_pid( fd, (char *)0, 0 );
			close( fd );
		}
		DEBUG3("Service_all: printer '%s' checking server pid %d", Printer_DYN, server_pid );
		if( server_pid > 0 && kill( server_pid, 0 ) == 0 ){
			DEBUG3("Get_queue_status: server %d active", server_pid );
			continue;
		}
		change = Find_flag_value(&Spool_control,CHANGE,Value_sep);
		printing_enabled = !(Pr_disabled(&Spool_control) || Pr_aborted(&Spool_control));

		Free_line_list( &Sort_order );
		if( Scan_queue( &Spool_control, &Sort_order,
				&printable,&held,&move, 1, first_scan, 0 ) ){
			Close_gdbm();
			continue;
		}
		forwarding = Find_str_value(&Spool_control,FORWARDING,Value_sep);
		if( change || move || (printable && (printing_enabled||forwarding)) ){
			if( Server_queue_name_DYN ){
				pr = Server_queue_name_DYN;
			} else {
				pr = Printer_DYN;;
			}
			DEBUG1("Service_all: starting '%s'", pr );
			SNPRINTF(buffer,sizeof(buffer))"%s\n",pr );
			if( Write_fd_str(reportfd,buffer) < 0 ) cleanup(0);
		}
	}
	Free_line_list( &Sort_order );
	Errorcode = 0;
	cleanup(0);
}

void Service_queue( struct line_list *args )
{
	int subserver;

	Set_DYN(&Printer_DYN, Find_str_value(args, PRINTER,Value_sep) );
	subserver = Find_flag_value( args, SUBSERVER, Value_sep );

	Free_line_list(args);
	Do_queue_jobs( Printer_DYN, subserver );
	cleanup(0);
}

plp_signal_t sigchld_handler (int signo)
{
	signal( SIGCHLD, SIG_DFL );
	write(Lpd_request,"\n", 1);
}

void Setup_waitpid (void)
{
	signal( SIGCHLD, SIG_DFL );
}

void Setup_waitpid_break (void)
{
	(void) plp_signal_break(SIGCHLD, sigchld_handler);
}

void Fork_error( int fork_failed )
{
	DEBUG1("Fork_error: %d", fork_failed );
	if( fork_failed < 0 ){
		LOGMSG(LOG_CRIT)"LPD: fork failed! LPD not accepting any requests");
	}
}
