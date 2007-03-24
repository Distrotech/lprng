/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 ***************************************************************************/

#ifndef _LPD_WORKER_H_
#define _LPD_WORKER_H_ 1

void Setup_lpd_call( struct line_list *passfd, struct line_list *args );
int Make_lpd_call( char *name, struct line_list *passfd, struct line_list *args );
void Do_work( char *name, struct line_list *args );
int Start_worker( char *name, struct line_list *parms, int fd );

#endif
