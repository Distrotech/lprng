/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: XXX.c
 * PURPOSE:
 **************************************************************************/

static char *const _id = "$Id: freespace.c,v 3.1 1996/06/30 17:12:44 papowell Exp $";

/***************************************************************************
 * Check_space()
 * Courtesy of Justin Mason and others
 * check to see if there is a restriction on the amount of file space
 * needed.  The space need is measured in K (1024) or M
 * i.e. - MI=1024 -> 1M ;  MI=10M -> 10M;
 * if the MI=filename,  use the value in the file name instead.
 * I guess that they want to configure the file space information
 * independently...
 ***************************************************************************/

/*************************************************************************
 * portable macros to work with statfs (theoretically portable, at least)
 *************************************************************************/
#include "lp.h"

#ifdef HAVE_SYS_MOUNT_H
# include <sys/mount.h>
#endif
#ifdef HAVE_SYS_STATVFS_H
# include <sys/statvfs.h>
#endif
#ifdef HAVE_SYS_STATFS_H
# include <sys/statfs.h>
#endif
#ifdef HAVE_SYS_VFS_H
# include <sys/vfs.h>
#endif

# if USE_STATFS_TYPE == STATVFS
#  define plp_statfs(path,buf) statvfs(path,buf)
#  define plp_struct_statfs struct statvfs
#  define statfs(path, buf) statvfs(path, buf)
#  define plp_fs_free_bytes(f) (f.f_bavail * f.f_bsize)
# endif

# if USE_STATFS_TYPE == ULTRIX_STATFS
#  define plp_statfs(path,buf) statfs(path,buf)
#  define plp_struct_statfs struct fs_data
#  define plp_fs_free_bytes(f) (f.fd_bfree * f.fd_bsize)
# endif

# if USE_STATFS_TYPE ==  SVR3__STATFS
#  define plp_struct_statfs struct statfs
#  define plp_fs_free_bytes(f) (f.f_bfree * f.f_bsize)
#  define plp_statfs(path,buf) statfs(path,buf,sizeof(struct statfs),0)
# endif

# if USE_STATFS_TYPE == STATFS
#  define plp_struct_statfs struct statfs
#  define plp_fs_free_bytes(f) (f.f_bavail * f.f_bsize)
#  define plp_statfs(path,buf) statfs(path,buf)
# endif

/***************************************************************************
 * Space_needed() - get the amount of free space needed in the spool directory
 ***************************************************************************/
unsigned long Space_needed( char *min_space, struct dpathname *dpath )
{
	char *end, *s;
	unsigned long needed = 0;
	char line[MAXPATHLEN];
	int fd, i;
	struct stat statb;

	/* get the limits */
	if( min_space && *min_space ){
		if( (s = strchr( min_space, '\n' )) ) s = 0;
		DEBUG6("Space_needed: MI '%s'", min_space );
		end = min_space;
		needed = strtol( min_space, &end, 10 );
		if( min_space == end ){
			/* could be a file name */
			if( min_space[0] != '/' ){
				min_space = Add_path( dpath, min_space );
			}
			DEBUG6("Space_needed: trying to open '%s'", min_space );
			fd = Checkread( min_space, &statb );
			DEBUG6("Space_needed: file '%s' fd %d", min_space, fd );
			if( fd >= 0 ){
				/* we read a line */ 
				i = read( fd, line, sizeof(line)-1 );
				close(fd);
				if( i > 0 ){
					line[i] = 0;
					min_space = end = line;
					needed = strtol(min_space, &end, 10 );
				}
			} else {
				logerr( LOG_ERR, "Space_needed: cannot open '%s'", s);
			}
		}
		switch( *end ){
		case 'M': case 'm': needed = 1024 * needed; break;
		}
	}

	DEBUG6("Space_needed: need %ld", needed );
	return( needed );
}

/***************************************************************************
 * Space_avail() - get the amount of free space avail in the spool directory
 ***************************************************************************/

unsigned long Space_avail( struct dpathname *dpath )
{
	plp_struct_statfs fsb;
	unsigned long space = 0;
	char *pathname;

	pathname = Clear_path( dpath );
	if( plp_statfs( pathname, &fsb ) == -1 ){
		logerr( LOG_ERR, "Space_avail: cannot stat '%s'", pathname );
	}
	space = (plp_fs_free_bytes( fsb ) + 1023)/1024;
	DEBUG6("Space_avail: path '%s', space %ld", pathname, space );
	return( space );
}

int Check_space( int jobsize, char *min_space, struct dpathname *dpath )
{
	unsigned long needed;
	unsigned long avail;

	needed = Space_needed( min_space, dpath );
	if( needed == 0 ) return( 0 );
	avail = Space_avail( dpath );
	DEBUG6("Check_space: needed %ld, job %d, space avail %d",
		needed, jobsize, avail );
	return( (needed + ((jobsize+1023)/1024)) < avail );
}
