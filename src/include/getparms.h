/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: getparms.h
 * PURPOSE: getparms.c functions
 * getparms.h,v 3.1 1996/12/28 21:40:27 papowell Exp
 **************************************************************************/

#ifndef _GETPARMS_H
#define _GETPARMS_H

/*****************************************************************
 * option checking assistance functions, see getparms.c for details
 *****************************************************************/
void Dienoarg(int option);
void Check_int_dup (int option, int *value, char *arg, int maxvalue);
void Check_str_dup(int option, char **value, char *arg, int maxlen );
void Check_dup(int option, int *value);

#endif
