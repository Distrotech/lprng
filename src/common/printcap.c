/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: printcap.c
 * PURPOSE: Process a printcap file
 **************************************************************************/

static char *const _id =
"$Id: printcap.c,v 3.7 1996/09/09 14:24:41 papowell Exp papowell $";

#include "lp.h"
#include "printcap.h"
#include "lp_config.h"
#include "pr_support.h"

/***************************************************************************

Commentary:
Patrick Powell Sun Apr 23 15:21:29 PDT 1995

Printcap files and printcap entry:

A printcap entry is contains all the information associated
with a particular printer.  The data structures in include/printcap.h
are used to process printcap information

 ***************************************************************************/

/***************************************************************************
 * int Readprintcap( struct printcapfile *pcf, char *file, int fd )
 *  reads a printcap file into memory, then extracts the information
 *  pcf - printcapfile data structure, holds information
 *  file - file name for error messages
 *  fd   - file descriptor to read from
 * 
 * struct printcap *Filterprintcap( char *name, struct printcapfile *pcf,
 *	   int *index )
 *  Scan through the list of filters, and check to see if one has information
 * 
 *  Printcap information is read into a dynamically allocated buffer;
 *  This buffer is malloced/realloced as required.
 *  Note that all strings are kept in the buffer.
 *
 * int parse_pc( struct printcapfile *pcf, char *file, char *begin )
 *  pcf - printcapfile data structure, holds information
 *  file - file name for error messages
 *  char *begin - beginning of new printcap information in
 *    a buffer.
 *
 *  printcap file is parsed by using a state based parser:
 *    0 - look for name on start of line
 *    1 - look for termination of name with | or :
 *        | -  start of another name
 *        : -  start of options
 *    2 - look for termination of option with :
 *        blank line -> end of input -> state 0
 * 
 *   Side effect:
 *   printcap entries are now in the following form:
 *   <name>[|<name]*[:option:W*]+
 *   That is: a name must be supplied;
 *      additional names must start with |
 *   Options only need to start with a : - i.e.
 *      :option      - start of option
 *   Note: options now can be of variable length.
 *
 * int Bufferprintcap( struct printcapfile *pcf, char *file, char *buffer )
 *   process a buffer (i.e.- ascii text with terminating 0)
 *   and put in printcap information.
 *
 ***************************************************************************
 * void Getprintcap( char *path, struct printcap *pc )
 * Read printcap information from a colon separated set of files
 *   1. break path up into set of path names
 *   2. read the printcap information into memory
 *   3. parse the printcap informormation
 *
 ***************************************************************************
 * add_buffer( pcf, size )
 * expand the printcapfile buffer by size+2 bytes;
 * return pointer to start of size byte area in memory
 *
 ***************************************************************************
 * int Readprintcap( struct printcapfile *pcf, char *file, int fd,
 *	struct stat *statb )
 *  - get the size of file
 *  - expand printcap buffer
 *  - read file into expanded buffer
 *  - parse file
 *
 ***************************************************************************
 * int Bufferprintcap( struct printcapfile *pcf, char *file, char *buffer )
 *  - insert a buffer into the printcap information
 *  - parse the added printcap information
 *
 ***************************************************************************
 * int  parse_pc( struct printcapfile *pcf, char *file, char *begin )
 *  - parse the printcap information in a buffer
 *  - this uses a state based parser
 *     see Commentary at start of file
 *
 ***************************************************************************
 * next_line( struct printcapfile *pcf, int incr )
 *  get the address of the current (incr = 0) or next (incr !=0 )
 *  entry in the field array
 *  - if necessary, realloc the fields array
 *
 ***************************************************************************
 *  void next_entry( struct printcapfile *pcf, int incr )
 *  get the address of the current (incr = 0) or next (incr !=0 )
 *  entry in the entry array
 *  - if necessary, realloc the fields array
 *
 ***************************************************************************
 *  int check_pc_entry( struct printcapfile *pcf, struct printcap *pc )
 *  1. check the printcap entry for consistency
 *  2. update the printcap file information
 *     - end of the info
 *     - we scan the list of variable names, looking for blanks
 *       end of list terminated by a 0 entry
 *     - we move them up in the list
 *  RETURN: 0 if ok, non-zero if error
 *
 ***************************************************************************
 * void doinclude( struct printcapfile *pcf, char *file );
 * 1. locate the include file
 * 2. stat it and get the size
 * 3. malloc a new buffer of size old+new
 * 4. copy the buffer up to the current start of line to new area
 * 5. copy the new file to new area
 * 6. append the remainder of the old buffer 
 *
 *
 ***************************************************************************
 * dump_printcapfile( char *title, struct printcapfile *cf )
 * Dump printcapfile information
 *
 ***************************************************************************
 * Free_pcf( struct printcapfile *pcf )
 *  Free all of the memory in the  printcap buffer
 *
 ***************************************************************************
 * Get_pc_option( char *option, struct printcap *pc )
 *  search the printcap entry for the option name;
 *
 ***************************************************************************
 * struct printcap *Get_pc_name( char *str, struct printcapfile *pcf,
 *		int *index );
 *  search the names for the printer
 *  - we start the search AFTER the index entry;
 *    if *index < 0,  at the beginning
 *
 ***************************************************************************
 * Get_pc_vars( struct keyword *key, struct printcap *pc, char *error,
 *  int errlen );
 *
 * Search the printcap entry for values for the keywords in the list
 * Do the translation as required.
 *  Note: we do not allow list variables in the printcap file
 *
 ***************************************************************************
 * Initialize_pc_vars( struct printcapfile *pcf, struct pc_var_list *vars,
 *   char *list );
 *  Initialize the printcap variables with values from the list
 * Note: the keyword lists are sorted alphabetically
 *
 ***************************************************************************
 * char *Get_printer_comment( char *pr )
 *  Get the last entry in the printer name list if there is more than one
 ***************************************************************************/

static void next_line( struct printcapfile *pcf, int incr );
static void next_entry( struct printcapfile *pcf, int incr );
static void doinclude( struct printcapfile *pcf, char *file );
int check_pc_entry( struct printcapfile *pcf, struct printcap *pc );
void dump_printcap( char *title,  struct printcap *pcf );
void dump_printcapfile( char *title,  struct printcapfile *pcf );
static void do_key_conversion( struct keywords *key,
	struct printcap *pc, char *s );
static int parse_pc( struct printcapfile *pcf, char *file, char *begin );

static char *Printer_vars( char *name, char *error, int errlen,
	struct printcapfile *printcapfile,
	struct pc_var_list *var_list, int start,
	struct pc_used *pc_used );
static void Add_pc_to_list( struct printcap *pc, struct pc_used *pc_used );


/* sorting function */
int mstrcmp( const void *l, const void *r )
{
	/* DEBUG8( "l '%s', r '%s'", *(char **)l, *(char **)r ); / **/
	return( strcmp( *(char **)l, *(char **)r ) );
}

/***************************************************************************
 * void Getprintcap(struct printcapfile *pcf, char *path)
 * Read printcap information from a colon separated set of files
 *   1. break path up into set of path names
 *   2. read the printcap information into memory
 *   3. parse the printcap informormation
 ***************************************************************************/

void Getprintcap( struct printcapfile *pcf, char *path, int nofilter )
{
	char *s, *end;
	int fd, c;
	struct stat statb;
	char entry[MAXPATHLEN];
	int err;

	pcf->init = 1;
	DEBUG0("Getprintcap: paths '%s'", path );
	/* copy to the temporary place */
	entry[0] = 0;
	if( path ) strncpy( entry, path, sizeof(entry) );

	for(path = entry; path && *path; path = end ){
		end = strpbrk( path, ";:" );
		if( end ){
			*end++ = 0;
		}
		while( (c = *path) && isspace(c) ) ++path;
		if( c == 0 ) continue;
		if( Verbose || Debug > 0 ){
			logDebug( "Printcap file '%s'", path );
		}
		switch( c ){
		case '|':
			DEBUG8("Getprintcap: filter '%s'", path );
			if( nofilter ){
				fatal( LOG_ERR, "Getprintcap: nofilter 'TRUE' and filter '%s'",
					path );
			}
			++path;
			s = add_buffer( &pcf->filters, strlen(path)+1 );
			--s;
			strcpy( s, path );
			DEBUG8("Getprintcap: added filter '%s'", s );
			break;

		case '/':
			fd =  Checkread( path, &statb );
			err = errno;
			DEBUG8("Getprintcap: file '%s', size %d", path, statb.st_size );
			if( ( Verbose || Debug > 0 ) && fd < 0 ){
				logDebug( "Cannot open '%s' - %s", path, Errormsg(err) );
			}
			if( fd >= 0 && Readprintcap( pcf, path, fd, &statb ) ){
				log( LOG_DEBUG, "Error reading %s", path );
			}
			close(fd);
			break;
		default:
			fatal( LOG_ERR,
				"Getprintcap: entry not filter or absolute pathname '%s'",
				path );
		}
	}
	if( Debug > 8 ){
		dump_printcapfile( "Printcap", pcf );
	}
}

/***************************************************************************
 * int Readprintcap( struct printcapfile *pcf, char *file, int fd )
 *  - get the size of file
 *  - expand printcap buffer
 *  - read file into expanded buffer
 *  - parse file
 ***************************************************************************/
int Readprintcap( struct printcapfile *pcf, char *file, int fd,
	struct stat *statb )
{
	int i, len;				/* ACME Integers and pointers */
	char *begin;			/* beginning of data */
	char *s;				/* ACE, cheaper and better */

	/* check the file */
	if( fd < 0 ){
		logerr( LOG_DEBUG, "Readprintcap: bad fd for '%s'", file );
		return( -1 );
	}

	/* malloc data structures */
	
	DEBUG8("Readprintcap: file '%s' size %d", file, statb->st_size );
	begin = add_buffer( &pcf->buffers, statb->st_size );
	DEBUG8("Readprintcap: buffer 0x%x", begin );
	s = begin;

	for( len = 1, i = statb->st_size;
		i > 0 && (len = read( fd, s, i)) > 0;
			i -= len, s += len );
	if( len <= 0 ){
		logerr( LOG_ERR, "Readprintcap: cannot read '%s'", file );
		close( fd );
		return( -1 );
	}
	return( parse_pc( pcf, file, begin ) );
}


/***************************************************************************
 * struct printcap *Filterprintcap( char *name, struct printcapfile *pcf,
 *  int *index )
 *  - for each entry in the printcapfile filters list do the following:
 *    - make the filter
 *    - read from the filter until EOF
 *    - kill off the filter process
 *    - if entry read, add it to the printcap file
 ***************************************************************************/
struct printcap *Filterprintcap( char *name, struct printcapfile *pcf,
	int *index )
{
	struct printcap *pc = 0;	/* printcap entry */
	char *buffer;				/* buffer list and buffer */
	char **filters, *filter;	/* list of filter and current filter */
	char *s;					/* ACME for pointers */
	int i, j;					/* ACME again! for quality */
	char **names;				/* names in printcap file */

	/* get the printer name to send */
	DEBUG4("Filterprintcap: filters '%d'", pcf->filters.count );
	/* create a pipe to read the file into */
	filters = pcf->filters.list;
	for( i = 0; i < pcf->filters.count; ++i ){
		filter = filters[i];
		buffer = Filter_read( name, &pcf->buffers, filter );
		if( buffer ){
			/* now we can parse the printcap entry */
			if( parse_pc( pcf, filter, buffer ) ){
				fatal( LOG_ERR, "Filterprintcap: bad printcap info from '%s'",
					filter );
			}
			/*
			 * now we try to find the printcap entry in the returned value
			 */
			i = *index;
			if( i < 0 ) i = 0;
			DEBUG8("Filterprintcap: checking for '%s', starting %d, max %d",
				name, i, pcf->pcs.count );
			for( ; i < pcf->pcs.count; ++i ){
				pc = &((struct printcap *)(pcf->pcs.list))[i];
				if( Debug > 9 ){
					dump_printcap( "PRINTCAP", pc );
				}
				names = &pc->pcf->lines.list[pc->name];
				for( j = 0; j < pc->namecount; ++ j ){
					s = names[j];
					DEBUG8("Filterprintcap: want '%s' comparing to '%s'",
						name, s );
					if( strcmp( s, name ) == 0 ){
						*index = i+1;
						DEBUG8("Filterprintcap: found '%s'", name );
						return( pc );
					}
				}
			}
		}
	}
	return( (void *)0 );
}


/***************************************************************************
 * char *Filter_read( char *name - name written to filter STDIN
 *      struct malloc_list - received information stored in this list
 *      char *filter - filter name
 * 1. The filter is created and the name string written to the filter
 *     STDIN.
 * 2. The output from the filter is read and stored in the malloc_list;
 *     this is also returned from the filter.
 * Note: the options passed to the filter are the defaults for
 *   the filters.
 ***************************************************************************/

char *Filter_read( char *name, struct malloc_list *list, char *filter )
{
	int buffer_total = 0;		/* total length of saved buffers */
	int buffer_index = 0;			/* buffer index in the printcapfile */
	int pipes[2];				/* pipe to read from filter */
	char *s;					/* ACME pointers */
	int i, len;					/* ACME integers */
	char *buffer, **buffers;	/* list of buffers and a buffer */
	char line[LINEBUFFER];		/* buffer for a line */
	int list_index = 0;

	buffer = 0;
	/* make a pipe */

	if( name == 0 || *name == 0 ){
		fatal( LOG_ERR, "Filter_read: no name" );
	}
	plp_snprintf( line, sizeof( line ), "%s\n", name );
	if( pipe( pipes ) < 0 ){
		logerr_die( LOG_ERR, "Filter_read: cannot make pipes for '%s'",
			filter );
	}
	DEBUG8("Filter_read: pipe [%d,%d]", pipes[0],pipes[1] );
	/* create the printcap info process */
	Make_filter( 'f',(void *)0,&Pr_fd_info,filter,1, 0, pipes[1],
		(void *)0, (void *)0, 0, 0 );
	DEBUG8("Filter_read: filter pid %d, input fd %d, sending '%s'",
		Pr_fd_info.pid, Pr_fd_info.input, name?name:"<NULL>" );
	close( pipes[1] );

	/* at this point, you must write to the filter pipe */
	if( Write_fd_str( Pr_fd_info.input, line ) < 0 ){
		logerr( LOG_INFO, "Filter_read: filter '%s' failed", filter );
	}
	close( Pr_fd_info.input );

	/*
	 * read from the filter into a buffer
	 */
	do{
		if( buffer_total - buffer_index < LARGEBUFFER + 1 ){
			if( buffer == 0 ){
				buffer_total = LARGEBUFFER + 1024;
				list_index = list->count;
				buffer = add_buffer( list, buffer_total );
				buffers = (void *)list->list;
				if( buffers[list_index] != buffer ){
					fatal( LOG_ERR, "Filter_read: wrong buffer!" );
				}
			} else {
				buffer_total += LARGEBUFFER + 1024;
				buffer = realloc( buffer, buffer_total );
				if( buffer == 0 ){
					logerr_die( LOG_ERR, "Filter_read: realloc failed" );
				}
				buffers = (void *)list->list;
				buffers[list_index] = buffer;
			}
		}
		len = buffer_total - buffer_index - 1;
		s = buffer+buffer_index;
		i = len;
		for( ;
			len > 0 && (i = read( pipes[0], s, len ) ) > 0;
			len -= i, s += i, buffer_index += i );
		DEBUG8("Filter_read: len %d", buffer_index );
		/* check to see if we have a buffer read */
	} while( i > 0 );

	if( buffer_index == 0 ){
		/* nothing read */
		free( buffer );
		buffer = 0;
		--list->count;
	} else {
		buffer[buffer_index] = 0;
	}

	Close_filter( &Pr_fd_info, 0 );
	DEBUG8( "Filter_read: buffer_index %d, '%s'",
		buffer_index, buffer );
	return( buffer );
}

/***************************************************************************
 * int Bufferprintcap( struct printcapfile *pcf, char *file, char *buffer )
 *  - insert a buffer into the printcap information
 *  - parse the added printcap information
 ***************************************************************************/

int Bufferprintcap( struct printcapfile *pcf, char *file, char *buffer )
{
	char *begin;
	int len;

	/* we expand the existing printcap file */

	len = strlen( buffer );
	DEBUG8("Bufferprintcap: file '%s', len", file, len );
	begin = add_buffer( &pcf->buffers, len );
	strcpy( begin, buffer );
	return( parse_pc( pcf, file, begin ) );
}

/***************************************************************************
 * int  parse_pc( struct printcapfile *pcf, char *file, char *begin )
 *  - parse the printcap information in a buffer
 *  - this uses a state based parser
 *     see Commentary at start of file
 * RETURNS: 0 if successful, != 0 if not
 ***************************************************************************/

static int parse_pc( struct printcapfile *pcf, char *file, char *buffer )
{
	char *s;				/* ACME again */
	struct printcap *pc;	/* printcap entry */
	int line;				/* line number */
	char *end;				/* end of line */
	int state;				/* state of parser */
	int status = 1;			/* status of result */
	int need_separator = 0;	/* looking for a separator */
	char *begin;			/* beginning of line */
	int sep;				/* separator */

	/* state:
	 *        0 - getting name of printcap entry
	 *        1 - getting more names for printcap entry 
	 *        2 - getting fields for printcap entry 
	 *        -1 - error in format
	 */

	state = 0;		/* looking for names */
	line = 0;		/* line 0 in file */
	s = buffer;		/* start of buffer */
	begin = 0;		/* clear flag */
	next_line( pcf, 0 );	/* set up field array */
	next_entry( pcf, 0 );	/* set up entry entry */
	pc = 0;			/* no names found yet */
	DEBUG8("parse_pc: pcf->lines list,max,count-> = 0x%x, %d, %d, '%s'",
		pcf->lines.list, pcf->lines.max, pcf->lines.count,
		pcf->lines.list[pcf->lines.count] );
	DEBUG8("parse_pc: pcf->pcs list,max,count-> = 0x%x, %d, %d",
		pcf->pcs.list, pcf->pcs.max, pcf->pcs.count );

	for( ; state >= 0 && s && *s; s = end ){
		++line;
		begin = s;
		end = strchr( begin, '\n' );
		/* check for escaped end of line */
		while( end && &end[-1] >= begin && end[-1] == '\\' ){
			/* we append the next line to this line by replacing \n with 
				blanks */
			end[-1] = ' ';
			end[0] = ' ';
			++line;
			end = strchr( end, '\n' );
		}
		if( end ){
			*end++ = 0;
			if( *end == 0 ) end = 0;
		}
		DEBUG8("parse_pc: begin 0x%x len %d '%s'",
			begin, strlen(begin), begin);
		/* OK, we are done with line gathering */
		s = begin;
		DEBUG8("parse_pc: newline state %d, s = 0x%x [%s] %d",
			state,s,s, s-buffer );

		/* blank first charcter - get rid of blanks */
		while( *s && isspace( *s ) ) ++s;
		/* throw away comments and blank lines */
		sep = *s;
		if( sep == '#' || sep == 0 ) continue;

		if( sep != ':' &&  sep != '|' ){
			/* start of a definition */
			state = 0;
		}
		/* state 0 and we have a bad first character */
		if( state == 0 && (sep == ':' || sep == '|' ) ){
			state = -1;
			continue; 
		}
		if( state ){
			++s;
			need_separator = 1;
		}
		/* we set up the start of the entry */
		DEBUG8("parse_pc: now state %d, s = 0x%x [%c]", state,s,*s);
		pcf->lines.list[pcf->lines.count] = s;
		DEBUG8("parse_pc: pcf->lines.list[pcf->lines.count] 0x%x[0x%x]= '%s'",
			pcf->lines.list,pcf->lines.count,
			pcf->lines.list[pcf->lines.count] );
		next_line( pcf, 0 );	/* make sure we have space */
		if( state == 0 ){
			/* look for name of printcap entry */
			{
				/* INCLUDE FILE HANDLING */
				/* 1. we check for an include file specification */
				char inc[] = "include";
				char *fn;

				if( strncmp( s, inc, sizeof(inc)-1 ) == 0 ){
					/* we have an include file */
					fn = s+sizeof(inc)-1;
					if( isspace( *fn ) ){
						++fn;
						DEBUG8("parse_pc: include file '%s'", fn );
						doinclude( pcf, fn );
						state = 0;
						continue;
					}
				}
			}
			/* finish old entry */
			if( pc && check_pc_entry( pcf, pc ) ){
				state = -1;
				pc = 0;
				break;
			}

			next_line( pcf,0 );	/* make sure we have space */

			pcf->lines.list[pcf->lines.count] = s;
			DEBUG8("parse_pc: state %d pcf->lines.count [0x%x]= '%s'",
			state, pcf->lines.count, pcf->lines.list[pcf->lines.count] );

			pc = &((struct printcap *)(pcf->pcs.list))[pcf->pcs.count];
			pc->name = pcf->lines.count;
			DEBUG8("parse_pc: pc 0x%x, pc->name %d", pc, pc->name );
			pc->pcf = pcf;

			next_line( pcf, 1 );
			need_separator = 1;
			DEBUG8("state %d, main name '%s'", state, s);
			state = 1;
			s =  strpbrk( s, ":|" );
			if( s == 0 ){
				/* one name on this line only */
				need_separator = 0;
				continue;
			}
			sep = *s;
			*s++ = 0;
			pcf->lines.list[pcf->lines.count] = s;
		}

		/* state 1: look for the end of the current name
		 * we are either at the start of a line or on the same line
		 * if need_separator == 0 we are at the start of a new line
		 *            != 0 we are on the same line
		 */

		if( state == 1 ){
			DEBUG8("state %d, need_separator %d, sep '%c' '%s'",
				state, need_separator, sep,s);
			while( s && sep == '|' ){
				DEBUG8("state %d, sep '%c' '%s'", state, sep, s);
				DEBUG8("parse_pc: state %d pcf->fieldcount [0x%x]= '%s'",
				state, pcf->lines.count, pcf->lines.list[pcf->lines.count] );
				if( need_separator ){
					next_line( pcf, 1 );
				}
				need_separator = 1;
				/* check for end of line */
				s = strpbrk( s, "|:" );
				if( s ){
					sep = *s;
					*s++ = 0;
				}
				pcf->lines.list[pcf->lines.count] = s;
			}
			/* if s, then found ':' */
			if( s ){
				/* we put in a null entry and then */
				/* we go to state 2 */
				state = 2;
				pcf->lines.list[pcf->lines.count] = 0;
				next_line( pcf, 1 );
				need_separator = 1;
				pcf->lines.list[pcf->lines.count] = s;
			}
		}

		if( state == 2 ){
			DEBUG8("state %d, need_separator %d, sep '%c' '%s'",
				state, need_separator, sep, s);
			DEBUG8("parse_pc: state %d pcf->fieldcount [0x%x]= '%s'",
				state, pcf->lines.count, pcf->lines.list[pcf->lines.count] );
			if( sep != ':' ){
				state = -1;
				continue;
			}
			while( s ){
				DEBUG8("state %d, s 0x%x '%s'", state, s,s);
				DEBUG8("parse_pc: state %d, pcf->lines.count [0x%x]= '%s'",
				state, pcf->lines.count, pcf->lines.list[pcf->lines.count] );
				if( need_separator ){
					next_line( pcf, 1 );
				}
				need_separator = 1;
				while( (s = strchr( s, ':' )) && s[-1] == '\\' ){
					char *t = s-1;
					while( (t[0] = t[1]) ) ++t; 
				}
				if( s ){
					sep = *s;
					*s++ = 0;
				}
				pcf->lines.list[pcf->lines.count] = s;
			}
			need_separator = 0;
		}
	}
	DEBUG8("parse_pc: end state %d need_separator %d pcf->lines.count [0x%x]= '%s'",
		state, need_separator,  pcf->lines.count, pcf->lines.list[pcf->lines.count] );
	/* should be looking for name (state 0) or option(state 2) */
	if( state == 1 ){
		state = -1;
	}
	if( state != -1 && pc && check_pc_entry( pcf, pc ) ){
		state = -1;
	}
	if( state == -1 ){
		log( LOG_ERR, "parse_pc: file '%s', line %d has error near '%s'",
		file, line, s );
		status = -1;
	} else {
		status = 0;
	}

	if( Debug>4 ){
		dump_printcapfile( file, pcf );
	}
	return( status );
}

/***************************************************************************
 * next_line( struct printcapfile *pcf, int incr )
 *  get the address of the current (incr = 0) or next (incr !=0 )
 *  entry in the field array
 *  - if necessary, realloc the fields array
 ***************************************************************************/

static void next_line( struct printcapfile *pcf, int incr )
{
	if( incr ){
		++pcf->lines.count;
	} 
	if( pcf->lines.count >= pcf->lines.max ){
		extend_malloc_list( &pcf->lines, sizeof( char * ), 100 );
	}
	DEBUG8("next_line: incr %d pcf->pcs.count [0x%x]", incr, pcf->pcs.count );
}

/***************************************************************************
 *  void next_entry( struct printcapfile *pcf, int incr )
 *  get the address of the current (incr = 0) or next (incr !=0 )
 *  entry in the entry array
 *  - if necessary, realloc the fields array
 ***************************************************************************/

static void next_entry( struct printcapfile *pcf, int incr )
{
	if( incr ){
		++pcf->pcs.count;
	} 
	if( pcf->pcs.count >= pcf->pcs.max ){
		extend_malloc_list( &pcf->pcs, sizeof( struct printcap ), 10 );
	}
	DEBUG8("next_entry: incr %d pcf->pcs.count [0x%x]", incr, pcf->pcs.count );
}

/***************************************************************************
 *  int check_pc_entry( struct printcapfile *pcf, struct printcap *pc )
 *  1. check the printcap entry for consistency
 *  2. update the printcap file information
 *     - end of the info
 *     - we scan the list of variable entries, looking for blank lines
 *     - we move them up in the list
 *     - end of list terminated by a 0 entry
 *  RETURN: 0 if ok, non-zero if error
 ***************************************************************************/

int check_pc_entry ( struct printcapfile *pcf, struct printcap *pc )
{
	int i, j;		/* ACME again! */
	char *s;		/* AJAX for strings! */
	int status = 0;	/* status of checks */

	DEBUG8( "check_pc_entry: pcf 0x%x, pc 0x%x", pcf, pc );
	DEBUG8(
	"check_pc_entry: pc 0x%x, name %d, count %d, options %d, count %d",
	pc, pc->name, pc->namecount, pc->options, pc->optioncount );

	/* set the last field to 0 */
	next_line( pcf, 0 );	/* make sure there is space */
	pcf->lines.list[pcf->lines.count] = 0;

	if( Debug > 8 ){
		for( i = pc->name; i <= pcf->lines.count; ++i ){
			logDebug("entry [%d] '%s'",i, pcf->lines.list[i] );
		}
	}
	
	for(j= i= pc->name; pcf->lines.list[i] && i < pcf->lines.count; ++i ){
		DEBUG8("name [%d,%d] '%s' '%s'", i, j,
			pcf->lines.list[i], pcf->lines.list[j] );
		s = pcf->lines.list[i];
		while( *s && isspace( *s ) ) ++s;
		if( *s ){
			trunc_str(s);
			pcf->lines.list[j] = s;
			++j;
		}
	}

	pc->namecount = j - pc->name;
	pcf->lines.list[j++] = pcf->lines.list[i++];

	/* set name and options */
	pc->options = j;

	for( ; i <= pcf->lines.count && pcf->lines.list[i]; ++i ){
		DEBUG8("option [%d,%d] '%s' '%s'", i,j,
			pcf->lines.list[i], pcf->lines.list[j] );
		s = pcf->lines.list[i];
		while( *s && isspace( *s ) ) ++s;
		if( *s ){
			trunc_str(s);
			pcf->lines.list[j] = s;
			++j;
		}
	}
	/* put in the terminating 0 */
	pc->optioncount = j - pc->options;
	pcf->lines.list[j++] = pcf->lines.list[i++];

	/* clear the unused entries at end */
	for( i = j; i < pcf->lines.count; ++i ){
		pcf->lines.list[i] = 0;
	}
	pcf->lines.count = j;

	DEBUG8("name %d, namecount %d, options %d, optioncount %d",
		pc->name, pc->namecount, pc->options, pc->optioncount );
	/* get a new entry */
	next_entry( pcf, 1 );

	return(status);
}

/***************************************************************************
 * sort_printcap - sort the information in the printcap entry
 ***************************************************************************/
static void sort_printcap( struct printcap *pc )
{
	struct printcapfile *pcf = pc->pcf;

	if( pc->ordered ) return;
	if( Debug > 9 ) dump_printcap( "before sorting", pc );

	qsort( &pcf->lines.list[pc->options], pc->optioncount,
		sizeof( char * ), mstrcmp );

	if( Debug > 9 ) dump_printcap( "after sorting", pc );

	pc->ordered = 1;
}

	
/***************************************************************************
 * char *doinclude( struct printcapfile *pcf, *	char *file );
 * 1. check the nesting depth; if too deep report error
 * 2. call Getprintcap() to parse file
 ***************************************************************************/
static int depth;
static void doinclude( struct printcapfile *pcf, char *file )
{
	if( ++depth  > 10 ){
		fatal( LOG_ERR, "doinclude: printcap file nesting too deep '%s'",
			file );
	}
	Getprintcap( pcf, file, 1 );
	--depth;
}

/***************************************************************************
 * dump_printcapfile( char *title, struct printcapfile *cf )
 * Dump printcapfile information
 ***************************************************************************/

void dump_printcap( char *title,  struct printcap *pc )
{
	int i;
	char **s;
	if( title ) logDebug( "*** printcap %s ***", title );
	if( pc ){
		logDebug( "name %d, namecount %d, options %d, optioncount %d",
			pc->name, pc->namecount, pc->options, pc->optioncount );
		logDebug( "printcapfile 0x%x", pc->pcf );
		s = &pc->pcf->lines.list[pc->name];
		for( i = 0; i < pc->namecount; ++i ){
			logDebug( "name  [%3d] = %3d-> '%s'", i, pc->name+i, s[i] );
		}
		s = &pc->pcf->lines.list[pc->options];
		for( i = 0; i < pc->optioncount; ++i ){
			logDebug( "option[%3d] = %3d-> '%s'", i, pc->options+i, s[i] );
		}
	}
}

void dump_printcapfile( char *title,  struct printcapfile *pcf )
{
	int i;
	if( title ) logDebug( "*** printcapfile %s ***", title );
	if( pcf ){
		logDebug(
			"printcap[] 0x%x, entrycount %d, fieldcount %d",
			pcf->pcs, pcf->pcs.count, pcf->lines.count );
		for( i = 0; i < pcf->pcs.count; ++i ){
			char buff[32];
			plp_snprintf( buff, sizeof(buff), "[%d]", i );
			dump_printcap( buff , &((struct printcap *)(pcf->pcs.list))[i] );
		}
	}
}

/***************************************************************************
 * Free_pcf( struct printcapfile *pcf )
 *  Free all of the memory in the  printcap buffer
 ***************************************************************************/
void Free_pcf( struct printcapfile *pcf )
{
	if( pcf ){
		clear_malloc_list( &pcf->pcs, 0 );
		clear_malloc_list( &pcf->lines, 0 );
		clear_malloc_list( &pcf->buffers, 1 );
		clear_malloc_list( &pcf->filters, 1 );
		memset( pcf, 0, sizeof( pcf[0] ) );
	}
}


/***************************************************************************
 * char *Get_first_printer( struct printcapfile *pcf )
 *  return the name of the first printer in the printcap file
 * 1. You may not have an entry - you may need to call a filter to get it.
 * 2. If you call a filter, you have to call the 'all' entry.
 * 3. If you have an 'all' entry, you use the first printer in the list
 * 4. This printer may be of the form printer@host
 ***************************************************************************/

char *Get_first_printer( struct printcapfile *pcfile )
{
	struct printcap *pc;		/* printcap entry */
	char *name = 0;				/* name of printer */
	char **lines;
	char error[LINEBUFFER];
	static char *namef;
	char *s;

	/*
	 * first, check for the first printer in the list, if any
	 */

	if( namef ) free(namef); namef = 0;

	if( pcfile->pcs.count > 0 ){
		lines = pcfile->lines.list;
		pc = (void *)pcfile->pcs.list;
		name = lines[pc->name];
	}
	DEBUG4("Get_first_printer: '%s'", name );
	if( name == 0 || strcmp( name, "all" ) == 0 ){
		/* get the all printcap entry;
		 * you want the printcap file
		 */
		name = 0;
		DEBUG4("Get_first_printer: checking for 'all'" );
		Get_printer_vars( "all", error, sizeof(error),
			pcfile, &Pc_var_list, Default_printcap_var, (void *)0 );
		if( All_list && *All_list ){
			/* we want the first name in the list */
			namef = safestrdup( All_list );
			name = namef;
			while( isspace( *name ) ) ++name;
			if( (s = strpbrk( name, ",:; \t" )) ) *s = 0;
		} else {
			name = 0;
		}
	}

	DEBUG4("Get_first_printer: found '%s'", name );
	return( name );
}

void Check_pc_table()
{
	int i;
	struct keywords *names;
	char *s, *t;

	names = Pc_var_list.names;
	for( i = 1; i < Pc_var_list.count; ++i ){
		s = names[i-1].keyword;
		t = names[i].keyword;
		if( t == 0 ) break;
		DEBUG8("Check_pc_table: '%s' ?? '%s'", s, t );
		if( strcmp( s, t ) > 0 ){
			fatal( LOG_ERR, "Check_pc_table: '%s' and '%s' out of order",
				s, t );
		}
	}
}

/***************************************************************************
 * Get_pc_option( char *option, struct printcap *pc )
 *  search the options for  the named option;
 *
 ***************************************************************************/

char *Get_pc_option( char *str, struct printcap *pc )
{
	char **options; 
	int min = 0;
	int max = pc->optioncount - 1;
	int i, c, len;
	char *s;

	DEBUG8("Get_pc_option: looking for '%s'", str );
	if( pc == 0 ) return( 0 );

	sort_printcap( pc );

	len = strlen(str);
	options = &pc->pcf->lines.list[pc->options];
	while( min <= max ){
		i = (min + max)/2;
		s = options[i];
		DEBUG8("Get_pc_option: min %d, max %d, [%d]->'%s'",
			min, max,i, s );
		c = strncmp( s, str, len );
		if( c == 0 ){
			/* if we have punctuation, then we are ok */
			if( s[len] == 0 || ispunct( s[len] ) ){
				return(s+len);
			}
			/* we are above the string */
		} else if( c < 0 ){
			min = i + 1;
		} else {
			max = i - 1;
		}
	}
	return( 0 );
}

/***************************************************************************
 * struct printcap *Get_pc_name( char *str, struct printcapfile *pcf,
 *		int *index );
 *  search the names for the printer
 *  - we start the search AFTER the pc entry; if pc == 0, start at beginning
 ***************************************************************************/

struct printcap *Get_pc_name( char *str, struct printcapfile *pcf,
	int *index )
{
	int i, j;
	char **names;
	char *s;
	struct printcap *pc;

	if( pcf == 0 ) return( 0 );

	i = *index;
	if( i < 0 ) i = 0;
	DEBUG8("Get_pc_name: checking for '%s', starting %d, max %d",
		str, i, pcf->pcs.count );
	for( ; i < pcf->pcs.count; ++i ){
		pc = &((struct printcap *)(pcf->pcs.list))[i];
		names = &pc->pcf->lines.list[pc->name];
		DEBUG8("Get_pc_name: entry [%d] '%s'", i, names[0] );
		for( j = 0; j < pc->namecount; ++ j ){
			s = names[j];
			DEBUG8("Get_pc_name: need '%s' have '%s'", str, s );
			if( strcmp( s, str ) == 0 ){
				*index = i+1;
				DEBUG8("Get_pc_name: found '%s'", str );
				return( pc );
			}
		}
	}

	/*
	 * if searching for the first time,  try getting from a filter
	 */
	pc = 0;
	DEBUG8("Get_pc_name: did not find '%s' in database", str );
	if( *index < 0 ){ 
		pc = Filterprintcap( str, pcf, index );
	}
	if( pc == 0 ){
		*index = i;
	}
	return( pc );
}

/***************************************************************************
 * Get_pc_vars( struct pc_var_list *keylist, struct printcap *pc,
 *     char *error, int errlen, struct malloc_list *pc_used );
 *
 * Search the printcap entry for values for the keywords in the list
 * Do the translation as required.
 *  Note: we do not allow list variables in the printcap file
 ***************************************************************************/

int Get_pc_vars( struct pc_var_list *keylist, struct printcap *pc,
	char *error, int errlen, struct pc_used *pc_used )
{
	char **options;
	char *s, *end;		/* option string */
	int c, n;
	struct keywords *key = keylist->names;
	struct sockaddr_in sin;		/* inet address */
	struct hostent *hostent;	/* host name */

	if( pc == 0 || key == 0 ){
		return(0);
	}

	sort_printcap( pc );

	/*
	 * check for 'oh' entry
	 */
	if( (s = Get_pc_option( "oh", pc )) ){
		/* get the IP address */
		DEBUG6("Get_pc_vars: oh entry '%s'", s );
		if( (end = strchr( s, '=' )) ) s = end+1;
		if( (hostent = gethostbyname(s)) ){
			/*
			 * set up the address information
			 */
			if( hostent->h_addrtype == AF_INET ){
				memcpy( &sin.sin_addr, hostent->h_addr, hostent->h_length );
			} else {
				/* did not work */
				return(0);
			}
		} else {
			DEBUG6("Get_pc_vars: '%s', trying inet address", s);
			sin.sin_addr.s_addr = inet_addr(s);
			if( sin.sin_addr.s_addr == -1){
				logerr(LOG_DEBUG, "Get_pc_vars: unknown host '%s'", s);
				return(0);
			}
		}
		DEBUG6("Get_pc_vars: oh IP '%08lx', HostIP '%08lx'",
			sin.sin_addr.s_addr, HostIP );
		if( sin.sin_addr.s_addr != HostIP ) return(0);
	}

	/*
	 * record the name of the printcap used
	 */

	Add_pc_to_list( pc, pc_used );

	options =  &pc->pcf->lines.list[pc->options];

	while( *options && key->keyword ){

		c = key->keyword[0] - (*options)[0];
		/* if c < 0, then key is below options, c > 0, then key above options */
		/* DEBUG8("Get_pc_vars: compare first char of key '%s' to '%s' = %d",
			key->keyword, *options,c); / **/
		if( c < 0 ){
			++key;
			continue;
		} else if( c > 0 ){
			++options;
			continue;
		}
		s = *options;
		n = strlen(key->keyword);
		c = strncmp( key->keyword, s, n );

		/* DEBUG8("Get_pc_vars: compare key '%s' to '%s' = %d",
			key->keyword,s,c); / **/
		if( c == 0 ){
			DEBUG8("Get_pc_vars: key '%s' value '%s'", s, &s[n] );
			/* now we do conversion */
			/* check for a non-alpha at the end of keyword */
			if( !isalnum( s[ n ] ) ){
				do_key_conversion( key, pc, s+n );
				++options;
				++key;
			} else {
				++key;
			}
		} else if( c < 0 ){
			++key;
		} else {
			++options;
		}
	}
	/* now we check to see if the Printcap entry had a 'tc'
	 * redirection entry
	 */
	DEBUG8("Get_pc_vars: checking for 'tc'" );
	if( (s = Get_pc_option( "tc", pc )) ){
		static int tc_depth;
		int lastpc;
		char *name;
		char copy[LINEBUFFER];	/* should be long enough, recursion!! */

		/* sigh, we now need to get and find the pc entry */
		DEBUG8("Get_pc_vars: found tc '%s", s );
		if( s[0] != '=' ){
			fatal( LOG_ERR, "Get_pc_values: wrong format for tc entry '%s'",s);
		}
		s = s+1;
		if( tc_depth > 10 ){
			fatal( LOG_ERR, "Get_pc_values: tc entry '%s' nesting too deep",s);
		}
		++tc_depth;
		/*
		 * find the variables
		 */
		if( strlen( s ) >= sizeof(copy)-1 ){
			fatal( LOG_ERR, "Get_pc_values: tc entry 'tc=%s' too long",s);
		}
		strcpy( copy, s );
		lastpc = -1;
		for( s = copy; s && *s; s = end ){
			end = strpbrk( s, ", \t" );
			if( end ){
				*end++ = 0;
			}
			while( (c = *s) && isspace( c ) ) ++s;
			if( c == 0 ) continue;
			if( (name = Printer_vars( s, error, errlen, pc->pcf,
						keylist, lastpc, pc_used )) ){
				DEBUG3( "Get_pc_vars: found pc entry '%s' for '%s'", name, s );
			} else {
				fatal( LOG_ERR, "Get_pc_vars: did not find tc entry '%s'", s );
			}
		}
		--tc_depth;
	}
	return(1);
}

/***************************************************************************
 * do_key_conversion: convert the printcap entry to a variable value
 * and do assignment
 ***************************************************************************/
static void do_key_conversion( struct keywords *key,
	struct printcap *pc, char *s )
{
	int i = 0, err = 0;
	char *end;		/* end of conversion */
	char *t;
	if( pc ){
		t = pc->pcf->lines.list[pc->name];
	} else {
		t = "<DEFAULTS>";
	}

	switch( key->type ){
	case FLAG_K:
		if( *s ){
			if( *s == '@' ){
				((int *)key->variable)[0] = ((int *)key->variable)[0];
			} else {
				fatal( LOG_ERR,
				"printcap '%s', bad flag value for '%s%s'", t, key->keyword,s);
			}
		} else {
			((int *)key->variable)[0] = 1;
		}
		DEBUG8("do_key_conversion: key '%s' FLAG value %d", key->keyword, 
			((int *)key->variable)[0]);
		break;
	case INTEGER_K:
		if( *s && *s != '#' ){
			err = 1;
		} else if( *s ){
			end = s+1;
			i = strtol( s+1, &end, 0 );
			if( (s+1 == end) ){
				err = 1;
				break;
			}
		}
		if( err ){
			fatal( LOG_ERR,
			"printcap '%s', bad integer value for '%s%s'", t, key->keyword,s);
		} else {
			((int *)key->variable)[0] = i;
		}
		DEBUG8("do_key_conversion: key '%s' INTEGER value %d", key->keyword, 
			((int *)key->variable)[0]);
		break;
	case STRING_K:
		i = 0;
		if( *s == 0 || *s != '=' ){
			fatal( LOG_ERR,
			"printcap '%s', bad string value for '%s%s'", t, key->keyword,s);
		} else {
			((char **)key->variable)[0] = s+1;
		}
		DEBUG8("do_key_conversion: key '%s' STRING value '%s'", key->keyword, 
			((char **)key->variable)[0] );
		break;
	default:
		fatal( LOG_ERR,
		"printcap '%s', variable '%s', unknown type",
			pc->pcf->lines.list[pc->name], key->keyword );
		break;
	}
}

/***************************************************************************
 * Initialize_pc_vars( struct printcapfile *pcf,
 *	struct pc_var_list *vars, char *line );
 *
 *  initialize the values in the keyword list with lines from 'line'
 *  1. the pc_var_list and initialization line must be sorted alphabetically
 *  2. we scan each of the lines, one by one.
 *  3.  for each of the entries in the keylist, we search for a match
 *  4.  when we find a match we set the value
 *  5. After setting default values, we then get the printcap file values
 ***************************************************************************/

void Initialize_pc_vars( struct printcapfile *pcf,
	struct pc_var_list *vars, char *init )
{
	char *s, *end;
	int compare, n;
	struct keywords *k;
	static char *init_copy;

	/* use a copy of the init and path */
	if( init_copy ){
		free(init_copy);
		init_copy = 0;
	}
	if( init ){
		init_copy = safestrdup( init );
	}

	DEBUG8("Initialize_pc_vars: input '%s'", init );
	if( vars ){
		for( k = vars->names; k->keyword; ++k ){
			if( k->flag ) continue;	/* do not set to 0 */
			switch( k->type ){
				case STRING_K: ((char **)(k->variable))[0] = 0; break;
				case INTEGER_K:
				case FLAG_K: ((int *)(k->variable))[0] = 0; break;
				default: break;
			}
		}
		k = vars->names;
		for( s = init_copy; s && *s; s = end ){
			end = strchr( s, '\n' );
			if( end ){
				*end++ = 0;
			}
			while( *s && isspace( *s ) ) ++s;
			if( *s == 0 ) continue;

		next_key:
			if( k->keyword == 0 ) break;
			compare = k->keyword[0] - s[0];

			/* DEBUG8("Initialize_pc_vars: compare letter key '%s' to '%s' %d",
				k->keyword,s,compare ); / **/
			/* negative: keyword below, postive: keyword above */
			if( compare < 0 ){
				++k;
				goto next_key;
			} else if( compare > 0 ){
				continue;
			}
			n = strlen( k->keyword );
			compare = strncmp( k->keyword, s, n ); 
			/* DEBUG8("Initialize_pc_vars: compare string key '%s' to '%s' %d",
				k->keyword,s,compare ); / **/
			if( compare == 0 ){
				/* check for a alphanumeric at the end of option value */
				/* if there is one, keyword is below */
				if( isalnum( s[ n ] ) ){
					compare = -1;
				}
			}
			if( compare < 0 ){
				++k;
				goto next_key;
			} else if( compare > 0 ){
				continue;
			}
			DEBUG8("Initialize_pc_vars: found '%s', value '%s' ",
				k->keyword, s+n );
			/* we have found the keyword.  Now set the value */
			do_key_conversion( k, 0, s+n );
			++k;
		}
		if( Debug > 8 ){
			dump_parms( "Initialize_pc_vars", vars->names );
		}
	}
}

/***************************************************************************
 * Get_printer_vars(
 *   char *name -  name to look up
 *   char *error, int errlen: - error message and maximum length
 *   struct printcapfile *printcapfile,
 *   struct pc_var_list *var_list - array of list of variables to set
 *   char *default - default values to set
 *   int server- if set, search for server information of form
 *    printer@hostname
 *   struct pc_used *pc_used - a list of the printcaps used
 * 1. search first for the name;
 *    if the first entry found has a different primary name,
 *    we repeat with the primary name.
 * 2. then search for name@ShortHost
 * 3. then search for name@FQDNHost
 ***************************************************************************/
char *Get_printer_vars( char *name, char *error, int errlen,
	struct printcapfile *printcapfile,
	struct pc_var_list *var_list, char *defval,
	struct pc_used *pc_used )
{
	int lastpc;
	char *primary_name;
	/* set printer name */
	DEBUG3( "Get_printer_vars: looking for '%s'", name );

	if( Debug > 8 ){
		int i;
		char **list;
		logDebug( "Get_printer_vars: filters '%d'",
			printcapfile->filters.count );
		list = printcapfile->filters.list;
		for( i = 0; i < printcapfile->filters.count; ++i ){
			logDebug( "Get_printers_vars: [%d] '%s'",
				i, list[i] );
		}
	}

	Initialize_pc_vars( printcapfile, var_list, defval );

	lastpc = -1;

	primary_name = Printer_vars( name, error, errlen,
		printcapfile, var_list, lastpc, pc_used );
	if( primary_name ){
		DEBUG3( "Get_printer_vars: found '%s'", name );
	} else {
		DEBUG3( "Get_printer_vars: did not find '%s'", name );
		plp_snprintf( error, errlen, "did not find '%s'", name );
		return( 0 );
	}
	return( primary_name );
}

/***************************************************************************
 * Printer_vars(
 *   char *name -  name to look up
 *   struct char *error, int errlen, - for errors
 *   struct printcapfile *printcapfile - printcap information
 *   struct keyword **var_list - array of list of variables to set
 *   int start - starting point of search
 *
 * 1. Search the printcap for printer
 * 2. If found, read the printcap information, else set error and return 1
 * 3. If spool directory has a printcap, read it as well
 *
 * Note: we search the printcap entries for the actual name in the name list.
 * we return the first name of the first entry where it is found.
 * You may need to repeat this search again with the 'primary' name
 *  if this name is different.
 ***************************************************************************/

static char *Printer_vars( char *name, char *error, int errlen,
	struct printcapfile *printcapfile, struct pc_var_list *var_list,
	int start, struct pc_used *pc_used )
{
	struct printcap *pc, *tpc;
	int lastpc; 
	int firstpc = start;
	char *retname;
	struct stat statb;
	int fd;
	char *path;
	struct dpathname dpath;
	int found;

	/* set printer name */
	DEBUG3( "Printer_vars: looking for '%s', starting at %d", name, start );
	lastpc = start;

	found = 0;
	if( (pc = Get_pc_name( name, printcapfile, &lastpc )) ){
		tpc = (void *)(printcapfile->pcs.list);
		firstpc = pc - tpc;
		retname = pc->pcf->lines.list[pc->name];
		DEBUG3( "Printer_vars: printer '%s' found, Printer '%s', firstpc %d",
			name, retname, firstpc );
		do{
			found |= Get_pc_vars( var_list, pc, error, errlen, pc_used );
		} while( (pc = Get_pc_name( name, printcapfile, &lastpc )) );
	} else {
		plp_snprintf( error, errlen, "unknown printer '%s'", name );
		return(0);
	}
	if( found == 0 ){
		plp_snprintf( error, errlen, "printer '%s' not used on host", name );
		return(0);
	}
	/*
	 * Security alert:
	 * If the spool directory is NFS mounted, then you do NOT
	 * want to read the printcap information from the spool directory
	 * Any host could modify it and create havoc.
	 * We can also use a separate control directory to solve this problem.
	 * Note that we can have both conditions...
	 */
	path = 0;
	if( Spool_dir && *Spool_dir ){
		path = Spool_dir;
	}
	if( Control_dir && *Control_dir ){
		path = Control_dir;
	}
	if( (!NFS_spool_dir || Control_dir ) && path ){
		Init_path( &dpath, path );
		path = Add_path( &dpath, "printcap" );

		printcapfile->dpath = dpath;
		fd = Checkread( path, &statb );
		if( fd >= 0 ){
			DEBUG4("Printer_vars: reading file '%s'", path);
			if( Readprintcap( printcapfile, path, fd, &statb ) ){
				log( LOG_ERR, "Cannot process printcap file %s", path);
			} else {
				DEBUG4("Printer_vars: checking printcap file '%s'", path);
				while( (pc = Get_pc_name( name, printcapfile, &lastpc )) ){
					Get_pc_vars( var_list, pc, error, errlen, pc_used );
				}
			}
		}
		close(fd);
	}
	DEBUG4("Printer_vars: printer '%s' returning '%s' Spool_dir '%s'",
		name, retname, Spool_dir );
	return(retname);
}

/***************************************************************************
 * Get_printer_comment( char *name )
 *   char *name -  name to look up
 * look for the comment field of the printer name
 ***************************************************************************/

char *Get_printer_comment( struct printcapfile *printcapfile, char *name )
{
	struct printcap *pc;
	int lastpc;
	char *s;

	/* set printer name */
	s = 0;
	DEBUG3( "Get_printer_comment: looking for '%s'", name );

	lastpc = -1;
	if( (pc = Get_pc_name( name, printcapfile, &lastpc )) && pc->namecount > 1 ){
		/* now we get the last one */
		s = pc->pcf->lines.list[pc->name+pc->namecount-1];
		DEBUG3( "Get_printer_comment: printer '%s' found, last '%s'",
			name, s );
	}
	return(s);
}

/***************************************************************************
 * char *Search_option_val( char *option, struct pc_used *pc_used )
 *  search the printcap for printer 'name' for the option 'option'
 *
 ***************************************************************************/

char *Search_option_val( char *option, struct pc_used *pc_used )
{
	struct malloc_list *pcl;
	struct printcap **ppc, *pc;
	int i, j, count, len;
	char **pc_lines, *s = 0;

	pcl = &pc_used->pc_used;
	ppc = (void *)pcl->list;
	len = strlen( option );

	DEBUG8("Search_option_val: count %d", pcl->count );
	for( i = 0; i < pcl->count; ++i ){
		pc = ppc[i];
		DEBUG8("Search_option_val: entry '%s'", pc->pcf->lines.list[pc->name] );
		pc_lines = &pc->pcf->lines.list[pc->options];
		count = pc->optioncount;
		for( j = 0; j < count; ++j ){
			DEBUG8("Search_option_val: [%d] '%s'", j, pc_lines[j] );
			if( strncmp( option, pc_lines[j], len ) == 0 ){
				s = pc_lines[j];
				DEBUG8("Search_option_val: found '%s'", s?s:"NULL" );
			}
		}
	}
	if( s ) s += len;

	DEBUG8("Search_option_val: option '%s' value '%s'", option, s?s:"NULL" );
	return( s );
}

/***************************************************************************
 * Add_pc_to_list:
 *  record the printcap entry that was used to produce the printcap
 *  information.  This will later be expanded, if necessary
 ***************************************************************************/
static void Add_pc_to_list( struct printcap *pc, struct pc_used *pc_used )
{
	struct printcap **ppc;
	struct malloc_list *pcl;
	if( pc && pc_used ){
		pcl = &pc_used->pc_used;
		if( pcl->count >= pcl->max ){
			extend_malloc_list( pcl, sizeof( ppc[0] ), 100 );
		}
		ppc = (void *)pcl->list;
		ppc[ pcl->count++ ] = pc;
	}
}

/***************************************************************************
 * Linearize_pc_list:
 *  linearize the printcap information
 *  1. the name information from the first printcap entry will be
 *     used as the main name information
 *  2. for each printcap entry in the list,  get the name information
 *  3. extend the simple list by the number of entries
 *  4. merge sort the entries,  discarding the oldest ones
 *  5. return the merged list
 *
 ***************************************************************************/

char *Linearize_pc_list( struct pc_used *pc_used, char *parm_name )
{
	struct malloc_list *pcl, *list;
	struct printcap **ppc, *pc;
	int i, j, len, c;
	int count, oldcount, total;
	char **newlines, **oldlines, **pc_lines;
	static struct malloc_list working;
	char *s, *line;
	char *name;						/* name of the printcap entry */

	pcl = &pc_used->pc_used;
	ppc = (void *)pcl->list;

	/* clear the entries */

	list = &pc_used->pc_list;
	clear_malloc_list( list, 0 );

	/* get the printcap name */
	pc = ppc[0];
	name = pc->pcf->lines.list[pc->name];

	/* work your way down the list */

	DEBUG8("Linearize_pc_list: count %d", pcl->count );
	for( i = 0; i < pcl->count; ++i ){
		pc = ppc[i];
		DEBUG8("Linearize_pc_list: entry '%s'", pc->pcf->lines.list[pc->name] );
		pc_lines = &pc->pcf->lines.list[pc->options];
		count = pc->optioncount;
		for( j = 0; j < count; ++j ){
			DEBUG8("Linearize_pc_list: [%d] '%s'", j, pc_lines[j] );
		}
		/*
		 * we extend the list
		 */
		total = count + list->count + 1;
		while( list->max < total ){
			extend_malloc_list( list, sizeof( char * ), 100 );
		}
		while( working.max < total ){
			extend_malloc_list( &working, sizeof( char * ), 100 );
		}
		/*
		 * we now have room to add the entries
		 * we will use a variation of merge sort
		 */
		oldlines = list->list;
		newlines = working.list;
		working.count = 0;
		oldcount = 0;
		/* work our way through the new list */
		for( j = 0; j < count;  ){
			if( oldcount < list->count ){
				/* we need to do a comparison */
				line = oldlines[oldcount];
				/* we check to find the key, i.e. =, # or @ */
				s = strpbrk( line, "=#@" );
				/* get the length of name */
				if( s ){
					len = s - line;
				} else {
					len = strlen( line );
				}
				c = strncasecmp( line, pc_lines[j], len );
				/*DEBUG9("Linearize_pc_list: compare=%d len %d old '%s' to new '%s'",
					c, len, line, pc_lines[j] ); */
				if( c == 0 ){
					/* we throw away the old entry */
					newlines[ working.count++ ] = pc_lines[j++];
					++oldcount;
				} else if( c < 0 ){
					newlines[ working.count++ ] = line;
					++oldcount;
				} else {
					newlines[ working.count++ ] = pc_lines[j++];
				}
			} else {
				/* we copy the elements in place */
				newlines[ working.count++ ] = pc_lines[j++];
			}
		}
		/* we ran out of new lines, check for the old ones left */
		while( oldcount < list->count ){
			newlines[ working.count++ ] = oldlines[oldcount++];
		}
		newlines[ working.count ] = 0;
		/* now we copy to the original list */
		memcpy( list->list, working.list, (working.count+1)*sizeof( char * ) );
		list->count = working.count;
	}

	len = 0;
	if( parm_name ) len += strlen( parm_name )+1;

	/* get the printer name */
	DEBUG9("Linearize_pc_list: name '%s', %d entries", name, list->count );
	len += strlen(name) + 2;
	pc_lines = list->list;
	for( i = 0; i < list->count; ++i ){
		DEBUG9("Linearize_pc_list: sorted [%d] '%s`", i, pc_lines[i] );
		len += strlen( pc_lines[i] ) + 3;
	}
	len += 1;
	if( len > pc_used->size ){
		if( pc_used->block ) free( pc_used->block );
		pc_used->block = 0;
		pc_used->size = len;
		malloc_or_die( pc_used->block, len );
	}
	s = pc_used->block;
	s[0] = 0;
	if( parm_name ) strcpy( s, parm_name );
	s += strlen(s);
	strcpy( s, name );
	s += strlen(s);
	*s++ = '\n';
	for( i = 0; i < list->count; ++i ){
		*s++ = ' ';
		*s++ = ':';
		strcpy( s, pc_lines[i] );
		s += strlen(s);
		*s++ = '\n';
	}
	*s = 0;
	DEBUG9("Linearize_pc_list: result '%s'", pc_used->block );
	return( pc_used->block );
}

void Clear_pc_used( struct pc_used *pc_used )
{
	clear_malloc_list( &pc_used->pc_used, 0 );
}

/*
 * char *Find_filter( int key, struct pc_used *pc_used )
 *  search the printcap until you find the filter specified
 * key = key to search for, i.e.- 'o' = "of"
 */

char *Find_filter( int key, struct pc_used *pc_used )
{
	static char option[] = "Xf";
	char *s = 0;

	option[0] = key;
	
	s = Search_option_val( option, pc_used );
	if( s && *s == '='){
		++s;
	}
	return( s );
}

/*
 * printcap variables used by LPD for printing
 * THESE MUST BE IN SORTED ORDER
 */
static struct keywords long_pc_vars[]
 = {
	{ "ab",  FLAG_K,  &Always_banner },
		 /*  always print banner, ignore lpr -h option */
	{ "ac",  STRING_K,  &Allow_class },
		 /*  allow these classes to be printed */
	{ "achk",  FLAG_K,  &Accounting_check },
		 /*  query accounting server when connected */
	{ "ae",  STRING_K,  &Accounting_end },
		 /*  accounting at end (see also af, la, ar, as) */
	{ "af",  STRING_K,  &Accounting_file },
		 /*  name of accounting file (see also la, ar) */
	{ "ah",  FLAG_K,  &Auto_hold },
		 /*  automatically hold all jobs */
	{ "all",  STRING_K,  &All_list },
		
	{ "ar",  FLAG_K,  &Accounting_remote },
		 /*  write remote transfer accounting (if af is set) */
	{ "as",  STRING_K,  &Accounting_start },
		 /*  accounting at start (see also af, la, ar) */
	{ "be",  STRING_K,  &Banner_end },
		 /*  end banner printing program overides bp */
	{ "bk",  FLAG_K,  &Backwards_compatible },
		 /*  backwards-compatible: job file strictly RFC-compliant */
	{ "bkf",  FLAG_K,  &Backwards_compatible_filter },
		 /*  backwards-compatible filters: use simple paramters */
	{ "bl",  STRING_K,  &Banner_line },
		 /*  short banner line sent to banner printer */
	{ "bp",  STRING_K,  &Banner_printer },
		 /*  banner printing program (see ep) */
	{ "bq",  STRING_K,  &Bounce_queue_dest },
		 /*  use filters on bounce queue files */
	{ "br",  INTEGER_K,  &Baud_rate },
		 /*  if lp is a tty, set the baud rate (see ty) */
	{ "bs",  STRING_K,  &Banner_start },
		 /*  banner printing program overrides bp */
	{ "cd",  STRING_K,  &Control_dir },
		 /*  control directory */
	{ "cm",  STRING_K,  &Comment_tag },
		 /*  comment identifying printer (LPQ) */
	{ "co",  INTEGER_K,  &Cost_factor },
		 /*  cost in dollars per thousand pages */
	{ "connect_grace", INTEGER_K, &Connect_grace },
		 /* connection control for remote printers */
	{ "connect_interval", INTEGER_K, &Connect_interval, 0, 1 },
		 /* connection control for remote printers */
	{ "connect_retry", INTEGER_K, &Connect_try, 0, 1 },
		 /* connection control for remote printers */
	{ "connect_timeout", INTEGER_K, &Connect_timeout, 0, 1 },
		 /* connection control for remote printers */
	{ "db",  STRING_K,  &New_debug },
		 /*  debug level set for queue handler */
	{ "fc",  INTEGER_K,  &Clear_flag_bits },
		 /*  if lp is a tty, clear flag bits (see ty) */
	{ "fd",  FLAG_K,  &Forwarding_off },
		 /*  if true, no forwarded jobs accepted */
	{ "ff",  STRING_K,  &Form_feed },
		 /*  string to send for a form feed */
	{ "fo",  FLAG_K,  &FF_on_open },
		 /*  print a form feed when device is opened */
	{ "fq",  FLAG_K,  &FF_on_close },
		 /*  print a form feed when device is closed */
	{ "fs",  INTEGER_K,  &Set_flag_bits },
		 /*  like `fc' but set bits (see ty) */
	{ "fx",  STRING_K,  &Formats_allowed },
		 /*  valid output filter formats */
	{ "hl",  FLAG_K,  &Banner_last },
		 /*  print banner after job instead of before */
	{ "if",  STRING_K,  &IF_Filter },
		 /*  filter command, run on a per-file basis */
	{ "la",  FLAG_K,  &Local_accounting },
		 /*  write local printer accounting (if af is set) */
	{ "ld",  STRING_K,  &Leader_on_open },
		 /*  leader string printed on printer open */
	{ "lf",  STRING_K,  &Log_file },
		 /*  error log file (servers, filters and prefilters) */
	{ "lk", FLAG_K,  &Lock_it },
		 /* lock the IO device */
	{ "logger_destination",  STRING_K,  &Logger_destination, 0, 1 },
	{ "longnumber",  FLAG_K,  &Long_number, 0, 1 },
		 /*  use long job number when a job is submitted */
	{ "lp",  STRING_K,  &Lp_device },
		 /*  device name or lp-pipe command to send output to */
	{ "mc",  INTEGER_K,  &Max_copies },
		 /*  maximum copies allowed */
	{ "mi",  STRING_K,  &Minfree },
		 /*  minimum space (Kb) to be left in spool filesystem */
	{ "ml",  INTEGER_K,  &Min_printable_count },
		 /*  minimum printable characters for printable check */
	{ "mx",  INTEGER_K,  &Max_job_size },
		 /*  maximum job size (1Kb blocks, 0 = unlimited) */
	{ "nb",  INTEGER_K,  &Nonblocking_open },
		 /*  maximum copies allowed */
	{ "nw",  FLAG_K,  &NFS_spool_dir },
		 /*  spool dir is on an NFS file system (see rm, rp) */
	{ "of",  STRING_K,  &OF_Filter },
		 /*  output filter, run once for all output */
	{ "pl",  INTEGER_K,  &Page_length },
		 /*  page length (in lines) */
	{ "pr",  STRING_K,  &Pr_program },
		 /*  pr program for p format */
	{ "ps",  STRING_K,  &Status_file },
		 /*  printer status file name */
	{ "pw",  INTEGER_K,  &Page_width },
		 /*  page width (in characters) */
	{ "px",  INTEGER_K,  &Page_x },
		 /*  page width in pixels (horizontal) */
	{ "py",  INTEGER_K,  &Page_y },
		 /*  page length in pixels (vertical) */
	{ "qq",  FLAG_K,  &Use_queuename },
		 /*  put queue name in control file */
	{ "rm",  STRING_K,  &RemoteHost },
		 /*  remote-queue machine (hostname) (with rm) */
	{ "router",  STRING_K,  &Routing_filter },
		 /*  routing filter, returns destinations */
	{ "rp",  STRING_K,  &RemotePrinter },
		 /*  remote-queue printer name (with rp) */
	{ "rs",  INTEGER_K,  &Rescan_time },
		 /*  remote-queue printer name (with rp) */
	{ "rt",  INTEGER_K,  &Send_try },
		 /*  number of times to try printing or transfer (0=infinite) */
	{ "rw",  FLAG_K,  &Read_write },
		 /*  open the printer for reading and writing */
	{ "save_when_done",  FLAG_K,  &Save_when_done, 0, 1 },
		 /*  save job when done */
	{ "sb",  FLAG_K,  &Short_banner },
		 /*  short banner (one line only) */
	{ "sc",  FLAG_K,  &Suppress_copies },
		 /*  suppress multiple copies */
	{ "sd",  STRING_K,  &Spool_dir },
		 /*  spool directory (only ONE printer per directory!) */
	{ "send_data_first", FLAG_K, &Send_data_first, 0, 1 },
		 /* failure actiont to take after send_try attempts failed */
	{ "send_failure_action", STRING_K, &Send_failure_action, 0, 1 },
		 /* failure action to take after send_try attempts failed */
	{ "send_timeout", INTEGER_K, &Send_timeout, 0, 1 },
		 /* timeout for each write to device to complete */
	{ "send_try", INTEGER_K, &Send_try, 0, 1 },
		 /* numbers of times to try sending job - 0 is infinite */
	{ "sf",  FLAG_K,  &No_FF_separator },
		 /*  suppress form feeds separating multiple jobs */
	{ "sh",  FLAG_K,  &Suppress_header },
		 /*  suppress headers and/or banner page */
	{ "ss",  STRING_K,  &Server_queue_name },
		 /*  name of queue that server serves (with sv) */
	{ "sv",  STRING_K,  &Server_names },
		 /*  names of servers for queue (with ss) */
	{ "sy",  STRING_K,  &Stty_command },
		 /*  stty commands to set output line characteristics */
	{ "tr",  STRING_K,  &Trailer_on_close },
		 /*  trailer string to print when queue empties */
	{ "translate_format",  STRING_K,  &Xlate_format },
		 /*  translate format from one to another - similar to tr(1) utility */
	{ "ty",  STRING_K,  &Stty_command },
		 /*  stty commands to set output line characteristics */
	{ "use_identifier",  FLAG_K,  &Use_identifier, 0, 1 },
		 /*  put identifier in control file */
	{ "use_shorthost",  INTEGER_K,  &Use_shorthost, 0, 1 },
		 /*  Use short hostname for lpr control and data file names */
	{ "xc",  INTEGER_K,  &Clear_local_bits },
		 /*  if lp is a tty, clear local mode bits (see ty) */
	{ "xs",  INTEGER_K,  &Set_local_bits },
		 /*  like `xc' but set bits (see ty) */
	{ "xt",  FLAG_K,  &Check_for_nonprintable, 0, 1 },
		/*  formats to check for printable files */
	{ "xu",  STRING_K,  &Local_permission_file },
		 /*  additional permissions file for this queue */
	{ 0 }
   }
;

struct pc_var_list Pc_var_list
  = {
	long_pc_vars,
	sizeof( long_pc_vars )/ sizeof( long_pc_vars[0] )
	}
;
