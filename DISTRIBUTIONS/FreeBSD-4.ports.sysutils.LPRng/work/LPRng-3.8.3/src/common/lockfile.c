/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2001, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: lockfile.c,v 1.34 2001/12/03 22:08:12 papowell Exp $";

/***************************************************************************
 * MODULE: lockfile.c
 * lock file manipulation procedures.
 ***************************************************************************
 * File Locking Routines:
 * int Do_lock( char *filename, int *lock, int *create, struct stat *statb )
 *     filename- name of file
 *     lock- non-zero- attempt to lock, *lock = 1 if success, 0 if fail
 *     create- non-zero- if file does not exist, create
 *             *create = 1 if needed to create and success,
 *                       0 if did not need to create
 *  1. opens file;  if unable to open and create != 0, trys to
 *                  create the file; *create = 1 if created;
 *  2. call Lock_fd to check for problems such as links,
 *     permissions, and somebody playing games with symbolic links
 *
 *     Returns: fd >= 0 - fd of the file
 *              -1      - file does not exist or has error condition
 *              *lock = 0 if no unable to lock
 *                      1 if locked
 *                      -1 if timeout or other error condition
 *     Side Effect: updates statb with status
 *
 * int Lock_fd( int fd, char *filename, int *lock, struct stat *statb );
 *     Lock_fd checks for problems such as links,
 *     permissions, and somebody playing games with symbolic links
 *
 *     Returns: fd >= 0 - fd of the file
 *              -1      - file does not exist or has error condition
 *              *lock = 0 if no unable to lock
 *
 ***************************************************************************
 * Lock File Manipulation:
 * Each active server has a lock file, which it uses to record its
 * activity.  The lock file is created and then locked;
 * the deamon will place its PID and an activity in the lock file.
 *
 * The struct lockfile{} gives the format of this file.
 *
 * Programs wanting to know the server status will read the file.
 * Note:  only active servers, not status programs, will lock the file.
 * This prevents a status program from locking out a server.
 * The information in the lock file may be stale,  as the lock program
 * may update the file without the knowledge of the checker.
 * However, by making the file fixed size and small, we can read/write
 * it with a single operation,  making the window of modification small.
 ***************************************************************************/

#include "lp.h"
#include "lockfile.h"
#include "fileopen.h"
/**** ENDINCLUDE ****/

#if defined(HAVE_SYS_TTYCOM_H)
#include <sys/ttycom.h>
#endif
#if defined(HAVE_SYS_TTOLD_H) && !defined(IRIX)
#include <sys/ttold.h>
#endif
#if defined(HAVE_SYS_IOCTL_H)
#include <sys/ioctl.h>
#endif


/***************************************************************************
 * Do_lock( fd , int block )
 * does a lock on a file;
 * if block is nonzero, block until file unlocked
 * Returns: < 0 if lock fn failed
 *            0 if successful
 ***************************************************************************/

int Do_lock( int fd, int block )
{
    int code = -2;

	DEBUG3("Do_lock: fd %d, block '%d'", fd, block );

#if defined(HAVE_FLOCK)
	if( code == -2 ){
		int err;
		int how;

		if( block ){
			how = LOCK_EX;
		} else {
			how = LOCK_EX|LOCK_NB;
		}

		DEBUG3 ("Do_lock: using flock" );
		code = flock( fd, how );
		err = errno;
		if( code < 0 ){
			DEBUG1( "Do_lock: flock failed '%s'", Errormsg( err ));
			code = -1;
		} else {
			code = 0;
		}
		errno = err;
	}
#endif
#if defined(HAVE_LOCKF)
	if( code == -2 ){
		int err;
		int how;

		if( block ){
			how = F_LOCK;
		} else {
			how = F_TLOCK;
		}

		DEBUG3 ("Do_lock: using lockf" );
		code = lockf( fd, how, 0);
		err = errno;
		if( code < 0 ){
			DEBUG1( "Do_lock: lockf failed '%s'", Errormsg( err));
			code = -1;
		} else {
			code = 0;
		}
		errno = err;
	}
#endif
#if defined(HAVE_FCNTL)
	if( code == -2 ){
		struct flock file_lock;
		int err;
		int how;
		DEBUG3 ("Do_lock: using fcntl with SEEK_SET, block %d", block );

		how = F_SETLK;
		if( block ) how = F_SETLKW;

		memset( &file_lock, 0, sizeof( file_lock ) );
		file_lock.l_type = F_WRLCK;
		file_lock.l_whence = SEEK_SET;
		code = fcntl( fd, how, &file_lock);
		err = errno;
		if( code < 0 ){
			code = -1;
		} else {
			code = 0;
		}
		DEBUG3 ("devlock_fcntl: status %d", code );
		errno = err;
	}
#endif

	DEBUG3 ("Do_lock: status %d", code);
	return( code);
}




/***************************************************************************
 * LockDevice(fd, block)
 * Tries to lock the device file so that two or more queues can work on
 * the same print device. First does a non-blocking lock, if this fails,
 * puts a nice message in the status file and blocks in a second lock.
 * (contributed by Michael Joosten <joost@cadlab.de>)
 *
 * Finally, you can set locking off (:lk@:)
 *
 * RETURNS: >= 0 if successful, < 0 if fails
 ***************************************************************************/

int LockDevice(int fd, int block )
{
	int lock = -1;
	int err = errno;

	DEBUG2 ("LockDevice: locking '%d'", fd );

#if defined(TIOCEXCL) && !defined(HAVE_BROKEN_TIOCEXCL)
	DEBUG2 ("LockDevice: TIOCEXL on '%d', isatty %d",
		fd, isatty( fd ) );
    if( isatty (fd) ){
        /* use the TIOCEXCL ioctl. See termio(4). */
		DEBUG2 ("LockDevice: TIOCEXL on '%d'", fd);
        lock = ioctl( fd, TIOCEXCL, (void *) 0);
		err = errno;
        if( lock < 0) {
			lock = -1;
			LOGERR(LOG_INFO) "LockDevice: TIOCEXCL failed");
		} else {
			lock = 0;
		}
    }
#endif
	if( lock < 0 ){
		lock = Do_lock( fd, block );
	}

	errno = err;
	return( lock );
}
