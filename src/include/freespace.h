/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: freespace.h
 * PURPOSE: find file system free space functions
 * $Id: freespace.h,v 3.0 1996/05/19 04:06:19 papowell Exp $
 **************************************************************************/

unsigned long Space_needed( char *min_space, struct dpathname *dpath );
unsigned long Space_avail( struct dpathname *dpath );
int Check_space( int jobsize, char *min_space, struct dpathname *dpath);
