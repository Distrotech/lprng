/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: perms_.c
 * PURPOSE: Process a perms_ file
 **************************************************************************/

static char *const _id =
"permissions.c,v 3.14 1998/03/24 02:43:22 papowell Exp";

#include "lp.h"
#include "fileopen.h"
#include "globmatch.h"
#include "gethostinfo.h"
#include "malloclist.h"
#include "permission.h"
#include "setup_filter.h"
#include "dump.h"
#if defined(HAVE_ARPA_NAMESER_H)
# include <arpa/nameser.h>
#endif
#if defined(HAVE_RESOLV_H)
# include <resolv.h>
#endif
/**** ENDINCLUDE ****/

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
 ***************************************************************************/

static int Read_perms( struct perm_file *perms, char *file, int fd,
	struct stat *statb );
static int parse_perms( struct perm_file *perms, char *file, char *buffer );

/***************************************************************************
 * Free_perms( struct perm_file *perms )
 *  Free all of the memory in the permission buffer
 ***************************************************************************/
void Free_perms( struct perm_file *perms )
{
	if( perms ){
		clear_malloc_list( &perms->files, 1 );
		clear_malloc_list( &perms->filters, 1 );
		clear_malloc_list( &perms->lines, 0 );
		clear_malloc_list( &perms->values, 0 );
	}
}

/***************************************************************************
 * void Get_perms(char *name, struct perm_file *perms, char *path)
 * Read perm information from a colon separated set of files
 *   1. break path up into set of path names
 *   2. read the perm_ information into memory
 *   3. parse the perm_ informormation
 ***************************************************************************/

int Get_perms( char *name, struct perm_file *perms, char *path )
{
	char *end;
	int fd, c;
	struct stat statb;
	int err;
	char pathname[MAXPATHLEN];
	int found_perms = 0;

	safestrncpy( pathname, path );
	DEBUGF(DDB1)("Get_perms: '%s'", name );
	for( path = pathname; path && *path; path = end ){
		end = strpbrk( path, ",;:" );
		if( end ){
			*end++ = 0;
		}
		while( isspace(*path) ) ++path;
		if( (c = *path) == 0 ) continue;
		DEBUGF(DDB1)( "Get_perms: permissions file '%s'", path );

		switch( c ){
			case '/':
				DEBUGF(DDB2)("Get_perms: file '%s'", path );
				fd =  Checkread( path, &statb );
				err = errno;
				if( fd < 0 ){
					DEBUGF(DDB2)("Get_perms: cannot open '%s' - %s",
						path, Errormsg(err) );
				} else if( Read_perms( perms, path, fd, &statb ) ){
					DEBUGF(DDB1)( "Get_perms: error reading %s, %s",
						path, Errormsg(err) );
				} else {
					found_perms |= 1;
				}
				close(fd);
				break;
			case '|':
				DEBUGF(DDB2)("Get_perms: filter '%s' name '%s'", path, name );
				if( name ){
					found_perms |= !Filter_perms( name, perms, path+1 );
				}
				break;
			default:
				log( LOG_ERR, "Get_perms: permission entry '%s' invalid", path );
			break;
		}
	}
	DEBUGFC(DDB4){
		char msg[64];
		plp_snprintf( msg, sizeof(msg),
			"Get_perms: found %d, all perms database", found_perms );
		dump_perm_file( msg, perms );
	}
	return( found_perms );
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

	DEBUGF(DDB3)("Read_perm: file '%s' size %d", file, statb->st_size );
	add_str( &perms->files, file,__FILE__,__LINE__  );
	begin = add_buffer( &perms->files, statb->st_size+1,__FILE__,__LINE__ );
	DEBUGF(DDB3)("Read_perms: buffer 0x%x", begin );

	s = begin;
	for( len = 1, i = statb->st_size;
		i > 0 && (len = read( fd, s, i)) > 0;
			i -= len, s += len );
	*s = 0;
	if( len <= 0 ){
		logerr( LOG_ERR, "Read_perm: cannot read '%s'", file );
		close( fd );
		return( -1 );
	}
	DEBUGF(DDB3)("Read_perms: contents read" );
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
	int error = 1;

	DEBUGF(DDB1)("Filter_perms: filter '%s'", filter );
	add_str( &perms->filters, filter,__FILE__,__LINE__ );
	buffer = Filter_read( name, &perms->filters, filter );
	if( buffer ){
		error = parse_perms( perms, filter, buffer );
	}
	return( error );
}

/***************************************************************************
 * int  parse_perms( struct perm_file *perms, char *file, char *begin )
 *  - parse the perm_ information in a buffer
 *  - this uses a state based parser
 *     see Commentary at start of file
 ***************************************************************************/

#undef PAIR
#ifndef _UNPROTO_
# define PAIR(X) { #X, INTEGER_K, (void *)0, X }
#else
# define __string(X) "X"
# define PAIR(X) { __string(X), INTEGER_K, (void *)0, X }
#endif


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
PAIR(REMOTEGROUP),
PAIR(AUTH),
PAIR(AUTHUSER),
PAIR(FWDUSER),
PAIR(IFIP),
PAIR(AUTHTYPE),
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
	char *start;
	int c, i;
	struct perm_line *permline, *permlines;	/* permissions line */
	struct perm_val *permval, *permvals;	/* permission value */
	char **permlist;

	DEBUGF(DDB2)("parse_perms: file '%s'", file );
	if( perms->lines.max == 0 ){
		extend_malloc_list( &perms->lines, sizeof( permline[0] ), 100,__FILE__,__LINE__ );
	}
	permlines = (void *)perms->lines.list;

	if( perms->values.max == 0 ){
		extend_malloc_list( &perms->values, sizeof( permval[0] ), 10,__FILE__,__LINE__ );
	}
	if( perms->values.count == 0 ){
		memset( permlines, 0, sizeof(permlines[0]) );
		perms->values.count++;
	}
	permvals = (void *)perms->values.list;

	/* set the first entry in the list to 0 */
	if( perms->list.max == 0 ){
		extend_malloc_list( &perms->list,sizeof(char *), 10,__FILE__,__LINE__ );
	}
	permlist = (void *)perms->list.list;
	if( perms->list.count == 0 ){
		permlist[perms->list.count++] = 0;
	}
	permlist = (void *)perms->list.list;

	/* first, scan all lines looking for entry */
	DEBUGF(DDB4)( "parse_perms: '%s'", buffer );
	for( start = buffer; start && *start; start = end ){
		while( (c = *start ) && isspace( c ) ) ++start;
		if( (end = strchr( start, '\n' )) ){
			*end++ = 0;
		}
		if( c == 0 || c == '#' ){
			continue;
		}
		if( end && ((end-2) >= start) && end[-2] == '\\' ){
			end[-2] = ' ';
			end[-1]  = ' ';
			end = start;
			continue;
		}

		if( strncasecmp( start, "include", 7  ) == 0 ){
			char *s;
			static int depth;
			s = start+7;
			if( !isspace( *s ) ){
				fatal( LOG_ERR, "bad include line '%s'", start );
			}
			if( ++depth  > 10 ){
				fatal( LOG_ERR, "doinclude: perm_ file nesting too deep '%s'",
					file );
			}
			Get_perms( (void *)0, perms, s );
			--depth;
			continue;
		}

		/* now take this entry and split it up into fields */
		DEBUGF(DDB3)( "parse_perms: count %d, line '%s'", perms->lines.count,start );
		if( perms->lines.count+1 >= perms->lines.max ){
			extend_malloc_list( &perms->lines, sizeof( permline[0] ), 10,__FILE__,__LINE__ );
			permlines = (void *)perms->lines.list;
		}
		permline = &permlines[perms->lines.count++];
		memset( permline, 0, sizeof(permline[0]));
		/* set the flag for occupied */
		permline->flag = 1;

		for( s = start; s && *s; s = endval ){
			endval = strpbrk( s, " \t" );
			while( endval && isspace(*endval) ){
				*endval++ = 0;
			}
			c = *s;
			if( c == 0 ) continue;
			if( c == '#' ) break;
			/* put each field in a perm entry */
			if( perms->values.count+1 >= perms->values.max ){
				extend_malloc_list( &perms->values,sizeof(permval[0]),10,__FILE__,__LINE__ );
				permvals = (void *)perms->values.list;
			}
			permval = &permvals[perms->values.count++];
			memset( permval, 0, sizeof( permval[0] ) );
			permval->token = s;
			if( permline->list == 0 ){
				permline->list = permval-permvals;
			}

			/* now we find the set of options for each keyword */
			if( (t = strchr( s, '=' )) ){
				*t++ = 0;
			}
			trunc_str( s );
			/* check for the control file line selection form */
			if( strlen(s) == 1 && isupper(*s) ){
				permval->key = CONTROLLINE;
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
				endfield = strchr( s, ',' );
				if( endfield ){
					*endfield++ = 0;
				}
				while( (c = *s ) && isspace( c ) ) ++s;
				/* comment */
				if( c == '#' ) break;
				if( c == 0 ) continue;

				if( perms->list.count+1 >= perms->list.max ){
					extend_malloc_list( &perms->list,sizeof(char *),100,__FILE__,__LINE__ );
					permlist = (void *)perms->list.list;
				}
				list = &permlist[perms->list.count++];
				if( permval->list == 0 ) permval->list = list-permlist;
				*list = s;
			}
			if( permval->list ){
				if( perms->list.count+1 >= perms->list.max ){
					extend_malloc_list( &perms->list,sizeof(char *),100,__FILE__,__LINE__ );
					permlist = (void *)perms->list.list;
				}
				list = &permlist[perms->list.count++];
				*list = 0;
			}
		}
		if( permline->list ){
			if( perms->values.count+1 >= perms->values.max ){
				extend_malloc_list( &perms->values,sizeof(permval[0]),10,__FILE__,__LINE__ );
				permvals = (void *)perms->values.list;
			}
			permval = &permvals[perms->values.count++];
			memset( permval, 0, sizeof( permval[0] ) );
		}
	}

	DEBUGFC(DDB2){
		char msg[64];
		plp_snprintf( msg, sizeof(msg), "parse_perms: file '%s'", file );
		dump_perm_file( msg, perms );
	}
	return( status );
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
static int match_pr( struct perm_val *val, char *str, int invert );
static int match_ip( struct perm_val *val, struct host_information *host,
		int invert );
static int match_addrip( struct perm_val *val, struct sockaddr *host,
		int invert );
static int match_host( struct perm_val *val, struct host_information *host,
		int invert );
static int match_range( struct perm_val *val, int port, int invert );
static int match_char( struct perm_val *val, int value, int invert );
static int match_group( struct perm_val *val, char *str, int invert );
static int match_auth( struct perm_val *val, struct control_file *cf, int invert );
static int Default_perm;

void Init_perms_check( void )
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
	DEBUGF(DDB2)("Init_perms_check: Last_default'%s', Default_perm '%s'",
		perm_str( Last_default_perm ), perm_str( Default_perm ) );
}


static char **perm_list;
int Perms_check( struct perm_file *perms, struct perm_check *check,
	struct control_file *cf )
{
	int i, j, c;
	int invert = 0;
	int result, m;
	struct perm_line *perm_lines, *line;
	struct perm_val *perm_val, *val;
	char *s;					/* string */

	DEBUGFC(DDB3)dump_perm_check( "Perms_check", check );
	perm_lines = (void *)perms->lines.list;
	perm_val = (void *)perms->values.list;
	perm_list = perms->list.list;
	if( perms ){
		for( i = 0; i < perms->lines.count; ++i ){
			line = &perm_lines[i];

			/* skip no entry line */
			if( line->list == 0 ) continue;
			DEBUGF(DDB2)("Perms_check: current default %s, line %d",
				perm_str( Last_default_perm ), i );
			DEBUGFC(DDB3)dump_perm_line("Perms_check:", line, perms);

			val = &perm_val[line->list];
			/* clear invert after one test */
			invert = 0;
			result = 0;
			m = -1;
			for( j = 0; val[j].token ; ++j ){
				if( invert > 0 ){
					invert = -1;
				} else {
					invert = 0;
				}
				DEBUGF(DDB2)("Perms_check: before key '%s', result = '%s'",
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
						m = match_ip( &val[j], check->remotehost, invert );
						break;
					default:
						m = match_ip( &val[j], check->host, invert );
						break;
					}
					break;

				case IFIP:
					m = match_addrip( &val[j], check->addr, invert );
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
 				case REMOTEGROUP:
					m = 1;
 					switch (check->service){
 					case 'X': break;
 					case 'M': case 'C': case 'S':
 						m = match_group( &val[j], check->remoteuser, invert );
						break;
 					default:
 						m = match_group( &val[j], check->user, invert );
 						break;
					}
					break;
				case REMOTEHOST:
					m = 1;
 					switch (check->service){
 					default:
						m = match_host( &val[j], check->remotehost, invert );
						break;
					}
 					break;
				case AUTH:
					m = 1;
 					switch (check->service){
 					case 'X': break;
					default:
						m = match_auth( &val[j], cf, invert );
					}
 					break;
				case AUTHUSER:
					m = 1;
 					switch (check->service){
 					case 'X': break;
					default:
						m = match( &val[j], cf->auth_id+1, invert );
					}
					break;
				case AUTHTYPE:
					m = 1;
 					switch (check->service){
 					case 'X': break;
					default:
						m = match( &val[j], check->authtype, invert );
					}
					break;
				case FWDUSER:
					m = 1;
 					switch (check->service){
 					case 'X': break;
					default:
						m = match( &val[j], cf->forward_id+1, invert );
					}
					break;
				case REMOTEIP:
					m = 1;
					switch( check->service ){
					default:
						m = match_ip( &val[j], check->remotehost, invert );
						break;
					}
					break;
				case CONTROLLINE:
					/* check to see if we have control line */
					m = 1;
					DEBUGF(DDB3)("Perms_check: CONTROLLINE %s", val[j].token);
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
						m = match_pr( &val[j], check->printer, invert );
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
						m = !Same_host( check->host, check->remotehost );
						if( invert ) m = !m;
						break;
					}
					break;
				case SAMEHOST:
					m = 1;
 					switch (check->service){
					default: break;
					case 'R': case 'Q': case 'M': case 'C': case 'S':
						/* SAMEHOST check succeeds if REMOTEIP == IP */
						m = Same_host(check->host, check->remotehost);
						if( m ){
						/* check to see if both remote and local are
							server */
						int r, h;
						r = Same_host(check->remotehost,&HostIP);
						if( r ) r = Same_host(check->remotehost,&LocalhostIP);
						h = Same_host(check->host,&HostIP);
						if( h ) h = Same_host(check->host,&LocalhostIP);
						DEBUGF(DDB3)(
							"Perms_check: SAMEHOST server name check r=%d,h=%d",
							r, h );
						if( h == 0 && r == 0 ){
							m = 0;
						}
						}
						if( invert ) m = !m;
						break;
					}
					break;
				case SAMEUSER:
					m = 1;
 					switch (check->service){
					default: break;
					case 'Q': case 'M': case 'C': case 'S':
						/* check succeeds if remoteuser == user */
						m = (safestrcmp( check->user, check->remoteuser ) != 0);
						if( invert ) m = !m;
						DEBUGF(DDB3)(
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
						/* check succeeds if remote IP and server IP == IP */
						m = Same_host(check->remotehost,&HostIP);
						if( m ) m = Same_host(check->remotehost,&LocalhostIP);
						if( invert ) m = !m;
						break;
					}
					break;

				case DEFAULT:
					if( j || val[j+1].token == 0 || val[j+2].token  ){
						log( LOG_ERR,
						"Perms_check: bad default entry" );
						goto next_line;
					}
					switch( val[j+1].key ){
						case REJECT: Last_default_perm =  REJECT; break;
						case ACCEPT: Last_default_perm =  ACCEPT; break;
						default:
							log( LOG_ERR,
							"Perms_check: bad value for default: '%s'", 
							val[j].token );
							break;
					}
					DEBUGF(DDB3)("Perms_check: DEFAULT '%s'",
						perm_str( Last_default_perm ) );
					goto next_line;
				}
				/* if status is non-zero, test failed */
				DEBUGF(DDB3)("Perms_check: key '%s', result after '%s', match %d",
					 perm_str(val[j].key), perm_str( result ), m );
				if( m != 0 ){
					goto next_line;
				}
			}
			/* make sure at least one test done */
			if( m >= 0 && j && val[j].token == 0 ){
				if( result == 0 ){
					result = Last_default_perm;
				}
				DEBUGF(DDB2)("Perms_check: result %d '%s'",
					result, perm_str( result ) );
				return( result );
			}
		next_line: ;
		}
	}
	DEBUGF(DDB2)("Perms_check: result 0 (no match)" );
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
 	DEBUGF(DDB3)("match: str '%s'", str );
 	list = &perm_list[val->list];
 	if(str && val->list)for( i = 0; result && list[i]; ++i ){
 		/* now do the match */
 		result = Globmatch( list[i], str );
		DEBUGF(DDB3)("match: list[%d]='%s', result %d", i, list[i],  result );
	}
	if( invert ) result = !result;
 	DEBUGF(DDB3)("match: str '%s' final result %d", str, result );
	return( result );
}

/***************************************************************************
 * static int match_host( struct perm_val *val, char *host );
 * static int match_pr( struct perm_val *val, char *host );
 *  returns 1 on failure, 0 on success
 *  - match the hostname/printer strings against the list of options
 *    options are glob type regular expressions;  we implement this
 *    currently using the most crude of pattern matching
 *  - if string is null or pattern list is null, then match fails
 *    if both are null, then match succeeds
 ***************************************************************************/
#if !defined(HAVE_INNETGR_DEF)
extern int innetgr(const char *netgroup,
	const char *machine, const char *user, const char *domain);
#endif

static int match_host( struct perm_val *val, struct host_information *host,
	int invert )
{
 	char **list;
 	int result = 1;
 	int i;
 	DEBUGF(DDB3)("match: host '%s'", host?host->fqdn:0 );
 	list = &perm_list[val->list];
 	if(host&&host->fqdn&&val->list)for( i = 0; result && list[i]; ++i ){
 		/* now do the match */
		if( list[i][0] == '@' ) {	/* look up host in netgroup */
#ifdef HAVE_INNETGR
			result = !innetgr( list[i]+1, host->fqdn, NULL, NULL );
#else /* HAVE_INNETGR */
			DEBUGF(DDB3)("match: no innetgr() call, netgroups not permitted");
#endif /* HAVE_INNETGR */
		} else {
	 		result = Globmatch( list[i], host->fqdn );
		}
		DEBUGF(DDB3)("match: list[%d]='%s', result %d", i, list[i],  result );
	}
	if( invert ) result = !result;
 	DEBUGF(DDB3)("match: host '%s' final result %d", host?host->fqdn:0,
		result );
	return( result );
}

static int match_pr( struct perm_val *val, char *str, int invert )
{
 	char **list;
 	int result = 1;
 	int i;
 	DEBUGF(DDB3)("match_pr: str '%s'", str );
 	list = &perm_list[val->list];
 	if(str && val->list)for( i = 0; result && list[i]; ++i ){
 		/* now do the match */
		if( list[i][0] == '@' ) {	/* look up host in netgroup */
#ifdef HAVE_INNETGR
			result = !innetgr( list[i]+1, str, NULL, NULL );
#else /* HAVE_INNETGR */
			DEBUGF(DDB3)("match_pr: no innetgr() call, netgroups not permitted");
#endif /* HAVE_INNETGR */
		} else {
	 		result = Globmatch( list[i], str );
		}
		DEBUGF(DDB3)("match_pr: list[%d]='%s', result %d", i, list[i],  result );
	}
	if( invert ) result = !result;
 	DEBUGF(DDB3)("match_pr: str '%s' final result %d", str, result );
	return( result );
}

/***************************************************************************
 * static int match_ip( struct perm_val *val )
 * check the IP address and mask
 * entry has the format:  IPADDR/MASK, mask is x.x.x.x or n (length)
 ***************************************************************************/
int form_addr_and_mask(char *v,void *addr,void *mask,int addrlen, int family )
{
	char val[LINEBUFFER];
	int result = 1;
	char *s, *t;
	int i, m, bytecount, bitmask;

	memset( addr, 0, addrlen );
	memset( mask, ~0, addrlen );
	if( v == 0 ) return(1);
	safestrncpy( val,v );
	s = strchr( val, '/' );
	if( s ){
		*s++ = 0;
	}
	result = inet_pton(family, val, addr );
	if( s ){
		t = 0;
		m = strtol( s, &t, 0 );
		if( t == 0 || *t ){
			result = inet_pton(family, s, mask );
		} else {
			/* we clear the lower bits */
			s = mask;
			bytecount = m/8;
			bitmask = m & 0x7;
			for( i = bytecount; i < addrlen; ++i ){
				if( bitmask ){
					s[i] = ~((1<<(8-bitmask))-1);
					bitmask = 0;
				} else {
					s[i] = 0;
				}
			}
		}
	}
	if(DEBUGL4){
	char buffer[64];
	logDebug("form_addr_and_mask: result %d, addr '%s'",
		result, inet_ntop( family, addr, buffer, sizeof(buffer) ) );
	logDebug("form_addr_and_mask: mask '%s'",
		inet_ntop( family, mask, buffer, sizeof(buffer) ) );
	}
	return( result );
}


int cmp_ip_addr( unsigned char *h, unsigned char *a, unsigned char *m, int len )
{
	int match, i;

	match = 0;
	for( i = 0; match == 0 && i < len; ++i ){
		DEBUGF(DDB4)("cmp_ip_addr: [%d] mask 0x%02x addr 0x%02x host 0x%02x",
			i, m[i], a[i], h[i] );
		match = m[i] & ( a[i] ^ h[i] );
	}
	DEBUGF(DDB3)("cmp_ip_addr: result 0x%02x", match );
	return( match != 0 );
}

static int ipmatch( char *v, struct host_information *host )
{
	struct sockaddr addr, mask;
	int result = 1;
	unsigned char *h;
	int i;
	int addrlen = sizeof( struct in_addr );

	/* get the first address in the list */
	addrlen = host->host_addrlength;
	if( addrlen > sizeof(addr) ){
		fatal(LOG_ERR, "ipmatch: address size mismatch" );
	}
	if( form_addr_and_mask( v, (void *)&addr, (void *)&mask,
		addrlen, host->host_addrtype ) == 0 ){
		return( 1 );
	}
	h = (void *)host->host_addr_list.list;
	for( i = 0; result && i < host->host_addr_list.count; ++i ){
		result = cmp_ip_addr(h, (void *)&addr, (void *)&mask, addrlen);
		h += addrlen;
	}
	DEBUGF(DDB3)("ipmatch: result %d", result );
	return( result );
}

static int match_ip( struct perm_val *val, struct host_information *host,
	int invert )
{
	char **list;
	int result = 1;
	int i;

	DEBUGF(DDB3)("match_ip: host '%s'", host->fqdn );
	list = &perm_list[val->list];
	if(val->list)for( i = 0; result && list[i]; ++i ){
		/* now do the match */
		result = ipmatch( list[i], host );
	}
	if( invert ) result = !result;
	DEBUGF(DDB3)("match_ip: result %d", result );
	return( result );
}

/*
 * int Match_ipaddr_value( char *str, struct host_information *host )
 *  str has format addr,addr,addr
 * Match the indicated address against the host
 *  returns: 0 if match
 *           1 if no match
 */
int Match_ipaddr_value( char *str, struct host_information *host )
{
	int result = 1;
	char *end, *buffer = 0;
	DEBUGF(DDB2)("Match_ipaddr_value: str '%s'", str);
	if( str && *str ){
		buffer = safestrdup( str );
		for( str = buffer; result && str; str = end ){
			while( isspace( *str ) ) ++str;
			end = strpbrk( str, ",; \t");
			if( end ) *end++ = 0;
			if( *str == 0 ) continue;
			if( *str == '@' ) {	/* look up host in netgroup */
#ifdef HAVE_INNETGR
				result = !innetgr( str+1, host->fqdn, NULL, NULL );
#else /* HAVE_INNETGR */
				DEBUGF(DDB3)("match: no innetgr() call, netgroups not permitted");
#endif /* HAVE_INNETGR */
			} else {
				result = Globmatch( str, host->fqdn );
				if( result ) result = ipmatch( str, host );
			}
			DEBUGF(DDB2)("Match_ipaddr_value: checked '%s', result %d", str, result);
		}
		free(buffer);
	}
	DEBUGF(DDB2)("Match_ipaddr_value: result %d, on name '%s'", result, str);
	return( result );
}

/***************************************************************************
 * static int match_addrip( struct perm_val *val )
 * check the IP address and mask against sin_addr value
 * entry has the format:  IPADDR/MASK, mask is x.x.x.x or n (length)
 ***************************************************************************/

static int addripmatch( char *v, struct sockaddr *host )
{
	struct sockaddr addr, mask;
	int result = 1;
	unsigned char *h = 0;
	int addrlen = 0;

	/* get the first address in the list */

	if( host->sa_family == AF_INET ){
		addrlen = sizeof( struct in_addr );
		h = (void *)&((struct sockaddr_in *)host)->sin_addr;
#if defined(IN6_ADDR)
	} else if( host->sa_family == AF_INET6 ){
		addrlen = sizeof( struct in6_addr );
		h = (void *)&((struct sockaddr_in6 *)host)->sin6_addr;
#endif
	} else {
		fatal( LOG_ERR, "addripmatch: bad family '%d'", host->sa_family );
	}
	if( form_addr_and_mask( v, (void *)&addr, (void *)&mask,
		addrlen, host->sa_family ) == 0 ){
		return( 1 );
	}
	result = cmp_ip_addr( h, (void *)&addr, (void *)&mask, addrlen );
	DEBUGF(DDB3)("ipmatch: result %d", result );
	return( result );
}

static int match_addrip( struct perm_val *val, struct sockaddr *addr, int invert )
{
	char **list;
	int result = 1;
	int i;

	DEBUGFC(DDB3){
		char buffer[64];
		logDebug("match_addrip: addr '%s'",
			inet_ntop_sockaddr(addr, buffer, sizeof(buffer)) );
	}
	list = &perm_list[val->list];
	if(val->list)for( i = 0; result && list[i]; ++i ){
		/* now do the match */
		result = addripmatch( list[i], addr );
	}
	if( invert ) result = !result;
	DEBUGF(DDB3)("match_ip: result %d", result );
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
	DEBUGF(DDB3)("portmatch: low %d, high %d, port %d, result %d",
		low, high, port, result );
	return( result );
}

static int match_range( struct perm_val *val, int port, int invert )
{
	char **list;
	int result = 1;
	int i;

	DEBUGF(DDB3)("match_ip: ip '0x%x'", port );
	list = &perm_list[val->list];
	if(val->list)for( i = 0; result && list[i]; ++i ){
		/* now do the match */
		result = portmatch( list[i], port );
	}
	if( invert ) result = !result;
	DEBUGF(DDB3)("match_ip: port '%d' result %d", port, result );
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

	DEBUGF(DDB3)("match_char: value '0x%x' '%c'", value, value );
	list = &perm_list[val->list];
	if(val->list)for( i = 0; result && list[i]; ++i ){
		/* now do the match */
		result = (strchr( list[i], value ) == 0 );
		DEBUGF(DDB3)("match_char: val %c, str '%s', match %d",
			value, list[i], result);
	}
	if( invert ) result = !result;
	DEBUGF(DDB3)("match_char: value '%c' result %d", value, result );
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
 	DEBUGF(DDB3)("match_group: str '%s'", str );
 	list = &perm_list[val->list];
 	if(val->list)for( i = 0; str && result && list[i]; ++i ){
 		/* now do the match */
 		result = ingroup( list[i], str );
	}
	if( invert ) result = !result;
 	DEBUGF(DDB3)("match: str '%s' value %d", str, result );
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

	DEBUGF(DDB3)("ingroup: checking '%s' for membership in group '%s'", user, group);
	if( group == 0 || user == 0 ){
		return( result );
	}
	/* first try getgrnam, see if it is a group */
	pwent = getpwnam(user);
	if( (grent = getgrnam( group )) ){
		DEBUGF(DDB3)("ingroup: group id: %d\n", grent->gr_gid);
		if( pwent && (pwent->pw_gid == grent->gr_gid) ){
			DEBUGF(DDB3)("ingroup: user default group id: %d\n", pwent->pw_gid);
			result = 0;
		} else for( members = grent->gr_mem; result && *members; ++members ){
			DEBUGF(DDB3)("ingroup: member '%s'", *members);
			result = (strcmp( user, *members ) != 0);
		}
	} else if( strchr( group, '*') ){
		/* wildcard in group name, scan through all groups */
		setgrent();
		while( result && (grent = getgrent()) ){
			DEBUGF(DDB3)("ingroup: group name '%s'", grent->gr_name);
			/* now do match against group */
			if( Globmatch( group, grent->gr_name ) == 0 ){
				if( pwent && (pwent->pw_gid == grent->gr_gid) ){
					DEBUGF(DDB3)("ingroup: user default group id: %d\n",
					pwent->pw_gid);
					result = 0;
				} else {
					DEBUGF(DDB3)("ingroup: found '%s'", grent->gr_name);
					for( members = grent->gr_mem; result && *members; ++members ){
						DEBUGF(DDB3)("ingroup: member '%s'", *members);
						result = (strcmp( user, *members ) != 0);
					}
				}
			}
		}
		endgrent();
	} else if( group[0] == '@' ) {	/* look up user in netgroup */
#ifdef HAVE_INNETGR
		if( !innetgr( group+1, NULL, user, NULL ) ) {
			DEBUGF(DDB3)( "ingroup: user %s NOT in netgroup %s", user, group+1 );
		} else {
			DEBUGF(DDB3)( "ingroup: user %s in netgroup %s", user, group+1 );
			result = 0;
		}
#else /* HAVE_INNETGR */
		DEBUGF(DDB3)( "ingroup: no innetgr() call, netgroups not permitted" );
#endif /* HAVE_INNETGR */
	}
	DEBUGF(DDB3)("ingroup: result: %d", result );
	return( result );
}

int Check_for_rg_group( char *user )
{
	int match = 0;
	char buffer[MAXPATHLEN];
	char *s, *end;

	DEBUGF(DDB3)("Check_for_rg_group: name '%s', Restricted_group '%s'",
		user, Restricted_group );
	if( Restricted_group && *Restricted_group ){
		match = 1;
		safestrncpy( buffer, Restricted_group );
		for( s = buffer; match && s && *s; s = end ){
			end = strpbrk( s, " \t,;" );
			if( end ){
				*end++ = 0;
			}
			if( *s ){
				match = ingroup( s, user );
			}
		}
	}
	DEBUGF(DDB3)("Check_for_rg_group: result: %d", match );
	return( match );
}

/***************************************************************************
 * static int match_auth( struct perm_val *val, char *str );
 *  returns 1 on failure, 0 on success
 *  - gets the authentication
 *    0 = none
 *    1 = cfp->auth_id
 *    2 = cfp->forward_id
 *    3 = cfp->forward_id and cfp->auth_id
 *  - scan the listed keywords to see if one matches
 ***************************************************************************/
static int match_auth( struct perm_val *val, struct control_file *cf,
	int invert )
{
 	char **list, *s;
 	int result = 1;
 	int i, key = 0;
	if( cf ){
		if( cf->auth_id[0] ) key |= 1;
		if( cf->forward_id[0] ) key |= 2;
		list = &perm_list[val->list];
		DEBUGF(DDB3)("match_auth: key %d, val->list %d",
			key, val->list );
		if( val->list == 0 ){
			if( key ) result = 0;
		} else for( i = 0; result && (s = list[i]); ++i ){
			/* now do the match */
			if( strcasecmp( s, "NONE" ) == 0 ){
				if( key == 0 ) result = 0;
			} else if( strcasecmp( s, "USER" ) == 0 ){
				if( key & 1 ) result = 0;
			} else if( strcasecmp( s, "FWD" ) == 0 ){
				if( key & 2 ) result = 0;
			}
		}
	}
	if( invert ) result = !result;
 	DEBUGF(DDB3)("match_auth: value %d", result );
	return( result );
}
