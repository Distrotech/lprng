/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: malloclist.h
 * PURPOSE: malloclist.c functions
 * malloclist.h,v 3.4 1998/03/24 02:43:22 papowell Exp
 **************************************************************************/

#ifndef _MALLOCLIST_H
#define _MALLOCLIST_H

/***************************************************************************
 * extend_malloc_list( struct malloc_list *buffers, int element )
 *   add more entries to the array in the list
 ***************************************************************************/

void extend_malloc_list( struct malloc_list *buffers, int element, int incr,
	char *file, int ln);

/***************************************************************************
 * add_buffer( pcf, size ), add_str( pcf, str )
 *  add a new entry to the malloc list
 * return pointer to start of size byte area in memory
 ***************************************************************************/

char *add_buffer( struct malloc_list *buffers, int len, char *file, int ln );
char *add_str( struct malloc_list *buffers, char *str, char *file, int ln );


/***************************************************************************
 * clear_malloc_list( struct malloc_list *buffers, int free_list )
 *  if the free_list value is non-zero,  free the list pointers
 *  set the buffer count to 0
 ***************************************************************************/

void clear_malloc_list( struct malloc_list *buffers, int free_list );

/*
 * clear a control file data structure
 */
void Clear_control_file( struct control_file *cf );

char *Add_job_line( struct control_file *cf, char *str, int nodup,
	char *file, int ln );

char *Insert_job_line( struct control_file *cf, char *str, int nodup,
	int position, char *file, int ln );

#endif
