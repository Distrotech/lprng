/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2001, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 * $Id: user_objs.h,v 1.12 2002/02/25 17:43:23 papowell Exp $
 ***************************************************************************/



#ifndef _USER_OBJS_H_
#define _USER_OBJS_H_ 1

/* PROTOTYPES */
int test_chooser( struct line_list *servers,
	struct line_list *available, int *use_subserver );
char *test_sort_key( struct job *job );

#endif
