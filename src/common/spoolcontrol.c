/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: spoolcontrol.c
 * PURPOSE: read and write the spool queue control file
 **************************************************************************/

static char *const _id =
"$Id: spoolcontrol.c,v 3.0 1996/05/19 04:06:13 papowell Exp $";

#include "lp.h"
#include "printcap.h"
#include "jobcontrol.h"

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
	{ "autohold", INTEGER_K, &Auto_hold },
	{ "class", STRING_K, &Classes},
	{ "server_order", STRING_K, &Server_order},
	{ 0 },
};

int Get_spool_control( struct stat *oldstatb )
{
	char *s, *t, *end;					/* ACME Pointers */
	int fd, i, len;
	struct stat statb;
	static char buffer[SMALLBUFFER];
	int lock, create;

	if( Debug > 4 ){
		DEBUG4( "Get_spool_control: before checking " );
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
	s = Add2_path( CDpathname, "control.", Printer );

	if( oldstatb && (stat(s, &statb) == 0)
		&& statb.st_ctime == oldstatb->st_ctime ){
		/* no change */
		DEBUG5("Get_spool_control: file '%s' no change", s );
		return(0);
	}

	/* open and lock the file */
	fd = Lockf( s, &lock, &create, &statb );
	if( fd < 0 ){
		logerr( LOG_ERR,
			"Get_spool_control: cannot create file '%s'",s);
		return(1);
	}
	if( lock == 0 ){
		DEBUG8("Get_spool_control: waiting for lock" );
		/* we need to lock the file */
		lock = Do_lock( fd, s, 1 );
	}
	if( lock <= 0 ){
		DEBUG8("Get_spool_control: locking failed" );
		Errorcode = JABORT;
		logerr_die( LOG_ERR,
			"Get_spool_control: cannot lock file '%s'",s);
	}

	DEBUG5("Get_spool_control: file '%s', fd %d", s, fd );
	if( oldstatb ){
		*oldstatb = statb;
	}
	/* read in new status */
	buffer[0] = 0;
	if( (len = statb.st_size) != 0 ){
		if( len >= sizeof(buffer) ){
			len = sizeof(buffer) - 1;
		}
		for( s = buffer;
			len > 0 && (i = read( fd, s, len)) > 0;
			len -= i, s += i );
		*s = 0;
	}
	close(fd);
	DEBUG6("Get_spool_control: '%s'", buffer );

	/* clear the current status */

#if 0
	/* disable this - you want control file to OVERIDE defaults */
	/* Wed Apr 24 07:29:39 PDT 1996 */
	for( i = 0; status_key[i].keyword; ++i ){
		switch( status_key[i].type ){
			case FLAG_K:
			case INTEGER_K: *(int *)(status_key[i].variable) = 0; break;
			case STRING_K: *(char **)(status_key[i].variable) = 0; break;
			default: break;
		}
	}
#endif

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
		DEBUG6("Get_spool_control: key '%s' value '%s'", s, t );
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
					DEBUG6("Get_spool_control: update '%s' with integer '%d'",
					status_key[i].keyword, len );
					*(int *)(status_key[i].variable) = len;
					break;
				case STRING_K:
					DEBUG6("Get_spool_control: update '%s' with string '%s'", 
					status_key[i].keyword, t );
					*(char **)(status_key[i].variable) = t;
					break;
				default: break;
			}
		}
	}
	if( Debug > 4 ){
		DEBUG4( "Get_spool_control:" );
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

int Set_spool_control()
{
	struct dpathname dpath;
	struct stat statb;
	char buffer[SMALLBUFFER];
	char *s, *t;
	int lock, create;
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
	DEBUG8("Set_spool_control: file '%s', '%s'",s, buffer );

	fd = Lockf( s, &lock, &create, &statb );
	if( fd < 0 ){
		logerr( LOG_ERR,
			"Set_spool_control: cannot create file '%s'",s);
		return( 1 );
	}
	if( lock == 0 ){
		DEBUG8("Set_spool_control: waiting for lock" );
		/* we want to lock the file */
		lock = Do_lock( fd, s, 1 );
	}
	if( lock <= 0 ){
		DEBUG8("Set_spool_control: locking failed" );
		Errorcode = JABORT;
		logerr_die( LOG_ERR,
			"Set_spool_control: cannot lock file '%s'",s);
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
	close( fd );
	return( 0 );
}
