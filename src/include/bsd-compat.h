/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: bsd-compat.h
 * PURPOSE: bsd-compat.c functions
 * bsd-compat.h,v 3.6 1998/03/24 02:43:22 papowell Exp
 **************************************************************************/

#ifndef _BSD_COMPAT_H
#define _BSD_COMPAT_H

/*****************************************************************
 * Signal handlers
 * plp_signal(int, plp_sigfunc_t);
 *  SIGALRM should be the only signal that terminates system calls
 *  with EINTR error code ; all other signals should NOT terminate
 *  them.  Note that the signal handler code should assume this.
 *  (Justin Mason, July 1994)
 *  (Ref: Advanced Programming in the UNIX Environment, Stevens)
 * plp_block_signals()
 *  blocks the set of signals used by PLP code; May need this in
 *  places where you do not want any further signals, such as termination
 *  code.
 * plp_unblock_signals() unblocks them.
 *****************************************************************/

plp_sigfunc_t plp_signal (int signo, plp_sigfunc_t func);
plp_sigfunc_t plp_signal_break (int signo, plp_sigfunc_t func);
void plp_block_all_signals ( plp_block_mask *omask );
void plp_block_one_signal( int sig, plp_block_mask *omask );
void plp_sigpause( void );
void plp_unblock_all_signals ( plp_block_mask *omask );

/* perform safe string duplication, even with null pointers */
char *safexstrdup (const char *p, int extra );
/* perform safe comparison, even with null pointers */
int safestrcmp( const char *s1, const char *s2 );
/* usleep using select() */
int plp_usleep( int t );
int plp_sleep( int t );
int Get_max_servers( void );
int plp_rand( int range );
char *Brk_check_size( void );

#endif
