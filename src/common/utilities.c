/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: utilities.c,v 5.2 1999/10/23 02:37:09 papowell Exp papowell $";

#include "lp.h"

#include "utilities.h"
#include "getopt.h"
#include "errorcodes.h"

/**** ENDINCLUDE ****/

/*
 * Time_str: return "cleaned up" ctime() string...
 *
 * in YY/MO/DY/hr:mn:sc
 * Thu Aug 4 12:34:17 BST 1994 -> 12:34:17
 */

char *Time_str(int shortform, time_t t)
{
    static char buffer[99];
	struct tm *tmptr;
	struct timeval tv;

	tv.tv_usec = 0;
	if( t == 0 ){
		if( gettimeofday( &tv, 0 ) == -1 ){
			Errorcode = JFAIL;
			logerr_die( LOG_ERR,"Time_str: gettimeofday failed");
		}
		t = tv.tv_sec;
	}
	tmptr = localtime( &t );
	if( shortform && Full_time_DYN == 0 ){
		plp_snprintf( buffer, sizeof(buffer),
			"%02d:%02d:%02d.%03d",
			tmptr->tm_hour, tmptr->tm_min, tmptr->tm_sec,
			(int)(tv.tv_usec/1000) );
	} else {
		plp_snprintf( buffer, sizeof(buffer),
			"%d-%02d-%02d-%02d:%02d:%02d.%03d",
			tmptr->tm_year+1900, tmptr->tm_mon+1, tmptr->tm_mday,
			tmptr->tm_hour, tmptr->tm_min, tmptr->tm_sec,
			(int)(tv.tv_usec/1000) );
	}
	/* now format the time */
	if( Ms_time_resolution_DYN == 0 ){
		char *s;
		if( ( s = safestrrchr( buffer, '.' )) ){
			*s = 0;
		}
	}
	return( buffer );
}


/*
 * Time_str: return "cleaned up" ctime() string...
 *
 * in YY/MO/DY/hr:mn:sc
 * Thu Aug 4 12:34:17 BST 1994 -> 12:34:17
 */

char *Pretty_time( time_t t )
{
    static char buffer[99];
	struct tm *tmptr;
	struct timeval tv;

	tv.tv_usec = 0;
	if( t == 0 ){
		if( gettimeofday( &tv, 0 ) == -1 ){
			Errorcode = JFAIL;
			logerr_die( LOG_ERR,"Time_str: gettimeofday failed");
		}
		t = tv.tv_sec;
	}
	tmptr = localtime( &t );
	strftime( buffer, sizeof(buffer), "%b %d %R %Y", tmptr );

	return( buffer );
}

time_t Convert_to_time_t( char *str )
{
	time_t t = 0;
	if(str) t = strtol(str,0,0);
	DEBUG5("Convert_to_time_t: %s = %d", str, t );
	return(t);
}

/***************************************************************************
 * Print the usage message list or any list of strings
 *  Use for copyright printing as well
 ***************************************************************************/

void Printlist( char **m, FILE *f )
{
	if( *m ){
		fprintf( f, *m, Name );
		fprintf( f, "\n" );
		++m;
	}

    for( ; *m; ++m ){
        fprintf( f, "%s\n", *m );
    }
	fflush(f);
}


/***************************************************************************
 * Utility functions: write a string to a fd (bombproof)
 *   write a char array to a fd (fairly bombproof)
 * Note that there is a race condition here that is unavoidable;
 * The problem is that there is no portable way to disable signals;
 *  post an alarm;  <enable signals and do a read simultaneously>
 * There is a solution that involves forking a subprocess, but this
 *  is so painful as to be not worth it.  Most of the timeouts
 *  in the LPR stuff are in the order of minutes, so this is not a problem.
 *
 * Note: we do the write first and then check for timeout.
 ***************************************************************************/

int Write_fd_len( int fd, const char *msg, int len )
{
	int i;

	i = len;
	while( len > 0 && (i = write( fd, msg, len ) ) >= 0 ){
		len -= i, msg += i;
	}
	return( i );
}

int Write_fd_len_timeout( int timeout, int fd, const char *msg, int len )
{
	int i;
	if( Set_timeout() ){
		Set_timeout_alarm( timeout  );
		i = Write_fd_len( fd, msg, len );
	} else {
		i = -1;
	}
	Clear_timeout();
	return( i );
}

int Write_fd_str( int fd, const char *msg )
{
	if( msg && *msg ){
		return( Write_fd_len( fd, msg, strlen(msg) ));
	}
	return( 0 );
}

int Write_fd_str_timeout( int timeout, int fd, const char *msg )
{
	if( msg && *msg ){
		return( Write_fd_len_timeout( timeout, fd, msg, strlen(msg) ) );
	}
	return( 0 );
}

int Read_fd_len_timeout( int timeout, int fd, char *msg, int len )
{
	int i;
	if( Set_timeout() ){
		Set_timeout_alarm( timeout  );
		i = read( fd, msg, len );
	} else {
		i = -1;
	}
	Clear_timeout();
	return( i );
}


/**************************************************************
 * 
 * signal handling:
 * SIGALRM should be the only signal that terminates system calls;
 * all other signals should NOT terminate them.
 * This signal() emulation function attepts to do just that.
 * (Derived from Advanced Programming in the UNIX Environment, Stevens, 1992)
 *
 **************************************************************/


/* solaris 2.3 note: don't compile this with "gcc -ansi -pedantic";
 * due to a bug in the header file, struct sigaction doesn't
 * get declared. :(
 */

/* plp_signal will set flags so that signal handlers will continue
 * note that in Solaris,  you MUST reinstall the
 * signal hanlders in the signal handler!  The default action is
 * to try to restart the system call - note that the code should
 * be written so that you check for error returns, and continue
 * so this is merely a convenience.
 */

plp_sigfunc_t plp_signal (int signo, plp_sigfunc_t func)
{
#ifdef HAVE_SIGACTION
	struct sigaction act, oact;

	act.sa_handler = func;
	(void) sigemptyset (&act.sa_mask);
	act.sa_flags = 0;
# ifdef SA_RESTART
	act.sa_flags |= SA_RESTART;             /* SVR4, 4.3+BSD */
# endif
	if (sigaction (signo, &act, &oact) < 0) {
		return (SIG_ERR);
	}
	return (plp_sigfunc_t) oact.sa_handler;
#else
	/* sigaction is not supported. Just set the signals. */
	return (plp_sigfunc_t)signal (signo, func); 
#endif
}

/* plp_signal_break is similar to plp_signal,  but will cause
 * TERMINATION of a system call if possible.  This allows
 * you to force a signal to cause termination of a system
 * wait or other action.
 */

plp_sigfunc_t plp_signal_break (int signo, plp_sigfunc_t func)
{
#ifdef HAVE_SIGACTION
	struct sigaction act, oact;

	act.sa_handler = func;
	(void) sigemptyset (&act.sa_mask);
	act.sa_flags = 0;
# ifdef SA_INTERRUPT
	act.sa_flags |= SA_INTERRUPT;            /* SunOS */
# endif
	if (sigaction (signo, &act, &oact) < 0) {
		return (SIG_ERR);
	}
	return (plp_sigfunc_t) oact.sa_handler;
#else
	/* sigaction is not supported. Just set the signals. */
	return (plp_sigfunc_t)signal (signo, func); 
#endif
}

/**************************************************************/

void plp_block_all_signals ( plp_block_mask *oblock )
{
#ifdef HAVE_SIGPROCMASK
	sigset_t block;

	(void) sigfillset (&block); /* block all signals */
	if (sigprocmask (SIG_SETMASK, &block, oblock) < 0)
		logerr_die( LOG_ERR, "plp_block_all_signals: sigprocmask failed");
#else
	*oblock = sigblock( ~0 ); /* block all signals */
#endif
}


void plp_unblock_all_signals ( plp_block_mask *oblock )
{
#ifdef HAVE_SIGPROCMASK
	sigset_t block;

	(void) sigemptyset (&block); /* block all signals */
	if (sigprocmask (SIG_SETMASK, &block, oblock) < 0)
		logerr_die( LOG_ERR, "plp_unblock_all_signals: sigprocmask failed");
#else
	*oblock = sigblock( 0 ); /* unblock all signals */
#endif
}

void plp_set_signal_mask ( plp_block_mask *in, plp_block_mask *out )
{
#ifdef HAVE_SIGPROCMASK
	if (sigprocmask (SIG_SETMASK, in, out ) < 0)
		logerr_die( LOG_ERR, "plp_set_signal_mask: sigprocmask failed");
#else
	if( out ){
		*out = sigblock( *in ); /* block all signals */
	} else {
	 	(void)sigblock( *in ); /* block all signals */
	}
#endif
}

void plp_unblock_one_signal ( int sig, plp_block_mask *oblock )
{
#ifdef HAVE_SIGPROCMASK
	sigset_t block;

	(void) sigemptyset (&block); /* clear out signals */
	(void) sigaddset (&block, sig ); /* clear out signals */
	if (sigprocmask (SIG_UNBLOCK, &block, oblock ) < 0)
		logerr_die( LOG_ERR, "plp_unblock_one_signal: sigprocmask failed");
#else
	*oblock = sigblock( 0 );
	(void) sigsetmask (*oblock & ~ sigmask(sig) );
#endif
}

void plp_block_one_signal( int sig, plp_block_mask *oblock )
{
#ifdef HAVE_SIGPROCMASK
	sigset_t block;

	(void) sigemptyset (&block); /* clear out signals */
	(void) sigaddset (&block, sig ); /* clear out signals */
	if (sigprocmask (SIG_BLOCK, &block, oblock ) < 0)
		logerr_die( LOG_ERR, "plp_block_one_signal: sigprocmask failed");
#else
	*oblock = sigblock( sigmask( sig ) );
#endif
}

void plp_sigpause( void )
{
#ifdef HAVE_SIGPROCMASK
	sigset_t block;
	(void) sigemptyset (&block); /* clear out signals */
	(void) sigsuspend( &block );
#else
	(void)sigpause( 0 );
#endif
}

/**************************************************************
 * Bombproof versions of strcasecmp() and strncasecmp();
 **************************************************************/

/* case insensitive compare for OS without it */
int safestrcasecmp (const char *s1, const char *s2)
{
	int c1, c2, d;
	if( (s1 == s2) ) return(0);
	if( (s1 == 0 ) && s2 ) return( -1 );
	if( s1 && (s2 == 0 ) ) return( 1 );
	for (;;) {
		c1 = *s1++;
		c2 = *s2++;
		if (isalpha (c1) && isalpha (c2)) {
			c1 = tolower (c1);
			c2 = tolower (c2);
		}
		if( (d = (c1 - c2 )) || c1 == 0 ) return(d);
	}
	return( 0 );
}

/* case insensitive compare for OS without it */
int safestrncasecmp (const char *s1, const char *s2, int len )
{
	int c1, c2, d;
	if( (s1 == s2) && s1 == 0 ) return(0);
	if( (s1 == 0 ) && s2 ) return( -1 );
	if( s1 && (s2 == 0 ) ) return( 1 );
	for (;len>0;--len){
		c1 = *s1++;
		c2 = *s2++;
		if (isalpha (c1) && isalpha (c2)) {
			c1 = tolower (c1);
			c2 = tolower (c2);
		}
		if( (d = (c1 - c2 )) || c1 == 0 ) return(d);
	}
	return( 0 );
}

/* perform safe comparison, even with null pointers */
int safestrcmp( const char *s1, const char *s2 )
{
	if( (s1 == s2) ) return(0);
	if( (s1 == 0 ) && s2 ) return( -1 );
	if( s1 && (s2 == 0 ) ) return( 1 );
	return( strcmp(s1, s2) );
}


/* perform safe comparison, even with null pointers */
int safestrncmp( const char *s1, const char *s2, int len )
{
	if( (s1 == s2) && s1 == 0 ) return(0);
	if( (s1 == 0 ) && s2 ) return( -1 );
	if( s1 && (s2 == 0 ) ) return( 1 );
	return( strncmp(s1, s2, len) );
}


/* perform safe strchr, even with null pointers */
char *safestrchr( const char *s1, int c )
{
	if( s1 ) return( strchr( s1, c ) );
	return( 0 );
}


/* perform safe strrchr, even with null pointers */
char *safestrrchr( const char *s1, int c )
{
	if( s1 ) return( strrchr( s1, c ) );
	return( 0 );
}


/* perform safe strchr, even with null pointers */
char *safestrpbrk( const char *s1, const char *s2 )
{
	if( s1 && s2 ) return( strpbrk( s1, s2 ) );
	return( 0 );
}

/***************************************************************************
 * plp_usleep() with select - simple minded way to avoid problems
 ***************************************************************************/
int plp_usleep( int i )
{
	struct timeval t;
	DEBUG3("plp_usleep: starting usleep %d", i );
	if( i > 0 ){
		memset( &t, 0, sizeof(t) );
		t.tv_usec = i;
		i = select( 0,
			FD_SET_FIX((fd_set *))(0),
			FD_SET_FIX((fd_set *))(0),
			FD_SET_FIX((fd_set *))(0),
			&t );
		DEBUG3("plp_usleep: select done, status %d", i );
	}
	return( i );
}


/***************************************************************************
 * plp_sleep() with select - simple minded way to avoid problems
 ***************************************************************************/
int plp_sleep( int i )
{
	struct timeval t;
	DEBUG3("plp_sleep: starting sleep %d", i );
	if( i > 0 ){
		memset( &t, 0, sizeof(t) );
		t.tv_sec = i;
		i = select( 0,
			FD_SET_FIX((fd_set *))(0),
			FD_SET_FIX((fd_set *))(0),
			FD_SET_FIX((fd_set *))(0),
			&t );
		DEBUG3("plp_sleep: select done, status %d", i );
	}
	return( i );
}


/***************************************************************************
 * int get_max_processes()
 *  get the maximum number of processes allowed
 ***************************************************************************/

int Get_max_servers( void )
{
	int n = 0;	/* We need some sort of limit here */

#if defined(HAVE_GETRLIMIT) && defined(RLIMIT_NPROC)
	struct rlimit pcount;
	if( getrlimit(RLIMIT_NPROC, &pcount) == -1 ){
		fatal( LOG_ERR, "Get_max_servers: getrlimit failed" );
	}
	n = pcount.rlim_cur;
	DEBUG1("Get_max_servers: getrlimit returns %d", n );
#else
# if defined(HAVE_SYSCONF) && defined(_SC_CHILD_MAX)
	if( n == 0 && (n = sysconf(_SC_CHILD_MAX)) < 0 ){
		fatal( LOG_ERR, "Get_max_servers: sysconf failed" );
	}
	DEBUG1("Get_max_servers: sysconf returns %d", n );
# else
#  if defined(CHILD_MAX)
		n = CHILD_MAX;
		DEBUG1("Get_max_servers: CHILD_MAX %d", n );
#  else
		n = 20;
		DEBUG1("Get_max_servers: default %d", n );
#  endif
# endif
#endif
	n = n/2;

	if( Max_servers_active_DYN && n > Max_servers_active_DYN ) n = Max_servers_active_DYN;
	DEBUG1("Get_max_servers: returning %d", n );
	return( n );
}


/***************************************************************************
 * int get_max_processes()
 *  get the maximum number of processes allowed
 ***************************************************************************/

int Get_max_fd( void )
{
	int n = 0;	/* We need some sort of limit here */

#if defined(HAVE_GETRLIMIT) && defined(RLIMIT_NOFILE)
	struct rlimit pcount;
	if( getrlimit(RLIMIT_NOFILE, &pcount) == -1 ){
		fatal( LOG_ERR, "Get_max_fd: getrlimit failed" );
	}
	n = pcount.rlim_cur;
	DEBUG1("Get_max_fd: getrlimit returns %d", n );
#else
# if defined(HAVE_SYSCONF) && defined(_SC_OPEN_MAX)
	if( n == 0 && (n = sysconf(_SC_OPEN_MAX)) < 0 ){
		fatal( LOG_ERR, "Get_max_servers: sysconf failed" );
	}
	DEBUG1("Get_max_fd: sysconf returns %d", n );
# else
	n = 20;
	DEBUG1("Get_max_fd: using default %d", n );
# endif
#endif

	DEBUG1("Get_max_fd: returning %d", n );
	return( n );
}


char *Brk_check_size( void )
{
	static char b[128];
	static char* Top_of_mem;	/* top of allocated memory */
	char *s = sbrk(0);
	int   v = s - Top_of_mem;
	if( Top_of_mem == 0 ){
		plp_snprintf(b, sizeof(b), "BRK: initial value 0x%lx", Cast_ptr_to_long(s) );
	} else {
		plp_snprintf(b, sizeof(b), "BRK: new value 0x%lx, increment %d", Cast_ptr_to_long(s), v );
	}
	Top_of_mem = s;
	return(b);
}

char *mystrncat( char *s1, const char *s2, int len )
{
	int size;
	s1[len-1] = 0;
	size = strlen( s1 );
	if( s2 && len - size > 0  ){
		strncpy( s1+size, s2, len - size );
	}
	return( s1 );
}
char *mystrncpy( char *s1, const char *s2, int len )
{
	s1[0] = 0;
	if( s2 && len-1 > 0 ){
		strncpy( s1, s2, len-1 );
		s1[len-1] = 0;
	}
	return( s1 );
}

/*
 * Set_non_block_io(fd)
 * Set_block_io(fd)
 *  Set blocking or non-blocking IO
 *  Dies if unsuccessful
 * Get_nonblock_io(fd)
 *  Returns O_NONBLOCK flag value
 */

int Get_nonblock_io( int fd )
{
	int mask;
	/* we set IO to non-blocking on fd */

	if( (mask = fcntl( fd, F_GETFL, 0 ) ) == -1 ){
		return(-1);
	}
	mask &= O_NONBLOCK;
	return( mask );
}

int Set_nonblock_io( int fd )
{
	int mask;
	/* we set IO to non-blocking on fd */

	if( (mask = fcntl( fd, F_GETFL, 0 ) ) == -1 ){
		return(-1);
	}
	mask |= O_NONBLOCK;
	if( (mask = fcntl( fd, F_SETFL, mask ) ) == -1 ){
		return(-1);
	}
	return(0);
}

int Set_block_io( int fd )
{
	int mask;
	/* we set IO to blocking on fd */

	if( (mask = fcntl( fd, F_GETFL, 0 ) ) == -1 ){
		return(-1);
	}
	mask &= ~O_NONBLOCK;
	if( (mask = fcntl( fd, F_SETFL, mask ) ) == -1 ){
		return(-1);
	}
	return(0);
}

/*
 * Read_write_timeout
 *  int readfd, char *inbuffer, int maxinlen -
 *    read data from this fd into this buffer before this timeout
 *  int *readlen  - reports number of bytes read
 *  int writefd, char **outbuffer, int *outlen -
 *     **outbuffer and **outlen are updated after write
 *     write data from to this fd from this buffer before this timeout
 *  int timeout
 *       > 0  - wait total of this long
 *       0    - wait indefinately
 *      -1    - do not wait
 *  Returns:
 *   **outbuffer, *outlen updated
 *    0    - success
 *    -1   - IO error on output
 *    -2   - Timeout
 *    -3   - IO error on input
 */

int Read_write_timeout(
	int readfd, char *inbuffer, int maxinlen, int *readlen,
	int writefd, char **outbuffer, int *outlen, int timeout )
{
	time_t start_t, current_t;
	int elapsed, readnonblocking, writenonblocking, m, err, done, retval;
	struct timeval timeval, *tp;
    fd_set readfds, writefds; /* for select() */

	DEBUG4( "Read_write_timeout: read(fd %d, buffer 0x%lx, maxinlen %d, readlen 0x%lx->%d",
		readfd, Cast_ptr_to_long(inbuffer), maxinlen, Cast_ptr_to_long(readlen),
		readlen?*readlen:0 );
	DEBUG4( "Read_write_timeout: write(fd %d, buffer 0x%lx->0x%lx, len 0x%lx->%d, timeout %d)",
		writefd, Cast_ptr_to_long(outbuffer), Cast_ptr_to_long(outbuffer?*outbuffer:0),
		Cast_ptr_to_long(outlen), outlen?*outlen:0, timeout );

	time( &start_t );

	readnonblocking = writenonblocking = 0;
	if( readfd >= 0
		&& !(readnonblocking = Get_nonblock_io( readfd )) ){
		Set_nonblock_io( readfd );
	}
	if( writefd >= 0 && *outlen > 0
		&& !(writenonblocking = Get_nonblock_io( writefd)) ){
		Set_nonblock_io( writefd );
	}

	retval = done = 0;
	while(!done){
		tp = 0;
		memset( &timeval, 0, sizeof(timeval) );
		if( timeout > 0 ){
			time( &current_t );
			elapsed = current_t - start_t;
			if( timeout > 0 && elapsed >= timeout ){
				break;
			}
			timeval.tv_sec = m = timeout - elapsed;
			tp = &timeval;
			DEBUG4("Read_write_timeout: timeout now %d", m );
		} else if( timeout < 0 ){
			/* we simply poll once */
			tp = &timeval;
		}
		FD_ZERO( &writefds );
		FD_ZERO( &readfds );
		m = 0;
		if( writefd >= 0 && *outlen > 0 ){
			FD_SET( writefd, &writefds );
			if( m <= writefd ) m = writefd+1;
		}
		if( readfd >= 0 ){
			FD_SET( readfd, &readfds );
			if( m <= readfd ) m = readfd+1;
		}
		errno = 0;
		DEBUG4("Read_write_timeout: starting select" );
        m = select( m,
            FD_SET_FIX((fd_set *))&readfds,
            FD_SET_FIX((fd_set *))&writefds,
            FD_SET_FIX((fd_set *))0, tp );
		err = errno;
		DEBUG4("Read_write_timeout: select returned %d, errno '%s'",
			m, Errormsg(err) );
		if( m < 0 ){
			if( err != EINTR ){
				/* error */
				retval = -1;
				done = 1;
			}
		} else if( m == 0 ){
			/* timeout */
			retval = -2;
			done = 1;
		} else {
			if( readfd >=0 && FD_ISSET( readfd, &readfds ) ){
				DEBUG4("Read_write_timeout: read possible on fd %d", readfd );
				m = read( readfd, inbuffer, maxinlen );
				if( readlen ) *readlen = m;
				/* caller leaves space for this */
				if( m >= 0 ) inbuffer[m] = 0;
				if( m < 0 ) retval = -3;
				done = 1;
			}
			if( writefd >=0 && FD_ISSET( writefd, &writefds ) ){
				DEBUG4("Read_write_timeout: write possible on fd %d", writefd );
				m = write( writefd, *outbuffer, *outlen );
				DEBUG4("Read_write_timeout: wrote %d", m );
				if( m <= 0 ){
					/* we have EOF on the file descriptor */
					retval = -1;
					done = 1;
				} else {
					*outlen -= m;
					*outbuffer += m;
					if( *outlen == 0 ){
						done = 1;
					}
				}
			}
		}
	}
	err = errno;
	if( !readnonblocking ) Set_block_io( readfd );
	if( !writenonblocking ) Set_block_io( writefd );
	errno = err;
	return( retval );
}

/***************************************************************************
 * Set up alarms so PLP doesn't hang forever during transfers.
 ***************************************************************************/

/*
 * timeout_alarm
 *  When we get the alarm,  we close the file descriptor (if any)
 *  we are working with.  When we next do an option, it will fail
 *  Note that this will cause any ongoing read/write operation to fail
 * We then to a longjmp to the routine, returning a non-zero value
 * We set an alarm using:
 *
 * if( (setjmp(Timeout_env)==0 && Set_timeout_alarm(t,s)) ){
 *   timeout dependent stuff
 * }
 * Clear_alarm
 * We define the Set_timeout macro as:
 *  #define Set_timeout(t,s) (setjmp(Timeout_env)==0 && Set_timeout_alarm(t,s))
 */

 static plp_signal_t timeout_alarm (int sig)
{
	Alarm_timed_out = 1;
	signal( SIGALRM, SIG_IGN );
#if defined(HAVE_SIGLONGJMP)
	siglongjmp(Timeout_env,1);
#else
	longjmp(Timeout_env,1);
#endif
}


/***************************************************************************
 * Set_timeout( int timeout, int *socket )
 *  Set up a timeout to occur; note that you can call this
 *   routine several times without problems,  but you must call the
 *   Clear_timeout routine sooner or later to reset the timeout function.
 *  A timeout value of 0 never times out
 * Clear_alarm()
 *  Turns off the timeout alarm
 ***************************************************************************/

void Set_timeout_alarm( int timeout )
{
	int err = errno;

	signal(SIGALRM, SIG_IGN);
	alarm(0);
	Alarm_timed_out = 0;
	Timeout_pending = 0;

	if( timeout > 0 ){
		Timeout_pending = timeout;
		plp_signal_break(SIGALRM, timeout_alarm);
		alarm (timeout);
	}
	errno = err;
}

void Clear_timeout( void )
{
	int err = errno;

	signal( SIGALRM, SIG_IGN );
	alarm(0);
	Timeout_pending = 0;
	errno = err;
}

/*
 * setuid.c:
 * routines to manipulate user-ids securely (and hopefully, portably).
 * The * internals of this are very hairy, because
 * (a) there's lots of sanity checking
 * (b) there's at least three different setuid-swapping
 * semantics to support :(
 * 
 *
 * Note that the various functions saves errno then restores it afterwards;
 * this means it's safe to do "root_to_user();some_syscall();user_to_root();"
 * and errno will be from the system call.
 * 
 * "root" is the user who owns the setuid executable (privileged).
 * "user" is the user who runs it.
 * "daemon" owns data files used by the PLP utilities (spool directories, etc).
 *   and is set by the 'user' entry in the configuration file.
 * 
 * To_user();	-- set euid to user, ruid to root
 * To_ruid_user();	-- set ruid to user, euid to root
 * To_root();	-- set euid to root
 * To_daemon();	-- set euid to daemon
 * To_root();	-- set euid to root
 * Full_daemon_perms() -- set both UID and EUID, one way, no return
 * 
 */

/***************************************************************************
 * Commentary:
 * Patrick Powell Sat Apr 15 07:56:30 PDT 1995
 * 
 * This has to be one of the ugliest parts of any portability suite.
 * The following models are available:
 * 1. process has <uid, euid>  (old SYSV, BSD)
 * 2. process has <uid, euid, saved uid, saved euid> (new SYSV, BSD)
 * 
 * There are several possibilites:
 * 1. need euid root   to do some operations
 * 2. need euid user   to do some operations
 * 3. need euid daemon to do some operations
 * 
 * Group permissions are almost useless for a server;
 * usually you are running as a specified group ID and do not
 * need to change.  Client programs are slightly different.
 * You need to worry about permissions when creating a file;
 * for this reason most client programs do a u mask(0277) before any
 * file creation to ensure that nobody can read the file, and create
 * it with only user access permissions.
 * 
 * > int setuid(uid) uid_t uid;
 * > int seteuid(euid) uid_t euid;
 * > int setruid(ruid) uid_t ruid;
 * > 
 * > DESCRIPTION
 * >      setuid() (setgid()) sets both the real and effective user ID
 * >      (group  ID) of the current process as specified by uid (gid)
 * >      (see NOTES).
 * > 
 * >      seteuid() (setegid()) sets the effective user ID (group  ID)
 * >      of the current process.
 * > 
 * >      setruid() (setrgid()) sets the real user ID  (group  ID)  of
 * >      the current process.
 * > 
 * >      These calls are only permitted to the super-user or  if  the
 * >      argument  is  the  real  or effective user (group) ID of the
 * >      calling process.
 * > 
 * > SYSTEM V DESCRIPTION
 * >      If the effective user ID  of  the  calling  process  is  not
 * >      super-user,  but if its real user (group) ID is equal to uid
 * >      (gid), or if the saved set-user (group) ID  from  execve(2V)
 * >      is equal to uid (gid), then the effective user (group) ID is
 * >      set to uid (gid).
 * >      .......  etc etc
 * 
 * Conclusions:
 * 1. if EUID == ROOT or RUID == ROOT then you can set EUID, UID to anything
 * 3. if EUID is root, you can set EUID 
 * 
 * General technique:
 * Initialization
 *   - use setuid() system call to force EUID/RUID = ROOT
 * 
 * Change
 *   - assumes that initialization has been carried out and
 * 	EUID == ROOT or RUID = ROOT
 *   - Use the seteuid() system call to set EUID
 * 
 ***************************************************************************/

#if !defined(HAVE_SETREUID) && !defined(HAVE_SETEUID) && !defined(HAVE_SETRESUID)
#error You need one of setreuid(), seteuid(), setresuid()
#endif

/***************************************************************************
 * Commentary
 * setuid(), setreuid(), and now setresuid()
 *  This is probably the easiest road.
 *  Note: we will use the most feature ridden one first, as it probably
 *  is necessary on some wierd system.
 *   Patrick Powell Fri Aug 11 22:46:39 PDT 1995
 ***************************************************************************/
#if !defined(HAVE_SETEUID) && !defined(HAVE_SETREUID) && defined(HAVE_SETRESUID)
# define setreuid(x,y) (setresuid( (x), (y), -1))
# define HAVE_SETREUID
#endif

/***************************************************************************
 * setup_info()
 * 1. checks for the correct daemon uid
 * 2. checks to see if called (only needs to do this once)
 * 3. if UID 0 or EUID 0 forces both UID and EUID to 0 (test)
 * 4. Sets UID_root flag to indicate that we can change
 ***************************************************************************/

 static void setup_info(void)
{
	int err = errno;
	static int SetRootUID;	/* did we set UID to root yet? */

	DaemonUID = Getdaemon(); /* do this each time in case we change it */
	if( SetRootUID == 0 ){
		OriginalEUID = geteuid();	
		OriginalRUID = getuid();	
		/* we now make sure that we are able to use setuid() */
		/* notice that setuid() will work if EUID or RUID is 0 */
		if( OriginalEUID == 0 || OriginalRUID == 0 ){
			/* set RUID/EUID to ROOT - possible if EUID or UID is 0 */
			if(
#				ifdef HAVE_SETEUID
					setuid( (uid_t)0 ) || seteuid( (uid_t)0 )
#				else
					setuid( (uid_t)0 ) || setreuid( 0, 0 )
#				endif
				){
				fatal( LOG_ERR,
					"setup_info: RUID/EUID Start %d/%d seteuid failed",
					OriginalRUID, OriginalEUID);
			}
			if( getuid() || geteuid() ){
				fatal( LOG_ERR,
				"setup_info: IMPOSSIBLE! RUID/EUID Start %d/%d, now %d/%d",
					OriginalRUID, OriginalEUID, 
					getuid(), geteuid() );
			}
			UID_root = 1;
		}
		SetRootUID = 1;
	}
	errno = err;
}

/***************************************************************************
 * seteuid_wrapper()
 * 1. you must have done the initialization
 * 2. check to see if you need to do anything
 * 3. check to make sure you can
 ***************************************************************************/
 static int seteuid_wrapper( int to )
{
	int err = errno;
	uid_t euid;


	DEBUG4(
		"seteuid_wrapper: Before RUID/EUID %d/%d, DaemonUID %d, UID_root %d",
		OriginalRUID, OriginalEUID, DaemonUID, UID_root );
	if( UID_root ){
		/* be brutal: set both to root */
		if( setuid( 0 ) ){
			logerr_die( LOG_ERR,
			"seteuid_wrapper: setuid() failed!!");
		}
#if defined(HAVE_SETEUID)
		if( seteuid( to ) ){
			logerr_die( LOG_ERR,
			"seteuid_wrapper: seteuid() failed!!");
		}
#else
		if( setreuid( 0, to) ){
			logerr_die( LOG_ERR,
			"seteuid_wrapper: setreuid() failed!!");
		}
#endif
	}
	euid = geteuid();
	DEBUG4( "seteuid_wrapper: After uid/euid %d/%d", getuid(), euid );
	errno = err;
	return( to != euid );
}


/***************************************************************************
 * setruid_wrapper()
 * 1. you must have done the initialization
 * 2. check to see if you need to do anything
 * 3. check to make sure you can
 ***************************************************************************/
 static int setruid_wrapper( int to )
{
	int err = errno;
	uid_t ruid;


	DEBUG4(
		"setruid_wrapper: Before RUID/EUID %d/%d, DaemonUID %d, UID_root %d",
		OriginalRUID, OriginalEUID, DaemonUID, UID_root );
	if( UID_root ){
		/* be brutal: set both to root */
		if( setuid( 0 ) ){
			logerr_die( LOG_ERR,
			"setruid_wrapper: setuid() failed!!");
		}
#if defined(HAVE_SETRUID)
		if( setruid( to ) ){
			logerr_die( LOG_ERR,
			"setruid_wrapper: setruid() failed!!");
		}
#else
		if( setreuid( to, 0) ){
			logerr_die( LOG_ERR,
			"setruid_wrapper: setreuid() failed!!");
		}
#endif
	}
	ruid = getuid();
	DEBUG4( "setruid_wrapper: After uid/euid %d/%d", getuid(), geteuid() );
	errno = err;
	return( to != ruid );
}


/*
 * Superhero functions - change the EUID to the requested one
 *  - these are really idiot level,  as all of the tough work is done
 * in setup_info() and seteuid_wrapper() 
 */
int To_root(void)
{
	setup_info(); return( seteuid_wrapper( 0 )	);
}
int To_daemon(void)
{
	setup_info(); return( seteuid_wrapper( DaemonUID )	);
}
int To_user(void)
{
	setup_info(); return( seteuid_wrapper( OriginalRUID )	);
}
int To_ruid_user(void)
{
	setup_info(); return( setruid_wrapper( OriginalRUID )	);
}
int To_uid( int uid )
{
	setup_info(); return( seteuid_wrapper( uid ) );
}

/*
 * set both uid and euid to the same value, using setuid().
 * This is unrecoverable!
 */

int setuid_wrapper(int to)
{
	int err = errno;
	if( UID_root ){
		/* Note: you MUST use setuid() to force saved_setuid correctly */
		if( setuid( (uid_t)0 ) ){
			logerr_die( LOG_ERR, "setuid_wrapper: setuid(0) failed!!");
		}
		if( setuid( (uid_t)to ) ){
			logerr_die( LOG_ERR, "setuid_wrapper: setuid(%d) failed!!", to);
		}
	}
    DEBUG4("after setuid: (%d, %d)", getuid(),geteuid());
	errno = err;
	return( to != getuid() || to != geteuid() );
}

int Full_daemon_perms(void)
{
	setup_info(); return(setuid_wrapper(DaemonUID));
}
int Full_root_perms(void)
{
	setup_info(); return(setuid_wrapper( 0 ));
}
int Full_user_perms(void)
{
	setup_info(); return(setuid_wrapper(OriginalRUID));
}


/***************************************************************************
 * Getdaemon()
 *  get daemon uid
 *
 ***************************************************************************/

int Getdaemon(void)
{
	char *str = 0;
	char *t;
	struct passwd *pw;
	int uid;

	str = Daemon_user_DYN;
	DEBUG4( "Getdaemon: using '%s'", str );
	if(!str) str = "daemon";
	t = str;
	uid = strtol( str, &t, 10 );
	if( str == t || *t ){
		/* try getpasswd */
		pw = getpwnam( str );
		if( pw ){
			uid = pw->pw_uid;
		}
	}
	DEBUG4( "Getdaemon: uid '%d'", uid );
	if( uid == 0 ) uid = getuid();
	DEBUG4( "Getdaemon: final uid '%d'", uid );
	return( uid );
}

/***************************************************************************
 * Getdaemon_group()
 *  get daemon gid
 *
 ***************************************************************************/

int Getdaemon_group(void)
{
	char *str = 0;
	char *t;
	struct group *gr;
	gid_t gid;

	str = Daemon_group_DYN;
	DEBUG4( "Getdaemon_group: Daemon_group_DYN '%s'", str );
	if( !str ) str = "daemon";
	DEBUG4( "Getdaemon_group: name '%s'", str );
	t = str;
	gid = strtol( str, &t, 10 );
	if( str == t ){
		/* try getpasswd */
		gr = getgrnam( str );
		if( gr ){
			gid = gr->gr_gid;
		}
	}
	DEBUG4( "Getdaemon_group: gid '%d'", gid );
	if( gid == 0 ) gid = getgid();
	DEBUG4( "Getdaemon_group: final gid '%d'", gid );
	return( gid );
}

/***************************************************************************
 * set daemon uid and group
 * 1. get the current EUID
 * 2. set up the permissions changing
 * 3. set the RGID/EGID
 ***************************************************************************/

int Setdaemon_group(void)
{
	uid_t euid;
	int status;
	int err;

	DaemonGID = Getdaemon_group();
	DEBUG4( "Setdaemon_group: set '%d'", DaemonGID );
	if( UID_root ){
		euid = geteuid();
		To_root();	/* set RUID/EUID to root */
		status = setgid( DaemonGID );
		err = errno;
		if( To_uid( euid ) ){
			err = errno;
			logerr_die( LOG_ERR, "setdaemon_group: To_uid '%d' failed '%s'",
				euid, Errormsg( err ) );
		}
		if( status < 0 || DaemonGID != getegid() ){
			logerr_die( LOG_ERR, "setdaemon_group: setgid '%d' failed '%s'",
			DaemonGID, Errormsg( err ) );
		}
	}
	return( 0 );
}


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

void Reset_daemonuid(void)
{
	uid_t uid;
    uid = Getdaemon();  /* get the config file daemon id */
    if( uid != DaemonUID ){
        if( uid == 0 ){
            DaemonUID = OriginalRUID;   /* special case for testing */
        } else {
            DaemonUID = uid;
        }
    }
	To_daemon();        /* now we are running with desired UID */
    DEBUG4( "DaemonUID %d", DaemonUID );
}


#ifdef HAVE_SYS_MOUNT_H
# include <sys/mount.h>
#endif
#ifdef HAVE_SYS_STATVFS_H
# include <sys/statvfs.h>
#endif
#ifdef HAVE_SYS_STATFS_H
# include <sys/statfs.h>
#endif
#if defined(HAVE_SYS_VFS_H) && !defined(SOLARIS)
# include <sys/vfs.h>
#endif

#ifdef SUNOS
 extern int statfs(const char *, struct statfs *);
#endif

# if USE_STATFS_TYPE == STATVFS
#  define plp_statfs(path,buf) statvfs(path,buf)
#  define plp_struct_statfs struct statvfs
#  define statfs(path, buf) statvfs(path, buf)
#  define USING "STATVFS"
#  define BLOCKSIZE(f) (unsigned long)(f.f_frsize?f.f_frsize:f.f_bsize)
#  define BLOCKS(f)    (unsigned long)f.f_bavail
# endif

# if USE_STATFS_TYPE == ULTRIX_STATFS
#  define plp_statfs(path,buf) statfs(path,buf)
#  define plp_struct_statfs struct fs_data
#  define USING "ULTRIX_STATFS"
#  define BLOCKSIZE(f) (unsigned long)f.fd_bsize
#  define BLOCKS(f)    (unsigned long)f.fd_bfree
# endif

# if USE_STATFS_TYPE ==  SVR3_STATFS
#  define plp_struct_statfs struct statfs
#  define plp_statfs(path,buf) statfs(path,buf,sizeof(struct statfs),0)
#  define USING "SV3_STATFS"
#  define BLOCKSIZE(f) (unsigned long)f.f_bsize
#  define BLOCKS(f)    (unsigned long)f.f_bfree
# endif

# if USE_STATFS_TYPE == STATFS
#  define plp_struct_statfs struct statfs
#  define plp_statfs(path,buf) statfs(path,buf)
#  define USING "STATFS"
#  define BLOCKSIZE(f) (unsigned long)f.f_bsize
#  define BLOCKS(f)    (unsigned long)f.f_bavail
# endif


/***************************************************************************
 * Check_space() - check to see if there is enough space
 ***************************************************************************/

double Space_avail( char *pathname )
{
	double space = 0;
	plp_struct_statfs fsb;

	if( plp_statfs( pathname, &fsb ) == -1 ){
		DEBUG2( "Check_space: cannot stat '%s'", pathname );
	} else {
		space = BLOCKS(fsb) * (BLOCKSIZE(fsb)/1024.0);
	}
	return(space);
}

