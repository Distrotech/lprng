/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: malloclist.c
 * PURPOSE: manage dynmaic memory allocated lists
 **************************************************************************/

static char *const _id =
"$Id: malloclist.c,v 3.2 1996/09/09 14:24:41 papowell Exp papowell $";
#include "lp.h"

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
			fatal( LOG_ERR, "extend_malloc_list: element %s, original %d",
				element, buffers->size);
		}
		s = realloc( buffers->list, size );
		if( s == 0 ){
			logerr_die( LOG_ERR,
				"extend_malloc_list: realloc 0x%x to %d failed",
				buffers->list, size );
		}
	} else {
		s = malloc( size );
		if( s == 0 ){
			logerr_die( LOG_ERR,
				"extend_malloc_list: malloc %d failed", size );
		}
		buffers->size = element;
	}
	buffers->list = (void *)s;
	memset( s+orig, 0, size-orig );
	DEBUG9( "extend_malloc_list: list 0x%x, count %d, max %d, element %d",
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
	DEBUG9( "add_buffer: 0x%x before list 0x%x, count %d, max %d, len %d",
		buffers,
		buffers->list, buffers->count, buffers->max, len );
	if( buffers->count >= buffers->max ){
		extend_malloc_list( buffers, sizeof( buffers->list[0] ), 10 );
	}

	/* malloc data structures */
	len = len + 1;
	s = malloc( len );
	if( s == 0 ){
		logerr_die( LOG_ERR, "add_buffer: malloc %d failed", len );
	}
	s[0] = 0;
	s[len-1] = 0;

	buffers->list[buffers->count] = s;
	++buffers->count;
	DEBUG9( "add_buffer: 0x%x after list 0x%x, count %d, max %d, allocated 0x%x, len %d",
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
	DEBUG9(
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
	struct malloc_list d_cf, d_df, d_if;
	struct malloc_list d_dlines, d_dlist;

	/* clear the buffer areas */
	DEBUG9( "Clear_control_file: cf 0x%x", cf );

	clear_malloc_list( &cf->control_file, 1 );	/* free data as well */
	d_cf = cf->control_file;
	clear_malloc_list( &cf->data_file, 0 );
	d_df = cf->data_file;
	clear_malloc_list( &cf->info_lines, 0 );
	d_if = cf->info_lines;
	clear_malloc_list( &cf->destination_lines, 0 );
	d_dlines = cf->destination_lines;
	clear_malloc_list( &cf->destination_list, 0 );
	d_dlist = cf->destination_list;
	if( cf->status_file.list ){
		free( cf->status_file.list );
	}

	/* clear the information */


	memset( cf, 0, sizeof( cf[0] ) );

	cf->control_file = d_cf;
	cf->data_file = d_df;
	cf->info_lines = d_if;
	cf->destination_lines = d_dlines;
	cf->destination_list = d_dlist;
}

char *Add_job_line( struct control_file *cf, char *str )
{
	char **lines, *buffer;
	if( cf->info_lines.count >= cf->info_lines.max ){
		extend_malloc_list( &cf->info_lines, sizeof( char * ), 100 );
	}
	lines = (void *)cf->info_lines.list;
	buffer = add_buffer( &cf->control_file, strlen(str)+1 );
	strcpy( buffer, str );
	lines[ cf->info_lines.count ] = buffer;
	++cf->info_lines.count;
	if( cf->info_lines.count >= cf->info_lines.max ){
		extend_malloc_list( &cf->info_lines, sizeof( char * ), 100 );
	}
	lines = (void *)cf->info_lines.list;
	lines[ cf->info_lines.count ] = 0;
	return( buffer );
}

char *Prefix_job_line( struct control_file *cf, char *str )
{
	int i;
	char **lines, *buffer;

	++cf->info_lines.count;
	if( cf->info_lines.count >= cf->info_lines.max ){
		extend_malloc_list( &cf->info_lines, sizeof( char * ), 100 );
	}
	lines = (void *)cf->info_lines.list;
	for( i = cf->info_lines.count; i > 0; --i ){
		lines[i] = lines[i-1];
	}
	lines[ cf->info_lines.count ] = 0;
	buffer = add_buffer( &cf->control_file, strlen(str)+1 );
	strcpy( buffer, str );
	lines[ 0 ] = buffer;
	++cf->info_lines.count;
	return( buffer );
}
