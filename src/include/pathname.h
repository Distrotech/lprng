/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: pathname.h
 * PURPOSE: pathname.c functions
 * pathname.h,v 3.1 1996/12/28 21:40:31 papowell Exp
 **************************************************************************/

#ifndef _PATHNAME_H
#define _PATHNAME_H

void Fix_dir( char *p, int size, char *s );
void Init_path( struct dpathname *p, char *s );
char *Expand_path( struct dpathname *p, char *s );
char *Clear_path( struct dpathname *p );
char *Add_path( struct dpathname *p, char *s );
char *Add2_path( struct dpathname *p, char *s1, char *s2 );

#endif
