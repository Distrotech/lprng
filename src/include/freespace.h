/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: freespace.h
 * PURPOSE: find file system free space functions
 * freespace.h,v 3.1 1996/12/28 21:40:26 papowell Exp
 **************************************************************************/

unsigned long Space_needed( char *min_space, struct dpathname *dpath );
unsigned long Space_avail( struct dpathname *dpath );
int Check_space( int jobsize, char *min_space, struct dpathname *dpath);
