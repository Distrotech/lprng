/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: cleantext.h
 * PURPOSE: cleantext.c functions
 * cleantext.h,v 3.1 1996/12/28 21:40:24 papowell Exp
 **************************************************************************/

#ifndef _CLEANTEXT_H
#define _CLEANTEXT_H

/*****************************************************************
 * char *Clean_name( char *s )
 *  scan input line for non-alphanumeric, _ characters
 *  return pointer to bad character found
 * char *Clean_FQDNname( char *s )
 *  scan input line for non-alphanumeric, _, -, @ characters
 *
 * char *Find_meta( char *s )
 *  scan input line for meta character or non-printable character
 *  return pointer to bad character found
 * void Clean_meta( char *s )
 *  scan input line for meta character or non-printable character
 *  and replace with '_'
 *
 * Check_format( int kind, char *name )
 *  check to see that control and data file names match;
 *  if kind = 0, reset match check
 *****************************************************************/
char *Clean_name( char *s );
char *Clean_FQDNname( char *s );
char *Find_meta( char *s );
int Is_meta( int c );
void Clean_meta( char *s );
int Check_format( int kind, char *name, struct control_file *cfp );

#endif
