/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: spoolcontrol.c
 * PURPOSE: read and write the spool queue control file
 **************************************************************************/

static char *const _id =
"$Id: spoolcontrol.c,v 3.8 1997/12/16 15:06:35 papowell Exp $";

#include "lp.h"
#include "errorcodes.h"
#include "lockfile.h"
#include "malloclist.h"
#include "pathname.h"
/**** ENDINCLUDE ****/

/***************************************************************************
Get_spool_control()
	Get the control file from the spool directory and encode the information
	in it. The control file has the lines:
		printing [on|off]
		spooling [on|off]
Set_spool_control()
	Write the current status of the spooling information to the control file
 ***************************************************************************/

static struct keywords status_key[] = {
	{ "printing_disabled", INTEGER_K, &Printing_disabled },
	{ "spooling_disabled", INTEGER_K, &Spooling_disabled },
	{ "debug", STRING_K, &Control_debug},
	{ "redirect", STRING_K, &Forwarding},
	{ "holdall", INTEGER_K, &Hold_all },
	{ "class", STRING_K, &Classes},
	{ "server_order", STRING_K, &Server_order},
	{ 0 },
};

int Get_spool_control( struct stat *oldstatb, int *fdptr )
{
	char *s, *t, *end;					/* ACME Pointers */
	int fd, i, len;
	struct stat statb;
	static struct malloc_list save_status;
	char *buffer;
	char *filename;

	if( fdptr ) *fdptr = 0;
	filename = Add2_path( CDpathname, "control.", Printer );

	if( oldstatb && (stat(filename, &statb) == 0)
		&& statb.st_mtime == oldstatb->st_mtime
#if defined(ST_MTIME_NSEC)
		&& statb.ST_MTIME_NSEC == oldstatb->ST_MTIME_NSEC
#endif
	){
		/* no change */
		DEBUG3("Get_spool_control: file '%s' no change", filename );
		return(0);
	}

	DEBUG3( "Get_spool_control: clearing values" );
	for( i = 0; (s = status_key[i].keyword); ++i ){
		switch( status_key[i].type ){
			case FLAG_K:
			case INTEGER_K:
				*(int *)(status_key[i].variable) = 0;
				break;
			case STRING_K:
				*(char **)(status_key[i].variable) = 0;
				break;
			default: break;
		}
	}
	/* open and lock the file */
	fd = Lockf( filename, &statb );
	if( fd < 0 ){
		logerr_die( LOG_ERR,
			"Get_spool_control: cannot create file '%s'",filename);
	}

	DEBUG3("Get_spool_control: file '%s', fd %d", filename, fd );
	if( oldstatb ){
		*oldstatb = statb;
	}
	/* read in new status */
	len = statb.st_size;
	if( len + 1 >= save_status.max  ){
		extend_malloc_list( &save_status, 1, len+1 );
	}
	buffer = (void *)save_status.list;
	for( i = 0, s = buffer;
		len > 0 && (i = read( fd, s, len)) > 0;
		len -= i, s += i );
	if( i < 0 ){
		Errorcode = JABORT;
		logerr_die( LOG_ERR, "Get_spool_control: '%s' read failed",
			filename );
	}
	*s = 0;
	if( fdptr == 0 ){
		close(fd);
	} else {
		*fdptr = fd;
	}
	DEBUG3("Get_spool_control: '%s'", buffer );

	/* clear the current status */

	for( s = buffer; s && *s; s = end ){
		end = strchr( s, '\n' );
		if( end ){
			*end++ = 0;
		}
		t = strchr(s, ' ' );
		if( t ){
			*t++ = 0;
			while( isspace( *t ) ) ++t;
		}
		DEBUG3("Get_spool_control: key '%s' value '%s'", s, t );
		for( i = 0;
			status_key[i].keyword
				&& strcasecmp( s, status_key[i].keyword );
			++i );
		if( status_key[i].keyword ){
			len = 0;
			if( t ) len = strtol( t, (void *)0, 0 );
			switch( status_key[i].type ){
				case FLAG_K:
				case INTEGER_K:
					DEBUG3("Get_spool_control: update '%s' with integer '%d'",
					status_key[i].keyword, len );
					*(int *)(status_key[i].variable) = len;
					break;
				case STRING_K:
					DEBUG3("Get_spool_control: update '%s' with string '%s'", 
					status_key[i].keyword, t );
					*(char **)(status_key[i].variable) = t;
					break;
				default: break;
			}
		}
	}
	if(DEBUGL4 ){
		DEBUG3( "Get_spool_control:" );
		for( i = 0; (s = status_key[i].keyword); ++i ){
			switch( status_key[i].type ){
				case FLAG_K:
				case INTEGER_K:
					logDebug( "  %s %d", status_key[i].keyword,
					*(int *)(status_key[i].variable) );
					break;
				case STRING_K:
					t = *(char **)(status_key[i].variable);
					if( t ) logDebug( "  %s %s", status_key[i].keyword,t);
					break;
				default: break;
			}
		}
	}
	return( 1 );
}


/***************************************************************************
 * Set_spool_control
 *  write the spool control file with suitable values
 *  Control file has the format:
 *    printing_disabled 1/0
 *    spooling_disabled 1/0
 ***************************************************************************/

int Set_spool_control( int *fdptr, int forcechange )
{
	struct dpathname dpath;
	struct stat statb, newstatb;
	char buffer[SMALLBUFFER];
	char *s, *t;
	int i, fd, len;
	
	dpath = *CDpathname;

	buffer[0] = 0;
	buffer[sizeof(buffer)-1] = 0;
	for( i = 0; status_key[i].keyword ; ++i ){
		len = strlen(buffer);
		s = buffer+len;
		len = sizeof( buffer ) - len - 1 ;
		t = 0;
		switch( status_key[i].type ){
		case FLAG_K:
		case INTEGER_K:
			plp_snprintf( s, len, "%s %d\n",
				status_key[i].keyword,
				*(int *)(status_key[i].variable) );
			break;
		case STRING_K:
			t = *(char **)(status_key[i].variable);
			if( t && *t ){
				plp_snprintf( s, len, "%s %s\n",
				status_key[i].keyword, t );
			}
			break;
		default: break;
		}
	}

	s = Add2_path( CDpathname, "control.", Printer );
	DEBUG4("Set_spool_control: file '%s', '%s'",s, buffer );

	if( fdptr == 0 ){
		fd = Lockf( s, &statb );
		if( fd < 0 ){
			logerr( LOG_ERR,
				"Set_spool_control: cannot create file '%s'",s);
			return( 1 );
		}
	} else {
		fd = *fdptr;
	}
	if( ftruncate( fd, 0 ) < 0 ){
		logerr( LOG_ERR,
			"Set_spool_control: cannot truncate '%s'",s);
	}
	if( Write_fd_str( fd, buffer ) < 0 ){
		Errorcode = JABORT;
		logerr_die( LOG_ERR,
			"Set_spool_control: cannot write '%s'",s);
	}
	if( fstat(fd, &newstatb)  ){
		logerr_die( LOG_ERR,
			"Set_spool_control: cannot stat '%s'",s);
	}
	if( forcechange && statb.st_mtime == newstatb.st_mtime
		&& statb.st_size == newstatb.st_size
#if defined(ST_MTIME_NSEC)
		&& statb.ST_MTIME_NSEC == newstatb.ST_MTIME_NSEC
#endif
	){
		DEBUG3("Set_spool_control: file '%s' no change, sleeping", s );
		plp_sleep(1);
	}
	if( Write_fd_str( fd, "\n" ) < 0 ){
		Errorcode = JABORT;
		logerr_die( LOG_ERR,
			"Set_spool_control: cannot write '%s'",s);
	}
	close( fd );
	return( 0 );
}
