/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: pr_support.c
 * PURPOSE: Printer opening
 **************************************************************************/

static char *const _id =
"pr_support.c,v 3.19 1998/03/24 02:43:22 papowell Exp";

/***************************************************************************
 Commentary:
Patrick Powell, Fri May 19 08:01:34 PDT 1995
This code is taken almost directly from the PLp_device Version 4.0 code.
Additional sanity checks and cleanups have been added.

 ***************************************************************************
 * MODULE: Print_support.c
 * handle the actual output to the Printer
 ***************************************************************************
 * int Print_open( int timeout ): opens the Printer
 * int Print_close(): closes the Printer
 * int Print_ready(): combines Print_open() and Print_of_fd()
 * int of_stop(): stops the 'of' filter
 * int Print_string( str, len ) : prints a string through 'of' or to Printer
 * int Print_copy( fd) : copies a file through 'of' or to Printer
 * int Print_filter( file, cmd) : makes a filter and copies file
 * int Print_banner(): prints a banner through 'of' or to Printer
 * Note: the above procedures which return values return JSUCC on success,
 *   and JFAIL or JABORT on failure
 ***************************************************************************/

#include "lp.h"
#include "printcap.h"
#include "decodestatus.h"
#include "errorcodes.h"
#include "fileopen.h"
#include "killchild.h"
#include "linksupport.h"
#include "lockfile.h"
#include "pr_support.h"
#include "setstatus.h"
#include "setup_filter.h"
#include "stty.h"
#include "timeout.h"
#include "waitchild.h"
/**** ENDINCLUDE ****/

/***************************************************************************
 * int Print_open( struct filter *filter, struct control_file *cfp,
 *   int timeout, int interval, int max_timeout )
 *
 * 	Open the Printer, and set the filter structure with the pid and output
 *   device.
 * 	If the RW printcap flag is set, output is opened RW, otherwise
 * 	opened writeonly in append mode.
 *
 * Side Effect:
 *   terminates if Printer is unable to be opened
 ****************************************************************************/

int Print_open( struct filter *filter,
	struct control_file *cfp, int timeout, int interval, int grace,
	int max_timeout, struct printcap_entry *printcap_entry, int accounting_port )
{
	int attempt = 0, err = 0, n;
	struct stat statb;
	int device_fd = -1;
	time_t tm;
	char tm_str[32];
	char *s, *end;

	DEBUG1( "Print_open: device '%s', max_timeout %d, interval %d, timeout %d",
		Lp_device, max_timeout, interval, timeout );
	time( &tm );
	tm_str[0] = 0;
	safestrncat( tm_str, Time_str(1,tm) );
	if( Lp_device == 0 || *Lp_device == 0 ){
		fatal(LOG_ERR, "Print_open: printer '%s' missing Lp_device value",
			Printer );
	}
	if( Lp_device[0] != '|' && (s = strchr( Lp_device, '%' )) ){
		/* we have a host%port form */
		*s++ = 0;
		end = s;
		Destination_port = strtol( s, &end, 10 );
		if( ((end - s) != strlen( s )) || Destination_port < 0 ){
		fatal( LOG_ERR,
			"Print_open: 'lp' entry has bad port number '%s'",
				Lp_device );
		}
	} else if( Lp_device[0] != '|' && Lp_device[0] != '/' ){
		fatal(LOG_ERR, "Print_open: printer '%s', bad 'lp' entry '%s'", 
			Printer, Lp_device );
	}


	/* set flag for closing on timeout */
	do{
		setstatus( cfp,
		"opening '%s' at %s, attempt %d, timeout %d, grace %d",
			Lp_device, tm_str, ++attempt, timeout, grace );
		if (grace) {
			setstatus( cfp, "waiting Grace period '%s' : '%d'",
				Lp_device, grace);
			plp_sleep(grace);
		}

		filter->pid = 0;
		filter->input = 0;
		switch( Lp_device[0] ){
		case '|':
			if( DevNullFD <= 0 ){
				logerr_die( LOG_CRIT, "Print_open: bad DevNullFD");
			} 
			if( Make_filter( 'f', cfp, filter,
				Lp_device+1, 1, Read_write, DevNullFD, printcap_entry, (void *)0,
				accounting_port, Logger_destination != 0, 0 ) ){
				setstatus( cfp, "%s", cfp->error );
				filter->input = -1;
			}
 			device_fd = filter->input;
			break;
		case '/':
			device_fd = Checkwrite_timeout( timeout, Lp_device, &statb, 
					(Lock_it || Read_write)
						?(O_RDWR):(O_APPEND|O_WRONLY), 0, Nonblocking_open );
			err = errno;

			/* Note: there is no timeout race condition here -
				we either set the device_fd or we did not */
			if( Lock_it && device_fd >= 0 ){
				/*
				 * lock the device so that multiple servers can
				 * use it
				 */
				if( isatty( device_fd ) ){
					if( LockDevice( device_fd, Lp_device ) <= 0 ){
						err = errno;
						setstatus( cfp,
							"Device '%s' is locked!", Lp_device );
						close( device_fd );
						device_fd = -1;
					}
				} else if( S_ISREG(statb.st_mode ) ){
					if( Do_lock( device_fd, Lp_device, 1 ) < 0 ){
						err = errno;
						setstatus( cfp,
							"Device '%s' is locked!", Lp_device );
						close( device_fd );
						device_fd = -1;
					}
				}
			}
			break;
		default:
			DEBUG1( "Print_open: doing link open '%s'", Lp_device );
			device_fd = Link_open( Lp_device, timeout );
			err = errno;
			break;
		}

		if( device_fd < 0 ){
			errno = err;
			n = interval*( 1 << (attempt-1) );
			if( max_timeout > 0 && n > max_timeout ) n = max_timeout;
			DEBUG1( "Print_open: cannot open device '%s' - '%s'",
				Lp_device, Errormsg(err) );
			setstatus( cfp, "cannot open '%s' - '%s', attempt %d, sleeping %d",
					Lp_device, Errormsg( err), attempt, n );
			if( n > 0 ){
				plp_sleep(n);
			}
		}
		if( isatty(device_fd ) ) Do_stty( device_fd );
	} while( device_fd < 0 );

	filter->input = device_fd;
	DEBUG1 ("Print_open: %s is fd %d", Lp_device, device_fd);
	return( device_fd );
}


/***************************************************************************
 * Print_flush()
 * 1. Flush all of the open pipe and/or descriptors
 ***************************************************************************/
void Print_flush( void )
{
	DEBUG3("Print_flush: flushing printers");
	Flush_filter( &Device_fd_info );
	Flush_filter( &OF_fd_info );
	Flush_filter( &XF_fd_info );
	Flush_filter( &Pr_fd_info );
	Flush_filter( &Af_fd_info );
	Flush_filter( &As_fd_info );
}

/***************************************************************************
 * void Print_close(struct control_file *cfp, int timeout)
 * 1. Close all of the open pipe and/or descriptors
 * 2. Signal the filter processes to start up
 * 3. We wait for a while for them to complete
 ***************************************************************************/

/***************************************************************************
 * int Close_tty( struct filter *filter, int timeout, const char *key )
 * - filter is the data structure with the filte interformation
 * - timeout > 0  we block for timeout seconds, waiting for exit
 * - timeout = 0  we block indefinately
 * - timeout < 0  we do not block
 * key is identifier who called
 ***************************************************************************/

int Close_tty( struct filter *filter, int timeout, const char *key )
{
	int fd, err = 0, result = 0;
	if( (fd = filter->input) > 0 && isatty( fd ) ){
		DEBUG3("Close_tty: fd %d, timeout %d, key %s",
			filter->input, timeout, key );
		filter->input = -1;
		if( Set_timeout() ){
			Set_timeout_alarm( timeout, 0 );
			result = close( fd );
			err = errno;
		} else {
			result = -1;
			err = EINTR;
		}
		Clear_timeout();
	}
	errno = err;
	return( result );
}

void Print_close(struct control_file *cfp, int timeout)
{
	pid_t result;
	plp_status_t status;
	int err;

	DEBUG3("Print_close: closing printers");
	/* shut them all down first */
	Close_filter( cfp, &Device_fd_info, timeout, "Print_close" );
	Close_tty(    &Device_fd_info, timeout, "Print_close" );
	Close_filter( cfp, &OF_fd_info, timeout, "Print_close" );
	Close_tty(    &OF_fd_info, timeout, "Print_close" );
	Close_filter( cfp, &XF_fd_info, timeout, "Print_close" );
	Close_tty(    &XF_fd_info, timeout, "Print_close" );
	Close_filter( cfp, &Pr_fd_info, timeout, "Print_close" );
	Close_tty(    &Pr_fd_info, timeout, "Print_close" );
	Close_filter( cfp, &Af_fd_info, timeout, "Print_close" );
	Close_tty(    &Af_fd_info, timeout, "Print_close" );
	Close_filter( cfp, &As_fd_info, timeout, "Print_close" );
	Close_tty(    &As_fd_info, timeout, "Print_close" );
	DEBUG1( "Print_close: gathering children" );
	do{
		status = 0;
		result = plp_waitpid( -1, &status, 0 );
		err = errno;
		DEBUG1( "Print_close: result %d, status 0x%x", result, status );
	} while( result > 0 || (result == -1 && err != ECHILD) );
	DEBUG1( "Print_close: all children gathered" );
}

/***************************************************************************
 * Print_abort()
 * 1. Close all of the open pipe and/or descriptors
 * 2. Wait for all the filter processes to terminate
 * 3. After timeout period, kill them off
 * 
 ***************************************************************************/

void Print_abort( void )
{
	pid_t result;
	plp_status_t status;
	int step = 0;
	int err;

	DEBUG1( "Print_abort: starting shutdown" );

	/* be gentle at first */
	/* Print_flush(); */
	/* Print_close(0, -1); */
	while(1){
		switch( step++ ){
			case 0:
			DEBUG4( "Print_abort: step %d, killing SIGINT", step );
				killchildren( SIGINT ); break;	/* hit it once */
			case 1:
			DEBUG4( "Print_abort: step %d, killing SIGQUIT", step );
				killchildren( SIGQUIT ); break;   /* hit it harder */
			case 2:
			DEBUG4( "Print_abort: step %d, killing SIGKILL", step );
				killchildren( SIGKILL ); break;   /* hit it really hard */
			case 3: return; /* give up */
		}
		DEBUG1( "Print_abort: gathering children" );
		do{
			status = 0;
			result = plp_waitpid_timeout(3, -1, &status, 0 );
			err = errno;
			DEBUG1( "Print_abort: result %d, status 0x%x", result, status );
		} while( result > 0 );
		if( ((result == -1 ) && err == ECHILD) ){
			DEBUG1( "Print_abort: all children gathered" );
			break;
		}
	}
	DEBUG1( "Print_abort: done" );
}

int of_start( struct filter *filter )
{
	DEBUG1 ("of_start: sending SIGCONT to OF_Filter (pid %d)", filter->pid );
	if( filter->pid > 0 && kill( filter->pid, SIGCONT) < 0 ){
		logerr( LOG_WARNING, "cannot restart output filter");
		return( JFAIL);
	}
	return JSUCC;
}

/***************************************************************************
 * of_stop()
 * wait for the output filter to stop itself
 * Return: JSUCC if stopped, JFAIL if it died
 ***************************************************************************/

static char filter_stop[] = "\031\001";		/* magic string to stop OF_Filter */

int of_stop( struct filter *filter, int timeout, struct control_file *cfp )
{
	int pid = 0;
	int err;
	plp_status_t statb;
	DEBUG2 ("of_stop: writing stop string to fd %d, pid %d",
		filter->input, filter->pid );
	if( Print_string( filter, filter_stop, strlen( filter_stop ),
			timeout ) != JSUCC ){
		return( JFAIL );
	}

	DEBUG2 ("of_stop: waiting for OF_Filter %d", filter->pid );
	do{
		statb = 0;
		Alarm_timed_out = 0;
		pid = plp_waitpid_timeout(timeout, filter->pid, &statb, WUNTRACED );
		err = errno;
		DEBUG3( "of_stop: pid %d, statb 0x%x", pid, statb );
	}while( Alarm_timed_out == 0 && pid == -1 && err != ECHILD );

	if( Alarm_timed_out || !WIFSTOPPED (statb) ){
		setstatus( cfp, "of filter died (%s)", Decode_status( &statb));
		Errorcode = JABORT;
		if( Alarm_timed_out == 0 && WIFEXITED(statb) && WEXITSTATUS(statb) ){
			Errorcode = WEXITSTATUS( statb );
		}
		cleanup(0);
	}
	DEBUG2 ("of_stop: output filter stopped");

	return( JSUCC);
}


/***************************************************************************
 * Print_string( struct filter, char *str, int len, int timeout )
 * print a string through a filter
 * 1. Enable the line Printer
 * 2. get the filter or device fd;
 * 3. put out the string
 * 4. if unsuccessful, close the Printer
 * Return: JSUCC if successful, JFAIL otherwise
 ***************************************************************************/

int Print_string( struct filter *filter, char *str, int len, int timeout )
{
	int i = 0;		/* ACME again */
	int err;

	if( str == 0 || len <= 0 ){
		return( JSUCC);
	}
	i = Write_fd_len_timeout( timeout, filter->input, str, len );
	err = errno;
	if( Alarm_timed_out || i < 0 ){
		if( Alarm_timed_out ){
			log( LOG_WARNING, "Print_string: write timeout error");
		} else {
			logerr( LOG_WARNING, "Print_string: write error - %s",
				Errormsg(err) );
		}
		return( JFAIL);
	}
	return( JSUCC);
}


/***************************************************************************
 * Print_copy( int fd, struct filter *filter, int timeout )
 * copy a file through a filter
 * 1. copy the file to the appropriate output device
 * 2. if an error, close the Printer
 ***************************************************************************/

int Print_copy( struct control_file *cfp, int fd, struct stat *statb,
	struct filter *filter, int timeout, int status, char *file_name )
{
	char buf[LARGEBUFFER];
	int in;                     /* bytes read */
	int t = 0;						/* amount written */
	int total = 0;
	int err;					/* errno status */
	int oldfraction;			/* how much done */
	int quanta = 4;				/* resolution of amount */

	DEBUG3("Print_copy: fd %d to fd %d, pid %d, timeout %d, size %d, file_name %s",
		fd, filter->input, filter->pid, timeout, (int)(statb->st_size), file_name );

	
	oldfraction = 0;
	while( (in = read( fd, buf, sizeof( buf) ) ) > 0 ){
		DEBUG4("Print_copy: read %d, total %d", in, total );
		t = Write_fd_len_timeout( timeout, filter->input, buf, in );
		err = errno;

		DEBUG3("Print_copy: printed %d", t );
		if( Alarm_timed_out || t < 0 ){
			if( Alarm_timed_out ){
				if( status ){
					setstatus( cfp, "timeout after writing %d bytes",
					total );
				}
				logerr( LOG_WARNING,
					"Print_copy: write error timeout after %d bytes");
			} else {
				if( status ){
					setstatus( cfp, "write error after %d bytes '%s'",
					total, Errormsg(err) );
				}
				errno = err;
				logerr( LOG_WARNING, "write error after %d bytes", total );
			}
			return( JFAIL);
		}
		total += in;
		t = (int)(quanta*((double)(total)/statb->st_size));
		if( t != oldfraction ){
			oldfraction = t;
			t = (100*t)/quanta;
			setstatus( cfp, "printed %d percent of %d bytes of %s",
				t,(int)(statb->st_size), file_name); 
		}
	}
	/*
	 * completed the reading
	 */
	err = errno;
	if( in < 0 ){
		if( status ){
			setstatus( cfp, "read error after '%d' bytes, '%s'",
			total, Errormsg(err) );
		}
		errno = err;
		logerr( LOG_WARNING, "Print_copy: read error");
		return( JFAIL);
	}
	if( status )setstatus( cfp, "printed all %d bytes", total ); 

	DEBUG2("Print_copy: printed %d bytes", total);
	return( JSUCC);
}
