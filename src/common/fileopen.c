/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: fileopen.c
 * PURPOSE: file opening code
 **************************************************************************/

static char *const _id =
"$Id: fileopen.c,v 3.0 1996/05/19 04:05:58 papowell Exp $";

#include "lp.h"
#include "lp_config.h"

/***************************************************************************
Commentary:
Patrick Powell Mon May  1 05:37:02 PDT 1995
 
These routines were created in order to centralize all file open
and checking.  Hopefully,  if there are portability problems, these
routines will be the only ones to change.
 
 ***************************************************************************/

/***************************************************************************
 * int Checkread( char *file, struct stat *statb )
 * open a file for reading,  and check its permissions
 * Returns: fd of open file, -1 if error.
 *
 ***************************************************************************
 * int Checkwrite( char *file, struct stat *statb, int rw, int create,
 *    int delay )
 *  - if rw != 0, add the options specified
 *  - if create != 0, create if it does not exist
 *  - if delay != 0, do not block on open
 * open a file or device for writing,  and check its permissions
 * Returns: fd of open file, -1 if error.
 *     status in *statb

 ***************************************************************************/

/***************************************************************************
 * int Checkread( char *file, struct stat *statb )
 * open a file for reading,  and check its permissions
 * Returns: fd of open file, -1 if error.
 ***************************************************************************/

int Checkread( char *file, struct stat *statb )
{
	int fd = -1;
	int status = 0;
	int err = 0;

	/* open the file */
	DEBUG4("Checkread: file '%s'", file );

	if( (fd = open( file, O_RDONLY|O_NOCTTY, Spool_file_perms ) )< 0 ){
		status = -1;
		err = errno;
		DEBUG4( "Checkread: cannot open '%s', %s", file, Errormsg(err) );
	}

    if( status >= 0 && fstat( fd, statb ) < 0 ) {
		err = errno;
        logerr(LOG_ERR,
		"Checkread: fstat of '%s' failed, possible security problem", file);
        status = -1;
    }

	/* check for a security loophole: not a file */
	if( status >= 0 && !(S_ISREG(statb->st_mode))){
		/* AHA!  not a regular file! */
		DEBUG4( "Checkread: '%s' not regular file, mode = 0%o",
			file, statb->st_mode );
		status = -1;
	}

	if( status < 0 ){
		close( fd );
		fd = -1;
	}
	DEBUG4("Checkread: '%s' fd %d", file, fd );
	errno = err;
	return( fd );
}


/***************************************************************************
 * int Checkwrite( char *file, struct stat *statb, int rw, int create, int del )
 *  - if rw != 0, open for both read and write
 *  - if create != 0, create if it does not exist
 * open a file or device for writing,  and check its permissions
 * Returns: fd of open file, -1 if error.
 *     status in *statb
 ***************************************************************************/
int Checkwrite( char *file, struct stat *statb, int rw, int create, int delay )
{
	int fd = -1;
	int status = 0;
	int options = O_NOCTTY|O_APPEND;
	int mask;
	int err = errno;

	/* open the file */
	DEBUG4("Checkwrite: file '%s', rw %d, create %d, delay %d",
		file, rw, create, delay );

	if( delay ){
		options |= NONBLOCK;
	}
	if( rw ){
		options |= rw;
	} else {
		options |= O_WRONLY;
	}
	if( create ){
		options |= O_CREAT;
	}
	if( (fd = open( file, options, Spool_file_perms )) < 0 ){
		err = errno;
		status = -1;
		DEBUG4( "Checkwrite: cannot open '%s', %s", file, Errormsg(err) );
	}
	if( fd >= 0 && delay ){
		/* turn off nonblocking */
		mask = fcntl( fd, F_GETFL, 0 );
		if( mask == -1 ){
			logerr(LOG_ERR, "Checkread: fcntl F_GETFL of '%s' failed", file);
			status = -1;
		} else {
			DEBUG4( "Checkread: F_GETFL value '0x%x', BLOCK 0x%x",
				mask, NONBLOCK );
			mask &= ~NONBLOCK;
			mask = fcntl( fd, F_SETFL, mask );
			if( mask == -1 ){
				logerr(LOG_ERR, "Checkread: fcntl F_SETFL of '%s' failed",
					file );
				status = -1;
			}
			DEBUG4( "Checkread: after F_SETFL value now '0x%x'",
				fcntl( fd, F_GETFL, 0 ) );
		}
	}

    if( status >= 0 && fstat( fd, statb ) < 0 ) {
		err = errno;
        logerr(LOG_ERR, "Checkwrite: fstat of '%s' failed, possible security problem", file);
        status = -1;
    }

	/* check for a security loophole: not a file */
	if( status >= 0 && (S_ISDIR(statb->st_mode))){
		/* AHA!  Directory! */
		DEBUG4( "Checkwrite: '%s' directory, mode 0%o",
			file, statb->st_mode );
		status = -1;
	}
	if( status < 0 ){
		close( fd );
		fd = -1;
	}
	DEBUG8("Checkwrite: file '%s' fd %d, inode 0x%x", file, fd, statb->st_ino );
	errno = err;
	return( fd );
}
