/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: dump.h
 * PURPOSE: dump.c functions
 * dump.h,v 3.3 1997/02/04 23:34:23 papowell Exp
 **************************************************************************/

#ifndef _DUMP_H
#define _DUMP_H

#include "permission.h"

/**********************************************************************
 *dump_params( char *title, struct keywords *k );
 * - dump the list of keywords and variable values given by the
 *   entries in the array.
 **********************************************************************/

void dump_parms( char *title, struct keywords *k );

/***************************************************************************
 * dump_config_list( char *title, struct keywords **list );
 * Dump configuration information
 *   This is supplied as a list of pointers to arrays of keywords
 ***************************************************************************/

void dump_config_list( char *title, struct keywords **list );

/***************************************************************************
 * dump_data_file( char *title, struct data_file *list, int count );
 * Dump data file information
 ***************************************************************************/

void dump_data_file( char *title,  struct data_file *list );
void dump_data_file_list( char *title,  struct data_file *list, int count );


/***************************************************************************
 * dump_malloc_list( char *title, struct malloc_list *list );
 *  Dump malloc_list data
 ***************************************************************************/
void dump_malloc_list( char *title,  struct malloc_list *list );

/***************************************************************************
 * dump_control_file( char *title, struct control_file *cf );
 * Dump Control file information
 ***************************************************************************/

void dump_control_file( char *title,  struct control_file *cf );

/***************************************************************************
 * dump_control_file_list( char *title, struct control_file **cf );
 * Dump Control file information
 ***************************************************************************/

void dump_control_file_list( char *title,  struct control_file **cf );

/***************************************************************************
 * dump_filter( char *title, struct filter *filter );
 * Dump file information
 ***************************************************************************/

void dump_filter( char *title,  struct filter *filter );

/***************************************************************************
 * dump_host_information( char *title, struct host_information *info );
 * Dump file information
 ***************************************************************************/

void dump_host_information( char *title,  struct host_information *info );

void dump_perm_file( char *title,  struct perm_file *perms );
void dump_perm_check( char *title,  struct perm_check *check );
void dump_perm_check( char *title,  struct perm_check *check );
void dump_perm_line( char *title, struct perm_line *line, struct perm_file *perms);
#endif
