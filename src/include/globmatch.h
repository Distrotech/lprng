/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 * $Id: globmatch.h,v 1.74 2004/09/24 20:20:00 papowell Exp $
 ***************************************************************************/



#ifndef _GLOBMATCH_H_
#define _GLOBMATCH_H_ 1

/* PROTOTYPES */
int Globmatch( const char *pattern, const char *str );
int Globmatch_list( struct line_list *l, const char *str );

#endif
