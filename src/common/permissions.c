/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: perms_.c
 * PURPOSE: Process a perms_ file
 **************************************************************************/

static char *const _id =
"$Id: permissions.c,v 3.3 1996/08/31 21:11:58 papowell Exp papowell $";

#include "lp.h"
#include "lp_config.h"
#include "printcap.h"
#include "permission.h"
#include "globmatch.h"

/***************************************************************************

Commentary:
Patrick Powell Sun Apr 23 15:21:29 PDT 1995

Permission files and perm_ entry:

A perm_ entry is contains all the information associated
with a particular printer.  The data structures in include/perm_.h
are used to process perm_ information

 ***************************************************************************/

/***************************************************************************
 * void Get_perms( char *name, struct perm_file *perms, char *path )
 * Read perm_ information from a colon separated set of files
 *   1. break path up into set of path names
 *   2. if a filter in the list, send the filter 'name' and read
 *      from filter into memory
 *   3. if a file read the perm information into memory
 *   4. parse the perm_ informormation
 *
 ***************************************************************************
 * int Read_perms( struct perm_file *perms, char *file, int fd,
 *	struct stat *statb )
 *  - get the size of file
 *  - expand perm buffer
 *  - read file into expanded buffer
 *  - parse file
 *
 ***************************************************************************
 * int Buffer_perms( struct perm_file *perms, char *file, char *buffer )
 *  - insert a buffer into the perm_ information
 *  - parse the added permission information
 *
 ***************************************************************************/

/***************************************************************************
 * void Get_perms(char *name, struct perm_file *perms, char *path)
 * Read perm_ information from a colon separated set of files
 *   1. break path up into set of path names
 *   2. read the perm_ information into memory
 *   3. parse the perm_ informormation
 ***************************************************************************/
static int parse_perms( struct perm_file *perms, char *file, char *buffer );
void dump_perm_file( char *title,  struct perm_file *perms );
void dump_perm_check( char *title,  struct perm_check *check );
static void doinclude( struct perm_file *perms, char *file );
char *perm_str( int val );
static int Read_perms( struct perm_file *perms, char *file, int fd,
	struct stat *statb );

void Get_perms( char *name, struct perm_file *perms, char *path )
{
	char *end;
	int fd, c;
	struct stat statb;
	static char *pathname;
	int err;

	if( pathname ){
		free( pathname );
		pathname = 0;
	} 

	if( path && *path ){
		pathname = safestrdup( path );
	}

	DEBUG0("Get_perms: %s", pathname );

	for( path = pathname; path && *path; path = end ){
		end = strpbrk( path, ",;:" );
		if( end ){
			*end++ = 0;
		}
		while( isspace(*path) ) ++path;
		if( (c = *path) == 0 ) continue;
		if( Verbose || Debug > 0 ){
			logDebug( "Get_perms: permissions file '%s'", path );
		}

		switch( c ){
			case '/':
				fd =  Checkread( path, &statb );
				err = errno;
				DEBUG8("Get_perms: file '%s', size %d", path, statb.st_size );
				if( ( Verbose || Debug > 0 ) && fd < 0 ){
					logDebug( "Cannot open '%s' - %s", path, Errormsg(err) );
				}
				if( fd >= 0 && Read_perms( perms, path, fd, &statb ) ){
					DEBUG0( "Error reading %s, %s", path, Errormsg(err) );
				}
				close(fd);
				break;
			case '|':
				if( name ){
					Filter_perms( name, perms, path+1 );
				}
				break;
			default:
				log( LOG_ERR, "Get_perms: permission entry '%s' invalid", path );
			break;
		}
	}
	if( Debug > 6 ){
		char msg[64];
		plp_snprintf( msg, sizeof(msg), "Get_perms: all perms database" );
		dump_perm_file( msg, perms );
	}
}

/***************************************************************************
 * int Read_perms( struct perm_file *perms, char *file, int fd )
 *  - get the size of file
 *  - expand perm_ buffer
 *  - read file into expanded buffer
 *  - parse file
 ***************************************************************************/
static int Read_perms( struct perm_file *perms, char *file, int fd,
	struct stat *statb )
{
	int i, len;				/* ACME Integers and pointers */
	char *begin;			/* beginning of data */
	char *s;				/* ACE, cheaper and better */

	/* malloc data structures */
	
	DEBUG8("Read_perm: file '%s' size %d", file, statb->st_size );
	begin = add_buffer( &perms->buffers, statb->st_size );
	DEBUG8("Read_perms: buffer 0x%x", begin );
	s = begin;

	for( len = 1, i = statb->st_size;
		i > 0 && (len = read( fd, s, i)) > 0;
			i -= len, s += len );
	if( len <= 0 ){
		logerr( LOG_ERR, "Read_perm: cannot read '%s'", file );
		close( fd );
		return( -1 );
	}
	return( parse_perms( perms, file, begin ) );
}


/***************************************************************************
 * int Filter_perms( char *name, perm_file *perms, char *filter )
 *  - get information from the filter
 *  - expand perm buffer
 *  - read file into expanded buffer
 *  - parse file
 ***************************************************************************/
int Filter_perms( char *name, struct perm_file *perms, char *filter )
{
	char *buffer;

	buffer = Filter_read( name, &perms->buffers, filter );
	return( parse_perms( perms, filter, buffer ) );
}

/***************************************************************************
 * int Buffer_perms( struct perm_file *perms, char *file, char *buffer )
 *  - insert a buffer into the perm_ information
 *  - parse the added perm_ information
 ***************************************************************************/

int Buffer_perms( struct perm_file *perms, char *file, char *buffer )
{
	char *begin;
	int len;

	/* we expand the existing perm_ file */

	len = strlen( buffer );
	DEBUG8("Buffer_perms: file '%s', len", file, len );
	begin = add_buffer( &perms->buffers, len );
	strcpy( begin, buffer );
	return( parse_perms( perms, file, begin ) );
}

/***************************************************************************
 * int  parse_perms( struct perm_file *perms, char *file, char *begin )
 *  - parse the perm_ information in a buffer
 *  - this uses a state based parser
 *     see Commentary at start of file
 ***************************************************************************/

#define PAIR(X) { #X, INTEGER_K, (void *)0, X }
static struct keywords permwords[] = {
PAIR(REJECT),
PAIR(ACCEPT),
PAIR(NOT),
PAIR(SERVICE),
PAIR(USER),
PAIR(HOST),
PAIR(IP),
PAIR(PORT),
PAIR(REMOTEHOST),
PAIR(REMOTEIP),
PAIR(PRINTER),
PAIR(DEFAULT),
PAIR(FORWARD),
PAIR(SAMEUSER),
PAIR(SAMEHOST),
PAIR(CONTROLLINE),
PAIR(GROUP),
PAIR(SERVER),
PAIR(REMOTEUSER),
{0}
};

char *perm_str( int val )
{
	struct keywords *key;
	static char msg[32];
	for( key = permwords; key->keyword && key->maxval != val; ++key );
	if( key->keyword ){
		return( key->keyword );
	}
	plp_snprintf( msg, sizeof(msg), "%d", val );
	return( msg );
}

static int parse_perms( struct perm_file *perms, char *file, char *buffer )
{
	int status = 0;
	char *s, *t, *end, *endval, *endfield, **list;
	int c, i;
	struct perm_line *permline;	/* permissions line */
	struct perm_val *permval;	/* permission value */

	/* first, scan all lines looking for entry */
	DEBUG9( "parse_perms: '%s'", buffer );
	for( s = buffer; s && *s; s = end ){
		end = strchr( s, '\n' );
		if( end ){
			*end++ = 0;
		}
		while( (c = *s ) && isspace( c ) ) ++s;
		if( c == 0 || c == '#' ){
			goto newline;
		}

		/* now take this entry and split it up into fields */
		if( perms->lines.count >= perms->lines.max ){
			extend_malloc_list( &perms->lines, sizeof( permline[0] ), 10 );
		}

		DEBUG9( "parse_perms: count %d, line '%s'", perms->lines.count,s );
		permline = (void *)perms->lines.list;
		permline = &permline[perms->lines.count++];
		permline->name = permline->flag = 0;
		permline->options.count = 0;
		for( ; s && *s; s = endval ){
			while( (c = *s ) && isspace( c ) ) ++s;
			if( c == 0 || c == '#' ){
				goto newline;
			}
			endval = strpbrk( s, " \t" );
			if( endval ){
				*endval++ = 0;
			}

			/* check for include files */
			if( strcasecmp( s, "include" ) == 0 ){
				--perms->lines.count;
				doinclude( perms, endval );
				goto newline;
			}

			/* put each field in a perm entry */
			if( permline->options.count >= permline->options.max ){
				extend_malloc_list( &permline->options,sizeof(permval[0]),10);
			}
			permval = (void *)permline->options.list;
			permval = &permval[permline->options.count++];
			permval->values.count = 0;
			permval->key = 0;
			permval->token = s;
			if( (t = strchr( s, '=' )) ){
				*t++ = 0;
			}
			/* check for the control file line selection form */
			if( strlen(s) == 1 && isalpha(*s) ){
				permval->key = CONTROLLINE;
				if( islower(*s) ) *s = toupper( *s );
			} else {
				for( i = 0;
					permwords[i].keyword && strcasecmp(permwords[i].keyword,s);
					++i );
				if( permwords[i].keyword ){
					permval->key = permwords[i].maxval;
				} else {
					log( LOG_ERR, "parse_perms: file '%s' bad key '%s'",
						file, s );
					continue;
				}
			}
			for(s = t; s && *s; s = endfield ){
				while( (c = *s ) && isspace( c ) ) ++s;
				endfield = strchr( s, ',' );
				if( endfield ){
					*endfield++ = 0;
				}
				if( c == 0 || c == '#' ){
					goto newline;
				}

				if( permval->values.count >= permval->values.max ){
					extend_malloc_list( &permval->values,sizeof(char *),10);
				}
				list = (void *)permval->values.list;
				list = &list[permval->values.count++];
				*list = s;
			}
		}
newline: ;
	}
	
	if( Debug>9 ){
		char msg[64];
		plp_snprintf( msg, sizeof(msg), "parse_perms: file '%s'", file );
		dump_perm_file( msg, perms );
	}
	return( status );
}

/***************************************************************************
 * char *doinclude( struct perm_file *perms, *	char *file );
 * 1. check the nesting depth; if too deep report error
 * 2. call Get_perms() to parse file
 ***************************************************************************/
static int depth;
static void doinclude( struct perm_file *perms, char *file )
{
	if( ++depth  > 10 ){
		fatal( LOG_ERR, "doinclude: perm_ file nesting too deep '%s'",
			file );
	}
	Get_perms( (void *)0, perms, file );
	--depth;
}

/***************************************************************************
 * dump_perm_file( char *title, struct perm_file *cf )
 * Dump perm_file information
 ***************************************************************************/

void dump_perm_line( char *title,  struct perm_line *perms )
{
	int i, j;
	struct perm_val *val;
	char **s;
	if( perms ){
		logDebug( "%s name %d, flag %d", title?title:"",
			perms->name, perms->flag );
		val = (void *)perms->options.list;
		for( i = 0; i < perms->options.count; ++i ){
			logDebug( "  option [%3d] (%s)='%s', values count %d",
				i, perm_str( val[i].key ), val[i].token, val[i].values.count );
			s = (void *)val[i].values.list;
			for( j = 0; j < val[i].values.count; ++j ){
				logDebug( "    value [%3d] '%s'", j, s[j] );
			}
		}
	}
}

void dump_perm_file( char *title,  struct perm_file *perms )
{
	int i;
	char buff[32];
	struct perm_line *line;

	if( title ) logDebug( "*** perm_file %s ***", title );
	if( perms ){
		line = (void *)perms->lines.list;
		for( i = 0; i < perms->lines.count; ++i ){
			plp_snprintf( buff, sizeof(buff), "[%d] ", i );
			dump_perm_line( buff, &line[i] );
		}
	}
}

/***************************************************************************
 * dump_perm_check( char *title, struct perm_check *check )
 * Dump perm_check information
 ***************************************************************************/

void dump_perm_check( char *title,  struct perm_check *check )
{
	if( title ) logDebug( "*** perm_check %s ***", title );
	if( check ){
		logDebug(
			"  user '%s', rmtuser '%s', host '%s', rmthost '%s'",
			check->user, check->remoteuser, check->host, check->remotehost );
		logDebug(
 			" IP 0x%x,  rmtIP 0x%x, port %d",
			check->ip, check->remoteip, check->port );
		logDebug( "  printer '%s', service '%c'",
			check->printer, check->service );
	}
}

/***************************************************************************
 * Free_perms( struct perm_file *perms )
 *  Free all of the memory in the permission buffer
 ***************************************************************************/
void Free_perms( struct perm_file *perms )
{
	int i, j;
	struct perm_line *line;
	struct perm_val *val;
	if( perms ){
		line = (void *)perms->lines.list;
		for( i = 0; i < perms->lines.count; ++i ){
			val = (void *)line->options.list;
			for( j = 0; j < line->options.count; ++j ){
				clear_malloc_list( &val[j].values, 0 );
			}
			clear_malloc_list( &line[i].options, 0 );
		}
		clear_malloc_list( &perms->lines, 0 );
		clear_malloc_list( &perms->buffers, 1 );
	}
}


/***************************************************************************
 * Perms_check( struct perm_file *perms, struct perm_check );
 * - run down the list of permissions
 * - do the check on each of them
 * - if you get a distinct fail or success, return
 * 1. the NOT field inverts the result of the next test
 * 2. if one test fails,  then we go to the next line
 * 3. The entire set of tests is accepted if all pass, i.e. none fail
 ***************************************************************************/
static int match( struct perm_val *val, char *str, int invert );
static int match_ip( struct perm_val *val, unsigned long ip, int invert );
static int match_host( struct perm_val *val, char *host, int invert );
static int match_range( struct perm_val *val, int port, int invert );
static int match_char( struct perm_val *val, int value, int invert );
static int match_group( struct perm_val *val, char *str, int invert );

static int Default_perm;

void Init_perms_check()
{
	int i;
	static char *def;

	if( Default_permission != def ){
		def = Default_permission;
		Default_perm = 0;
		if( def ){
			for( i = 0;
				permwords[i].keyword
					&& strcasecmp(permwords[i].keyword, def );
				++i );
			Default_perm = permwords[i].maxval;
			switch( Default_perm ){
				case ACCEPT:
				case REJECT:
					break;
				default:
					fatal( LOG_ERR, "Init_perms_check: bad default perms '%s'",
						Default_permission );
					Default_perm = 0;
					break;
			}
		}
	}
	Last_default_perm = Default_perm;
	DEBUG4("Init_perms_check: Last_default'%s', Default_perm '%s'",
		perm_str( Last_default_perm ), perm_str( Default_perm ) );
}


int Perms_check( struct perm_file *perms, struct perm_check *check,
	struct control_file *cf )
{
	int i, j, c;
	int invert = 0;
	int result, m;
	struct perm_line *lines, *line;
	struct perm_val *val;
	char *s;					/* string */

	if( Debug > 4 ){
		dump_perm_check( "Perms_check", check );
	}
	if( perms ){
		lines = (void *)perms->lines.list;
		for( i = 0; i < perms->lines.count; ++i ){
			line = &lines[i];

			/* skip no entry line */
			if( line->options.count == 0 ) continue;
			DEBUG4("Perms_check: current default %s, line %d",
				perm_str( Last_default_perm ), i );
			if(Debug>4)dump_perm_line("Perms_check:", line);

			val = (void *)line->options.list;
			/* clear invert after one test */
			invert = 0;
			result = 0;
			m = -1;
			for( j = 0; j < line->options.count; ++j ){
				if( invert > 0 ){
					invert = -1;
				} else {
					invert = 0;
				}
				DEBUG5("Perms_check: before key '%s', result = '%s'",
					 perm_str(val[j].key), perm_str( result ) );
				switch( val[j].key ){
				case NOT:
					invert = 1;
					continue;
				case REJECT: result = REJECT; m = 0; break;
				case ACCEPT: result = ACCEPT; m = 0; break;
 				case USER:
					m = 1;
 					switch (check->service){
 					case 'X': break;
					default:
 						m = match( &val[j], check->user, invert );
 						break;
 					}
 					break;
				case HOST:
					m = 1;
 					switch (check->service){
 					case 'X':
						m = match_host( &val[j], check->remotehost, invert );
						break;
 					default:
						m = match_host( &val[j], check->host, invert );
						break;
					}
 					break;
				case GROUP:
					m = 1;
 					switch (check->service){
 					case 'X': break;
					default:
						m = match_group( &val[j], check->user, invert );
 						break;
 					}
 					break;
				case IP:
					m = 1;
 					switch (check->service){
					case 'X':
						m = match_ip( &val[j], check->remoteip, invert );
						break;
					default:
						m = match_ip( &val[j], check->ip, invert );
						break;
					}
					break;
				case PORT:
					m = 1;
 					switch (check->service){
 					case 'X': case 'M': case 'C': case 'S':
 						m = match_range( &val[j], check->port, invert );
 						break;
					}
					break;
 				case REMOTEUSER:
					m = 1;
 					switch (check->service){
 					case 'X': break;
 					case 'M': case 'C': case 'S':
 						m = match( &val[j], check->remoteuser, invert );
						break;
 					default:
 						m = match( &val[j], check->user, invert );
 						break;
					}
					break;
				case REMOTEHOST:
					m = 1;
 					switch (check->service){
 					default:
						m = match_host( &val[j], check->remotehost, invert );
						break;
 					case 'P':
						m = match_host( &val[j], check->host, invert );
						break;
					}
 					break;
				case REMOTEIP:
					m = 1;
					switch( check->service ){
					default:
						m = match_ip( &val[j], check->remoteip, invert );
						break;
					case 'P':
						m = match_ip( &val[j], check->ip, invert );
						break;
					}
					break;
				case CONTROLLINE:
					/* check to see if we have control line */
					DEBUG8("Perms_check: CONTROLLINE %s", val[j].token);
					if( cf == 0 ){
						m = 1;
					} else {
						c = val[j].token[0];
						if( !isupper(c) ){
							fatal( LOG_ERR, "Perms_check: bad CONTROLLINE '%s'",
								val[j].token );
						}
						/* match the control file line */
						if( (s = cf->capoptions[c - 'A']) ){
							/* s points to start of line */
							m = match( &val[j], s, invert );
						}
					}
					break;
				case PRINTER:
					m = 1;
 					switch (check->service){
 					case 'X': break;
					default:
						m = match( &val[j], check->printer, invert );
						break;
					}
					break;
				case SERVICE:
					m = match_char( &val[j], check->service, invert );
					break;
				case FORWARD:
					m = 1;
 					switch (check->service){
					default: break;
					case 'R': case 'Q': case 'M': case 'C': case 'S':
						/* FORWARD check succeeds if REMOTEIP != IP */
						m = !(check->ip != check->remoteip);
						if( invert ) m = !m;
						DEBUG8(
						"Perms_check: FORWARD IP '0x%x' != rmtIP '0%x', rslt %d",
						check->ip, check->remoteip, m );
						break;
					}
					break;
				case SAMEHOST:
					m = 1;
 					switch (check->service){
					default: break;
					case 'R': case 'Q': case 'M': case 'C': case 'S':
						/* SAMEHOST check succeeds if REMOTEIP == IP */
						m = !(check->ip == check->remoteip);
						if( invert ) m = !m;
						DEBUG8(
						"Perms_check: SAMEHOST IP '0x%x' == rmtIP '0%x', rslt %d",
						check->ip, check->remoteip, m );
						break;
					}
					break;
				case SAMEUSER:
					m = 1;
 					switch (check->service){
					default: break;
					case 'Q': case 'M': case 'C': case 'S':
						/* check succeeds if remoteuser == user */
						m = safestrcmp( check->user, check->remoteuser );
						if( invert ) m = !m;
						DEBUG8(
						"Perms_check: SAMEUSER '%s' == remote '%s', rslt %d",
						check->user, check->remoteuser, m );
						break;
					}
					break;
				case SERVER:
					m = 1;
 					switch (check->service){
					default: break;
					case 'R': case 'Q': case 'M': case 'C': case 'S':
						/* check succeeds if server and server IP == IP */
						m = !(check->ip == ntohl(HostIP) );
						if( invert ) m = !m;
						DEBUG8(
						"Perms_check: SERVER ip 0x%x == HostIP 0x%x, rslt %d",
							check->ip, ntohl(HostIP), m );
						break;
					}
					break;
				case DEFAULT:
					++j;
					if( j < line->options.count ){
						switch( val[j].key ){
							case REJECT: Last_default_perm =  REJECT; break;
							case ACCEPT: Last_default_perm =  ACCEPT; break;
							default:
								log( LOG_ERR,
								"Perms_check: bad value for default: '%s'", 
								val[j].token );
								break;
						}
						DEBUG8("Perms_check: DEFAULT '%s'",
							perm_str( Last_default_perm ) );
						++j;
						if( j < line->options.count ){
							log( LOG_ERR,
							"Perms_check: extra value for default: '%s'", 
							val[j].token );
						}
						goto next_line;
					} else {
						log( LOG_ERR,
						"Perms_check: missing value for default" );
					}
					goto next_line;
				}
				/* if status is non-zero, test failed */
				DEBUG6("Perms_check: key '%s', result after '%s', match %d",
					 perm_str(val[j].key), perm_str( result ), m );
				if( m != 0 ){
					goto next_line;
				}
			}
			/* make sure at least one test done */
			if( m >= 0 && j >= line->options.count ){
				if( result == 0 ){
					result = Last_default_perm;
				}
				DEBUG4("Perms_check: result %d '%s'",
					result, perm_str( result ) );
				return( result );
			}
		next_line: ;
		}
	}
	DEBUG4("Perms_check: result 0 (no match)" );
	return( 0 );
}


/***************************************************************************
 * static int match( struct perm_val *val, char *str );
 *  returns 1 on failure, 0 on success
 *  - match the string against the list of options
 *    options are glob type regular expressions;  we implement this
 *    currently using the most crude of pattern matching
 *  - if string is null or pattern list is null, then match fails
 *    if both are null, then match succeeds
 ***************************************************************************/

static int match( struct perm_val *val, char *str, int invert )
{
 	char **list;
 	int result = 1;
 	int i;
 	DEBUG8("match: str '%s', %d entries to check", str, val->values.count );
 	list = (void *)val->values.list;
 	for( i = 0; str && result && i < val->values.count; ++i ){
 		/* now do the match */
 		result = Globmatch( list[i], str );
		DEBUG8("match: list[%d]='%s', result %d", i, list[i],  result );
	}
	if( invert ) result = !result;
 	DEBUG8("match: str '%s' final result %d", str, result );
	return( result );
}

/***************************************************************************
 * static int match_host( struct perm_val *val, char *host );
 *  returns 1 on failure, 0 on success
 *  - match the string against the list of options
 *    options are glob type regular expressions;  we implement this
 *    currently using the most crude of pattern matching
 *  - if string is null or pattern list is null, then match fails
 *    if both are null, then match succeeds
 ***************************************************************************/

static int match_host( struct perm_val *val, char *host, int invert )
{
 	char **list;
 	int result = 1;
 	int i;
 	DEBUG8("match: host '%s', %d entries to check", host, val->values.count );
 	list = (void *)val->values.list;
 	for( i = 0; host && result && i < val->values.count; ++i ){
 		/* now do the match */
		if( list[i][0] == '@' ) {	/* look up host in netgroup */
#ifdef HAVE_INNETGR
			result = !innetgr( list[i]+1, host, NULL, NULL );
#else /* HAVE_INNETGR */
			DEBUG8("match: no innetgr() call, netgroups not permitted");
#endif /* HAVE_INNETGR */
		} else {
	 		result = Globmatch( list[i], host );
		}
		DEBUG8("match: list[%d]='%s', result %d", i, list[i],  result );
	}
	if( invert ) result = !result;
 	DEBUG8("match: host '%s' final result %d", host, result );
	return( result );
}

/***************************************************************************
 * static int match_ip( struct perm_val *val )
 * check the IP address and mask
 * entry has the format:  IPADDR/MASK, mask is x.x.x.x or n (length)
 ***************************************************************************/
int ipmatch( char *val, unsigned long ip )
{
	unsigned long addr, mask, m;
	int result = 1;
	char *s;

	mask = ~0;
	s = strchr( val, '/' );
	if( s ){
		*s = 0;
	}
	addr = ntohl(inet_addr( val ));
	if( s ){
		*s = '/';
		++s;
		if( strchr(s, '.' ) ){
			m = ntohl(inet_addr( s ));
			if( m == 0 ){
				log( LOG_DEBUG, "ipmatch: IP mask format error '%s'", s );
			} else {
				mask = m;
			}
		} else {
			m = atoi( s );
			if( m <= 0 || m > 32 ){
				log( LOG_DEBUG, "ipmatch: IP mask length error '%s'", s );
			} else {
				mask = ~(mask >> m);
			}
		}
	}
	result = (mask & ( ip ^ addr ));
	DEBUG8("ipmatch: addr 0x%08x, mask 0x%08x, ip 0x%08x, result 0x%08x",
		addr, mask, ip, result );
	return( result != 0 );
}

static int match_ip( struct perm_val *val, unsigned long ip, int invert )
{
	char **list;
	int result = 1;
	int i;

	DEBUG8("match_ip: ip '0x%x'", ip );
	list = (void *)val->values.list;
	for( i = 0; ip && result && i < val->values.count; ++i ){
		/* now do the match */
		result = ipmatch( list[i], ip );
	}
	if( invert ) result = !result;
	DEBUG8("match_ip: ip '0x%x' result %d", ip, result );
	return( result );
}

/***************************************************************************
 * static int match_range( struct perm_val *val, int port );
 * check the port number and/or range
 * entry has the format:  number     number-number
 ***************************************************************************/

int portmatch( char *val, int port )
{
	int low, high, err;
	char *end;
	int result = 1;
	char *s, *t, *tend;

	err = 0;
	s = strchr( val, '-' );
	if( s ){
		*s = 0;
	}
	end = val;
	low = strtol( val, &end, 10 );
	if( end == val || *end ) err = 1;

	high = low;
	if( s ){
		tend = t = s+1;
		high = strtol( t, &tend, 10 );
		if( t == tend || *tend ) err = 1;
		*s = '-';
	}
	if( err ){
		log( LOG_ERR, "portmatch: bad port range '%s'", val );
	}
	if( high < low ){
		err = high;
		high = low;
		low = err;
	}
	result = !( port >= low && port <= high );
	DEBUG8("portmatch: low %d, high %d, port %d, result %d",
		low, high, port, result );
	return( result );
}

static int match_range( struct perm_val *val, int port, int invert )
{
	char **list;
	int result = 1;
	int i;

	DEBUG8("match_ip: ip '0x%x'", port );
	list = (void *)val->values.list;
	for( i = 0; result && i < val->values.count; ++i ){
		/* now do the match */
		result = portmatch( list[i], port );
	}
	if( invert ) result = !result;
	DEBUG8("match_ip: port '%d' result %d", port, result );
	return( result );
}

/***************************************************************************
 * static int match_char( struct perm_val *val, int value );
 * check for the character value in one of the option strings
 * entry has the format:  string
 ***************************************************************************/

static int match_char( struct perm_val *val, int value, int invert )
{
	char **list;
	int result = 1;
	int i;

	DEBUG8("match_char: value '0x%x' '%c'", value, value );
	list = (void *)val->values.list;
	for( i = 0; result && i < val->values.count; ++i ){
		/* now do the match */
		result = (strchr( list[i], value ) == 0 );
		DEBUG8("match_char: val %c, str '%s', match %d",
			value, list[i], result);
	}
	if( invert ) result = !result;
	DEBUG8("match_char: value '%c' result %d", value, result );
	return( result );
}


/***************************************************************************
 * static int match_group( struct perm_val *val, char *str );
 *  returns 1 on failure, 0 on success
 *  - get the UID for the named user
 *  - scan the listed groups to see if there is a group
 *    check to see if user is in group
 ***************************************************************************/
static int ingroup( char *group, char *user );

static int match_group( struct perm_val *val, char *str, int invert )
{
 	char **list;
 	int result = 1;
 	int i;
 	DEBUG8("match_group: str '%s'", str );
 	list = (void *)val->values.list;
 	for( i = 0; str && result && i < val->values.count; ++i ){
 		/* now do the match */
 		result = ingroup( list[i], str );
	}
	if( invert ) result = !result;
 	DEBUG8("match: str '%s' value %d", str, result );
	return( result );
}

/***************************************************************************
 * static int ingroup( char* *group, char *user );
 *  returns 1 on failure, 0 on success
 *  scan group for user name
 * Note: we first check for the group.  If there is none, we check for
 *  wildcard (*) in group name, and then scan only if we need to
 ***************************************************************************/

static int ingroup( char *group, char *user )
{
	struct group *grent;
	struct passwd *pwent;
	char **members;
	int result = 1;

	DEBUG8("ingroup: checking '%s' for membership in group '%s'", user, group);
	if( group == 0 || user == 0 ){
		return( result );
	}
	/* first try getgrnam, see if it is a group */
	if( (grent = getgrnam( group )) ){
		DEBUG8("ingroup: group id: %d\n", grent->gr_gid);
		if( (pwent = getpwnam(user)) && (pwent->pw_gid == grent->gr_gid) ){
			DEBUG8("ingroup: user default group id: %d\n", pwent->pw_gid);
			result = 0;
		} else for( members = grent->gr_mem; result && *members; ++members ){
			DEBUG8("ingroup: member '%s'", *members);
			result = (strcmp( user, *members ) != 0);
		}
	} else if( strchr( group, '*') ){
		/* wildcard in group name, scan through all groups */
		setgrent();
		while( result && (grent = getgrent()) ){
			DEBUG8("ingroup: group name '%s'", grent->gr_name);
			/* now do match against group */
			if( Globmatch( group, grent->gr_name ) == 0 ){
				DEBUG8("ingroup: found '%s'", grent->gr_name);
				for( members = grent->gr_mem; result && *members; ++members ){
					DEBUG8("ingroup: member '%s'", *members);
					result = (strcmp( user, *members ) != 0);
				}
			}
		}
		endgrent();
	} else if( group[0] == '@' ) {	/* look up user in netgroup */
#ifdef HAVE_INNETGR
		if( !innetgr( group+1, NULL, user, NULL ) ) {
			DEBUG8( "ingroup: user %s NOT in netgroup %s", user, group+1 );
		} else {
			DEBUG8( "ingroup: user %s in netgroup %s", user, group+1 );
			result = 0;
		}
#else /* HAVE_INNETGR */
		DEBUG8( "ingroup: no innetgr() call, netgroups not permitted" );
#endif /* HAVE_INNETGR */
	}
	DEBUG8("ingroup: result: %d\n", result );
	return( result );
}
