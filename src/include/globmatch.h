/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2002, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 * $Id: globmatch.h,v 1.37 2002/08/12 00:01:46 papowell Exp $
 ***************************************************************************/



#ifndef _GLOBMATCH_H_
#define _GLOBMATCH_H_ 1

/* PROTOTYPES */
int glob_pattern( char *pattern, const char *str );
int Globmatch( char *pattern, const char *str );
int Globmatch_list( struct line_list *l, char *str );

#endif
