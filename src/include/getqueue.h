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
 * $Id: getqueue.h,v 3.2 1997/01/19 14:34:56 papowell Exp $
 **************************************************************************/

#ifndef _GETQUEUE_H
#define _GETQUEUE_H

int Parse_cf( struct dpathname *dpath, struct control_file *cf, int check_df );
void Scan_queue( int check_df, int new_queue );
void Job_count( int *hc, int *cnt );

#endif
