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
 * killchild.h,v 3.4 1997/10/15 04:06:28 papowell Exp
 **************************************************************************/

#ifndef _KILLCHILD_H
#define _KILLCHILD_H


int dofork( int new_process_group );
void Removepid( pid_t pid );
typedef void (*exit_ret)( void *p );
int register_exit( exit_ret exit, void *p );
void remove_exit( int i );
void clear_exit( void );
int Countpid( int clean );
plp_signal_t cleanup_HUP (int passed_signal);
plp_signal_t cleanup_INT (int passed_signal);
plp_signal_t cleanup_TERM (int passed_signal);
plp_signal_t cleanup_QUIT (int passed_signal);
plp_signal_t cleanup_USR1 (int passed_signal);
plp_signal_t cleanup (int passed_signal);
void killchildren( int signal );

#endif
