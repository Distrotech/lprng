/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2001, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 * $Id: child.h,v 1.14 2001/09/02 20:42:16 papowell Exp $
 ***************************************************************************/



#ifndef _CHILD_H_
#define _CHILD_H_ 1

typedef void (*exit_ret)( void *p );

struct exit_info {
	exit_ret exit;
	void *parm;
	char name[64];
};


/* PROTOTYPES */
pid_t plp_waitpid (pid_t pid, plp_status_t *statusPtr, int options);
pid_t plp_waitpid_timeout(int timeout,
	pid_t pid, plp_status_t *status, int options);
void Dump_pinfo( char *title, struct line_list *p ) ;
int Countpid(void);
void Killchildren( int sig );
pid_t dofork( int new_process_group );
void Register_exit( char *name, exit_ret exit_proc, void *p );
void Clear_exit( void );
plp_signal_t cleanup_USR1 (int passed_signal);
plp_signal_t cleanup_HUP (int passed_signal);
plp_signal_t cleanup_INT (int passed_signal);
plp_signal_t cleanup_QUIT (int passed_signal);
plp_signal_t cleanup_TERM (int passed_signal);
plp_signal_t cleanup (int passed_signal);
void Dump_unfreed_mem(char *title);

#endif
