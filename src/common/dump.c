/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: dump.c
 * PURPOSE: dump various data structures
 **************************************************************************/

static char *const _id = "$Id: dump.c,v 3.1 1996/08/25 22:20:05 papowell Exp papowell $";

#include "lp.h"

/**********************************************************************
 *dump_params( char *title, struct keywords *k )
 * - dump the list of keywords and variable values given by the
 *   entries in the array.
 **********************************************************************/

void dump_parms( char *title, struct keywords *k )
{
	char *s, **l;
	int i;

	if( title ) logDebug( "*** %s ***", title );
	for( ; k &&  k->keyword; ++k ){
		switch(k->type){
		case FLAG_K: case INTEGER_K:
			logDebug( "%s: %d", k->keyword, *(int *)(k->variable) );
			break;
		case STRING_K:
			s = *(char **)(k->variable);
			if( s == 0 ) s = "<NULL>";
			logDebug( "%s: %s", k->keyword, s );
			break;
		case LIST_K:
			l = *(char ***)(k->variable);
			logDebug( "%s:", k->keyword );
			for( i = 0; l && l[i]; ++i ){
				logDebug( " [%d] %s", i, l[i]);
			}
			break;
		default:
			logDebug( "%s: UNKNOWN TYPE", k->keyword );
		}
	}
	if( title ) logDebug( "*** <END> ***", title );
}

/***************************************************************************
 * dump_config_list( char *title, struct keywords **list )
 * Dump configuration information
 *   This is supplied as a list of pointers to arrays of keywords
 ***************************************************************************/

void dump_config_list( char *title, struct keywords **list )
{
	
	if( title ) logDebug( "*** %s ***", title );
	while(list && *list ){
		dump_parms( (char *)0, *list );
		logDebug( "***", title );
		++list;
	}
	if( title ) logDebug( "*** <END> ***", title );
}

/***************************************************************************
 * dump_data_file( char *title, struct data_file *list, int count )
 * Dump data file information
 ***************************************************************************/

void dump_data_file( char *title,  struct data_file *list )
{
	if( title ) logDebug( "*** %s ***", title );
	if( list ){
		logDebug( "openname '%s', transfername '%s', N '%s', U '%s'",
			list->openname, list->transfername, list->Ninfo, list->Uinfo );
		logDebug( "  fd %d, line %d, linecount %d, flags %d, copies %d",
			list->fd, list->line, list->linecount, list->flags, list->copies );
	}
}
void dump_data_file_list( char *title,  struct data_file *list, int count )
{
	int i;

	if( title ) logDebug( "*** %s ***", title );
	for( i = 0; list && i < count; ++i ){
		dump_data_file( (char *)0, &list[i] );
	}
	if( title ) logDebug( "*** <END> ***", title );
}


/***************************************************************************
 * dump_malloc_list( char *title, struct malloc_list *list )
 *  Dump malloc_list data
 ***************************************************************************/
void dump_malloc_list( char *title,  struct malloc_list *list )
{
	if( title ) logDebug( "*** %s ***", title );
	if( list ){
		logDebug( "list 0x%x, count %d, max %d, size %d",
			list->list, list->count, list->max, list->size );
	}
}

/***************************************************************************
 * dump_control_file( char *title, struct control_file *cf )
 * Dump Control file information
 ***************************************************************************/

void dump_control_file( char *title,  struct control_file *cf )
{
	int i, j;
	int count;
	char **line;
	if( title ) logDebug( "*** %s ***", title );
	if( cf ){
		logDebug( "name '%s', number %d, jobsize %d, filehostname '%s'",
			cf->name, cf->number, cf->jobsize, cf->filehostname );
		logDebug( "active %d, held_class %d",
			cf->active, cf->held_class );
		logDebug( "   flags 0x%x, priority_time %ld, hold_time %ld, error `%s'",
			 cf->flags, cf->priority_time, cf->hold_time, cf->error );
		logDebug( "   print_attempts %d, control_info %d",
			cf->print_attempts, cf->control_info );
		count = cf->info_lines.count;
		line = (void *)cf->info_lines.list;
		for( i = 0; i < count; ++i ){
			logDebug( "line [%d] '%s'", i, line[i] );
		}
		for( i = 0; i < 26; ++i ){
			if( cf->capoptions[i] ){
				logDebug( "option[%c] '%s'", i+'A', cf->capoptions[i] );
			}
		}
		for( i = 0; i < 10; ++i ){
			if( cf->digitoptions[i] ){
				logDebug( "option[%c] '%s'", i+'0', cf->digitoptions[i] );
			}
		}
		if( cf->destination_list.count ){
			struct destination *destination, *d;
			char **lines;
			destination = (void *)cf->destination_list.list;
			for( i = 0; i < cf->destination_list.count; ++i ){
				d = &destination[i];
				logDebug( "destination %d", i );
				logDebug( "  dest='%s', id='%s', error='%s', done %d",
					d->destination,d->identifier,d->error, d->done );
				logDebug( "  copies=%d, copy_done=%d, status=%d, active=%d",
					d->copies, d->copy_done, d->status, d->active );
				logDebug( "  arg_start=%d, arg_count=%d",
					d->arg_start, d->arg_count );
				lines = &cf->destination_lines.list[d->arg_start];
				for( j = 0; j < d->arg_count; ++j ){
					logDebug( "  arg[%d]='%s'", j, lines[j] );
				}
			}
		}
		dump_data_file_list( "Data files",
			(void *)cf->data_file.list, cf->data_file.count );
	}
}

/***************************************************************************
 * dump_control_file_list( char *title, struct control_file **cf )
 * Dump Control file information
 ***************************************************************************/

void dump_control_file_list( char *title,  struct control_file **cf )
{
	int i;
	char buff[LINEBUFFER];

	if( title ) logDebug( "*** %s ***", title );
	for( i = 0; cf && cf[i]; ++i ){
		plp_snprintf( buff, sizeof(buff), "control file [%d]", i );
		dump_control_file( buff, cf[i] );
	}
}

/***************************************************************************
 * dump_filter( char *title, struct filter *filter )
 * Dump file information
 ***************************************************************************/

void dump_filter( char *title,  struct filter *filter )
{
	int i;
	if( title ) logDebug( "*** %s ***", title );
	if( filter ){
		logDebug( "filter pid %d, input %d, argc %d, cmd '%s'",
			filter->pid, filter->input, filter->args.count, filter->cmd );
		for( i = 0; i < filter->args.count; ++i ){
			logDebug( "  [%d] '%s'", i, filter->args.list[i] );
		}
	}
}

