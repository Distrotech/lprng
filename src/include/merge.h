/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2001, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 * $Id: merge.h,v 1.18 2001/09/07 20:13:11 papowell Exp $
 ***************************************************************************/


#ifndef _MERGE_H_
#define _MERGE_H_ 1

/* PROTOTYPES */
int
Mergesort(void *base, size_t nmemb, size_t size, 
	int (*cmp)(const void *, const void *, const void *), const void * arg);

#endif
