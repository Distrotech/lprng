/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: printcap.c
 * PURPOSE: Process a printcap file
 **************************************************************************/

static char *const _id =
"printcap.c,v 3.12 1998/03/24 02:43:22 papowell Exp";

#include "lp.h"
#include "printcap.h"
#include "malloclist.h"
#include "merge.h"
#include "fileopen.h"
#include "setup_filter.h"
#include "pathname.h"
#include "dump.h"
#include "gethostinfo.h"
#include "globmatch.h"
#include "cleantext.h"
#include "getcnfginfo.h"
/**** ENDINCLUDE ****/

/***************************************************************************

Commentary:
Patrick Powell Sun Apr 23 15:21:29 PDT 1995
  Sun Nov 17 06:34:55 PST 1996

Printcap files and printcap entry:

A printcap entry is contains all the information associated
are used to process printcap information

 ***************************************************************************/

struct printcap_entry *Parse_raw_printcap( int first_line, char *key,
	char *path, struct file_entry *raw );
void Fix_raw_pc( struct printcap_entry *pc, char *path );
char **Get_pc_option_entry( char *str, struct printcap_entry *pc );
extern int Combine_options( char *name, struct printcap_entry *pc, char *key,
	int start_index );
extern int Filterprintcap( char *name, char *key, int *start_index,
	struct file_entry *raw, int break_on_line );
struct printcap_entry *Create_expanded_entry(
	struct printcap_entry *entry, char *key, int start_index );

/***************************************************************************
 * Free_printcap_info( struct printcap_entry *entry )
 *  Free all of the printcap information that is in the
 *  struct printcap_entry data structure.  This is usually
 *  only the line count information
 ***************************************************************************/



void Free_printcap_entry( struct printcap_entry *entry )
{
	struct malloc_list namelines, lines;
	lines = entry->lines;
	namelines = entry->namelines;

	memset( entry, 0, sizeof(entry[0]) );
	entry->lines = lines;
	entry->namelines = namelines;
	clear_malloc_list( &entry->lines, 0 );
	clear_malloc_list( &entry->namelines, 0 );
}

void dump_printcap_entry ( char *title,  struct printcap_entry *entry )
{
	
	int i;
	char **lines;
	struct malloc_list *list;
	if( title ) logDebug( "*** printcap %s ***", title );
	if( entry ){
		lines = entry->names;
		logDebug( "name 0x%x, namecount %d", lines, entry->namecount );
		logDebug( "  key 0x%x '%s', status_done %d, checked %d",
			entry->key, entry->key, entry->status_done, entry->checked );
		if( lines ){
			for( i = 0; i <= entry->namecount; ++i ){
				logDebug( "  name [%d] 0x%x '%s'", i, lines[i], lines[i] );
			}
		}
		lines = entry->options;
		logDebug( "options 0x%x, optioncount %d", lines, entry->optioncount  );
		if( lines ){
			for( i = 0; i <= entry->optioncount; ++i ){
				logDebug( "  option [%d] 0x%x '%s'", i, lines[i], lines[i] );
			}
		}
		list = &entry->namelines;
		logDebug(" malloc_list namelines list 0x%x, count %d, max %d, size %d",
            list->list, list->count, list->max, list->size );
		list = &entry->lines;
		logDebug(" malloc_list lines list 0x%x, count %d, max %d, size %d",
            list->list, list->count, list->max, list->size );
	}
}

/***************************************************************************
 * Free_file_entry( struct printcap_file_info *entry )
 *  Free the buffers or set list lengths to 0
 * Free_printcap_file_entry( struct printcap_file_info *entry )
 *  Free the printcap information
 *  Call Free_file_entry()
 ***************************************************************************/

void Free_file_entry( struct file_entry *entry )
{
	/* next, free all allocated buffers */
	clear_malloc_list( &entry->files, 1 );
	clear_malloc_list( &entry->entries, 0 );
	clear_malloc_list( &entry->filters, 1 );
	clear_malloc_list( &entry->printcaps, 0 );
	clear_malloc_list( &entry->expanded_str, 1 );
	entry->initialized = 0;
}

void Free_printcap_file_entry( struct file_entry *entry )
{ 
	int i;
	struct printcap_entry *printcaps;

	/* clear the printcap information */
	printcaps = (void *)entry->printcaps.list;
	for( i = 0; i < entry->printcaps.count; ++i ){
		Free_printcap_entry( &printcaps[i] );
	}
	Free_file_entry( entry );
}

void dump_printcap_file_entry ( char *title, struct file_entry *entry )
{
	int i;
	char **lines;
	struct malloc_list *list;
	struct printcap_entry *printcaps;

	/* first, free all file information */
	if( title ) logDebug( "*** printcap file_entry %s ***", title );
	list = &entry->files;
	lines = list->list;
	logDebug(" file malloc_list list 0x%x, count %d, max %d, size %d",
		list->list, list->count, list->max, list->size );
	if( lines ){
		for( i = 0; i < list->count; ++i ){
			logDebug(" file [%d] 0x%x '%s'", i, lines[i], lines[i] );
		}
	}
	
	list = &entry->entries;
	lines = list->list;
	logDebug(" entries malloc_list list 0x%x, count %d, max %d, size %d",
		list->list, list->count, list->max, list->size );
	if( lines ){
		for( i = 0; i < list->count; ++i ){
			logDebug(" entry [%d] 0x%x '%s'", i, lines[i], lines[i] );
		}
	}

	list = &entry->filters;
	lines = list->list;
	logDebug(" filter malloc_list list 0x%x, count %d, max %d, size %d",
		list->list, list->count, list->max, list->size );
	if( lines ){
		for( i = 0; i < list->count; ++i ){
			logDebug(" filter [%d] 0x%x '%s'", i, lines[i], lines[i] );
		}
	}

	/* now dump the printcap information */
	printcaps = (void *)entry->printcaps.list;
	for( i = 0; i < entry->printcaps.count; ++i ){
		char t[32];
		plp_snprintf( t, sizeof(t), "raw printcap [%d]", i );
		dump_printcap_entry( t,  &printcaps[i] );
	}
}

void Free_printcap_information( void )
{
	int i;
	struct printcap_entry *printcaps;

	/* release the allocated information */
	Free_printcap_file_entry( &Raw_printcap_files );
	Raw_printcap_files.initialized = 0;
	/* now clear the printcap information */
	printcaps = (void *)Expanded_printcap_entries.list;
	for( i = 0; i < Expanded_printcap_entries.count; ++i ){
		Free_printcap_entry( &printcaps[i] );
	}
	Expanded_printcap_entries.count = 0;
}

void dump_printcap_information ( char *title )
{
	int i;
	struct printcap_entry *printcaps;
	struct malloc_list *list;

	if( title ) logDebug( "*** printcap information %s ***", title );

	dump_printcap_file_entry( " Raw_printcap_files", &Raw_printcap_files );

	list = &Expanded_printcap_entries;
	printcaps = (void *)list->list;
	logDebug("Expanded_printcap malloc_list list 0x%x, count %d, max %d, size %d",
		list->list, list->count, list->max, list->size );
	if( printcaps ){
		for( i = 0; i < list->count; ++i ){
			char t[32];
			plp_snprintf( t, sizeof(t), "expanded printcap [%d]", i );
			dump_printcap_entry( t,  &printcaps[i] );
		}
	}
}

/***************************************************************************
 * void Getprintcap_pathlist( char *path )
 * Read printcap information from a (semi)colon or comma separated set of files
 *   or filter specifications
 *   1. break path up into set of path names
 *   2. read the printcap information into memory
 *   3. parse the printcap informormation
 ***************************************************************************/

void Getprintcap_pathlist( char *path, char *key, struct file_entry *raw )
{
	char *end;
	int fd, c;
	int lines;
	struct stat statb;
	char entry[MAXPATHLEN];
	int err;

	DEBUGF(DDB1)("Getprintcap_pathlist: paths '%s'", path );
	/* copy to the temporary place */
	if( path ) safestrncpy( entry, path );

	for(path = entry; path && *path; path = end ){
		end = strpbrk( path, ";:," );
		if( end ){
			*end++ = 0;
		}
		trunc_str( path );
		while( (c = *path) && isspace(c) ) ++path;
		if( c == 0 ) continue;
		DEBUGF(DDB1)( "Get_printcap_pathlist: file '%s'", path );
		switch( c ){
		case '|':
			DEBUGF(DDB2)("Getprintcap_pathlist: filter '%s'", path );
			add_str( &raw->filters, path,__FILE__,__LINE__  );
			break;

		case '/':
			fd =  Checkread( path, &statb );
			err = errno;
			if( fd < 0 ){
				DEBUGF(DDB1)( "Getprintcap_pathlist: cannot open '%s' - %s",
					path, Errormsg(err) );
			} else if( fd >= 0 ){
				char *s;
				DEBUGF(DDB2)("Getprintcap_pathlist: file '%s', size %d",
					path, statb.st_size );
				add_str( &raw->files, path,__FILE__,__LINE__  );
				if( (s = Readprintcap( path, fd, &statb, raw)) == 0 ){
					log( LOG_ERR, "Error reading %s", path );
				} else {
					lines = Parse_pc_buffer( s, path, raw,0 );
					Parse_raw_printcap( lines, key, path, raw );
				}
			}
			close(fd);
			break;
		default:
			fatal( LOG_ERR,
				"Getprintcap_pathlist: entry not filter or absolute pathname '%s'",
				path );
		}
	}

	DEBUGFC(DDB1){
		dump_printcap_information( "Getprintcap_pathlist" );
	}
}


/***************************************************************************
 * char *Readprintcap( char *file, int fd, struct statb *statb)
 *  - get the size of file
 *  - expand printcap buffer
 *  - read file into expanded buffer
 *  - parse file
 ***************************************************************************/

char *Readprintcap( char *file, int fd, struct stat *statb,
		struct file_entry *raw )
{
	int i, len;				/* ACME Integers and pointers */
	char *begin = 0;			/* beginning of data */
	char *s;			/* ACE, cheaper and better */

	/* check the file */
	if( fd < 0 ){
		logerr( LOG_DEBUG, "Readprintcap: bad fd for '%s'", file );
	} else {
		/* malloc data structures */
		
		DEBUGF(DDB2)("Readprintcap: file '%s' size %d", file, statb->st_size );
		begin = add_buffer( &raw->files, statb->st_size+1,__FILE__,__LINE__  );
		DEBUGF(DDB2)("Readprintcap: buffer 0x%x", begin );

		s = begin;
		for( len = 1, i = statb->st_size;
			i > 0 && (len = read( fd, s, i)) > 0;
				i -= len, s += len );
		*s = 0;
		if( len <= 0 ){
			logerr( LOG_ERR, "Readprintcap: cannot read '%s'", file );
			begin = 0;
		}
	}
	return( begin );
}

/***************************************************************************
 * int Filterprintcap( char *name, key, int **start_index )
 *  - for each entry in the printcapfile filters list do the following:
 *
 *    - make the filter, sending it the 'name' for access
 *    - read from the filter until EOF
 *    - kill off the filter process
 *    - if entry read, add it to the printcap file
 *    - parse the entry and add raw printcap information
 *      each printcap will be tagged with 'key'
 *    - return the number of new printcap entries and set
 *      *start_index to the first one.
 * RETURN: 0 if no entries found, 1 if more found.
 ***************************************************************************/

int Filterprintcap( char *name, char *key, int *start_index,
	struct file_entry *raw, int break_on_line )
{
	char *buffer = 0;				/* buffer list and buffer */
	char **filters, *filter;	/* list of filter and current filter */
	int i;					/* ACME again! for quality */
	int lines;		/* lines */
	int count;
	int raw_count;
	int found;

	/* get the printer name to send */
	count = raw->filters.count;
	raw_count = raw->printcaps.count;
	if( start_index ) *start_index = 0;
	DEBUGF(DDB2)("Filterprintcap: filters '%d'", count);
	/* create a pipe to read the file into */
	filters = raw->filters.list;
	for( i = 0; i < count; ++i ){
		filter = filters[i];
		buffer = Filter_read( name, &raw->files, filter );
		if( buffer && *buffer ){
			/* now we can parse the printcap entry */
			lines = Parse_pc_buffer( buffer, filter, raw, break_on_line);
			Parse_raw_printcap( lines, key, filter, raw );
		}
	}
	found = raw->printcaps.count - raw_count;
	return( found );
}


/***************************************************************************
 * char ** Parse_pc_buffer( char *buffer, char *pathname,
 *  struct file_entry *raw, int break_on_lines )
 * # comment - ignore to end of line
 * nnnn  <- primary printcap name
 * |nnnn <- secondary printcap name
 * :nnnn <- secondary printcap name
 * include filename
 *  -- if break_on_line is 0 Terminators are \n, :, |
 *  -- if break_on_line is 1 Terminators are \n
 *   If odd number of \ precedes : or \n, it is escaped.
 *     escaped : is left in place
 *     escaped \n and \ is replaced by white space.
 * 
 * The include will be replaced by a buffer.
 * 
 * Each entry found is added to the raw printcap line information.
 * 
 * Return: 0  if a parse error
 *         pointer to first entry in lines for file
 * 
 ***************************************************************************/

int Parse_pc_buffer( char *buffer, char *pathname,
	struct file_entry *raw, int break_on_line )
{
	struct malloc_list *line_list = &raw->entries;
	char **lines;
	int first_line;	/* lines and index of first line in list */
	char *start, *end, *s;
	int backslash_count;	/* number of backslashes before end char */
	int end_c;		/* Acme, Registered TM */
	static int include_depth;	/* depth of included files */

	DEBUGF(DDB2)("Parse_pc_buffer: path '%s'", pathname );
	/* allocate a line list and get the first entry */
	if( line_list->count+1 >= line_list->max ){
		extend_malloc_list( line_list, sizeof( lines[0]),
			line_list->count+100,__FILE__,__LINE__ );
	}
	lines = line_list->list;
	first_line = line_list->count;

	/* now start parsing the file */
	for( start = buffer; start && *start; start = end ){
		/* find the start of entry */
		DEBUGFC(DDB4){
			char smallpart[32];
			char *ss;
			strncpy( smallpart, start, sizeof(smallpart) -1 );
			smallpart[31] = 0;
			if( (ss = strchr(smallpart, '\n' )) ) *ss = 0;
			logDebug("Parse_pc_buffer: start '%s'", smallpart );
		}
		while( isspace( *start ) ) *start++ = 0;
		DEBUGFC(DDB4){
			char smallpart[32];
			char *ss;
			strncpy( smallpart, start, sizeof(smallpart) -1 );
			smallpart[31] = 0;
			if( (ss = strchr(smallpart, '\n' )) ) *ss = 0;
			logDebug("Parse_pc_buffer: start after spaces '%s'", smallpart );
		}

		end = start;
		/* check for end of file */
		if( *start == 0 ){
			continue;
		}
		/* throw away comment */
		if( *start == '#' ){
			*start = 0;
			if( (end = strchr( start+1, '\n' )) ){
				*end++ = 0;
			}
			continue;
		}

find_end:

		DEBUGFC(DDB4){
			char smallpart[32];
			char *ss;
			strncpy( smallpart, start, sizeof(smallpart) -1 );
			smallpart[31] = 0;
			if( (ss = strchr(smallpart, '\n' )) ) *ss = 0;
			logDebug("Parse_pc_buffer: looking for end of '%s'", smallpart );
		}

		/* if first character is :, then | can appear in entry */
		if( break_on_line ){
			end = strpbrk( end+1, "\n");
		} else if( *start != ':' ){
			end = strpbrk( end+1, "\n:|");
		} else {
			end = strpbrk( end+1, "\n:");
		}
		DEBUGFC(DDB4){
			char smallpart[32];
			char *ss;
			strncpy( smallpart, end, sizeof(smallpart) -1 );
			smallpart[31] = 0;
			if( (ss = strchr(smallpart, '\n' )) ) *ss = 0;
			logDebug("Parse_pc_buffer: end after find_end '%s'", smallpart );
		}

		/* now check to see if we have an escaped : or \n */
		/* we may have an escaped end of line entry */
		end_c = 0;
		if( end ){
			/* last character is \n, :, or | */
			end_c = end[0];
			backslash_count = 0;
			for( s = end-1; end >= start && *s == '\\'; --s ) ++backslash_count;
			if( break_on_line || end_c == '\n' ){
				if( backslash_count & 1 ){
					end[-1] = ' ';
					end[0]  = ' ';
					goto find_end;
				}
				*end = 0;
				++end;
			} else if( end_c == ':' || end_c == '|' ){
				/* if we do not escape :, it is end */
				if( backslash_count & 1 ){
					/* it is escaped */
					goto find_end;
				}
			}
		}
		
		if( strncasecmp( start, "include", 7 ) == 0
				&& isspace( start[7] ) ){
			int fd;
			char *include_info;
			char *include_file;
			struct stat statb;

			include_file = strpbrk(start," \t");
			if( end_c != '\n' || include_file == 0 ){
				fatal( LOG_ERR,
				  "include: '%s' line has bad format '%s'",
					pathname, start );
			}
			while( isspace( *include_file ) ) ++include_file;
			if( *include_file == 0 || *include_file != '/' ){
				fatal( LOG_ERR,
				  "include: '%s' line has bad format '%s'",
					pathname, start );
			}
			if( include_depth > 10 ){
				fatal( LOG_ERR,
					"include: '%s' nesting too deep",
					pathname );
			}
			DEBUGF(DDB2)("Parse_pc_buffer: file '%s'", include_file );
			++include_depth;
			fd =  Checkread( include_file, &statb );
			if( fd < 0 ){
				logerr_die( LOG_ERR, "Cannot open '%s'", include_file );

			}
			DEBUGF(DDB2)("Parse_pc_buffer: file '%s', size %d", include_file, statb.st_size );
			add_str( &raw->files, include_file,__FILE__,__LINE__  );
			if( (include_info = Readprintcap( include_file,
					fd, &statb, raw)) == 0 ){
				logerr_die( LOG_ERR, "Error reading %s", include_file );
			} else {
				Parse_pc_buffer( include_info, include_file,
					raw, break_on_line );
				lines = line_list->list;
			}
			close(fd);
			--include_depth;
			/* do not put in this entry */
			continue;
		}
		DEBUGFC(DDB4){
			char smallpart[32];
			char *ss;
			strncpy( smallpart, start, sizeof(smallpart) -1 );
			smallpart[31] = 0;
			if( (ss = strchr(smallpart, '\n' )) ) *ss = 0;
			logDebug("Parse_pc_buffer: found '%s' len %d",
				smallpart, strlen(start) );
		}
		/* get rid of short entries */
		if( start == 0 || *start == 0 ) continue;
		if( line_list->count+2 >= line_list->max ){
			extend_malloc_list( line_list, sizeof( lines[0]),  100,__FILE__,__LINE__ );
			lines = line_list->list;
		}
		lines[line_list->count++] = start;
	}
	lines[line_list->count] = 0;
	DEBUGF(DDB2)("Parse_pc_buffer: first_line 0x%x", first_line );
	return( first_line );
}

/***************************************************************************
 * struct printcap_entry *Parse_raw_printcap( int first_line, char *key,
 *	char *path )
 * Scan the printcap fields, extracting the raw printcap entries
 ***************************************************************************/

struct printcap_entry *Parse_raw_printcap( int first_line_index, char *key,
	char *path, struct file_entry *raw )
{
	struct malloc_list *printcap_list = &raw->printcaps;
	struct printcap_entry *printcaps, *pc;	/* printcap list and active one */
	struct printcap_entry *first_pc = 0;
	int i, c;			/* ACME! Fancy, and bruised */
	char *entry;		/* actual entry from list */
	char **first_line;

	DEBUGF(DDB2)( "Parse_raw_printcap: '%s', first_line_index 0x%x",
		path, first_line_index );

	first_line = &(raw->entries.list)[first_line_index];
	DEBUGF(DDB2)( "Parse_raw_printcap: first_line 0x%x '%s'",
		first_line, first_line[0] );

	/* allocate a line list and get the first entry */
	if( printcap_list->count+1 >= printcap_list->max ){
		extend_malloc_list( printcap_list, sizeof( printcaps[0]),
			printcap_list->count+10,__FILE__,__LINE__ );
	}
	printcaps = (void *)printcap_list->list;
	pc = 0;

	DEBUGFC(DDB4){
		for( i = 0; (entry = first_line[i]); ++i ){
			logDebug( "  [%d] '%s'", i, entry );
		}
	}
	/* we scan down through the entry list */
	for( i = 0; (entry = first_line[i]); ++i ){
		c = *entry;
		DEBUGF(DDB4)( "Parse_raw_printcap: [%d] '%s' -> '%c'", i, entry, c );
		if( c != ':' && c != '|' ){
			/* we have new printcap */
			if( pc ){
				/* fix up the raw pc information */
				Fix_raw_pc( pc, path );
			}
			if( printcap_list->count+1 >= printcap_list->max ){
				extend_malloc_list( printcap_list, sizeof( printcaps[0]), 10,__FILE__,__LINE__ );
				printcaps = (void *)printcap_list->list;
			}
			/* get the next entry */
			pc = &printcaps[ printcap_list->count++ ];
			if( first_pc == 0 ) first_pc = pc;
			Free_printcap_entry( pc );
			pc->names = &first_line[i];
			pc->key = key;
			++pc->namecount;
		} else if( c == '|' ){
			if( pc == 0 || pc->optioncount ){
				fatal( LOG_ERR, "Parse_raw_printcap: '%s' bad alias '%s'",
					path, entry );
			}
			/* get rid of | */
			*entry++ = 0;
			first_line[i] = entry;
			++pc->namecount;
		} else if( c == ':' ){
			if( pc == 0 || pc->namecount == 0 ){
				fatal( LOG_ERR, "Parse_raw_printcap: '%s' bad entry '%s'",
					path, entry );
			}
			/* get rid of : */
			*entry++ = 0;
			first_line[i] = entry;
			if( pc->optioncount == 0 ){
				pc->options = &first_line[i];
			}
			++pc->optioncount;
		}
	}
	if( pc ){
		/* fix up the raw pc information */
		Fix_raw_pc( pc, path );
	}
	return( first_pc );
}

/***************************************************************************
 * void Fix_raw_pc( struct printcap_entry *pc, char *path )
 *  - fix up aliases, eliminate null names
 *  - sort options into alphabetical order
 ***************************************************************************/

static int mstrcmp( const void *l, const void *r )
{
	return( strcmp( *(char **)l, *(char **)r ) );
}

void Fix_raw_pc( struct printcap_entry *pc, char *path )
{
	char **lines, **newlines, *s;
	int i, j;

	DEBUGFC(DDB3)dump_printcap_entry( "Fix_raw_pc- before", pc );
	/* first, fix up aliases */
	lines = pc->names;
	if( pc->namecount+1 >= pc->namelines.max ){
		extend_malloc_list( &pc->namelines, sizeof(lines[0]),
			pc->namecount+10,__FILE__,__LINE__  );
	}
	newlines = pc->namelines.list;
	for( i = 0, j = 0; i < pc->namecount; ++i ){
		if( (s = lines[i]) && *s ){
			while( isspace(*s) ) *s++ = 0;
			trunc_str( s );
			if( *s ){
				newlines[j++] = s;
			}
		}
	}
	newlines[j] = 0;
	pc->namecount = j;
	pc->namelines.count = j;
	pc->names = newlines;
	lines = pc->options;
	if( pc->optioncount+1 >= pc->lines.max ){
		extend_malloc_list( &pc->lines, sizeof(lines[0]), pc->optioncount+10,__FILE__,__LINE__  );
	}
	newlines = pc->lines.list;
	for( i = 0, j = 0; i < pc->optioncount; ++i ){
		if( (s = lines[i]) && *s ){
			while( isspace(*s) ) *s++ = 0;
			trunc_str( s );
			if( *s ){
				newlines[j++] = s;
			}
		}
	}
	newlines[j] = 0;
	pc->optioncount = j;
	pc->lines.count = j;
	pc->options = newlines;
	/* now we sort the options */
	if( Mergesort(pc->options, pc->optioncount, sizeof(lines[0]),mstrcmp)){
		fatal( LOG_ERR, "Fix_raw_pc: Mergesort failed" );
	}
	DEBUGFC(DDB3)dump_printcap_entry( "Fix_raw_pc- after", pc );
}

/***************************************************************************
 * char *Get_first_printer( struct printcapfile *pcf )
 *  return the name of the first printer in the printcap file
 * 1. You may not have an entry - you may need to call a filter to get it.
 * 2. If you call a filter, you have to call the 'all' entry.
 * 3. If you have an 'all' entry, you use the first printer in the list
 * 4. This printer may be of the form printer@host
 ***************************************************************************/

char *Get_first_printer()
{
	char *first_name = 0;

	/*
	 * first, see if there are printers in the list.
	 * if not, then get all of them from the raw printcap
	 */

	if( All_list.count == 0 ){
		Get_all_printcap_entries();
	}
	if( All_list.count ){
		/* we want the first name in the list */
		first_name = All_list.list[0];
	}

	DEBUGF(DDB1)("Get_first_printer: found '%s'", first_name );
	return( first_name );
}


/***************************************************************************
 * check to see if the Pc_var_list is ordered correctly
 ***************************************************************************/

void Check_pc_table( void )
{
	int i;
	struct keywords *names;
	char *s, *t;

	names = Pc_var_list;
	for( i = 1; (s = names[i-1].keyword) && (t = names[i].keyword); ++i ){
		DEBUGF(DDB4)("Check_pc_table: '%s' ?? '%s'", s, t );
		if( strcmp( s, t ) > 0 ){
			fatal( LOG_ERR,
			"Check_pc_table: variables '%s' and '%s' out of order",
				s, t );
		}
	}
}

/***************************************************************************
 * char **Get_pc_option_entry( char *option, struct printcap_entry *pc )
 *  search the options for  the named option;
 ***************************************************************************/

char **Get_pc_option_entry( char *str, struct printcap_entry *pc )
{
	char **options;
	int min = 0;
	int max = 0;
	int i, c, len;
	char *s;

	DEBUGF(DDB2)("Get_pc_option_entry: looking for '%s'", str );
	if( pc == 0 ) return( 0 );
	options = pc->options; 
	max = pc->optioncount-1;
	len = strlen(str);
	while( min <= max ){
		i = (min + max)/2;
		s = options[i];
		DEBUGF(DDB4)("Get_pc_option_entry: min %d, max %d, [%d]->'%s'",
			min, max,i, s );
		c = strncmp( s, str, len );
		if( c == 0 ){
			/* if we have punctuation, then we are ok */
			if( (c = s[len]) == 0 || isspace(c) || strchr( "@=#", c ) ){
				DEBUGF(DDB4)("Get_pc_option_entry: found [%d]->'%s'",
					i, s );
				return( &options[i] );
			}
			/* we are above the string */
			max = i - 1;
		} else if( c < 0 ){
			min = i + 1;
		} else {
			max = i - 1;
		}
	}
	return( 0 );
}

/***************************************************************************
 * char *Get_pc_option_value( char *option, struct printcap_entry *pc )
 *  search the options for  the named option;
 ***************************************************************************/
char *Get_pc_option_value( char *str, struct printcap_entry *pc )
{
	char **line;
	char *value = 0;
	line = Get_pc_option_entry( str, pc );
	if( line ){
		value = &(*line)[strlen(str)];
	}
	return( value );
}

/***************************************************************************
 * do_key_conversion: convert the printcap entry to a variable value
 * and do assignment
 ***************************************************************************/
static void do_key_conversion( char *name, struct keywords *key,
	char *s, struct file_entry *file_entry )
{
	int i = 0, err = 0;
	char *end;		/* end of conversion */

	switch( key->type ){
	case FLAG_K:
		if( *s ){
			if( *s == '@' ){
				((int *)key->variable)[0] = 0;
			} else {
				log( LOG_ERR,
				"printcap '%s': bad flag value for '%s%s'",
					name, key->keyword,s);
			}
		} else {
			((int *)key->variable)[0] = 1;
		}
		DEBUGF(DDB3)("do_key_conversion: key '%s' FLAG value %d",
			key->keyword, ((int *)key->variable)[0]);
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
			log( LOG_ERR,
			"printcap '%s', bad integer value for '%s%s'",
				name, key->keyword,s);
		} else {
			((int *)key->variable)[0] = i;
		}
		DEBUGF(DDB3)("do_key_conversion: key '%s' INTEGER value %d",
			key->keyword, ((int *)key->variable)[0]);
		break;
	case STRING_K:
		i = 0;
		if( *s != '=' ){
			log( LOG_ERR,
			"printcap '%s', bad string value for '%s' '%s'",
			name, key->keyword,s);
		} else {
			for( ++s; isspace(*s); ++s );
			if( *s == 0 ) s = 0;
			((char **)key->variable)[0] = s;
		}
		DEBUGF(DDB3)("do_key_conversion: key '%s' STRING value '%s'",
			key->keyword, ((char **)key->variable)[0] );
		break;
	default:
		fatal( LOG_ERR,
		"printcap '%s', variable '%s', unknown type",
			name, key->keyword );
		break;
	}
}

/***************************************************************************
 * Clear_var_list( struct pc_var_list *vars );
 *   Set the printcap variable value to 0 or null;
 ***************************************************************************/

void Clear_var_list( struct keywords *vars )
{
	DEBUGF(DDB2)("Clear_var_list: start");
	for( ; vars->keyword; ++vars ){
		switch( vars->type ){
			case STRING_K: ((char **)(vars->variable))[0] = 0; break;
			case INTEGER_K:
			case FLAG_K: ((int *)(vars->variable))[0] = 0; break;
			default: break;
		}
		if( vars->default_value ){
			Config_value_conversion( vars, vars->default_value );
		}
	}
}

/***************************************************************************
 * Set_var_list( char *name, struct keywords *vars, char **values );
 *  1. the pc_var_list and initialization list must be sorted alphabetically
 *  2. we scan each of the lines, one by one.
 *  3.  for each of the entries in the keylist, we search for a match
 *  4.  when we find a match we set the value
 ***************************************************************************/

void Set_var_list( char *name, struct keywords *vars, char **values,
	struct file_entry *file_entry )
{
	char *value;
	char *key;
	int c, n, compare;

	while( (key = vars->keyword) && (value = *values) ){
		/* do not set if the maxval flag is non-zero */
		if( vars->maxval ){
			DEBUGF(DDB4)("Set_var_list: skipping '%s'", key );
			++vars;
			continue;
		}
		n = strlen( vars->keyword );
		compare = strncmp( key, value, n ); 
		DEBUGF(DDB4)("Set_var_list: compare '%s' to '%s' = %d",
			key, value,compare );
		if( compare < 0 ){
			++vars;
			continue;
		} else if( compare > 0 ){
			++values;
			continue;
		}
		/* check for a separator in value */
		if( (c = value[n]) && c != '=' && c != '#' && c != '@' ){
			/* value longer than variable */
			++vars;
			continue;
		}
		DEBUGF(DDB3)("Set_var_list: found '%s', value '%s' ",
			key, value+n );
		/* we have found the keyword.  Now set the value */
		do_key_conversion( name, vars, value+n, file_entry );
		++values;
	}
}

/***************************************************************************
 * Printer_vars(
 *   char *name -  name to look up
 *   struct printcap_entry *pc_entry - printcap found
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

char *Full_printer_vars( char *name, struct printcap_entry **pc_entry )
{
	char *primary_name;
	struct printcap_entry *pc = 0;
	char *path;
	struct dpathname dpath;

	DEBUGF(DDB1)("Full_printer_vars: %s", name);
	if( pc_entry ){
		pc = *pc_entry;
	}
	primary_name = Get_printer_vars( name, &pc );
	if( primary_name ){
		Printer = primary_name;
		Expand_value( Pc_var_list, &Raw_printcap_files );
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
			int last_raw;
			Init_path( &dpath, path );
			path = Add_path( &dpath, "printcap" );
			last_raw = Raw_printcap_files.printcaps.count;
			Getprintcap_pathlist( path, primary_name, &Raw_printcap_files );
			/* now we check to see if we need to add more */
			if( Combine_options( primary_name, pc, primary_name,
				last_raw ) ){
				DEBUGFC(DDB3) dump_printcap_entry("Full_printer_vars - added", pc);
				Set_var_list( primary_name, Pc_var_list, pc->options,
					&Raw_printcap_files );
				Expand_value( Pc_var_list, &Raw_printcap_files );
				DEBUGFC(DDB3) dump_parms("Full_printer_vars - added",
					Pc_var_list);
			}
		}
	}
	if( pc_entry ) *pc_entry = pc;
	return(primary_name);
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

char *Linearize_pc_list( struct printcap_entry *pc, char *parm_name )
{
	int len, i;
	char **pc_lines;
	static char *buffer;
	static int   buffer_len;
	char *end, *s;

	DEBUGF(DDB1)("Linearize_pc_list: '%s'", pc->names[0] );

	len = 0;
	if( parm_name ) len += strlen( parm_name );
	len += strlen(pc->names[0]) + 2;
	pc_lines = pc->options;
	for( i = 0; i < pc->optioncount; ++i ){
		len += strlen( pc_lines[i] ) + 3;
	}
	len += 1;
	if( len > buffer_len ){
		if( buffer ) free(buffer);
		buffer_len = len;
		buffer = malloc_or_die( len );
	}
	s = buffer;
	s[0] = 0;
	end = s+len-1;
	if( parm_name ) strncpy( s, parm_name, end-s );
	s += strlen(s);
	strncpy( s, pc->names[0], end-s );
	s += strlen(s);
	*s++ = '\n';
	for( i = 0; i < pc->optioncount; ++i ){
		*s++ = ' ';
		*s++ = ':';
		strncpy( s, pc_lines[i], end-s );
		s += strlen(s);
		*s++ = '\n';
	}
	*s = 0;
	DEBUGF(DDB1)("Linearize_pc_list: result '%s'", buffer );
	return( buffer );
}
/***************************************************************************
 * void Create_expanded_entry( struct printcap_entry *entry, char *key,
 *  - turn a raw printcap entry into an expanded one
 *  - if we do not have any information, we do not create it.
 ***************************************************************************/

struct printcap_entry *Create_expanded_entry(
	struct printcap_entry *raw_pc, char *key, int start_index )
{
	struct malloc_list *expanded_list;	/* malloc_list */
	struct printcap_entry *expanded;			/* list */
	struct printcap_entry *expanded_pc;	/* entry */

	DEBUGF(DDB3)("Create_expanded_entry: '%s' key %d, start_index %d",
		raw_pc->names[0], key, start_index );
	expanded_list = &Expanded_printcap_entries;

	if( expanded_list->count+1 >= expanded_list->max ){
		extend_malloc_list( expanded_list, sizeof(expanded[0]),
		expanded_list->count+10,__FILE__,__LINE__  );
	}
	expanded = (void *)expanded_list->list;
	expanded_pc = &expanded[expanded_list->count];
	Free_printcap_entry( expanded_pc );

	/* we copy the name fields */
	expanded_pc->names = raw_pc->names;
	expanded_pc->namecount = raw_pc->namecount;
	/* now we add in the options, starting at this one */
	/* We do not add the entry if the :oh: field is not for
		our fully qualified domain name (FQDN) IP address */
	if( Combine_options( expanded_pc->names[0], expanded_pc,
		key, start_index ) ){
		++expanded_list->count;
		DEBUGFC(DDB3) dump_printcap_entry("Create_expanded_entry",
			expanded_pc );
	} else {
		DEBUGF(DDB3)("Create_expanded_entry: did not make entry");
		expanded_pc = 0;
	}
	return( expanded_pc );
}

/***************************************************************************
 * void Get_all_printcap_entries( void )
 * 1. scan the raw printcap database,  and extract the printcaps
 *    that are defined there.
 * 2. search for the 'all' printcap entry in the extracted information
 *    this search will first look in the extracted ones, and then
 *    will try a filter to see if it can make one.
 ***************************************************************************/
static char *all_field_value;

void Get_all_printcap_entries( void )
{
	struct malloc_list *expanded_list;	/* expanded list */
	struct malloc_list *raw_list;		/* raw list */
	struct printcap_entry *raw, *expanded;	/* entries */
	struct printcap_entry *raw_pc, *expanded_pc;
	char *name;
	int raw_count;
	int all_count, i;
	char *s, *end, **all_list;

	if( Raw_printcap_files.initialized == 0 ){
		Raw_printcap_files.initialized = 1;
		if( Printcap_path && *Printcap_path ){
			Getprintcap_pathlist( Printcap_path, 0, &Raw_printcap_files );
		}
		if( Is_server && Lpd_printcap_path && *Lpd_printcap_path ){
			Getprintcap_pathlist( Lpd_printcap_path, 0, &Raw_printcap_files );
		}
	}

	expanded_list = &Expanded_printcap_entries;
	expanded_list->count = 0;
	raw_list = &Raw_printcap_files.printcaps;
	if( expanded_list->max <= raw_list->count ){
		extend_malloc_list( expanded_list, sizeof( expanded[0]),
			raw_list->count+10,__FILE__,__LINE__  );
	}
	/* now we will scan the raw printcap database */
	raw = (void *)raw_list->list;
	expanded = (void *)expanded_list->list;
	/* ready for checking */
	for( raw_count = 0; raw_count < raw_list->count; ++raw_count ){
		raw_pc = &raw[raw_count];
		raw_pc->checked = 0;
	}
	for( raw_count = 0; raw_count < raw_list->count; ++raw_count ){
		raw_pc = &raw[raw_count];
		name = raw_pc->names[0];
		DEBUGF(DDB3)("Get_all_printcap_entries: [%d] '%s'", 
			raw_count, name );
		if( raw_pc->checked || name == 0 || ispunct( *name ) ){
			/* we skip this one */
			continue;
		}
		Create_expanded_entry( raw_pc, 0, raw_count );
	}
	DEBUGFC(DDB2) dump_printcap_information(
		"Get_all_printcap_entries - after processing raw");
	/* now we try to find the "all" printcap entry */
	expanded_pc = 0;
	if( Find_printcap_entry( "all", &expanded_pc ) ){
		char *all_value;
		DEBUGF(DDB2)("Get_all_printcap_entries: all entry 0x%x",
			expanded_pc );
		all_value = Get_pc_option_value( "all", expanded_pc );
		if( all_value == 0 ){
			fatal( LOG_ERR,
		"Get_all_printcap_entries: 'all' printcap does not have 'all' value." );
		}
		if( all_value[0] != '=' || strlen( all_value+1) == 0){
			fatal( LOG_ERR,
				"Get_all_printcap_entries: bad 'all' entry value" );
		}
		++all_value;
		/* now we save the value and cut it up */
		if( all_field_value ){
			free( all_field_value );
		}
		all_field_value = safestrdup( all_value );
		/* find the number of possible fields */
		all_count = 0;
		for( s = all_field_value; s && *s; s = end ){
			while( isspace(*s) ) ++s;
			end = strpbrk( s, " \t,;");
			if( end ) ++end;
			++all_count;
		}
		++all_count;
		if( All_list.max <= all_count ){
			extend_malloc_list( &All_list, sizeof(s), all_count+1,__FILE__,__LINE__  );
		}
		all_list = All_list.list;
		All_list.count = 0;
		for( s = all_field_value; s && *s; s = end ){
			while( isspace(*s) ) ++s;
			all_list[All_list.count++] = s;
			end = strpbrk( s, " \t,;");
			if( end ) *end++ = 0;
		}
		all_list[All_list.count] = 0;
	} else {
		DEBUGF(DDB3)("Get_all_printcap_entries: no all entry");
		if( All_list.max <= expanded_list->count ){
			extend_malloc_list( &All_list, sizeof(s), expanded_list->count+1,__FILE__,__LINE__  );
		}
		all_list = All_list.list;
		All_list.count = 0;
		expanded = (void *)expanded_list->list;
		for( i = 0; i < expanded_list->count; ++i ){
			expanded_pc = &expanded[i];
			name = expanded_pc->names[0];
			if( name && *name && !ispunct(*name) ){
				all_list[All_list.count++] = name;
			}
		}
	}
	DEBUGFC(DDB1)dump_printcap_information("Get_all_printcap_entries: after");
	DEBUGFC(DDB1){
		logDebug("Get_all_printcap_entries: count %d", All_list.count);
		all_list = All_list.list;
		for( i = 0; i < All_list.count; ++i ){
			logDebug( "  [%d] '%s'", i, all_list[i] );
		}
	}
}


/***************************************************************************
 * pc_cmp( l, r ) - compare two printcap keys (with values ignored)
 ***************************************************************************/
 
int pc_cmp( char *left, char *right )
{
	char *endleft = strpbrk( left, " \t=#@" );
	char *endright = strpbrk( right, " \t=#@" );
	int c1, c2, i = 0;
	int leftlen, rightlen; 
	leftlen = strlen(left);
	rightlen = strlen(right);
	if( endleft ){
		leftlen = endleft - left;
	}
	if( endright ){
		rightlen = endright - right;
	}
	/* get lengths */
	c1 = leftlen;
	c2 = rightlen;
	/* only do comparison if non-zero lengths */
	if( c1 && c2 ){
		while( ((c1 = left[i]) == (c2 = right[i])) && c1
			&& ++i < leftlen && i < rightlen );
	}
	DEBUGF(DDB4)("pc_cmp: %s to %s = %d", left,right,c1-c2);
	return( c1 - c2 );
}

/***************************************************************************
 * void Combine_options( char *name, struct printcap_entry *pc )
 *  1. scan the raw printcap entries for ones with matching
 *     primary name
 *  2. if a 'oh' entry, then we check to see if we use it. We check the
 *     glob match of the FQDN host name, or the short host form
 *  3. merge the printcap options with the current options.
 *  3. if a 'tc=' entry is now in the options,  then scan for the
 *     tc= entry options, and do the same merge and scan.
 ***************************************************************************/

int Combine_options( char *name, struct printcap_entry *pc, char *key,
	int start_index )
{
	struct malloc_list *raw_list;		/* raw list */
	struct printcap_entry *raw;	/* entries */
	struct printcap_entry *raw_pc;
	int raw_index;
	static int nesting;
	static struct malloc_list merged_list;
	char **merged_lines, **raw_lines, **pc_lines;
	char *s;
	int merged_count, raw_count, pc_count;
	int max_count;
	int i, not_found;
	int diff;
	char **tc_line;
	int found = 0;
	char ohname[SMALLBUFFER];
	char *end;


	DEBUGF(DDB2)(
		"Combine_options: start- name '%s', pc '%s', key %d, start_index %d nesting %d",
		name, pc->names[0], key, start_index, nesting );
	if( ++nesting > 10 ){
		fatal( LOG_ERR, "Combine_options:  loop in printcap references '%s'",
			name );
	}
	raw_list = &Raw_printcap_files.printcaps;
	for( raw_index = start_index; raw_index < raw_list->count; ++raw_index ){
		raw = (void *)raw_list->list;
		raw_pc = &raw[raw_index];
		/* must have the same name and key if key non-zero */
		not_found = 1;
		DEBUGF(DDB3)("Combine_options: checking entry('%s', key %d)",
			raw_pc->names[0], raw_pc->key );
		if( raw_pc->key && raw_pc->key != key ){
			continue;
		}
		for( i = 0; not_found && i < raw_pc->namecount; ++i ){
			not_found = strcmp( raw_pc->names[i], name );
		}
		if( not_found ){
			continue;
		}
		/*
		 * check for 'server' flag
		 */
		if( (s = Get_pc_option_value( "server", raw_pc )) ){
			DEBUGF(DDB3)("Combine_options: printcap server flag '%s',  server %d",
				s, Is_server );
			if( Is_server == 0 ) continue;
		}
		/*
		 * check for 'oh' entry
		 */
		if( (s = Get_pc_option_value( "oh", raw_pc )) ){
			/* get the IP address */
			if( *s != '=' ){
				fatal( LOG_ERR, "Combine_options: '%s' bad printcap :oh: entry",
					raw_pc->names[0] );
			}
			++s;
			safestrncpy( ohname, s );
			DEBUGF(DDB3)("Combine_options: oh entry '%s', fqdn '%s'",
				ohname, HostIP.fqdn );
			for( not_found = 1, s = ohname; not_found && s && *s; s = end ){
				while( isspace(*s) ) ++s;
				end = strpbrk(s, " \t,;" );
				if( end ){
					*end++ = 0;
				}
				/* we do a globmatch on the host's fqdn, then try IP addresses */
				DEBUGF(DDB3)("Combine_options: checking '%s'", s );
				if( Globmatch( s, HostIP.fqdn ) == 0 ){
					DEBUGF(DDB3)("Combine_options: globmatch '%s' to '%s'",
					s, HostIP.fqdn );
					not_found = 0;
				} else if( !strchr( s, '*' ) ){
					s = Find_fqdn( &LookupHostIP, s, 0 );
					DEBUGF(DDB3)("Combine_options: fqdn '%s' to '%s'",
						LookupHostIP.fqdn, HostIP.fqdn );
					if( Same_host( &LookupHostIP, &HostIP ) == 0 ){
						DEBUGF(DDB3)("Combine_options: same host" );
						not_found = 0;
					}
				}
			}
			if( not_found ){
				DEBUGF(DDB3)("Combine_options: not using this entry" );
				continue;
			}
		}
		found = 1;
		raw_pc->checked = 1;
		/* now we have to add options, eliminating duplicates */
		max_count = raw_pc->optioncount + pc->optioncount;
		DEBUGF(DDB2)( "Combine_options: found raw name 0x%x '%s', count %d",
			raw_pc->names, *raw_pc->names, raw_pc->optioncount );
		if( merged_list.max <= max_count ){
			extend_malloc_list( &merged_list, sizeof(merged_lines[0]),
				 max_count+100,__FILE__,__LINE__ );
		}
		merged_list.count = 0;
		merged_lines = merged_list.list;
		raw_lines = raw_pc->options;
		pc_lines = pc->options;
		merged_count = raw_count = pc_count = 0;
		DEBUGF(DDB2)( "Combine_options: raw_lines 0x%x '%s' pc_lines 0x%x '%s'",
			raw_lines, raw_count?raw_lines[0]:"",
			pc_lines, pc_lines?pc_lines[0]:"" );
		while( pc_count < pc->optioncount && raw_count < raw_pc->optioncount ){
			/* now combine them */
			diff = pc_cmp(pc_lines[pc_count], raw_lines[raw_count]);
			DEBUGF(DDB2)( "Combine_options: pc '%s' raw '%s' diff %d",
				pc_lines[pc_count], raw_lines[raw_count], diff );
			if( diff < 0 ){
				merged_lines[merged_count++] = pc_lines[pc_count++];
			} else if( diff > 0 ){
				merged_lines[merged_count++] = raw_lines[raw_count++];
			} else {
				/* we combine the tc entries */
				s = pc_lines[pc_count];
				if( s[0] == 't' && s[1] == 'c'
					&& pc_cmp(pc_lines[pc_count], "tc" ) == 0 ){
					merged_lines[merged_count++] = pc_lines[pc_count];
				}
				merged_lines[merged_count++] = raw_lines[raw_count++];
				++pc_count;
			}
		}
		while( pc_count < pc->optioncount ){
			DEBUGF(DDB2)( "Combine_options: pc trailing [%d] '%s'", 
				pc_count, pc_lines[pc_count] );
			merged_lines[merged_count++] = pc_lines[pc_count++];
		}
		while( raw_count < raw_pc->optioncount ){
			DEBUGF(DDB2)( "Combine_options: raw trailing [%d] '%s'", 
				raw_count, raw_lines[raw_count] );
			merged_lines[merged_count++] = raw_lines[raw_count++];
		}
		merged_lines[merged_count] = 0;
		DEBUGF(DDB2)( "Combine_options: merged_count %d", merged_count );
		/* now we have to put the merged list into the printcap */
		if( pc->lines.max <= merged_count+1 ){
			extend_malloc_list( &pc->lines, sizeof( merged_lines[0] ),
				merged_count + 10,__FILE__,__LINE__  );
		}
		pc_lines = pc->lines.list;
		for( i = 0; i < merged_count; ++i ){
			pc_lines[i] = merged_lines[i];
			DEBUGF(DDB2)( "Combine_options: merged [%d] '%s'", i,
				pc_lines[i] );
		}
		pc_lines[i] = 0;
		pc->lines.count = merged_count;
		pc->options = pc_lines;
		pc->optioncount = merged_count;
		DEBUGFC(DDB2) dump_printcap_entry( "Combine_options - after merged", pc );
		while( (tc_line = Get_pc_option_entry( "tc", pc )) ){
			DEBUGF(DDB2)( "Combine_options: tc '%s'", *tc_line );
			/* now we have to remove the tc entry */
			s = *tc_line;
			for( pc_lines = tc_line; pc_lines[1]; ++pc_lines ){
				pc_lines[0] = pc_lines[1];
			}
			--pc->optioncount;
			--pc->lines.count;
			/* now we get the tc entry name */
			s = strchr( s, '=' );
			if( s ) ++s;
			while( isspace( *s ) ) ++s;
			if( s == 0 || *s == 0){
				fatal( LOG_ERR, "Combine_options: bad tc field printcap '%s'",
					pc->names[0] );
			}
			/* now we repeat the search,  looking for tc field */
			DEBUGF(DDB3)( "Combine_options: looking for tc '%s'", s );
			if( Combine_options( s, pc, key, 0 ) == 0 ){
				int count;
				/* we did not find it,  check for filter generation */
				DEBUGF(DDB3)( "Combine_options: trying filter for tc '%s'", s );
				count = Filterprintcap( s, key, &start_index,
					&Raw_printcap_files, 0 );
				/* we got more from  filter, check   */
				if( count && Combine_options( s, pc, key, start_index ) == 0 ){
					fatal( LOG_ERR,
						"Combine_options: cannot find tc reference '%s'",
							s );
				}
			}
			DEBUGFC(DDB2) dump_printcap_entry(
				"Combine_options - after tc", pc );
		}
	}
	--nesting;
	DEBUGF(DDB2)("Combine_options: found %d", found );
	return( found );
}

/***************************************************************************
 *char *Find_printcap_entry( char *name, struct printcap_entry **pc );
 *  - search the expanded printcap list for an entry.
 *  - if not found, search the raw list and expand the entry.
 *  - if not found, try the filters and see if it finds it.
 *  - if not found, give up
 ***************************************************************************/

char *Find_printcap_entry( char *name, struct printcap_entry **pc_found )
{
	struct printcap_entry *printcaps, *pc;	/* printcaps list */
	struct malloc_list *list;
	int entry_count;		/* the entry being examined */
	int start_index;		/* where new entry placed */
	char **name_list;		/* list of names in printcap */
	int not_found;			/* flag for controlling search */
	int i;

	DEBUGF(DDB1)("Find_printcap_entry: looking for '%s'", name );
	DEBUGFC(DDB3)dump_printcap_information(
		"Find_printcap_entry: before search");
	if( Raw_printcap_files.initialized == 0 ){
		Raw_printcap_files.initialized = 1;
		if( Printcap_path && *Printcap_path ){
			DEBUGF(DDB1)("Find_printcap_entry: reading '%s'", Printcap_path );
			Getprintcap_pathlist( Printcap_path, 0, &Raw_printcap_files );
		}
		if( Is_server && Lpd_printcap_path && *Lpd_printcap_path ){
			DEBUGF(DDB1)("Find_printcap_entry: reading '%s'", Lpd_printcap_path );
			Getprintcap_pathlist( Lpd_printcap_path, 0, &Raw_printcap_files );
		}
	}

	list = &Expanded_printcap_entries;
	printcaps = (void *)list->list;
	not_found = 1;

	pc = 0;
	DEBUGF(DDB1)("Find_printcap_entry: expanded count %d, checking",
			list->count );
	for( entry_count = 0;
		not_found && entry_count < list->count;
		++entry_count ){
		pc = &printcaps[entry_count];
		name_list = pc->names;
		for( i = 0;
			i < pc->namecount && (not_found = Globmatch(name_list[i], name));
			++i );
	}
	if( not_found ){
		DEBUGF(DDB1)("Find_printcap_entry: not found, checking raw" );
		/* we search the raw printcap database now */
		list = &Raw_printcap_files.printcaps;
		printcaps = (void *)list->list;
		for( entry_count = 0;
			not_found && entry_count < list->count;
			++entry_count ){
			pc = &printcaps[entry_count];
			DEBUGF(DDB2)("Find_printcap_entry: checking '%s'", pc->names[0] );
			name_list = pc->names;
			for( i = 0;
			i < pc->namecount && (not_found = Globmatch(name_list[i], name));
			++i );
		}
		if( !not_found ){
			/* make an expanded entry from the raw one */
			DEBUGF(DDB2)("Find_printcap_entry: found '%s'", pc->names[0] );
			pc = Create_expanded_entry( pc, 0, entry_count-1 );
			not_found = (pc == 0);
		}
	}
	if( not_found ){
		/* try the Filterprintcap and redo search */
		DEBUGF(DDB1)("Find_printcap_entry: trying filter for '%s'", name );
		entry_count = Filterprintcap( name, 0, &start_index,
			&Raw_printcap_files, 0 );
		DEBUGF(DDB1)(
			"Find_printcap_entry: filter found %d entries starting at %d",
			entry_count, start_index );
		/* we got more from  filter, check   */
		if( entry_count ){
			list = &Raw_printcap_files.printcaps;
			printcaps = (void *)list->list;
			for( entry_count = start_index;
				not_found && entry_count < list->count;
				++entry_count ){
				pc = &printcaps[entry_count];
				name_list = pc->names;
				for( i = 0;
				i < pc->namecount && (not_found = Globmatch(name_list[i], name));
				++i );
			}
			if( !not_found ){
				/* make an expanded entry from the raw one */
				pc = Create_expanded_entry( pc, 0, entry_count-1 );
				not_found = (pc == 0);
			}
		}
	}
	if( not_found ){
		DEBUGF(DDB1)("Find_printcap_entry: did not find '%s'", name );
		pc = 0;
		name = 0;
	} else {
		char *s;
		DEBUGF(DDB1)("Find_printcap_entry: looking for '%s', found '%s'",
			name, pc->names[0] );
		DEBUGFC(DDB2)dump_printcap_entry("Find_printcap_entry", pc );
		name = pc->names[0];
		if( (s = Find_meta(name)) ){
			fatal( LOG_ERR, "Find_printcap_entry: name '%s' has meta character '%c'",
				name, *s );
		}
	}
	if( pc_found ) *pc_found = pc;
	return( name );
}

/***************************************************************************
 * Get_printer_vars(
 *   char *name -  name to look up
 *   char *error, int errlen: - error message and maximum length
 *   char *default - default values for variables
 ***************************************************************************/
char *Get_printer_vars( char *name, struct printcap_entry **pc_entry )
{
	struct printcap_entry *pc = 0;
	char *primary_name = 0;

	DEBUGF(DDB1)( "Get_printer_vars: looking for '%s'", name );
	if( pc_entry ){
		pc = *pc_entry;
	}
	if( pc == 0 ){
		primary_name = Find_printcap_entry( name, &pc );
	} else {
		primary_name = pc->names[0];
	}
	if( primary_name ){
		Set_var_list( primary_name, Pc_var_list, pc->options,
			&Raw_printcap_files );
		DEBUGFC(DDB2)dump_parms( "Get_printer_vars", Pc_var_list );
	} else {
		DEBUGF(DDB1)( "Get_printer_vars: did not find '%s'", name );
	}
	if( pc_entry ) *pc_entry = pc;
	return( primary_name );
}

/*
 * char *Find_filter( int key, struct printcap_entry *printcap_entry )
 *  search the printcap until you find the filter specified
 * key = key to search for, i.e.- 'o' = "of"
 */

char *Find_filter( int key, struct printcap_entry *printcap_entry )
{
	static char option[] = "Xf";
	char *s = 0;

	option[0] = key;
	
	s = Get_pc_option_value( option, printcap_entry );
	if( s && *s == '='){
		++s;
	} else {
		s = 0;
	}
	return( s );
}

/***************************************************************************
 * Expand_value:
 *  we expand the string into a fixed size buffer, and then we allocate
 *  a dynamic copy if needed
 ***************************************************************************/

void Expand_value( struct keywords *var_list, struct file_entry *file_entry )
{
	char *s;
	char copy[LARGEBUFFER];
	struct keywords *var;
	int changed;

	/* check to see if you need to expand */
	for( var = var_list; var->keyword; ++var ){
		if( var->type != STRING_K ) continue;
		s = ((char **)var->variable)[0];
		if( s == 0 || *s == 0 || strchr( s, '%' ) == 0 ){
			continue;
		}
		DEBUGF(DDB3)("Expand_value: original '%s'", s );
		changed = Expand_percent( s, copy, copy+sizeof(copy)-2 );
		copy[sizeof(copy)-1] = 0;
		DEBUGF(DDB3)("Expand_value: new value '%s'", copy );
		if( changed ){
			/* now we allocate a buffer entry */
			s = add_str( &file_entry->expanded_str, copy,__FILE__,__LINE__  );
			((char **)var->variable)[0] = s;
			DEBUGF(DDB3)("Expand_value: result '%s'", s );
		} else {
			DEBUGF(DDB3)("Expand_value: no change in '%s'", copy );
		}
	}
}

int Expand_percent( char *s, char *next, char *end )
{
	int changed = 0;
	struct keywords *keys;
	char *str;
	int c;
	DEBUGF(DDB3)("Expand_percent: Expanding '%s'", s );
	if(s) for( ; next < end && (c = *s); ++s ){
		if( c != '%' ){
			*next++ = c;
			continue;
		}
		/* we have a '%' character */
		/* get the next character */
		c = *++s;
		*next = c;
		if( c == 0 ) break;
		if( c == '%' ){
			*next++ = c;
			continue;
		}
		for( keys = Keyletter; keys->keyword && keys->keyword[0] != c; ++keys );
		if( keys->keyword ){
			str = *(char **)keys->variable;
			DEBUGF(DDB3)("Expand_percent: key '%c' '%s'", c, str );
			if( str && *str ){
				changed = 1;
				while( next < end && (*next = *str++) ) ++next;
			}
		} else {
			DEBUGF(DDB3)("Expand_percent: key '%c' not found", c );
			/* we have a unknown configuration expansion value */
			/* we leave it in for later efforts */
			*next++ = '%';
			if( next < end ) *next++ = c;
		}
	}
	*next = 0;
	return( changed );
}
