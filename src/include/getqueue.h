/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: getqueue.h
 * PURPOSE: getqueue.c functions
 * getqueue.h,v 3.3 1998/03/29 18:33:04 papowell Exp
 **************************************************************************/

#ifndef _GETQUEUE_H
#define _GETQUEUE_H

extern int Parse_cf( struct dpathname *dpath, struct control_file *cf, int check_df );
extern void Scan_queue( int check_df, int new_queue );
extern int Fix_data_file_info( struct control_file *cfp );
extern struct destination *Destination_cfp( struct control_file *cfp, int i );
extern int Job_printable_status( struct control_file *cfp, struct destination **dp,
	char *msg, int len);

#endif
