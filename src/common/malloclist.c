/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: malloclist.c
 * PURPOSE: manage dynmaic memory allocated lists
 **************************************************************************/

static char *const _id =
"malloclist.c,v 3.7 1998/03/24 02:43:22 papowell Exp";
#include "lp.h"
#include "malloclist.h"
/**** ENDINCLUDE ****/

/***************************************************************************
 * extend_malloc_list( struct malloc_list *buffers, int element )
 *   add more entries to the array in the list
 ***************************************************************************/
void extend_malloc_list( struct malloc_list *buffers, int element, int incr,
	char *file, int line)
{
	int size, orig;
	char *s;

	orig = buffers->max * element;
	buffers->max += incr;
	size = buffers->max * element;
	if( buffers->list ){
		if( element != buffers->size ){
			fatal( LOG_ERR, "extend_malloc_list: element %d, original %d",
				element, buffers->size);
		}
		if( size == 0 ){
			fatal( LOG_ERR, "extend_malloc_list: realloc of 0!!" );
		}
		s = realloc( buffers->list, size );
		if( s == 0 ){
			logerr_die( LOG_ERR,
				"extend_malloc_list: realloc 0x%x to %d failed",
				buffers->list, size );
		}
	} else {
		if( size == 0 ){
			fatal( LOG_ERR, "extend_malloc_list: malloc of 0!!" );
		}
		s = (void *)malloc_or_die( size );
		buffers->size = element;
	}
	buffers->list = (void *)s;
	memset( s+orig, 0, size-orig );
	DEBUG4( "extend_malloc_list: file %s, line %d- alloc 0x%x, buf 0x%x",
		file, line, s, buffers);
}

/***************************************************************************
 * add_buffer( pcf, size )
 *  add a new entry to the malloc list
 * return pointer to start of size byte area in memory
 ***************************************************************************/

char *add_buffer( struct malloc_list *buffers, int len, char *file, int ln )
{ 
	char *s;

	/* extend the list information */
	DEBUG4( "add_buffer: 0x%x before list 0x%x, count %d, max %d, len %d",
		buffers,
		buffers->list, buffers->count, buffers->max, len );
	if( buffers->count+2 >= buffers->max ){
		extend_malloc_list( buffers, sizeof( buffers->list[0] ), 10, file, ln );
	}
	/* malloc data structures */
	if( len == 0 ){
		fatal( LOG_ERR, "add_buffer: malloc of 0!!" );
	}
	s = malloc_or_die( len );
	memset(s,0,len);

	buffers->list[buffers->count] = s;
	++buffers->count;
	buffers->list[buffers->count] = 0;
	DEBUG4( "add_buffer: file '%s', line %d, alloc 0x%x, len %d, buf 0x%x, list 0x%x, count %d, max %d",
		file, ln, s, len, buffers,
		buffers->list, buffers->count, buffers->max, s);
	return( s );
}


/***************************************************************************
 * add_buffer( pcf, size )
 *  add a string entry to the malloc list
 * return pointer to start of size byte area in memory
 ***************************************************************************/

char *add_str( struct malloc_list *buffers, char *str, char *file, int ln )
{ 
	char *s;
	int len = strlen(str)+1;

	/* extend the list information */
	DEBUG4( "add_str: 0x%x before list 0x%x, count %d, max %d, len %d",
		buffers,
		buffers->list, buffers->count, buffers->max, len );
	if( buffers->count+2 >= buffers->max ){
		extend_malloc_list( buffers, sizeof( buffers->list[0] ), 10, file, ln );
	}

	/* malloc data structures */
	s = malloc_or_die( len );
	strcpy(s,str);

	buffers->list[buffers->count] = s;
	++buffers->count;
	buffers->list[buffers->count] = 0;
	DEBUG4( "add_str: file '%s', line %d, alloc 0x%x, str '%s', len %d, buf 0x%x, list 0x%x, count %d, max %d",
		file, ln, s, s, len, buffers,
		buffers->list, buffers->count, buffers->max, s);
	return( s );
}

/***************************************************************************
 * clear_malloc_list( struct malloc_list *buffers, int free_list )
 *  if the free_list value is non-zero,  free the list pointers
 *  set the buffer count to 0
 ***************************************************************************/

void clear_malloc_list( struct malloc_list *buffers, int free_list )
{
	int i;
	DEBUG4(
		"clear_malloc_list: 0x%x, free_list %d, list 0x%x, count %d, max %d",
		buffers, free_list, buffers->list, buffers->count, buffers->max );

	if( buffers->list && free_list ){
		for( i = 0; i < buffers->count; ++i ){
			free( buffers->list[i] );
			buffers->list[i] = 0;
		}
	}
	if( buffers->list ){
		free(buffers->list);
		buffers->list = 0;
		buffers->max = 0;
	}
	buffers->count = 0;
}

/*
 * clear a control file data structure
 */
void Clear_control_file( struct control_file *cf )
{
	struct control_file f;

	DEBUG4("Clear_control_file: 0x%x", cf );
	clear_malloc_list( &cf->control_file_image, 1 );
	clear_malloc_list( &cf->data_file_list, 0 );
	clear_malloc_list( &cf->control_file_lines, 0 );
	clear_malloc_list( &cf->control_file_copy, 0 );
	clear_malloc_list( &cf->control_file_print, 0 );
	clear_malloc_list( &cf->hold_file_lines, 0 );
	clear_malloc_list( &cf->hold_file_print, 0 );
	clear_malloc_list( &cf->destination_list, 0 );

	f = *cf;

	memset( cf, 0, sizeof( cf[0] ) );

	cf->control_file_image = f.control_file_image;
	cf->data_file_list = f.data_file_list;
	cf->control_file_lines = f.control_file_lines;
	cf->control_file_copy = f.control_file_copy;
	cf->control_file_print = f.control_file_print;
	cf->hold_file_info = f.hold_file_info;
	cf->hold_file_lines = f.hold_file_lines;
	cf->hold_file_print = f.hold_file_print;
	cf->destination_list = f.destination_list;
}

char *Add_job_line( struct control_file *cf, char *str, int nodup,
	char *file, int ln )
{
	char **lines;
	if( nodup == 0 ){
		str = add_str( &cf->control_file_image, str, file,ln );
	}
	if( cf->control_file_lines.count+2 >= cf->control_file_lines.max ){
		extend_malloc_list( &cf->control_file_lines, sizeof( char * ),
			10, file, ln );
	}
	lines = (void *)cf->control_file_lines.list;
	lines[ cf->control_file_lines.count++ ] = str;
	lines[ cf->control_file_lines.count ] = 0;
	return( str );
}

/***************************************************************************
 * Insert_job_line( struct control_file *cf, int char *str, int nodup,
 *          int position )
 *  insert a line into the job control file at the indicated position, moving rest down
 *  nodup = 1 - do not make a copy
 ***************************************************************************/

char *Insert_job_line( struct control_file *cf, char *str, int nodup,
	int position, char *file, int ln )
{
	int i;
	char **lines;

	/* extend if if necessary */
	if( cf->control_file_lines.count+2 >= cf->control_file_lines.max ){
		extend_malloc_list( &cf->control_file_lines, sizeof( char * ),
			10, file, ln );
	}
	lines = (void *)cf->control_file_lines.list;
	/* copy down one position - now lines.count is last pointer */
	for( i = cf->control_file_lines.count; i > position; --i ){
		lines[i] = lines[i-1];
	}
	if( nodup == 0 ){
		str = add_str( &cf->control_file_image, str, file,ln );
	}
	lines[ position ] = str;
	++cf->control_file_lines.count;
	lines[cf->control_file_lines.count] = 0;
	++cf->control_info;
	return( str );
}
