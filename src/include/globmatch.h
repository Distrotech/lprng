/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2000, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 * $Id: globmatch.h,v 5.3 2000/04/14 20:40:17 papowell Exp papowell $
 ***************************************************************************/



#ifndef _GLOBMATCH_H_
#define _GLOBMATCH_H_ 1

/* PROTOTYPES */
int glob_pattern( char *pattern, const char *str );
int Globmatch( char *pattern, const char *str );
int Globmatch_list( struct line_list *l, char *str );

#endif
