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
 * patselect.h,v 3.1 1996/12/28 21:40:31 papowell Exp
 **************************************************************************/

#ifndef _MERGE_H
#define _MERGE_H
int Patselect( struct token *token, struct control_file *cfp,
    struct destination **destination );

#endif
