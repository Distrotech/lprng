/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: fileopen.c
 * PURPOSE: file opening code
 **************************************************************************/

static char *const _id =
"$Id: fileopen.c,v 3.10 1997/12/16 15:06:25 papowell Exp $";

#include "lp.h"
#include "fileopen.h"
#include "errorcodes.h"
#include "lockfile.h"
#include "pathname.h"
#include "timeout.h"
#include "killchild.h"
#include "waitchild.h"
/**** ENDINCLUDE ****/

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
 ***************************************************************************/

int Checkread( char *file, struct stat *statb )
{
	int fd = -1;
	int status = 0;
	int err = 0;

	/* open the file */
	DEBUG3("Checkread: file '%s'", file );

	if( (fd = open( file, O_RDONLY|O_NOCTTY, Spool_file_perms ) )< 0 ){
		status = -1;
		err = errno;
		DEBUG3( "Checkread: cannot open '%s', %s", file, Errormsg(err) );
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
		DEBUG3( "Checkread: '%s' not regular file, mode = 0%o",
			file, statb->st_mode );
		status = -1;
	}

	if( status < 0 ){
		close( fd );
		fd = -1;
	}
	DEBUG3("Checkread: '%s' fd %d", file, fd );
	errno = err;
	return( fd );
}


/***************************************************************************
 * int Checkwrite( char *file, struct stat *statb, int rw, int create,
 *  int nodelay )
 *  - if rw != 0, open for both read and write
 *  - if create != 0, create if it does not exist
 *  - if nodelay != 0, use nonblocking open
 * open a file or device for writing,  and check its permissions
 * Returns: fd of open file, -1 if error.
 *     status in *statb
 ***************************************************************************/
int Checkwrite( char *file, struct stat *statb, int rw, int create,
	int nodelay )
{
	int fd = -1;
	int status = 0;
	int options = O_NOCTTY|O_APPEND;
	int mask;
	int err = errno;

	/* open the file */
	DEBUG3("Checkwrite: file '%s', rw %d, create %d, nodelay %d",
		file, rw, create, nodelay );

	memset( statb, 0, sizeof( statb[0] ) );
	if( nodelay ){
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
		DEBUG3( "Checkwrite: cannot open '%s', %s", file, Errormsg(err) );
	} else if( nodelay ){
		/* turn off nonblocking */
		mask = fcntl( fd, F_GETFL, 0 );
		if( mask == -1 ){
			logerr(LOG_ERR, "Checkwrite: fcntl F_GETFL of '%s' failed", file);
			status = -1;
		} else {
			DEBUG3( "Checkwrite: F_GETFL value '0x%x', BLOCK 0x%x",
				mask, NONBLOCK );
			mask &= ~NONBLOCK;
			mask = fcntl( fd, F_SETFL, mask );
			if( mask == -1 ){
				logerr(LOG_ERR, "Checkwrite: fcntl F_SETFL of '%s' failed",
					file );
				status = -1;
			}
			DEBUG3( "Checkwrite: after F_SETFL value now '0x%x'",
				fcntl( fd, F_GETFL, 0 ) );
		}
	}

    if( status >= 0 && fstat( fd, statb ) < 0 ) {
		err = errno;
        logerr_die(LOG_ERR, "Checkwrite: fstat of '%s' failed, possible security problem", file);
        status = -1;
    }

	/* check for a security loophole: not a file */
	if( status >= 0 && (S_ISDIR(statb->st_mode))){
		/* AHA!  Directory! */
		DEBUG3( "Checkwrite: '%s' directory, mode 0%o",
			file, statb->st_mode );
		status = -1;
	}
	if( fd == 0 ){
		int tfd;
		tfd = dup(fd);
		err = errno;
		if( tfd < 0 ){
			logerr(LOG_ERR, "Checkwrite: dup of '%s' failed", file);
			status = -1;
		} else {
			close(fd);
			fd = tfd;
		}
    }
	if( status < 0 ){
		close( fd );
		fd = -1;
	}
	DEBUG4("Checkwrite: file '%s' fd %d, inode 0x%x", file, fd, statb->st_ino );
	errno = err;
	return( fd );
}

/***************************************************************************
 * Make_temp_fd( char *name, int namelen )
 * 1. we can call this repeatedly,  and it will make
 *    different temporary files.
 * 2. we NEVER modify the temporary file name - up to the first '.'
 *    is the base - we keep adding suffixes as needed.
 * 3. Remove_files uses the tempfile information to find and delete temp
 *    files so be careful.
 ***************************************************************************/

static int Tempcount;

char *Init_tempfile( void )
{
	char *dir = 0;
	int len;
	struct stat statb;

	if( Tempfile == 0 ){
		malloc_or_die( Tempfile, sizeof( Tempfile[0] ) );
	}
	memset(Tempfile, 0, sizeof( Tempfile[0]) );

	/* if we have the openname set, we use this for base */
	if( Is_server ){
		if( SDpathname ){
			dir = Clear_path( SDpathname );
		}
		if( dir == 0 || stat( dir, &statb ) ||
			!S_ISDIR(statb.st_mode) ){
			dir = 0;
		}
		if( dir == 0 || *dir == 0 ){
			dir = Server_tmp_dir;
		}
		if( dir == 0 || stat( dir, &statb ) ||
			!S_ISDIR(statb.st_mode) ){
			fatal( LOG_ERR, "Init_tempfile: bad tempdir '%s'", dir );
		}
	} else {
		dir = getenv( "LPR_TMP" );
	}
	if( dir == 0 || *dir == 0 ){
		dir = Default_tmp_dir;
	}
	if( dir == 0 || *dir == 0 ){
		dir = "/tmp";
	}
	if( dir == 0 || stat( dir, &statb ) ||
		!S_ISDIR(statb.st_mode) ){
		fatal( LOG_ERR, "Init_tempfile: bad tempdir '%s'", dir );
	}
	Init_path( Tempfile, dir );
	len = Tempfile->pathlen;
	plp_snprintf( Tempfile->pathname+len, sizeof(Tempfile->pathname)-len,
		"bfA%03d", getpid() );
	Tempfile->pathlen = strlen( Tempfile->pathname );
	dir = Clear_path(Tempfile);
	Tempcount = 0;
	DEBUG3("Init_tempfile: temp file '%s'", dir );
	return(dir);
}

static int temp_registered;

int Make_temp_fd( char *temppath, int templen )
{
	int tempfd = -1;
	int len;
	struct stat statb;
	char pathname[MAXPATHLEN];

	if( temp_registered == 0 ){
		register_exit( (exit_ret)Remove_tempfiles, 0 );
		temp_registered = 1;
	}
	if( Tempfile == 0 || Tempfile->pathname[0] == 0){
		Init_tempfile();
	}
	/* put an arbitrary limit of 1000 on tempfiles */
	while( tempfd < 0 && Tempcount < 1000 ){
		safestrncpy( pathname, Clear_path( Tempfile ) );
		len = strlen(pathname);
		plp_snprintf( pathname+len, sizeof(pathname)-len, ".%d", ++Tempcount );
		if( temppath ) strncpy( temppath, pathname, templen );
		DEBUG0("Make_temp_fd: trying '%s'", pathname );
		tempfd = Checkwrite( pathname, &statb, O_RDWR|O_CREAT|O_EXCL, 1, 0 );
		DEBUG0("Make_temp_fd: tempfd %d", tempfd );
	}
	DEBUG0("Make_temp_fd: using '%s'", pathname );
	if( ftruncate( tempfd, 0 ) < 0 ){
		Errorcode = JABORT;
		logerr_die( LOG_ERR,
			"Make_temp_fd: cannot truncate file '%s'",pathname);
	}
	return( tempfd );
}


void Close_passthrough( struct filter *filter, int timeout )
{
	static int report;
	plp_status_t status;
	int err;
	DEBUGF(DRECV2)("Close_passthrough: pid %d", filter->pid );
	if( filter->pid > 0 ){
		if( filter->input > 0 ){
			close( filter->input );
			filter->input = -1;
		}
		if( filter->output > 0 ){
			close( filter->output );
			filter->output = -1;
		}
		report = status = err = 0;
		report = plp_waitpid_timeout( timeout, filter->pid,  &status, 0 );
		if( report <= 0 && filter->pid > 0 ) kill( filter->pid, SIGINT );
		filter->pid = -1;
	}
}

/***************************************************************************
 * Remove_tempfiles()
 *  - remove the tempfiles created for this job
 ***************************************************************************/

void Remove_tempfiles( void )
{
	/* scan for matching files with same suffix */
	struct dirent *d;				/* directory entry */
	DIR *dir;
	struct dpathname dpath;
	char pathname[MAXPATHLEN];
	char *filename, *p;
	int len;


	temp_registered = 0;
	DEBUGF(DRECV2)("Remove_tempfiles: Tempfile '%s'", Tempfile );
	Close_passthrough( &Passthrough_send, 3 );
	Close_passthrough( &Passthrough_receive, 3 );

	if( Tempfile && Tempfile->pathname[0] ){
		safestrncpy( pathname, Clear_path(Tempfile) );
		if( (filename = strrchr( pathname, '/' )) ){
			*filename++ = 0;
			Init_path( &dpath, pathname );
		} else {
			return;
		}
		Init_path( &dpath, pathname );
		DEBUGF(DRECV2)("Remove_tempfiles: pathname '%s', filename '%s'",
			pathname, filename );
		dir = opendir( pathname );
		len = strlen( filename );
		if( dir ) while( (d = readdir(dir)) ){
			if( strncmp( d->d_name, filename, len ) ) continue;
			if( strlen(d->d_name) == len ) continue;
			/* we found a match */
			p = Add_path( &dpath, d->d_name );
			DEBUGF(DRECV2)("Remove_tempfiles: removing '%s'", p );
			unlink( p );
		}
	}
	if( Tempfile ){
		Tempfile->pathname[0] = 0;
	}
}

/***************************************************************************
 * Remove_files( void *p )
 *  Remove files created for transfer of a job
 *  Called by cleanup functions
 ***************************************************************************/
static void unlinkf( char *s )
{
	if( s && s[0] ){
		DEBUGF(DRECV3)("Remove_files: unlinking '%s'", s );
		unlink( s );
	}
}

void Remove_files( void *nv )
{
	int i;
	struct data_file *data;	/* list of data files */

	DEBUGF(DRECV3)( "Remove_files: removing job files" );
	if( Cfp_static && Cfp_static->remove_on_exit ){
		data = (void *)Data_files.list;
		for( i = 0; i < Data_files.count; ++i ){
			unlinkf( data[i].openname );
			unlinkf( data[i].transfername );
		}
		unlinkf( Cfp_static->openname );
		unlinkf( Cfp_static->transfername );
		unlinkf( Cfp_static->hold_file );
	}
}

/***************************************************************************
 * int Checkwrite_timeout(int timeout, ... )
 *  Tries to do Checkwrite() with a timeout 
 ***************************************************************************/
int Checkwrite_timeout(int timeout,
	char *file, struct stat *statb, int rw, int create, int nodelay )
{
	int fd;
	if( Set_timeout() ){
		Set_timeout_alarm( timeout, 0);
		fd = Checkwrite( file, statb, rw, create, nodelay );
	} else {
		fd = -1;
	}
	Clear_timeout();
	return(fd);
}
