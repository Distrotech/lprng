/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: jobcontrol.c
 * PURPOSE: read and write the spool queue control file
 **************************************************************************/

static char *const _id =
"$Id: jobcontrol.c,v 3.3 1996/08/31 21:11:58 papowell Exp papowell $";

#include "lp.h"
#include "lp_config.h"
#include "printcap.h"
#include "pr_support.h"
#include "decodestatus.h"

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
#define MOVE       5
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

static struct keywords status_key[] = {
{ "active", INTEGER_K, (void *)0, ACTIVE },
{ "copies", INTEGER_K, (void *)0, COPIES },
{ "copy_done", INTEGER_K, (void *)0, COPY_DONE },
{ "dest", INTEGER_K, (void *)0, DEST },
{ "done", INTEGER_K, (void *)0, DONE },
{ "end", INTEGER_K, (void *)0, END },
{ "error", STRING_K, (void *)0, ERROR },
{ "hold", INTEGER_K, (void *)0, HOLD },
{ "ident", INTEGER_K, (void *)0, IDENT },
{ "move", INTEGER_K, (void *)0, MOVE },
{ "priority", INTEGER_K, (void *)0, PRIORITY },
{ "redirect", STRING_K, (void *)0, REDIRECT },
{ "remove", INTEGER_K, (void *)0, REMOVE },
{ "route", INTEGER_K, (void *)0, ROUTE },
{ "routed", INTEGER_K, (void *)0, ROUTED },
{ "status", INTEGER_K, (void *)0, STATUS },

{ 0 }
};

#define status_key_len (sizeof( status_key )/sizeof( status_key[0] ))

static char *Hold_file_pathname( struct control_file *cfp );


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
	DEBUG9("Find_key: find '%s'", key );
	while( top >= bottom ){
		mid = (top+bottom)/2;
		compare = strcasecmp( keys[mid].keyword, key );
		/* DEBUG9("Find_key: top %d, bottom %d, mid='%s'",
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
 *  destination_lines fields.  This information has the form
 *  route <key> value.  The various values are used to set entries in
 *  the destination_list fields.
 ***************************************************************************/

static void get_destination( struct control_file *cfp )
{
	struct destination *destination, *d;
	int i, j, dest_start;
	char *s, *t;
	struct keywords *key;

	/* clear out the destination list */
	cfp->destination_list.count = 0;
	/* check to see if there is information */
	if( cfp->destination_lines.count > 0 ){
		/* extend the list if necessary */
		if( cfp->destination_list.count >= cfp->destination_list.max ){
			extend_malloc_list( &cfp->destination_list,
				sizeof( struct destination), 10 );
		}
		destination = (void *)cfp->destination_list.list;
		destination = &destination[cfp->destination_list.count];
		memset( destination, 0, sizeof( destination[0] ) );
		dest_start = 0;
		for( i = 0; i < cfp->destination_lines.count; ++i ){
			s = cfp->destination_lines.list[i];
			/* DEBUG6("get_destination: checking '%s'", s ); */
			if( isupper( *s ) ){
				/* forget about lines starting with upper case */
				continue;
			}
			j = 0;
			t = strpbrk( s, " \t");
			if( t ){
				j = *t; *t = 0;
			}
			key = Find_key( status_key, status_key_len, s );
			if( t ){
				*t = j;
			} else{
				t = "";
			}
			if( key ){
				while( isspace( *t ) ) ++t;
				/* DEBUG6("get_destination: found key '%s'='%s'", 
					key->keyword, t ); */
				switch( key->maxval ){
				default:
					*s = 0;
					break;
				case ROUTE: break;
				case IDENT: safestrncpy( destination->identifier,t); break;
				case DEST: safestrncpy( destination->destination,t); break;
				case ERROR: safestrncpy( destination->error,t); break;
				case COPIES: destination->copies = atoi( t ); break;
				case COPY_DONE: destination->copy_done = atoi( t ); break;
				case STATUS: destination->status = atoi( t ); break;
				case ACTIVE: destination->active = atoi( t ); break;
				case DONE: destination->done = atoi( t ); break;
				case HOLD: destination->hold = atoi( t ); break;
				case END:
					destination->arg_start = dest_start;
					destination->arg_count = i - dest_start;
					if( destination->destination[0] &&
						++cfp->destination_list.count >=
							cfp->destination_list.max ){
						extend_malloc_list( &cfp->destination_list,
							sizeof( struct destination), 10 );
					}
					destination = (void *)cfp->destination_list.list;
					destination = &destination[cfp->destination_list.count];
					memset( destination, 0, sizeof( destination[0] ) );
					dest_start = i+1;
					break;
				}
			}
			*s = 0;
		}
	}
	destination = (void *)cfp->destination_list.list;
	for( i = 0; i < cfp->destination_list.count; ++i ){
		d = &destination[i];
		if( d->identifier[0] == 0 ){
			safestrncpy( d->identifier, cfp->identifier );
			j = strlen( d->identifier );
			plp_snprintf( d->identifier+j,
				sizeof(d->identifier)-j, ".%d", i+1 );
		}
	}
}

/***************************************************************************
 * Get_job_control( struct control_file )
 *	Get the job control file from the spool directory and
 *     decode the information in the file
 *  This will read the job control file into a buffer, and then
 *  parse it,  line by line.  Lines which have 'routing' information
 *  will then be parsed to find the various job destinations.
 ***************************************************************************/

int Get_job_control( struct control_file *cfp )
{
	struct stat statb;
	long value;
	char *s, *t, *end, *buffer;
	int i, fd, len;
	struct keywords *key;

	s = Hold_file_pathname( cfp );
	fd = Checkread( s, &statb );
	DEBUG5("Get_job_control: file '%s', fd %d, Auto_hold %d", s, fd, Auto_hold);
	if( fd < 0 && Auto_hold ){
		cfp->hold_time = time( (void *)0 );
	}
	/* if no change, forget about reading it */
	if( fd < 0 || (cfp->hstatb.st_ctime == statb.st_ctime) ){
		if( fd >= 0 ) close( fd );
		return( 0 );
	}

	/* we have to read the new status, clear the old */

	cfp->destination_lines.count = 0;
	cfp->destination_list.count = 0;

	cfp->priority_time = 0;
	cfp->hold_time = 0;
	cfp->remove_time = 0;
	cfp->move_time = 0;
	cfp->done_time = 0;
	cfp->routed_time = 0;
	cfp->active = 0;
	cfp->redirect[0] = 0;
	/* we do NOT clear out error 
	 * cfp->error[0] = 0;
	 */

	/* get the current fd status */
	cfp->hstatb = statb;

	len = roundup_2(statb.st_size+1,8);
	if( cfp->status_file.list == 0 || cfp->status_file.size < len ){
		buffer = (void *)cfp->status_file.list;
		if( buffer ){
			free( buffer );
		}
		cfp->status_file.size = len; 
		cfp->status_file.max = 1; 
		if( (buffer = malloc( len )) == 0 ){
			logerr_die( LOG_ERR,
				"Get_job_control: malloc %d failed", len );
		}
		cfp->status_file.list = (void *)buffer;
		DEBUG9("Get_job_control: malloc status_file 0x%x, len %d",
			buffer,len);
	}
	buffer = (void *)cfp->status_file.list;
	DEBUG9("Get_job_control: buffer 0x%x, len %d", buffer,
		cfp->status_file.size );
	/* get the values from the file */
	for( s = buffer, len = statb.st_size;
		len > 0 && (i = read( fd, s, len)) > 0;
		len -= i, s += i );
	*s = 0;
	close(fd);

	DEBUG9("Get_job_control: '%s'", buffer );
	for( s = buffer; s && *s; s = end ){
		end = strchr( s, '\n' );
		/* remove leading white space */
		if( end ){
			t = end;
			*end++ = 0;
		} else {
			t = s + strlen(s);
		}
		/* remove leading and trailing white space */
		while( *s && isspace( *s ) ) ++s;
		while( --t >= s && isspace( *t ) ) *t = 0; 
		/* throw away blank lines */
		if( *s == 0 ) continue;
		/* find the key */
		if( (t = strpbrk( s, " \t")) ) *t++ = 0;
		/* skip parameter leading white space */
		if( t ) while( isspace( *t ) ) ++t;
		DEBUG9("Get_job_control: '%s' '%s'", s,t );
		key = Find_key( status_key, status_key_len, s );
		if( key->keyword ){
			value = 0;
			if( t && *t ) value = strtol( t, (void *)0, 0 );
			DEBUG9("Get_job_control: found '%s' '%s' value %d",
				s, t, value );
			switch( key->maxval ){
			case HOLD: cfp->hold_time = value; break;
			case PRIORITY: cfp->priority_time = value; break;
			case REMOVE: cfp->remove_time = value; break;
			case MOVE: cfp->move_time = value; break;
			case DONE: cfp->done_time = value; break;
			case ROUTED: cfp->routed_time = value; break;
			case ACTIVE: cfp->active = value; break;
			case REDIRECT:
				if( t && *t ){
					strncpy( cfp->redirect, t, sizeof( cfp->redirect) );
				} else {
					cfp->redirect[0] = 0;
				}
				break;
			case ERROR:
				if( t && *t && cfp->error[0] == 0 ){
					strncpy( cfp->error, t, sizeof( cfp->error) );
				}
				break;
			case ROUTE:
				if( cfp->destination_lines.count + 1
						>= cfp->destination_lines.max ){
					extend_malloc_list( &cfp->destination_lines,
						sizeof( char *), 100 );
				}
				cfp->destination_lines.list[cfp->destination_lines.count++] = t;
				cfp->destination_lines.list[cfp->destination_lines.count] = 0;
				DEBUG4("Get_job_control: route line '%s'", t );
				break;
			default: break;
			}
		}
	}
	DEBUG4("Get_job_control: hold 0x%x, priority 0x%x, remove 0x%x",
		cfp->hold_time, cfp->priority_time, cfp->remove_time );
	if( Debug>4 ){
		logDebug( "Get_job_control: destination_lines %d", cfp->destination_lines.count );
		for( i = 0; i < cfp->destination_lines.count; ++i ){
			logDebug("  [%d] '%s'", i, cfp->destination_lines.list[i] );
		}
	}
	/* now get the destination information */
	get_destination( cfp );
	if( Debug>8 ) dump_control_file( "Get_job_control", cfp );
	return( 1 );
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
	DEBUG8("Set_job_control: dest line '%s'", buffer );
	write_line( fd, buffer, file );
}

static void write_route_int( int fd, char *header, int val, char *file )
{
	char buffer[SMALLBUFFER];
	plp_snprintf( buffer, sizeof(buffer)-2, "route %s %d", 
		header, val );
	DEBUG8("Set_job_control: dest line '%s'", buffer );
	write_line( fd, buffer, file );
}

int Set_job_control( struct control_file *cfp )
{
	struct stat statb;
	char buffer[SMALLBUFFER];
	char *s, *t, *hold_file;
	char **lines;
	int lock, create;
	int i, j, value, fd;
	struct destination *destination, *d;
	char thold_file[MAXPATHLEN];

	hold_file = Hold_file_pathname( cfp );
	safestrncpy( thold_file, hold_file );
	safestrncat( thold_file, "_" );
	DEBUG8("Set_job_control: file '%s'", thold_file );
	if(Debug>9){
		dump_control_file("Set_job_control",cfp);
	}
	fd = Lockf( thold_file, &lock, &create, &statb );
	if( fd < 0 ){
		logerr( LOG_ERR,
			"Set_job_control: cannot create hold file '%s'",thold_file);
		return( 1 );
	}
	if( lock == 0 ){
		DEBUG8("Set_job_control: waiting for lock" );
		/* we want to lock the file */
		lock = Do_lock( fd, thold_file, 1 );
	}
	if( lock <= 0 ){
		Errorcode = JABORT;
		logerr_die( LOG_ERR,
			"Set_job_control: cannot lock file '%s'",thold_file);
	}
	if( ftruncate( fd, 0 ) < 0 ){
		Errorcode = JABORT;
		logerr_die( LOG_ERR,
			"Set_job_control: cannot truncate hold file '%s'",thold_file);
	}
	value = 0;
	for( i = 0; status_key[i].keyword ; ++i ){
		t = 0;
		switch( status_key[i].maxval ){
		case HOLD:		value = cfp->hold_time; break;
		case PRIORITY:	value = cfp->priority_time; break;
		case REMOVE:	value = cfp->remove_time; break;
		case ACTIVE:	value = cfp->active; break;
		case MOVE:		value = cfp->move_time; break;
		case DONE:		value = cfp->done_time; break;
		case ROUTED:	value = cfp->routed_time; break;
		case REDIRECT:	t = cfp->redirect; if(t==0) t=""; break;
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
			DEBUG8("Set_job_control: '%s'", buffer );
			write_line( fd, buffer, thold_file );
		}
	}
	if( cfp->routed_time && cfp->destination_list.count > 0 ){
		destination = (void *)cfp->destination_list.list;
		for( i = 0; i < cfp->destination_list.count; ++i ){
			d = &destination[i];
			if( d->destination[0] == 0 ) continue;
			lines = &cfp->destination_lines.list[d->arg_start];
			for( j = 0; j < d->arg_count; ++j ){
				if( (s = lines[j])[0] ){
					plp_snprintf( buffer, sizeof(buffer)-2, "route %s", s );
					DEBUG8("Set_job_control: dest line '%s'", buffer );
					write_line( fd, buffer, thold_file );
				}
			}
			write_route_str( fd, "dest", d->destination, thold_file );
			write_route_str( fd, "ident", d->identifier, thold_file );
			write_route_str( fd, "error", d->error, thold_file );
			write_route_int( fd, "copies", d->copies, thold_file );
			write_route_int( fd, "copy_done", d->copy_done, thold_file );
			write_route_int( fd, "status", d->status, thold_file );
			write_route_int( fd, "active", d->active, thold_file );
			write_route_int( fd, "attempt", d->attempt, thold_file );
			write_route_int( fd, "done", d->done, thold_file );
			write_route_int( fd, "hold", d->hold, thold_file );
			write_route_str( fd, "end", "", thold_file );
		}
	}
	close( fd );
	if( rename( thold_file, hold_file ) ){
		Errorcode = JABORT;
		logerr_die( LOG_ERR, "rename of '%s' to '%s' failed - %s",
			thold_file, hold_file, Errormsg( errno ) );
	}
	return( 0 );
}

/***************************************************************************
 * int Remove_job_control( struct control_file *cfp )
 *  unlink the job control file;
 *  return 1 if success, 0 if failed
 ***************************************************************************/
int Remove_job_control( struct control_file *cfp )
{
	struct stat statb;
	char *s;

	s = Hold_file_pathname( cfp );
	/* remove it and then check it was removed */
	unlink(s);
	return( stat( s, &statb ) != -1 );
}


/***************************************************************************
 * char *Hold_file_pathname( struct control_file *cfp )
 *  get the hold file name for the job and put it in
 *  the control file.
 ***************************************************************************/
char *Hold_file_pathname( struct control_file *cfp )
{
	struct dpathname dpath;
	char *s;
	/*
	 * get the hold file pathname
	 */
	if( cfp->hold_file == 0 ){
		dpath = *CDpathname;
		s = Add_path( &dpath, cfp->name );
		s[dpath.pathlen] = 'h';
		cfp->hold_file = add_buffer( &cfp->control_file, strlen(s)+1 );
		strcpy( cfp->hold_file, s );
	}
	DEBUG7("Hold_file_pathname: '%s' -> '%s'", cfp->name, cfp->hold_file );
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
	int err = 0;
	char *command = Routing_filter;
	int p[2];
	char buffer[SMALLBUFFER];
	char *block = 0;
	int len, count, size;
	char *s, *t, *end;

	DEBUG6("Get_route: %s", command );

	if( command ) while( isspace( *command ) ) ++command;
	if( command && *command == '|' ) ++command;
	if( command ) while( isspace( *command ) ) ++command;
	if( command == 0 || *command == 0 ){
		goto done;
	}

	if( pipe(p) < 0 ){
		Errorcode = JABORT;
        logerr_die( LOG_ERR, "Get_route: pipe failed" );
	}
	DEBUG6("Get_route: pipe fd [%d, %d]", p[0], p[1] );
	Make_filter( 'f', cfp, &As_fd_info, command, 0, 0,
		p[1], (void *)0, (void *)0, 0, 0 );
	(void)close(p[1]);
	p[1] = -1;
	if( As_fd_info.input > 0 ){
		close( As_fd_info.input );
		As_fd_info.input = -1;
	}

	/* now we read from the filter */
	
	len = 0;
	block = (void *)cfp->status_file.list;
	if( block ) *block = 0;
	while( (count = read( p[0], buffer, sizeof(buffer) - 1 ) ) > 0 ){
		buffer[count] = 0;
		DEBUG6("Get_route: route '%s'", buffer );
		size = count + len + 1;
		block = (void *)cfp->status_file.list;
		if( size > cfp->status_file.size ){
			cfp->status_file.size = size = roundup_2(size,9);
			if( block ){
				if( (block = realloc( block, size )) == 0 ){
				logerr_die( LOG_ERR, "Get_route: realloc %d failed", size );
				}
			} else {
				if( (block = malloc( size )) == 0 ){
				logerr_die( LOG_ERR, "Get_route: malloc %d failed", size );
				}
			}
			cfp->status_file.list = (void *)block;
		}
		strcpy(block+len, buffer );
		len += count;
	}
	if( count < 0 ){
		logerr_die( LOG_ERR, "Get_route: read failed" );
	}

	close( p[0] );
	p[0] = -1;

	err = Close_filter( &As_fd_info, 1 );
	DEBUG6("Get_route: filter exit status %s", Server_status(err) );
	if( err ){
		plp_snprintf( cfp->error, sizeof(cfp->error),
			"no routing information - status %s", Server_status( err ) );
		goto done;
	}

	/* now we parse the lines */

	DEBUG6("Get_route: route info '%s'", block?block:"<NULL>" );

	cfp->destination_lines.count = 0;
	for( s = block; s && *s; s = end ){
		end = strchr( s, '\n' );
		if( end ){
			*end++ = 0;
		}
		while( isspace(*s) ) ++s;
		t = s+strlen(s);
		while( --t >= s && isspace( *t ) ) *t = 0;
		if( *s == 0 || *s == '#' ) continue;
		if( cfp->destination_lines.count >= cfp->destination_lines.max ){
			extend_malloc_list( &cfp->destination_lines, sizeof( char *), 100 );
		}
		cfp->destination_lines.list[cfp->destination_lines.count++] = s;
	}
	get_destination( cfp );
	if( cfp->destination_list.count > 0 ){
		cfp->routed_time = time( (void *)0 );
	}
	DEBUG6("Get_route: destination count %d", cfp->destination_list.count );
	if( Debug>8 ) dump_control_file( "Get_route", cfp );

done:
	return( err );
}
