/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2000, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 * $Id: merge.h,v 5.4 2000/06/09 21:55:32 papowell Exp papowell $
 ***************************************************************************/


#ifndef _MERGE_H_
#define _MERGE_H_ 1

/* PROTOTYPES */
int
Mergesort(void *base, size_t nmemb, size_t size, 
	int (*cmp)(const void *, const void *, const void *), const void * arg);

#endif
