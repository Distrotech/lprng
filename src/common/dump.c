/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: dump.c
 * PURPOSE: dump various data structures
 **************************************************************************/

static char *const _id = "$Id: dump.c,v 3.10 1998/01/18 00:10:32 papowell Exp papowell $";

#include "lp.h"
#include "permission.h"
#include "dump.h"
/**** ENDINCLUDE ****/

/**********************************************************************
 *dump_params( char *title, struct keywords *k )
 * - dump the list of keywords and variable values given by the
 *   entries in the array.
 **********************************************************************/

void dump_parms( char *title, struct keywords *k )
{
	char *s, **l;
	int i, v;

	if( title ) logDebug( "*** %s ***", title );
	for( ; k &&  k->keyword; ++k ){
		switch(k->type){
		case FLAG_K:
			v =	*(int *)(k->variable);
			logDebug( "%s FLAG %d", k->keyword, v);
			break;
		case INTEGER_K:
			v =	*(int *)(k->variable);
			logDebug( "%s# %d (0x%x, 0%o)", k->keyword,v,v,v);
			break;
		case STRING_K:
			s = *(char **)(k->variable);
			if( s ){
				logDebug( "%s= '%s'", k->keyword, s );
			} else {
				logDebug( "%s= <NULL>", k->keyword );
			}
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
		logDebug( "original '%s' openname '%s', transfername '%s'",
			list->original, list->openname, list->transfername );
		logDebug( "  N '%s', U '%s'", list->Ninfo, list->Uinfo );
		logDebug( "  fd %d, found %d, copies %d",
			list->fd, list->found, list->copies );
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
		logDebug( "original '%s', openname '%s'",
			cf->original, cf->openname );
		logDebug( "  transfername '%s', holdfile '%s'",
			cf->transfername, cf->hold_file );
		logDebug( "  number %d, recvd_number %d, jobsize %d, filehostname '%s'",
			cf->number, cf->recvd_number, cf->jobsize, cf->filehostname );
		logDebug( "  server %d, subserver %d, held_class %d",
			cf->hold_info.server, cf->hold_info.subserver, cf->hold_info.held_class );
		logDebug( "  flags 0x%x, priority_time %ld, hold_time %ld, error `%s'",
			cf->flags, cf->hold_info.priority_time,
			cf->hold_info.hold_time, cf->error );
		logDebug( "  done_time %ld, remove_time %ld",
			cf->hold_info.done_time, cf->hold_info.remove_time );
		logDebug( "  print_attempt %d, control_info %d, control_file_lines.count %d",
			cf->hold_info.attempt, cf->control_info, cf->control_file_lines.count );
		logDebug( "  auth_id '%s', forward_id '%s'",
			cf->auth_id, cf->forward_id );
		count = cf->control_file_lines.count;
		line = (void *)cf->control_file_lines.list;
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
		count = cf->hold_file_lines.count;
		logDebug(" hold file lines %d", count );
		line = (void *)cf->hold_file_lines.list;
		for( i = 0; i < count; ++i ){
			logDebug( "line [%d] '%s'", i, line[i] );
		}
		if( cf->destination_list.count ){
			struct destination *destination, *d;
			char **lines;
			destination = (void *)cf->destination_list.list;
			for( i = 0; i < cf->destination_list.count; ++i ){
				d = &destination[i];
				logDebug( "destination %d", i );
				logDebug( "  dest='%s', identifier '%s'",
					d->destination,d->identifier );
				logDebug( "  error='%s'",
					d->error);
				logDebug(
			"  done %d, copies=%d, copy_done=%d, status=%d, server=%d, subserver=%d seq %d",
					d->done,d->copies, d->copy_done, d->status, d->server, d->subserver,
					d->sequence_number );
				logDebug( "  priority='%c'",
					d->priority );
				logDebug( "  arg_start=%d, arg_count=%d",
					d->arg_start, d->arg_count );
				lines = &cf->hold_file_lines.list[d->arg_start];
				for( j = 0; j < d->arg_count; ++j ){
					logDebug( "  arg[%d]='%s'", j, lines[j] );
				}
			}
		}
		dump_data_file_list( "Data files",
			(void *)cf->data_file_list.list, cf->data_file_list.count );
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


/***************************************************************************
 * dump_host_information( char *title, struct host_information *info )
 * Dump file information
 ***************************************************************************/

void dump_host_information( char *title,  struct host_information *info )
{
	int i, j;
	char **list;
	unsigned char *s;
	if( title ) logDebug( "*** %s (0x%x) ***", title, info );
	if( info ){
		logDebug( "  info name count %d", info->host_names.count );
		list = info->host_names.list;
		for( i = 0; i < info->host_names.count; ++i ){
			logDebug( "    [%d] '%s'", i, list[i] );
		}
		logDebug( "  address type %d, length %d count %d",
				info->host_addrtype, info->host_addrlength,
				info->host_addr_list.count );
		s = (void *)info->host_addr_list.list;
		for( i = 0; i < info->host_addr_list.count; ++i ){
			char msg[64];
			int len;
			plp_snprintf( msg, sizeof(msg), "    [%d] 0x", i );
			for( j = 0; j < info->host_addrlength; ++j ){
				len = strlen( msg );
				plp_snprintf( msg+len, sizeof(msg)-len, "%02x",s[j] );
			}
			logDebug( "%s", msg );
			s += info->host_addrlength;
		}
	}
}

/***************************************************************************
 * dump_perm_file( char *title, struct perm_file *cf )
 ***************************************************************************/

void dump_perm_val( char *title, struct perm_val *val,
	struct perm_file *perms)
{
	int i;
	char **list = perms->list.list;
	if( val ){
		logDebug( "%s key %d, token '%s', list %d",
			title?title:"", val->key, val->token, val->list );
		for( i = val->list; list[i]; ++i ){
			logDebug( "   option [%2d] '%s'", i, list[i] );
		}
	}
}


void dump_perm_line( char *title, struct perm_line *line,
	struct perm_file *perms)
{
	struct perm_val *values = (void *)perms->values.list;
	int i;

	logDebug( "%s - perm line 0x%x, flag %d, list %d", title?title:"",
		line, line->flag, line->list );
	for( i = line->list; values[i].token; ++i ){
		char buffer[64];
		plp_snprintf( buffer, sizeof(buffer),
			"  entry [%2d]", i );
		dump_perm_val( buffer, &values[i], perms );
	}
}

void dump_perm_file( char *title,  struct perm_file *perms )
{
	int i;
	char buff[32];
	struct perm_line *line;

	if( title ) logDebug( "*** perm_file %s ***", title );
	if( perms ){
		line = (void *)perms->lines.list;
		for( i = 0; i < perms->lines.count; ++i ){
			plp_snprintf( buff, sizeof(buff), "[%d] ", i );
			dump_perm_line( buff, &line[i], perms );
		}
	}
}


/***************************************************************************
 * dump_perm_check( char *title, struct perm_check *check )
 * Dump perm_check information
 ***************************************************************************/

void dump_perm_check( char *title,  struct perm_check *check )
{
	if( title ) logDebug( "*** perm_check %s ***", title );
	if( check ){
		logDebug(
		"  user '%s', rmtuser '%s', printer '%s', service '%c'",
		check->user, check->remoteuser, check->printer, check->service );
		dump_host_information( "  host", check->host );
		dump_host_information( "  remotehost", check->remotehost );
	}
}

