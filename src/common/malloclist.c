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
"$Id: malloclist.c,v 3.6 1997/09/18 19:46:01 papowell Exp $";
#include "lp.h"
#include "malloclist.h"
/**** ENDINCLUDE ****/

/***************************************************************************
 * extend_malloc_list( struct malloc_list *buffers, int element )
 *   add more entries to the array in the list
 ***************************************************************************/

void extend_malloc_list( struct malloc_list *buffers, int element, int incr )
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
		s = malloc( size );
		if( s == 0 ){
			logerr_die( LOG_ERR,
				"extend_malloc_list: malloc %d failed", size );
		}
		buffers->size = element;
	}
	buffers->list = (void *)s;
	memset( s+orig, 0, size-orig );
	DEBUG4( "extend_malloc_list: list 0x%x, count %d, max %d, element %d",
		buffers->list, buffers->count, buffers->max, element );
}

/***************************************************************************
 * add_buffer( pcf, size )
 *  add a new entry to the printcap file list
 * return pointer to start of size byte area in memory
 ***************************************************************************/

char *add_buffer( struct malloc_list *buffers, int len )
{ 
	char *s;

	/* extend the list information */
	DEBUG4( "add_buffer: 0x%x before list 0x%x, count %d, max %d, len %d",
		buffers,
		buffers->list, buffers->count, buffers->max, len );
	if( buffers->count+1 >= buffers->max ){
		extend_malloc_list( buffers, sizeof( buffers->list[0] ),
			buffers->count+10 );
	}

	/* malloc data structures */
	if( len == 0 ){
		fatal( LOG_ERR, "add_buffer: malloc of 0!!" );
	}
	s = malloc( len );
	if( s == 0 ){
		logerr_die( LOG_ERR, "add_buffer: malloc %d failed", len );
	}
	s[0] = 0;
	s[len-1] = 0;

	buffers->list[buffers->count] = s;
	++buffers->count;
	DEBUG4( "add_buffer: 0x%x after list 0x%x, count %d, max %d, allocated 0x%x, len %d",
		buffers,
		buffers->list, buffers->count, buffers->max, s, len );
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

char *Add_job_line( struct control_file *cf, char *str, int nodup )
{
	char **lines, *buffer;
	if( cf->control_file_lines.count+2 >= cf->control_file_lines.max ){
		extend_malloc_list( &cf->control_file_lines, sizeof( char * ), 100 );
	}
	lines = (void *)cf->control_file_lines.list;
	if( nodup == 0 ){
		buffer = add_buffer( &cf->control_file_image, strlen(str)+1 );
		strcpy( buffer, str );
		str = buffer;
	}
	lines[ cf->control_file_lines.count++ ] = str;
	lines[ cf->control_file_lines.count ] = 0;
	return( str );
}

/***************************************************************************
 * char *Prefix_job_line( struct control_file *cf, char *str, int nodup )
 *  put the job line at the start of the control file
 *  nodup = 1 - do not make a copy
 ***************************************************************************/
/***************************************************************************
 * Insert_job_line( struct control_file *cf, int char *str, int nodup,
 *          int position )
 *  insert a line into the job file at the indicated position, moving rest down
 *  nodup = 1 - do not make a copy
 ***************************************************************************/

char *Insert_job_line( struct control_file *cf, char *str, int nodup, int position )
{
	int i;
	char **lines, *buffer;

	/* extend if if necessary */
	++cf->control_file_lines.count;
	if( cf->control_file_lines.count+1 >= cf->control_file_lines.max ){
		extend_malloc_list( &cf->control_file_lines, sizeof( char * ), 100 );
	}
	lines = (void *)cf->control_file_lines.list;
	/* copy down one position - now lines.count is terminating NULL pointer */
	for( i = cf->control_file_lines.count; i != position; --i ){
		lines[i] = lines[i-1];
	}
	if( nodup == 0 ){
		buffer = add_buffer( &cf->control_file_image, strlen(str)+1 );
		strcpy( buffer, str );
		lines[ position ] = buffer;
	} else {
		lines[ position ] = str;
		buffer = str;
	}
	return( buffer );
}
