/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2002, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: lpd.c,v 1.37 2002/08/12 00:01:44 papowell Exp $";


#include "lp.h"
#include "child.h"
#include "fileopen.h"
#include "errorcodes.h"
#include "initialize.h"
#include "linksupport.h"
#include "lpd_logger.h"
#include "getqueue.h"
#include "getopt.h"
#include "proctitle.h"
#include "lockfile.h"

/* force local definitions */
#undef EXTERN
#undef DEFINE
#undef DEFS

#define EXTERN
#define DEFINE(X) X
#define DEFS

#include "lpd.h"

 char* Lpd_listen_port_arg;	/* command line listent port value */
 char* Lpd_port_arg;	/* command line port value */
 char* Lpd_socket_arg; /* command line unix socket value */

#if HAVE_TCPD_H
#include <tcpd.h>
 int allow_severity = LOG_INFO;
 int deny_severity = LOG_WARNING;
#endif


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
	int lockfd;	/* the lock file descriptor */
	int err, newsock;
 	time_t last_time;	/* time that last Start_all was done */
 	time_t this_time;	/* current time */
	time_t servers_started_time;	/* time servers were started */
	plp_status_t status;
	int max_servers = 0;
	int start_fd = 0;
	int status_pid = 0;
	int request_pipe[2], status_pipe[2];
	int last_fork_pid_value;
	struct line_list args;
	struct sockaddr sinaddr;
	char *s;
	int first_scan = 1;
	int unix_sock = 0;

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

	if( UID_root && (OriginalRUID != ROOTUID) ){
		FATAL(LOG_ERR) "lpd installed SETUID root and started by user %d! Possible hacker attack", OriginalRUID);
	}

	Setup_configuration();

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
	pid = Get_lpd_pid();
#if defined(__CYGWIN__)
	if( (pid > 0) && ( kill(pid,0) || (errno != ESRCH) )) {
		DIEMSG( _("Another print spooler active, possibly lpd process '%d'"),
  				pid );
	}
	lockfd = Lock_lpd_pid();
	if( lockfd < 0 ){
		DIEMSG( _("cannot open or lock lockfile - %s"), Errormsg(errno) );
	}
	Set_lpd_pid( lockfd );
	close( lockfd );
	lockfd = -1;
#else
	lockfd = Lock_lpd_pid();
	if( lockfd < 0 ){
		DIEMSG( _("Another print spooler active, possibly lpd process '%d'"),
  				pid );
	}
	Set_lpd_pid( lockfd );
#endif

	s = Lpd_listen_port_arg;
	if( ISNULL(s) ) s = Lpd_listen_port_DYN;
	if( ISNULL(s) ) s = Lpd_port_DYN;
	if( !ISNULL(s) && safestrcasecmp( s,"off") && strtol(s,0,0) ){
		sock = Link_listen(s);
		DEBUG1("lpd: listening socket fd %d",sock);
		if( sock < 0 ){
			Errorcode = 1;
			DIEMSG("Cannot bind to lpd port '%s'", s);
		}
	}

	s = Lpd_socket_arg;
	if( ISNULL(s) ) s = Unix_socket_path_DYN;
	if( !ISNULL(s) && safestrcasecmp( s,"off") ){
		unix_sock = Unix_link_listen(s);
		DEBUG1("lpd: unix listening socket fd %d, path '%s'",unix_sock, s);
		if( unix_sock < 0 ){
			Errorcode = 1;
			DIEMSG("Cannot bind to UNIX socket '%s'", s );
		}
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

#if defined(__CYGWIN__)
	lockfd = Lock_lpd_pid();
	if( lockfd < 0 ) {
	   DIEMSG( "Can't open lockfile for writing" );
	}
	Set_lpd_pid( lockfd );
	close( lockfd );
	lockfd = -1;
#else
	Set_lpd_pid( lockfd );
#endif


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
		status_pid = Start_logger( status_pipe[0] );
		if( status_pid < 0 ){
			LOGERR_DIE(LOG_ERR) _("lpd: cannot start initial logger process") );
		}
	}

	/* open a connection to logger */
	setmessage(0,LPD,"Starting");

	/* get the maximum number of servers allowed */
	max_servers = Get_max_servers();
	DEBUG1( "lpd: maximum servers %d", max_servers );

	/*
	 * set up the select parameters
	 */

	FD_ZERO( &defreadfds );
	if( sock > 0 ) FD_SET( sock, &defreadfds );
	if( unix_sock > 0 ) FD_SET( unix_sock, &defreadfds );
	FD_SET( request_pipe[0], &defreadfds );

	/*
	 * start waiting for connections from processes
	 */

	last_time = time( (void *)0 );
 	servers_started_time = 0;

	last_fork_pid_value = start_fd = Start_all(first_scan);
	Fork_error( last_fork_pid_value );
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
			last_fork_pid_value = 1;
		}
		if( last_fork_pid_value > 0 && Logger_fd > 0 && status_pid < 0 ){
			DEBUG1( "lpd: restarting logger process");
			last_fork_pid_value = status_pid = Start_logger( status_pipe[0] );
			Fork_error( last_fork_pid_value );
			DEBUG1("lpd: status_pid %d", status_pid );
		}
		if( last_fork_pid_value < 0 ){
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
			DEBUG1("lpd: Poll_time_DYN %d, from last poll %d, to next poll %d",
				Poll_time_DYN, m, (int)timeval.tv_sec );
			if( m >= Poll_time_DYN ){
				if( Started_server || Force_poll_DYN ){
					last_fork_pid_value = start_fd = Start_all(first_scan);
					Fork_error( last_fork_pid_value );
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
		m = 0;

		/* allow a little space for people to send commands */
		this_time = time( (void *)0 );
		while( this_time - servers_started_time > Poll_start_interval_DYN
			&& last_fork_pid_value > 0 && Servers_line_list.count > 0
			&& m < Poll_servers_started_DYN
			&& n + m < max_servers-4 ){ 
			servers_started_time = this_time;
			s = Servers_line_list.list[0];
			DEBUG1("lpd: starting server '%s'", s );
			Set_str_value(&args,PRINTER,s);
			last_fork_pid_value = pid = Start_worker( "queue",&args, 0 );
			Fork_error( last_fork_pid_value );
			Free_line_list(&args);
			if( pid > 0 ){
				Remove_line_list( &Servers_line_list, 0 );
				Started_server = 1;
				++m;
			}
		}
		if( Servers_line_list.count > 0 && timeout == 0 ){
			memset(&timeval, 0, sizeof(timeval));
			timeval.tv_sec = Poll_start_interval_DYN;
			timeout = &timeval;
		}

		DEBUG1("lpd: started_server %d, last_fork_pid_value %d, processes %d active, max %d",
			Started_server, last_fork_pid_value, n, max_servers );
		/* do not accept incoming call if no worker available */
		readfds = defreadfds;
		if( n >= max_servers || last_fork_pid_value < 0 ){
			DEBUG1( "lpd: not accepting requests" );
			if( sock > 0 ) FD_CLR( sock, &readfds );
			if( unix_sock > 0 ) FD_CLR( unix_sock, &readfds );
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
			DEBUG1( "lpd: signal or time out, last_fork_pid_value %d", last_fork_pid_value );
			/* we try to fork now */
			if( last_fork_pid_value < 0 ) last_fork_pid_value = 1;
			continue;
		}
		if( sock > 0 && FD_ISSET( sock, &readfds ) ){
			int len;
			len = sizeof( sinaddr );
			newsock = accept( sock, &sinaddr, &len );
			err = errno;
			DEBUG1("lpd: connection fd %d", newsock );
			if( newsock > 0 ){
#if defined(TCPWRAPPERS)
/*
 * libwrap/tcp_wrappers:
 * draht@suse.de, Mon Jan 28 2002
 */

			    struct request_info wrap_req;

			    request_init(&wrap_req, RQ_DAEMON, "lpd" , RQ_FILE, newsock, NULL);
			    fromhost(&wrap_req);
			    openlog("lpd", LOG_PID, LOG_LPR); /* we syslog(3) initialized, no closelog(). */
			    if (hosts_access(&wrap_req)) {
				/* We accept. */
				syslog(LOG_INFO, "connection from %s", eval_client(&wrap_req));
#endif

				pid = Start_worker( "server", &args, newsock );
				if( pid < 0 ){
					LOGERR(LOG_INFO) _("lpd: fork() failed") );
					Write_fd_str( newsock, "\002Server load too high\n");
				} else {
					DEBUG1( "lpd: listener pid %d running", pid );
				}
#if defined(TCPWRAPPERS)
			    } else { /* we do not accept the connection: */
				syslog(LOG_WARNING, "connection refused from %s", eval_client(&wrap_req));
			    }
/* 
 * end libwrap
 */
#endif
				close( newsock );
				Free_line_list(&args);
			} else {
				errno = err;
				LOGERR(LOG_INFO) _("lpd: accept on listening socket failed") );
			}
		}
		if( unix_sock > 0 && FD_ISSET( unix_sock, &readfds ) ){
			int len;
			len = sizeof( sinaddr );
			newsock = accept( unix_sock, &sinaddr, &len );
			err = errno;
			DEBUG1("lpd: unix socket connection fd %d", newsock );
			if( newsock > 0 ){
#if defined(TCPWRAPPERS)
/*
 * libwrap/tcp_wrappers:
 * draht@suse.de, Mon Jan 28 2002
 */

			    struct request_info wrap_req;

			    request_init(&wrap_req, RQ_DAEMON, "lpd" , RQ_FILE, newsock, NULL);
			    fromhost(&wrap_req);
			    openlog("lpd", LOG_PID, LOG_LPR); /* we syslog(3) initialized, no closelog(). */
			    if (hosts_access(&wrap_req)) {
				/* We accept. */
				syslog(LOG_INFO, "connection from %s", eval_client(&wrap_req));
#endif

				pid = Start_worker( "server", &args, newsock );
				if( pid < 0 ){
					LOGERR(LOG_INFO) _("lpd: fork() failed") );
					Write_fd_str( newsock, "\002Server load too high\n");
				} else {
					DEBUG1( "lpd: listener pid %d running", pid );
				}
#if defined(TCPWRAPPERS)
			    } else { /* we do not accept the connection: */
				syslog(LOG_WARNING, "connection refused from %s", eval_client(&wrap_req));
			    }
/* 
 * end libwrap
 */
#endif

				close( newsock );
				Free_line_list(&args);
			} else {
				errno = err;
				LOGERR(LOG_INFO) _("lpd: accept on listening socket failed") );
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

void Set_lpd_pid(int lockfd)
{
	/* we write our PID */
	if( ftruncate( lockfd, 0 ) ){
		LOGERR_DIE(LOG_ERR) _("lpd: Cannot truncate lock file") );
	}
	Server_pid = getpid();
	DEBUG1( "lpd: writing lockfile fd %d with pid '%d'",lockfd,Server_pid );
	Write_pid( lockfd, Server_pid, (char *)0 );
}

int Lock_lpd_pid(void)
{
	int lockfd;
	char *path;
	struct stat statb;
	int euid = geteuid();

	path = safestrdup3( Lockfile_DYN,".", Lpd_port_DYN, __FILE__, __LINE__ );
	To_euid_root();
	lockfd = Checkwrite( path, &statb, O_RDWR, 1, 0 );
	if( lockfd < 0 ){
		LOGERR_DIE(LOG_ERR) _("lpd: Cannot open lock file '%s'"), path );
	}
#if !defined(__CYGWIN__)
	fchown( lockfd, DaemonUID, DaemonGID );
	fchmod( lockfd, (statb.st_mode & ~0777) | 0644 );
#endif
	To_euid(euid);
	if( Do_lock( lockfd, 0 ) < 0 ){
		close( lockfd );
		lockfd = -1;
	}
	return(lockfd);
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
		Started_server = 1;
		buffer[status] = 0;
		/* we split up read line and record information */
		Split(&l,buffer,Whitespace,0,0,0,0,0,0);
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
	N_("usage: %s [-FV][-D dbg][-L log][-P path][-p port][-R remote LPD TCP/IP destination port]\n"),
	N_(" Options\n"),
	N_(" -D dbg      - set debug level and flags\n"),
	N_(" -F          - run in foreground, log to STDERR\n"),
	N_(" -L logfile  - append log information to logfile\n"),
	N_(" -V          - show version info\n"),
	N_(" -p port     - TCP/IP listen port, 'off' disables TCP/IP listening port (lpd_listen_port)\n"),
	N_(" -P path     - UNIX socket path, 'off' disables UNIX listening socket (unix_socket_path)\n"),
	N_(" -R port     - remote LPD server port (lpd_port)\n"),
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
	Parse_debug("=",-1);
	FPRINTF( STDERR, "%s\n", Version );
	exit(1);
}

 char LPD_optstr[] 	/* LPD options */
 = "D:FL:VX:p:P:" ;

void Get_parms(int argc, char *argv[] )
{
	int option, verbose = 0;

	while ((option = Getopt (argc, argv, LPD_optstr )) != EOF){
		switch (option) {
		case 'D': Parse_debug(Optarg, 1); break;
		case 'F': Foreground_LPD = 1; break;
		case 'L': Logfile_LPD = Optarg; break;
		case 'V': ++verbose; break;
        case 'X': Worker_LPD = Optarg; break;
		case 'p': Lpd_listen_port_arg = Optarg; break;
		case 'P': Lpd_socket_arg = Optarg; break;
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
	Set_str_value(&args,CALL,"all");

	Check_max(&passfd,2);
	Set_decimal_value(&args,INPUT,passfd.count);
	passfd.list[passfd.count++] = Cast_int_to_voidstar(p[1]);
	Set_decimal_value(&args,FIRST_SCAN,first_scan);

	pid = Make_lpd_call( "all", &passfd, &args );

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

void Fork_error( int last_fork_pid_value )
{
	DEBUG1("Fork_error: %d", last_fork_pid_value );
	if( last_fork_pid_value < 0 ){
		LOGMSG(LOG_CRIT)"LPD: fork failed! LPD not accepting any requests");
	}
}
