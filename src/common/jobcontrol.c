/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: jobcontrol.c
 * PURPOSE: read and write the spool queue control file
 **************************************************************************/

static char *const _id =
"$Id: jobcontrol.c,v 3.9 1997/03/24 00:45:58 papowell Exp papowell $";

#include "lp.h"
#include "jobcontrol.h"
#include "decodestatus.h"
#include "dump.h"
#include "errorcodes.h"
#include "lockfile.h"
#include "malloclist.h"
#include "pathname.h"
#include "pr_support.h"
#include "setup_filter.h"
/**** ENDINCLUDE ****/

/***************************************************************************
 * The job control file has the lines:
 *		<key> value
 *       route <key> value
 * The <key> fields are used to set various entries in the control file,
 *  while the route <key> set routing information.
 * We need to scan and generate this file using control file information.
 ***************************************************************************/

/**********************
 * Keywords
 **********************/
#define HOLD       1
#define PRIORITY   2
#define REMOVE     3
#define ACTIVE     4
#define REDIRECT   6
#define ERROR      7
#define DONE       8
#define ROUTE      9
#define DEST       10
#define COPIES     11
#define COPY_DONE  12
#define STATUS     13
#define END        14
#define ROUTED     15
#define ATTEMPT    16
#define IDENT      17
#define RECEIVER   18
#define SEQUENCE   19

static struct keywords status_key[] = {
{ "active", INTEGER_K, (void *)0, ACTIVE },
{ "attempt", INTEGER_K, (void *)0, ATTEMPT },
{ "copies", INTEGER_K, (void *)0, COPIES },
{ "copy_done", INTEGER_K, (void *)0, COPY_DONE },
{ "dest", INTEGER_K, (void *)0, DEST },
{ "done", INTEGER_K, (void *)0, DONE },
{ "end", INTEGER_K, (void *)0, END },
{ "error", STRING_K, (void *)0, ERROR },
{ "hold", INTEGER_K, (void *)0, HOLD },
{ "ident", INTEGER_K, (void *)0, IDENT },
{ "priority", INTEGER_K, (void *)0, PRIORITY },
{ "receiver", INTEGER_K, (void *)0, RECEIVER },
{ "redirect", STRING_K, (void *)0, REDIRECT },
{ "remove", INTEGER_K, (void *)0, REMOVE },
{ "route", INTEGER_K, (void *)0, ROUTE },
{ "routed", INTEGER_K, (void *)0, ROUTED },
{ "status", INTEGER_K, (void *)0, STATUS },
{ "sequence", INTEGER_K, (void *)0, SEQUENCE },
{ 0 }
};

#define status_key_len (sizeof( status_key )/sizeof( status_key[0] ))

static char *Hold_file_pathname( struct control_file *cfp );
struct keywords *Find_key( struct keywords *keys, int len, char *key );
static void get_destination( struct control_file *cfp );

/***************************************************************************
 * Get_job_control( struct control_file, int *fd )
 *	Get the job control file from the spool directory and
 *     decode the information in the file
 *  This will read the job control file into a buffer, and then
 *  parse it,  line by line.  Lines which have 'routing' information
 *  will then be parsed to find the various job destinations.
 * Returns: 1 change
 *          0 no change
 ***************************************************************************/

int Get_job_control( struct control_file *cfp, int *fdptr )
{
	struct stat statb;
	long value;
	char *s, *t, *end, *buffer, **list;
	int i, fd, len, lock, create;
	struct keywords *key;
	char *hold_file;
	char id[LINEBUFFER];

	fd = -1;
	if( fdptr  && *fdptr > 0 ){
		fd = *fdptr;
		if( fstat( fd, &statb ) < 0 ){
			fd = -1;
		}
	}
	hold_file = Hold_file_pathname( cfp );

	DEBUG3("Get_job_control: file '%s', fd %d, Auto_hold %d, Hold_all %d",
		hold_file, fd, Auto_hold, Hold_all);

	/* if not changed, do not read it */
	if( fd < 0 ){
		i = stat( hold_file, &statb );
	} else {
		i = fstat( fd, &statb );
	}

	DEBUG3(
"Get_job_control: file '%s', new( st_ino 0x%x, st_mtime 0x%x, st_size 0x%x",
		hold_file, (int)statb.st_ino, (int)statb.st_mtime, (int)statb.st_size );
	DEBUG3(
"Get_job_control: file '%s', old( st_ino 0x%x, st_mtime 0x%x, st_size 0x%x",
		hold_file, (int)cfp->hstatb.st_ino, (int)cfp->hstatb.st_mtime,
		(int)cfp->hstatb.st_size );
#if defined(ST_MTIME_NSEC)
	DEBUG3(
"Get_job_control: file '%s', old( st_mtime_nsec 0x%x), new( st_mtime_nsec 0x%x )",
		hold_file, cfp->hstatb.ST_MTIME_NSEC, statb.ST_MTIME_NSEC);
#endif
	if( i == 0
		&& cfp->hstatb.st_ino == statb.st_ino
		&& cfp->hstatb.st_mtime == statb.st_mtime
		&& cfp->hstatb.st_size == statb.st_size
#if defined(ST_MTIME_NSEC)
		&& cfp->hstatb.ST_MTIME_NSEC == statb.ST_MTIME_NSEC
#endif
		){
		DEBUG3("Get_job_control: holdfile not changed" );
		return( 0 );
	}

	/* update the hold information */
	memset( &cfp->hold_info, 0, sizeof( cfp->hold_info ) );

	if( fd < 0 ){
		fd = Lockf( hold_file, &lock, &create, &statb );
		if( fdptr ) *fdptr = fd;
		if( fd < 0 ){
			Errorcode = JABORT;
			logerr_die( LOG_ERR,
				"Get_job_control: cannot create hold file '%s'",hold_file);
		}
		if( lock == 0 ){
			DEBUG4("Get_job_control: waiting for lock" );
			/* we want to lock the file */
			lock = Do_lock( fd, hold_file, 1 );
		}
		if( lock <= 0 ){
			Errorcode = JABORT;
			logerr_die( LOG_ERR,
				"Get_job_control: cannot lock file '%s'",hold_file);
		}
	} else if( fstat( fd, &statb ) == -1 ){
		logerr_die( LOG_ERR,
			"Get_job_control: fstat failed lock file '%s'",hold_file);
	}

	cfp->hstatb = statb;

	if( Auto_hold || Hold_all ){
		cfp->hold_info.hold_time = time( (void *)0 );
	}

	/* allocate a buffer to hold the file */
	len = roundup_2(statb.st_size+1,8);
	if( cfp->hold_file_info.list == 0 || cfp->hold_file_info.max < len ){
		buffer = (void *)cfp->hold_file_info.list;
		if( buffer ){
			free( buffer );
		}
		cfp->hold_file_info.max = len; 
		malloc_or_die( buffer, len );
		cfp->hold_file_info.list = (void *)buffer;
		DEBUG4("Get_job_control: malloc hold_file_info 0x%x, max %d",
			buffer,cfp->hold_file_info.max);
	}
	buffer = (void *)cfp->hold_file_info.list;
	cfp->hold_file_info.size = len = statb.st_size;
	DEBUG4("Get_job_control: buffer 0x%x, buffer len %d, file len %d",
		buffer, cfp->hold_file_info.size, len );

	/* get the values from the file */
	for( s = buffer;
		len > 0 && (i = read( fd, s, len)) > 0;
		len -= i, s += i );
	*s = 0;

	/* close the file */
	if( fdptr == 0 ){
		close(fd);
	} else {
		*fdptr = fd;
	}

	DEBUG3("Get_job_control: file contents '%s'", buffer );

	/* split the lines up */
	/* we have to read the new status, clear the old */
	cfp->hold_file_lines.count = 0;
	if( cfp->hold_file_lines.count+1 >= cfp->hold_file_lines.max ){
		extend_malloc_list( &cfp->hold_file_lines,
			sizeof( char *), cfp->hold_file_lines.count+100 );
	}
	list = cfp->hold_file_lines.list;
	for( s = buffer; s && *s; s = end ){
		end = strchr( s, '\n' );
		if( end ){
			*end++ = 0;
		}
		/* remove leading and trailing white space */
		while( *s && isspace( *s ) ) ++s;
		trunc_str(s);
		/* throw away blank lines */
		if( *s == 0 ) continue;

		/* find the key */
		if( (t = strpbrk( s, " \t")) == 0 ){
			t = s + strlen(s);
		}
		if( cfp->hold_file_lines.count+1 >= cfp->hold_file_lines.max ){
			extend_malloc_list( &cfp->hold_file_lines,sizeof(list[0]),100);
			list = cfp->hold_file_lines.list;
		}
		list[cfp->hold_file_lines.count++] = s;
		list[cfp->hold_file_lines.count] = 0;
		strncpy( id, s, t-s );
		id[t-s] = 0;
		while( isspace( *t ) ) ++t;

		DEBUG3("Get_job_control: line '%s' id '%s' value='%s'",s,id,t );
		key = Find_key( status_key, status_key_len, id );
		if( key && key->keyword ){
			value = 0;
			if( t && *t ) value = strtol( t, (void *)0, 0 );
			DEBUG4("Get_job_control: found '%s' '%s' value %d",
				s, t, value );
			switch( key->maxval ){
			case ATTEMPT: cfp->hold_info.attempt = value; break;
			case HOLD: cfp->hold_info.hold_time = value; break;
			case PRIORITY: cfp->hold_info.priority_time = value; break;
			case REMOVE: cfp->hold_info.remove_time = value; break;
			case DONE: cfp->hold_info.done_time = value; break;
			case ROUTED: cfp->hold_info.routed_time = value; break;
			case ACTIVE: cfp->hold_info.active = value; break;
			case RECEIVER: cfp->hold_info.receiver = value; break;
			case REDIRECT:
				if( t && *t ){
					strncpy( cfp->hold_info.redirect, t, sizeof( cfp->hold_info.redirect) );
				} else {
					cfp->hold_info.redirect[0] = 0;
				}
				break;
			case ERROR:
				if( t && *t && cfp->error[0] == 0 ){
					strncpy( cfp->error, t, sizeof( cfp->error) );
				}
				break;
			case ROUTE:
				if( cfp->destination_info_start == 0 ){
					cfp->destination_info_start = cfp->hold_file_lines.count-1;
				}
				break;
			default: break;
			}
		}
	}
	DEBUG3("Get_job_control: hold 0x%x, priority 0x%x, remove 0x%x",
		cfp->hold_info.hold_time, cfp->hold_info.priority_time, cfp->hold_info.remove_time );
	if(DEBUGL4 ){
		logDebug( "Get_job_control: hold_file_lines %d", cfp->hold_file_lines.count );
		for( i = 0; i < cfp->hold_file_lines.count; ++i ){
			logDebug("  [%d] '%s'", i, list[i] );
		}
	}
	/* now get the destination information */
	cfp->destination_list.count = 0;
	if( cfp->destination_info_start ){
		get_destination( cfp );
	}
	if(DEBUGL3 ) dump_control_file( "Get_job_control - return value", cfp );
	return( 1 );
}

/***************************************************************************
 * Find_key() - search the keywords entry for the key value
 *  - we handle tables where we have a 0 terminating value as well
 ***************************************************************************/
struct keywords *Find_key( struct keywords *keys, int len, char *key )
{
	int top, bottom, mid, compare;

	bottom = 0;
	top = len-1;
	/* we skip top one if it has a terminal 0 entry */
	if( keys[top].keyword == 0 ) --top;
	DEBUG4("Find_key: find '%s'", key );
	while( top >= bottom ){
		mid = (top+bottom)/2;
		compare = strcasecmp( keys[mid].keyword, key );
		/* DEBUG4("Find_key: top %d, bottom %d, mid='%s'",
			top, bottom, keys[mid].keyword ); */
		if( compare == 0 ){
			return( &keys[mid] );
		} else if( compare > 0 ){
			top = mid - 1;
		} else {
			bottom = mid + 1;
		}
	}
	return( 0 );
}


/***************************************************************************
 * get_destination()
 *  get the destination routing information from the control file
 *  hold_file_lines fields.  This information has the form
 *  route <key> value.  The various values are used to set entries in
 *  the destination_list fields.
 ***************************************************************************/

static void get_destination( struct control_file *cfp )
{
	struct destination *destinationp, *d;
	int i, dest_start;
	char *s, *t;
	struct keywords *key;
	char id[LINEBUFFER];

	/* clear out the destination list */
	cfp->destination_list.count = 0;

	/* extend the list if necessary */
	if( cfp->destination_list.count+1 >= cfp->destination_list.max ){
		extend_malloc_list( &cfp->destination_list,
			sizeof( struct destination), cfp->destination_list.count+10 );
	}
	destinationp = (void *)cfp->destination_list.list;
	d = &destinationp[cfp->destination_list.count];
	memset( (void *)d, 0, sizeof( d[0] ) );
	dest_start = cfp->destination_info_start;
	for( i = dest_start;
		i < cfp->hold_file_lines.count; ++i ){
		s = cfp->hold_file_lines.list[i];
		/* DEBUG3("get_destination: checking '%s'", s ); */
		/* skip over the 'route' keyword */
		if( strncmp(s, "route", 5 ) == 0 ) s += 5;
		while( isspace( *s ) ) ++s;
		if( isupper( *s ) ){
			/* do not need to worry about lines starting with upper case */
			continue;
		}
		t = strpbrk( s, " \t");
		if( t == 0 ){
			t = s + strlen(s);
		}
		strncpy( id, s, t - s );
		id[ t - s ] = 0;
		DEBUG3("get_destination: checking '%s'='%s'", id, t );
		key = Find_key( status_key, status_key_len, id );
		if( key ){
			while( isspace( *t ) ) ++t;
			DEBUG3("get_destination: found key '%s'='%s'", key->keyword, t );
			switch( key->maxval ){
			default: break;
			case ROUTE: break;
			case IDENT: 
				safestrncpy( d->identifier,"A");
				safestrncat( d->identifier,t);
				break;
			case DEST: safestrncpy( d->destination,t); break;
			case ERROR: safestrncpy( d->error,t); break;
			case COPIES: d->copies = atoi( t );
				DEBUG3("get_destination: copies '%s'->%d", t, d->copies );
				break;
			case COPY_DONE: d->copy_done = atoi( t ); break;
			case STATUS: d->status = atoi( t ); break;
			case ACTIVE: d->active = atoi( t ); break;
			case DONE: d->done = atoi( t ); break;
			case HOLD: d->hold = atoi( t ); break;
			case ATTEMPT: d->attempt = atoi( t ); break;
			case SEQUENCE: d->sequence_number = atoi( t ); break;
			case END:
				/* watch out for the no destination */
				d->arg_start = dest_start;
				d->arg_count = i - dest_start;
				DEBUG3("get_destination: dest '%s', copies now %d",
					d->destination, d->copies );
				if( d->destination[0] &&
					++cfp->destination_list.count >=
						cfp->destination_list.max ){
					extend_malloc_list( &cfp->destination_list,
						sizeof( struct destination), 10 );
				}
				destinationp = (void *)cfp->destination_list.list;
				d = &destinationp[cfp->destination_list.count];
				memset( (void *)d, 0, sizeof( d[0] ) );
				dest_start = i+1;
				break;
			}
		}
	}
	/* no end  - clear it up */
	d->arg_start = dest_start;
	d->arg_count = i - dest_start;
	if( d->destination[0] ){
		++cfp->destination_list.count;
	}
	for( i = 0; i < cfp->destination_list.count; ++i ){
		int len;
		d = &destinationp[i];
		if( d->identifier[0] == 0 ){
			safestrncpy( d->identifier, cfp->identifier );
			len = strlen( d->identifier );
			plp_snprintf( d->identifier+len,
				sizeof(d->identifier)-len, ".%d", i+1 );
		}
	}
}

static void write_line( int fd, char *buffer, char *file )
{
	if( Write_fd_str( fd, buffer ) < 0
		|| Write_fd_str( fd, "\n" ) < 0 ){
		Errorcode = JABORT;
		logerr_die( LOG_ERR,
			"write_line: cannot write file '%s'",file);
	}
}

static void write_route_str( int fd, char *header, char *str, char *file )
{
	char buffer[SMALLBUFFER];
	plp_snprintf( buffer, sizeof(buffer)-2, "route %s %s", 
		header, str );
	DEBUG4("write_route_str: dest line '%s'", buffer );
	write_line( fd, buffer, file );
}

static void write_route_int( int fd, char *header, int val, char *file )
{
	char buffer[SMALLBUFFER];
	plp_snprintf( buffer, sizeof(buffer)-2, "route %s %d", 
		header, val );
	DEBUG4("write_route_int: dest line '%s'", buffer );
	write_line( fd, buffer, file );
}


/***************************************************************************
 * Set_job_control( struct control_file, int *fd )
 *	Lock the job control file from the spool directory and
 *     write new information.
 * Returns: 0
 ***************************************************************************/

int Set_job_control( struct control_file *cfp, int *fdptr, int force_change )
{
	struct stat statb;
	char buffer[SMALLBUFFER];
	char *s, *t, *hold_file;
	char **lines;
	int lock, create;
	int i, j, value, fd;
	struct destination *destination, *d;

	/* used passed file */
	hold_file = Hold_file_pathname( cfp );
	DEBUG3("Set_job_control: file '%s'", hold_file );
	if(DEBUGL4){
		dump_control_file("Set_job_control",cfp);
	}

	if( fdptr == 0 || *fdptr < 0 ){
		fd = Lockf( hold_file, &lock, &create, &statb );
		if( fd < 0 ){
			Errorcode = JABORT;
			logerr_die( LOG_ERR,
				"Set_job_control: cannot create hold file '%s'",hold_file);
		}
		if( lock == 0 ){
			DEBUG4("Set_job_control: waiting for lock" );
			/* we want to lock the file */
			lock = Do_lock( fd, hold_file, 1 );
		}
		if( lock <= 0 ){
			Errorcode = JABORT;
			logerr_die( LOG_ERR,
				"Set_job_control: cannot lock file '%s'",hold_file);
		}
	} else {
		fd = *fdptr;
	}
	if( ftruncate( fd, 0 ) < 0 ){
		Errorcode = JABORT;
		logerr_die( LOG_ERR,
			"Set_job_control: cannot truncate hold file '%s'",hold_file);
	}
	value = 0;
	for( i = 0; status_key[i].keyword ; ++i ){
		t = 0;
		switch( status_key[i].maxval ){
		case HOLD:		value = cfp->hold_info.hold_time; break;
		case PRIORITY:	value = cfp->hold_info.priority_time; break;
		case REMOVE:	value = cfp->hold_info.remove_time; break;
		case ACTIVE:	value = cfp->hold_info.active; break;
		case RECEIVER:	value = cfp->hold_info.receiver; break;
		case DONE:		value = cfp->hold_info.done_time; break;
		case ROUTED:	value = cfp->hold_info.routed_time; break;
		case ATTEMPT:	value = cfp->hold_info.attempt; break;
		case REDIRECT:	t = cfp->hold_info.redirect; if(t==0) t=""; break;
		case ERROR:		t = cfp->error; if(t==0) t="";break;
		default: continue;
		}
		buffer[0] = 0;
		if( t == 0 ){
			plp_snprintf( buffer, sizeof(buffer)-2, "%s %d",
				status_key[i].keyword, value );
		} else if( *t ){
			plp_snprintf( buffer, sizeof(buffer)-2, "%s %s",
				status_key[i].keyword, t );
		}
		if( buffer[0] ){
			DEBUG4("Set_job_control: '%s'", buffer );
			write_line( fd, buffer, hold_file );
		}
	}
	if( cfp->hold_info.routed_time && cfp->destination_list.count > 0 ){
		destination = (void *)cfp->destination_list.list;
		for( i = 0; i < cfp->destination_list.count; ++i ){
			d = &destination[i];
			if( d->destination[0] == 0 ) continue;
			write_route_str( fd, "dest", d->destination, hold_file );
			write_route_str( fd, "ident", d->identifier+1, hold_file );
			write_route_str( fd, "error", d->error, hold_file );
			write_route_int( fd, "copies", d->copies, hold_file );
			write_route_int( fd, "copy_done", d->copy_done, hold_file );
			write_route_int( fd, "status", d->status, hold_file );
			write_route_int( fd, "active", d->active, hold_file );
			write_route_int( fd, "attempt", d->attempt, hold_file );
			write_route_int( fd, "done", d->done, hold_file );
			write_route_int( fd, "hold", d->hold, hold_file );
			write_route_int( fd, "sequence", d->sequence_number, hold_file );
			write_route_str( fd, "end", "", hold_file );
			lines = &cfp->hold_file_lines.list[d->arg_start];
			for( j = 0; j < d->arg_count; ++j ){
				if( (s = lines[j])[0] ){
					if( strncmp( s, "route", 5 ) == 0 ){
						s += 5;
					}
					while( isspace( *s ) ) ++s;
					if( isupper( *s ) ){
						plp_snprintf( buffer, sizeof(buffer)-2, "route %s", s );
						DEBUG4("Set_job_control: dest line '%s'", buffer );
						write_line( fd, buffer, hold_file );
					}
				}
			}
		}
	}
	if( fstat( fd, &statb ) < 0 ){
		Errorcode = JABORT;
		logerr_die(LOG_ERR, "Set_job_control: fstat '%s' failed", hold_file );
	}
	/* we make sure changes are noticed */
	if( force_change && cfp->hstatb.st_mtime == statb.st_mtime
#if defined(ST_MTIME_NSEC)
		&& cfp->hstatb.ST_MTIME_NSEC == statb.ST_MTIME_NSEC
#endif
	){
		plp_sleep(1);
		if( Write_fd_str( fd, "\n" ) < 0 ){
			logerr_die( LOG_ERR, "Set_job_control: write '%s' failed",
				hold_file );
		}
	}
	close( fd );
	if( stat( hold_file, &statb ) < 0 ){
		Errorcode = JABORT;
		logerr_die(LOG_ERR, "Set_job_control: stat '%s' failed", hold_file );
	}
	DEBUG3(
"Set_job_control: file '%s', new( st_ino 0x%x, st_mtime 0x%x, st_size 0x%x",
		hold_file, statb.st_ino, statb.st_mtime, statb.st_size );
	DEBUG3(
"Set_job_control: file '%s', old( st_ino 0x%x, st_mtime 0x%x, st_size 0x%x",
		hold_file, cfp->hstatb.st_ino, cfp->hstatb.st_mtime,
		cfp->hstatb.st_size );
#if defined(ST_MTIME_NSEC)
	DEBUG3(
"Get_job_control: file '%s', old( st_mtime_nsec 0x%x), new( st_mtime_nsec 0x%x )",
		hold_file, cfp->hstatb.ST_MTIME_NSEC, statb.ST_MTIME_NSEC);
#endif
	if( fdptr ) *fdptr = -1;
	return( 0 );
}

/***************************************************************************
 * char *Hold_file_pathname( struct control_file *cfp )
 *  get the hold file name for the job and put it in
 *  the control file.
 ***************************************************************************/
char *Hold_file_pathname( struct control_file *cfp )
{
	char *s;
	/*
	 * get the hold file pathname
	 */
	if( cfp->hold_file[0] == 0 ){
		DEBUG4("Hold_file_pathname: open '%s' transfer '%s'",
			cfp->openname, cfp->transfername );
		if( cfp->openname[0] ){
			strncpy( cfp->hold_file, cfp->openname, sizeof(cfp->hold_file ));
		} else {
			strncpy( cfp->hold_file,
				Add_path( SDpathname, cfp->transfername),
				sizeof(cfp->hold_file ));
		}
		if( (s = strrchr( cfp->hold_file, '/' )) ){
			++s;
		} else {
			s = cfp->hold_file;
		}
		*s = 'h';
	}
	DEBUG4("Hold_file_pathname: '%s'", cfp->hold_file );
	return( cfp->hold_file );
}

/***************************************************************************
 * Get_route( struct control_file )
 *	Get routing information from the routing filter
 *  This will have the format:
 *  <key> value
 *   where key is one of the routing keys
 *
 ***************************************************************************/

int Get_route( struct control_file *cfp )
{
	int i, err = 0;
	char *command = Routing_filter;
	int p[2];
	char buffer[SMALLBUFFER];
	char *block = 0;
	char **list;
	int len, count, size, sequence;
	char *s, *t, *end;
	struct destination *destinationp, *d;

	DEBUG3("Get_route: %s", command );

	if( command ) while( isspace( *command ) ) ++command;
	if( command && *command == '|' ) ++command;
	if( command ) while( isspace( *command ) ) ++command;
	if( command == 0 || *command == 0 ){
		goto error;
	}

	if( pipe(p) < 0 ){
		Errorcode = JABORT;
        logerr_die( LOG_ERR, "Get_route: pipe failed" );
	}
	DEBUG3("Get_route: pipe fd [%d, %d]", p[0], p[1] );
	if( (err = Make_filter( 'f', cfp, &As_fd_info, command, 0, 0,
		p[1], (void *)0, (void *)0, 0, 0, 0 )) ){
		goto error;
	}
	(void)close(p[1]);
	p[1] = -1;
	if( As_fd_info.input > 0 ){
		close( As_fd_info.input );
		As_fd_info.input = -1;
	}

	/* now we read from the filter */
	
	len = 0;
	block = (void *)cfp->hold_file_info.list;
	if( block ) *block = 0;
	while( (count = read( p[0], buffer, sizeof(buffer) - 1 ) ) > 0 ){
		buffer[count] = 0;
		DEBUG3("Get_route: route '%s'", buffer );
		size = count + len + 1;
		block = (void *)cfp->hold_file_info.list;
		if( size > cfp->hold_file_info.size ){
			cfp->hold_file_info.size = size = roundup_2(size,9);
			if( size == 0 ){
				fatal( LOG_ERR, "Get_route: malloc/realloc of 0!!" );
			}
			if( block ){
				if( (block = realloc( block, size )) == 0 ){
				logerr_die( LOG_ERR, "Get_route: realloc %d failed", size );
				}
			} else {
				if( (block = malloc( size )) == 0 ){
				logerr_die( LOG_ERR, "Get_route: malloc %d failed", size );
				}
			}
			cfp->hold_file_info.list = (void *)block;
		}
		strcpy(block+len, buffer );
		len += count;
	}
	if( count < 0 ){
		logerr_die( LOG_ERR, "Get_route: read failed" );
	}

	close( p[0] );
	p[0] = -1;

	err = Close_filter( &As_fd_info, 0, "router" );
	DEBUG3("Get_route: filter exit status %s", Server_status(err) );
	if( err ){
		plp_snprintf( cfp->error, sizeof(cfp->error),
			"no routing information - status %s", Server_status( err ) );
		goto error;
	}

	/* now we parse the lines */

	DEBUG3("Get_route: route info '%s'", block?block:"<NULL>" );

	if( cfp->destination_info_start == 0 ){
		cfp->destination_info_start = cfp->hold_file_lines.count;
	}
	if( cfp->hold_file_lines.count+1 >= cfp->hold_file_lines.max ){
		extend_malloc_list( &cfp->hold_file_lines, sizeof( char *),
		cfp->hold_file_lines.count+100 );
	}
	list = cfp->hold_file_lines.list;
	for( s = block; s && *s; s = end ){
		end = strchr( s, '\n' );
		if( end ){
			*end++ = 0;
		}
		while( isspace(*s) ) ++s;
		t = s+strlen(s);
		while( --t >= s && isspace( *t ) ) *t = 0;
		if( *s == 0 || *s == '#' ) continue;
		if( cfp->hold_file_lines.count+1 >= cfp->hold_file_lines.max ){
			extend_malloc_list( &cfp->hold_file_lines, sizeof( char *),
			cfp->hold_file_lines.count+100 );
			list = cfp->hold_file_lines.list;
		}
		list[cfp->hold_file_lines.count++] = s;
	}
	list[cfp->hold_file_lines.count] = 0;
	get_destination( cfp );
	if( cfp->destination_list.count > 0 ){
		sequence = 0;
		cfp->hold_info.routed_time = time( (void *)0 );
		destinationp = (void *)cfp->destination_list.list;
		for( i = 0; i < cfp->destination_list.count; ++i ){
			d = &destinationp[i];
			d->sequence_number = sequence;
			if( d->copies > 0 ){
				sequence += d->copies;
			} else {
				++sequence;
			}
		}
	}
	DEBUG3("Get_route: destination count %d", cfp->destination_list.count );
	if(DEBUGL3 ) dump_control_file( "Get_route- return value", cfp );

error:
	return( err );
}




/***************************************************************************
 * char *Copy_hf( struct control_file *cf, char *header )
 *  Make a copy of the status file with the header
 *  and return a pointer *  to it.
 *  If no_header is nonzero, do not put the header on.
 * Note that successive calls to an unchanged control file will
 *  not destroy the pointer validity.
 ***************************************************************************/

char *Copy_hf( struct malloc_list *data, struct malloc_list *copy,
	char *header, char *prefix )
{
	char **lines, *s;
	char *buffer = 0;
	int buffer_len;
	int i, len, prefix_len;

	DEBUG3("Copy_hf: data 0x%x, count %d, copy 0x%x, header '%s', prefix '%s'",
		data, data?data->count:0, copy, header, prefix );

	if( data->count ){
		if( prefix == 0 ) prefix = "";
		prefix_len = strlen( prefix );

		buffer = (void *)copy->list;
		buffer_len = copy->max;

		lines = data->list;
		len = 0;
		if( header && *header ) len += strlen( header ) + 2;
		for( i = 0; i < data->count; ++i ){
			if( (s = lines[i]) && *s ){
				len += strlen(s) + 1 + prefix_len;
			}
		}
		++len;
		if( len > buffer_len ){
			if( buffer ){
				free( buffer );
			}
			/* now we allocate a buffer */
			malloc_or_die( buffer, len );
			buffer_len = len;
			copy->max = len;
			copy->list = (void *)buffer;
		}
		buffer[0] = 0;
		if( header && *header ){
			plp_snprintf( buffer, buffer_len, "%s\n", header );
		}
		for( i = 0; i < data->count; ++i ){
			if( (s = lines[i]) && *s ){
				len = strlen( buffer );
				plp_snprintf( buffer+len, buffer_len - len,
					"%s%s\n", prefix, s );
			}
		}
	}
	return(buffer);
}
