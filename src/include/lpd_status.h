/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 * $Id: lpd_status.h,v 5.1 1999/09/12 21:33:03 papowell Exp papowell $
 ***************************************************************************/



#ifndef _LPD_STATUS_H_
#define _LPD_STATUS_H_ 1

/* PROTOTYPES */
int Job_status( int *sock, char *input );
void Get_queue_status( struct line_list *tokens, int *sock,
	int displayformat, int status_lines, struct line_list *done_list,
	int max_size );
void Print_status_info( int *sock, char *dir, char *file,
	char *prefix, int status_lines, int max_size );
void Get_local_or_remote_status( struct line_list *tokens, int *sock,
	int displayformat, int status_lines, struct line_list *done_list,
	int max_size );

#endif
