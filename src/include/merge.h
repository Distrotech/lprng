/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: merge.h
 * PURPOSE: merge.c functions
 * merge.h,v 3.1 1996/12/28 21:40:30 papowell Exp
 **************************************************************************/

#ifndef _MERGE_H
#define _MERGE_H

/* mergesort */
int  Mergesort(void *, size_t, size_t,
        int (*)(const void *, const void *));

#endif
