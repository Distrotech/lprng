/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: killchild.h
 * PURPOSE: killchild.c functions
 * $Id: killchild.h,v 3.2 1997/01/15 02:21:18 papowell Exp $
 **************************************************************************/

#ifndef _KILLCHILD_H
#define _KILLCHILD_H


int dofork( void );
void Removepid( pid_t pid );
typedef void (*exit_ret)( void *p );
int register_exit( exit_ret exit, void *p );
void remove_exit( int i );
void clear_exit( void );
int Countpid( void );
plp_signal_t cleanup (int passed_signal);

#endif
