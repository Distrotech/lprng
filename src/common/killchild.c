/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: killchild.c
 * PURPOSE: kill and create children
 **************************************************************************/

static char *const _id = "$Id: killchild.c,v 3.2 1996/08/25 22:20:05 papowell Exp papowell $";

#include "lp.h"
#include "decodestatus.h"
/*
 * Historical compatibility
 */
#ifdef HAVE_SYS_TTOLD_H
# include <sys/ttold.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif

/***************************************************************************
Commentary:
	When we fork a child, then we need to clean it up.
	Note that the forking operation should either be to create
	a subchild which will be in the process group, or one
	that will be in the same process group

We will keep a list of the children which will be in the new
process group.  When we exit, during cleanup,  we will
kill all of these off.  Note that we may have to dynamically allocate
memory to keep the list.

killchildren( signal, pid )
 - kill all children of this process
 ***************************************************************************/

struct pinfo {
	pid_t pid;
	pid_t parentpid;
};

static struct malloc_list prgrps;

void killchildren( int signal, pid_t pid )
{
	int i;
	struct pinfo *p;
	
	DEBUG8("killchildren: pid %d, signal %s", pid, Sigstr(signal) );
	if(Debug>8){
		p = (void *)prgrps.list;
		for( i = 0; i < prgrps.count; ++i ){
			logDebug( "[%d] pid %d parentpid %d", i, p[i].pid, p[i].parentpid );
		}
	}
	p = (void *)prgrps.list;
	for( i = 0; i < prgrps.count; ++i ){
		if( p[i].parentpid == pid ){
			killpg( p[i].pid, signal );
		}
	}
}

/*
 * dofork: fork a process and set it up as a process group leader
 */

int dofork()
{
	struct pinfo *p;
	pid_t pid;
	int i;
	char *s;

	pid = fork();
	if( pid == 0 ){
		/* set subgroups to 0 */
		prgrps.count = 0;

		/* you MUST put the process in another group;
		 * if you have a filter, and it does a 'killpg()' signal,
		 * if you do not have it in a separate group the effects
		 * would be catastrophic.
		 */

#if defined(HAVE_SETSID)
		i =	setsid();
		s = "setsid()";
#else
# if defined(HAVE_SETPGID)
		i =	setpgid(0,getpid());
		s =	"setpgid(0,getpid())";
# else
#   if defined(HAVE_SETPGRP_0)
		i =	setpgrp(0,getpid());
		s =	"setpgrp(0,getpid())";
#   else
	    i =	setpgrp();
	    s =	"setpgrp()";
#   endif
# endif
#endif
		if( i < 0 ){
			logerr_die( LOG_ERR, "forkpg: %s failed", s );
		}
#ifdef TIOCNOTTY
		/* BSD: non-zero process group id, so it cannot get control terminal */
		/* you MUST be able to turn off the control terminal this way */
		if ((i = open ("/dev/tty", O_RDWR, 0600 )) >= 0) {
			if( ioctl (i, TIOCNOTTY, (struct sgttyb *)0 ) < 0 ){
				logerr_die( LOG_ERR, "dofork: TIOCNOTTY failed" );
			}
			(void)close(i);
		}
#endif
		/* BSD: We have lost the controlling terminal at this point;
		 *  we are now the process group leader and cannot get one
		 *  unless we use the TIOCTTY ioctl call
		 * Solaris && SYSV: We have lost the controlling terminal at this point;
		 *  we are now the process group leader,
		 *  we may get control tty unless we open devices with
		 *	the O_NOCTTY flag;  if you do not have this open flag
		 *	you are in trouble.
		 * See: UNIX Network Programming, W. Richard Stevens,
		 *		 Section 2.6, Daemon Processes
		 *		Advanced Programming in the UNIX environment,
		 *		 W. Richard Stevens, Section 9.5
		 */
	} else if( pid != -1 ){
		if( prgrps.count >= prgrps.max ){
			extend_malloc_list( &prgrps, sizeof( struct pinfo), 10 );
		}
		p = (void *)prgrps.list;
		for( i = 0; i < prgrps.count; ++i ){
			if( p[i].parentpid == 0 ){
				p[i].parentpid = getpid();
				p[i].pid = pid;
				break;
			}
		}
		if( i >= prgrps.count ){
			p[prgrps.count].parentpid = getpid();
			p[prgrps.count].pid = pid;
			++prgrps.count;
		}
	}
	DEBUG8("dofork: pid %d, daughter %d, number of children %d",
		getpid(), pid, prgrps.count );
	if(Debug>8){
		p = (void *)prgrps.list;
		for( i = 0; i < prgrps.count; ++i ){
			logDebug( "[%d] pid %d parentpid %d", i, p[i].pid, p[i].parentpid );
		}
	}
	return( pid );
}

void removepid( pid_t pid )
{
	int i;
	struct pinfo *p;

	p = (void *)prgrps.list;
	for( i = 0; i < prgrps.count; ++i ){
		if( p[i].pid == pid ){
			p[i].pid = p[i].parentpid = 0;
			break;
		}
	}
} 

/*
 * routines to call on exit
 */


/***************************************************************************
 * User level implementation of on_exit() function
 *  register_exit( routine, parm )
 *    -registers a call to  routine(parm)
 *    - returns back entry in list
 *  void remove_exit( int i )
 *    - removes entry i from exit list
 *  void cleanup( int signal )
 *    - calls routines and does cleanup
 ***************************************************************************/

#define MAX_EXIT 4

struct exit_info {
	exit_ret exit;
	void *parm;
} exit_list[MAX_EXIT];

int register_exit( exit_ret exit, void *p )
{
	int i;
	for( i = 0; i < MAX_EXIT && exit_list[i].exit; ++i );
	if( i < MAX_EXIT ){
		exit_list[i].exit = exit;
		exit_list[i].parm = p;
	} else {
		fatal( LOG_ERR, "cannot register exit function" );
	}
	return( i );
}

void remove_exit( int i )
{
	exit_list[i].exit = 0;
}

plp_signal_t cleanup (int passed_signal)
{
	int i, pgrp;
	int signal = passed_signal;

	DEBUG6("cleanup: signal %d, Errorcode %d", signal, Errorcode);
	if( passed_signal == 0 ) signal = SIGINT;
    plp_block_signals (); /**/
	for( i = MAX_EXIT; i-- > 0; ){
		if( exit_list[i].exit ){
			exit_list[i].exit( exit_list[i].parm );
		}
	}
	i = getpid();
	/* kill children of this process */
	killchildren( signal, i );
	/* if a signal and a process leader, then kill all children */
	/* you may not believe this, but the SVR4 version does not take
		a parameter */

#if defined(HAVE_GETPGRP_0)
	pgrp = getpgrp(0);
#else
	pgrp = getpgrp();
#endif

	if( (i == pgrp) ){
		killpg( i, signal );
	}
	/* plp_unblock_signals (); / **/
	/* sleep for a second to allow the children to get gathered */
	/* sleep(1); / **/
	if( Errorcode ){
		exit( Errorcode );
	}
	exit(passed_signal);
}

