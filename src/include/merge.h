/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2001, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 * $Id: merge.h,v 1.2 2002/01/23 01:01:26 papowell Exp $
 ***************************************************************************/


#ifndef _MERGE_H_
#define _MERGE_H_ 1

/* PROTOTYPES */
int
Mergesort(void *base, size_t nmemb, size_t size, 
	int (*cmp)(const void *, const void *, const void *), const void * arg);

#endif
