/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: serverpid.c
 * PURPOSE: read and write server PIDs into files
 **************************************************************************/

static char *const _id =
"serverpid.c,v 3.4 1998/03/24 02:43:22 papowell Exp";

#include "lp.h"
#include "serverpid.h"
/**** ENDINCLUDE ****/


/**************************************************************************
 * Read_pid( int fd, char *str, int len )
 *   - Read the pid from a file
 **************************************************************************/
int Read_pid( int fd, char *str, int len )
{
	char line[LINEBUFFER];
	int n;

	if( lseek( fd, 0, SEEK_SET ) < 0 ){
		logerr_die( LOG_ERR, "Read_pid: lseek failed" );
	}

	if( str == 0 ){
		str = line;
		len = sizeof( line );
	}
	str[0] = 0;
	if( (n = read( fd, str, len-1 ) ) < 0 ){
		logerr_die( LOG_ERR, "Read_pid: read failed" );
	}
	str[n] = 0;
	n = atoi( line );
	DEBUG3( "Read_pid: %d", n );
	return( n );
}

/**************************************************************************
 * Write_pid( int fd )
 *   - Write the pid to a file
 **************************************************************************/
void Write_pid( int fd, int pid, char *str )
{
	char line[LINEBUFFER];

	if( ftruncate( fd, 0 ) ){
		logerr_die( LOG_ERR, "Write_pid: ftruncate failed" );
	}
	if( lseek( fd, 0, SEEK_SET ) < 0 ){
		logerr_die( LOG_ERR, "Write_pid: lseek failed" );
	}
	DEBUG1( "Write_pid: pid %d, '%s'", pid, str );

	if( str ){
		plp_snprintf( line, sizeof(line), "%s\n", str );
	} else {
		plp_snprintf( line, sizeof(line), "%d\n", pid );
	}
	if( Write_fd_str( fd, line ) < 0 ){
		logerr_die( LOG_ERR, "Write_pid: write failed" );
	}
}
