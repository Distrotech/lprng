/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lockfile.c
 * PURPOSE:
 **************************************************************************/

static char *const _id =
"lockfile.c,v 3.5 1997/12/16 15:06:29 papowell Exp";
/***************************************************************************
 * MODULE: lockfile.c
 * lock file manipulation procedures.
 ***************************************************************************
 * File Locking Routines:
 * int Lockf( char *filename, int *lock, int *create, struct stat *statb )
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

#ifdef HAVE_FCNTL
int devlock_fcntl( int fd, int nowait);
#endif
#ifdef HAVE_FLOCK
int devlock_flock( int fd, int nowait);
#endif
#ifdef HAVE_LOCKF
int devlock_lockf( int fd, int nowait);
#endif

#ifdef HAVE_SYS_TTYCOM_H
#include <sys/ttycom.h>
#endif
#if defined(HAVE_SYS_TTOLD_H) && !defined(IRIX)
#include <sys/ttold.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif


int Lockf( char *filename, struct stat *statb )
{
    int fd;			/* fd for file descriptor */
	int err = errno;
	
    /*
     * Open the lock file for RW
     */
	if( (fd = Checkwrite(filename, statb, O_RDWR, 1, 0 )) < 0) {
		err = errno;
		DEBUG3( "Lockf: lock '%s' open failed, %s",
			filename, Errormsg(err) );
	} else if( Lock_fd( fd, filename, statb ) < 0 ){
		err = errno;
		DEBUG3 ("Lockf: lockfd '%s' failed, %s",
			filename, Errormsg(err) );
		close( fd );
		fd = -1;
	}
    DEBUG3 ("Lockf: file '%s', fd %d", filename, fd );
	errno = err;
    return( fd );
}

int Lock_fd( int fd, char *filename, struct stat *statb )
{
	struct stat lstatb;
	int status = fd;

	/* stat and lstat the same file */
	if( status >= 0 && fstat( fd, statb ) < 0 ) {
		logerr(LOG_ERR, "Lockf: fstat of '%s' failed, possible security problem", filename);
		status = -1;
	}
	if( status >= 0 && lstat( filename, &lstatb ) < 0 ) {
		logerr(LOG_ERR, "Lockf: lstat of '%s' failed, possible security problem", filename);
		status = -1;
	}
	/* now check to see if we have the same INODE */
	if( status >= 0 && ( statb->st_dev != lstatb.st_dev
		|| statb->st_ino != lstatb.st_ino ) ){
		log(LOG_ERR, "Lockf: stat and lstat of '%s' differ, possible security problem",
			filename);
		status = -1;
	}
	/* check for a security loophole: not a file */
	if( status >= 0 && !(S_ISREG(statb->st_mode))){
		/* AHA!  not a regular file! */
		log( LOG_DEBUG, "Lockf: '%s' not regular file, mode = 0%o",
			filename, statb->st_mode );
		status = -1;
	}

	/* check for a security loophole: wrong permissions */

	if( status >= 0 && (077777 & (statb->st_mode ^ Spool_file_perms)) ){
		log( LOG_ERR, "Lockf: file '%s' perms 0%o instead of 0%o, possible security problem",
			filename, statb->st_mode & 077777, Spool_file_perms );
		status = -1;
	}

	/* try locking the file */
	if( status >= 0 ){
		status = Do_lock( fd, filename, 1 );
	}

    DEBUG3 ("Lock_fd: file '%s', status %d", filename, status );
    return( status );
}

/***************************************************************************
 * Do_lock( fd , char *filename, int block )
 * does a lock on a file;
 * if block is nonzero, block until file unlocked
 * Returns: >= 0 if successful, < 0 if lock fn failed
 ***************************************************************************/

int Do_lock( int fd, const char *filename, int block )
{
    int code = -1;

	DEBUG3("Do_lock: fd %d, file '%s' block '%d'", fd, filename, block );
#ifdef HAVE_FCNTL
	DEBUG3("Do_lock: using fcntl");
	code = devlock_fcntl( fd, block );
#else
# ifdef HAVE_LOCKF
    /*
     * want to try F_TLOCK
     */
	DEBUG3("Do_lock: using lockf");
	code = devlock_lockf( fd, block );
# else
    /* last resort -- doesn't work over NFS */
	DEBUG3("Do_lock: using flock");
	code = devlock_flock( fd, block );
# endif  /* HAVE_LOCKF */
#endif  /* HAVE_FCNTL */

    DEBUG3 ("Do_lock: status %d", code);
    return( code);
}
 
/****************************************************************************
 * try to get an exclusive open on the device. How to do this differs
 * widely from system to system, unfortunately.
 * Most of this code has been contributed by Michael Joosten
 * <joost@cadlab.de>.
 ***************************************************************************/

/*
 * the actual locking code
 *  -- #ifdef'ed based on the flag constants used by the lock
 * functions.
 */
#ifdef HAVE_FCNTL
int devlock_fcntl( int fd, int block)
{
	struct flock file_lock;
	int err;
	int status;
	int how;
	DEBUG3 ("devlock_fcntl: using fcntl with SEEK_SET, block %d", block );

 	how = F_SETLK;
 	if( block ) how = F_SETLKW;

	memset( &file_lock, 0, sizeof( file_lock ) );
	file_lock.l_type = F_WRLCK;
	file_lock.l_whence = SEEK_SET;
	status = fcntl( fd, how, &file_lock);
	err = errno;
	if( status < 0 ){
		status = -1;
	} else {
		status = 0;
	}
	DEBUG3 ("devlock_fcntl: status %d", status );
	errno = err;
	return( status );
}
#endif


#ifdef HAVE_FLOCK
int devlock_flock( int fd, int block)
{
	int err;
	int status;
	int how;

	if( block ){
		how = LOCK_EX;
	} else {
		how = LOCK_EX|LOCK_NB;
	}

	DEBUG3 ("devlock_flock: block %d", block );
	status = flock( fd, how );
	err = errno;
	if( status < 0 ){
		DEBUG0( "flock failed: %s", Errormsg( err ));
		if( err == EWOULDBLOCK ){
			status = 0;
		} else {
			status =  -1;
		}
	} else {
		status = 1;
	}
	errno = err;
	return( status );
}
#endif


#ifdef HAVE_LOCKF
int devlock_lockf( int fd, int block)
{
	int err;
	int status;
	int how;

	if( block ){
		how = F_LOCK;
	} else {
		how = F_TLOCK;
	}

	DEBUG3 ("devlock_lockf: using lockf, block %d", block );
	status = lockf( fd, how, 0L);
	err = errno;
	if( status < 0 ){
		DEBUG0( "lockf failed: %s", Errormsg( err));
		if( err == EACCES || err == EAGAIN ){
			status = 0;
		} else {
			status =  -1;
		}
	} else {
		status = 1;
	}
	errno = err;
	return( status );
}
#endif


/***************************************************************************
 * LockDevice(fd, char *devname)
 * Tries to lock the device file so that two or more queues can work on
 * the same print device. First does a non-blocking lock, if this fails,
 * puts a nice message in the status file and blocks in a second lock.
 * (contributed by Michael Joosten <joost@cadlab.de>)
 *
 * Finally, you can set locking off (:lk@:)
 *
 * RETURNS: >= 0 if successful, < 0 if fails
 ***************************************************************************/

int LockDevice(int fd, char *devname)
{
	int lock = 1;
	int err = errno;

	DEBUG2 ("LockDevice: locking '%s'", devname );

#if defined(TIOCEXCL) && !defined(HAVE_BROKEN_TIOCEXCL)
	DEBUG2 ("LockDevice: TIOCEXL on '%s', isatty %d",
		devname, isatty( fd ) );
    if( isatty (fd) ){
        /* use the TIOCEXCL ioctl. See termio(4). */
		DEBUG2 ("LockDevice: TIOCEXL on '%s'", devname);
        lock = ioctl( fd, TIOCEXCL, (void *) 0);
		err = errno;
        if( lock < 0) {
			logerr( LOG_INFO, "LockDevice: TIOCEXCL failed");
		} else {
			lock = 1;
		}
    } else
#endif
		lock = Do_lock( fd, devname, 0 );

	errno = err;
	return( lock );
}
