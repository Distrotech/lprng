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
 * $Id: malloclist.h,v 3.3 1997/02/04 22:01:49 papowell Exp papowell $
 **************************************************************************/

#ifndef _MALLOCLIST_H
#define _MALLOCLIST_H

/***************************************************************************
 * extend_malloc_list( struct malloc_list *buffers, int element )
 *   add more entries to the array in the list
 ***************************************************************************/

void extend_malloc_list( struct malloc_list *buffers, int element, int incr );

/***************************************************************************
 * add_buffer( pcf, size )
 *  add a new entry to the printcap file list
 * return pointer to start of size byte area in memory
 ***************************************************************************/

char *add_buffer( struct malloc_list *buffers, int len );


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

char *Add_job_line( struct control_file *cf, char *str, int nodup );

char *Insert_job_line( struct control_file *cf, char *str, int nodup, int position );

#endif
