/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: XXX.c
 * PURPOSE:
 **************************************************************************/

static char *const _id = "freespace.c,v 3.4 1997/12/24 20:10:12 papowell Exp";

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
#include "pathname.h"
#include "fileopen.h"
#include "freespace.h"
/**** ENDINCLUDE ****/

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
#  define plp_fs_free_bytes(f) ((double)f.f_bavail * (double)f.f_bsize)
# endif

# if USE_STATFS_TYPE == ULTRIX_STATFS
#  define plp_statfs(path,buf) statfs(path,buf)
#  define plp_struct_statfs struct fs_data
#  define plp_fs_free_bytes(f) ((double)f.fd_bfree * (double)f.fd_bsize)
# endif

# if USE_STATFS_TYPE ==  SVR3_STATFS
#  define plp_struct_statfs struct statfs
#  define plp_fs_free_bytes(f) ((double)f.f_bfree * (double)f.f_bsize)
#  define plp_statfs(path,buf) statfs(path,buf,sizeof(struct statfs),0)
# endif

# if USE_STATFS_TYPE == STATFS
#  define plp_struct_statfs struct statfs
#  define plp_fs_free_bytes(f) ((double)f.f_bavail * (double)f.f_bsize)
#  define plp_statfs(path,buf) statfs(path,buf)
# endif

/***************************************************************************
 * Space_needed() - get the amount of free space needed in the spool directory
 ***************************************************************************/
unsigned long Space_needed( char *space, struct dpathname *dpath )
{
	char *end, *min_space;
	unsigned long needed = 0;
	char line[MAXPATHLEN];
	char buffer[LINEBUFFER];
	int fd, i;
	struct stat statb;

	/* get the limits */
	if( space && *space ){
		safestrncpy(buffer, space );
		min_space = buffer;
		while( isspace(*min_space) ) ++min_space;
		trunc_str( min_space );
		DEBUG3("Space_needed: MI '%s'", min_space );
		end = min_space;
		if( *min_space == '#' ) ++min_space;
		needed = strtol( min_space, &end, 10 );
		if( min_space == end ){
			/* could be a file name */
			if( min_space[0] != '/' ){
				min_space = Add_path( dpath, min_space );
			}
			DEBUG3("Space_needed: trying to open '%s'", min_space );
			fd = Checkread( min_space, &statb );
			DEBUG3("Space_needed: file '%s' fd %d", min_space, fd );
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
				logerr( LOG_ERR, "Space_needed: cannot open '%s'",
				min_space);
			}
		}
		switch( *end ){
		case 'M': case 'm': needed = 1024 * needed; break;
		}
	}

	DEBUG3("Space_needed: need %ld", needed );
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
	DEBUG3("Space_avail: path '%s', space %ld", pathname, space );
	return( space );
}

/***************************************************************************
 * int Check_space( int jobsize, char *min_space, struct dpathname *dpath )
 *   RETURNS 0 if space available, 1 if none
 ***************************************************************************/
int Check_space( int jobsize, char *min_space, struct dpathname *dpath )
{
	unsigned long needed;
	unsigned long avail;
	int ok;

	jobsize = ((jobsize+1023)/1024);
	DEBUG3("Check_space: jobsize %dK", jobsize );
	needed = Space_needed( min_space, dpath );
	DEBUG3("Check_space: need %ldK free", needed );
	avail = Space_avail( dpath );
	DEBUG3("Check_space: available %ldK free", avail );
	ok = ( (needed + jobsize) >= avail );
	DEBUG3("Check_space: need %ldK free, job %dK, space avail %ldK, ok %d",
		needed, jobsize, avail, ok );
	return( ok );
}
