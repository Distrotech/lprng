/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: child.c,v 5.1 1999/09/12 21:32:33 papowell Exp papowell $";


#include "lp.h"
#include "getqueue.h"
#include "getopt.h"
#include "gethostinfo.h"
#include "proctitle.h"
#include "linksupport.h"
#include "child.h"
/**** ENDINCLUDE ****/

/*
 * Historical compatibility
 */
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif

/*
 * This avoids some problems
 * when you have things redefined in system include files.
 * Don't even think about how this came about - simply mutter
 * the mantra 'Portablility at any cost...' and be happy.
 */
#if !defined(TIOCNOTTY) && defined(HAVE_SYS_TTOLD_H) && !defined(IRIX)
#  include <sys/ttold.h>
#endif


/*
 * Patrick Powell
 *  We will normally want to wait for children explitly.
 *  Most of the time they can die, but we do not care.
 *  However, some times we want them to cause a BREAK condition
 *  on a system call.
 */

pid_t plp_waitpid (pid_t pid, plp_status_t *statusPtr, int options)
{
	int report;
	DEBUG2("plp_waitpid: pid %d, options %d", pid, options );
	report = waitpid(pid, statusPtr, options );
	DEBUG2("plp_waitpid: report %d, status %s", report,
		Decode_status( statusPtr ) );
	return report;
}

pid_t plp_waitpid_timeout(int timeout,
	pid_t pid, plp_status_t *status, int options)
{
	int report = -1;
	int err;
	DEBUG2("plp_waitpid_timeout: timeout %d, pid %d, options %d",
		timeout, pid, options );
	if( Set_timeout() ){
		Set_timeout_alarm( timeout  );
		report = plp_waitpid( pid, status, options );
		err = errno;
	} else {
		report = -1;
		err = EINTR;
	}
	Clear_timeout();
	DEBUG2("plp_waitpid_timeout: report %d, status %s", pid,
		Decode_status( status ) );
	errno = err;
	return( report );
}

/***************************************************************************
 * Commentary:
 * When we fork a child, then we need to clean it up.
 * Note that the forking operation should either be to create
 * a subchild which will be in the process group, or one
 * that will be in the same process group
 *
 * We will keep a list of the children which will be in the new
 * process group.  When we exit, during cleanup,  we will
 * kill all of these off.  Note that we may have to dynamically allocate
 * memory to keep the list.
 *
 * Killchildren( signal ) - kill all children of this process
 ***************************************************************************/

 struct pinfo {
	pid_t pid;
	pid_t ppid;
 };

void Dump_pinfo( char *title, struct line_list *p ) 
{
	struct pinfo *sp;
	int i;
	logDebug("*** Dump_pinfo %s - count %d ***", title, p->count );
	for( i = 0; i < p->count; ++i ){
		sp = (void *)p->list[i];
		logDebug("  pid %d, parent %d", sp->pid, sp->ppid );
	}
	logDebug("*** done ***");
}

int Countpid(void)
{
	int i;
	int pid = getpid();
	struct pinfo *p;

	if(DEBUGL4)Dump_pinfo("Countpid - before",&Process_list);
	for( i = 0; i < Process_list.count; ){
		p = (void *)Process_list.list[i];
		if( p->ppid == pid && kill(p->pid, 0) == 0 ){
			DEBUG4("Countpid: pid %d active", p->pid );
			++i;
		} else {
			DEBUG4("Countpid: pid %d not active, errno '%s'",
				p->pid, Errormsg(errno) );
			Remove_line_list(&Process_list,i);
		}
	}
	if(DEBUGL4)Dump_pinfo("Countpid - after", &Process_list);
	return( Process_list.count );
}

void Killchildren( int sig )
{
	int pid = getpid();
	int i;
	struct pinfo *p;
	
	DEBUG2("Killchildren: pid %d, signal %s, count %d",
			pid,Sigstr(sig), Process_list.count );

	for( i = 0; i < Process_list.count; ++i ){
		p = (void *)Process_list.list[i];
		DEBUG2("Killchildren: kill(%d,%s)", p->pid, Sigstr(sig));
		if( p->ppid == pid && p->pid > 0 && kill(p->pid, sig) == 0
			&& kill(p->pid, SIGCONT) == 0 ){
			DEBUG4("Killchildren: pid %d active", p->pid );
		} else {
			p->pid = 0;
			p->ppid = 0;
		}
	}
	if(DEBUGL2)Dump_pinfo("Killchildren - after",&Process_list);
}

/*
 * dofork: fork a process and set it up as a process group leader
 */

int dofork( int new_process_group )
{
	struct pinfo *p;
	pid_t pid;
	int i;
	char *s;

	pid = fork();
	if( pid == 0 ){
		/* set subgroups to 0 */
		Free_line_list(&Process_list);

		/* you MUST put the process in another process group;
		 * if you have a filter, and it does a 'killpg()' signal,
		 * if you do not have it in a separate group the effects
		 * are catastrophic.  Believe me on this one...
		 */

		if( new_process_group ){
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
			logerr_die( LOG_ERR, "dofork: %s failed", s );
		}
#ifdef TIOCNOTTY
		/* BSD: non-zero process group id, so it cannot get control terminal */
		/* you MUST be able to turn off the control terminal this way */
		if ((i = open ("/dev/tty", O_RDWR, 0600 )) >= 0) {
			if( ioctl (i, TIOCNOTTY, (void *)0 ) < 0 ){
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
		}
		/* we do not want to copy our parent's exit jobs or temp files */
		Clear_tempfile_list();
		Clear_exit();
		if( Name ) setproctitle( "lpd %s", Name );
	} else if( pid != -1 ){
		Check_max(&Process_list,1);
		p = malloc_or_die(sizeof(p[0]),__FILE__,__LINE__);
		Process_list.list[Process_list.count++] = (char *)p;
		p->pid = pid;
		p->ppid = getpid();
	}
	return( pid );
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

void Register_exit( char *name, exit_ret exit_proc, void *p )
{
	struct exit_info *e;

	e = malloc_or_die( sizeof(e[0]), __FILE__,__LINE__ );
	e->exit = exit_proc;
	e->parm = p;
	safestrncpy(e->name, name);
	Check_max(&Exit_list,1);
	Exit_list.list[Exit_list.count++] = (char *)e;
}

void Clear_exit( void )
{
	Free_line_list(&Exit_list);
}

plp_signal_t cleanup_USR1 (int passed_signal)
{
	DEBUG1("cleanup_USR1: signal %s, Errorcode %d", Sigstr(passed_signal), Errorcode);
	cleanup(SIGUSR1);
}
plp_signal_t cleanup_HUP (int passed_signal)
{
	DEBUG1("cleanup_HUP: signal %s, Errorcode %d", Sigstr(passed_signal), Errorcode);
	cleanup(SIGHUP);
}
plp_signal_t cleanup_INT (int passed_signal)
{
	DEBUG1("cleanup_INT: signal %s, Errorcode %d", Sigstr(passed_signal), Errorcode);
	cleanup(SIGINT);
}
plp_signal_t cleanup_QUIT (int passed_signal)
{
	DEBUG1("cleanup_QUIT: signal %s, Errorcode %d", Sigstr(passed_signal), Errorcode);
	cleanup(SIGQUIT);
}
plp_signal_t cleanup_TERM (int passed_signal)
{
	DEBUG1("cleanup_TERM: signal %s, Errorcode %d", Sigstr(passed_signal), Errorcode);
	cleanup(SIGTERM);
}

plp_signal_t cleanup (int passed_signal)
{
	plp_block_mask oblock;
	int i, n;
	int signalv = passed_signal;
	struct exit_info *e;

	plp_block_all_signals( &oblock ); /**/

	DEBUG2("cleanup: signal %s, Errorcode %d, exits %d",
		Sigstr(passed_signal), Errorcode, Exit_list.count);

	/* shut down all logging stuff */
	Doing_cleanup = 1;
	n = Get_max_fd();
	/* first we try to close all the output ports */
	for( i = 3; i < n; ++i ){
		struct stat statb;
		if( fstat( i, &statb ) == 0 && S_ISSOCK(statb.st_mode)
			&& Exit_linger_timeout_DYN > 0 ){
			Set_linger( i, Exit_linger_timeout_DYN );
		}
		close(i);
	}

	/* then we do exit cleanup */
	for( i = Exit_list.count-1; i >= 0;  --i ){
		e = (void *)(Exit_list.list[i]);
		DEBUG2("exit '%s' 0x%lx, parm 0x%lx", e->name,
			Cast_ptr_to_long(e->exit), Cast_ptr_to_long(e->parm));
		e->exit( e->parm );
	}
	Exit_list.count = 0;

	if( passed_signal == 0 ){
		signalv = SIGINT;
	} else if( passed_signal == SIGUSR1 ){
		passed_signal = 0;
		signalv = SIGINT;
		Errorcode = 0;
	}

	/* kill children of this process */
	Killchildren( signalv );
	DEBUG1("cleanup: done, exit(%d)", Errorcode);

	if( Errorcode == 0 ){
		Errorcode = passed_signal;
	}

	Dump_unfreed_mem("**** cleanup");

	exit(Errorcode);
}

void Dump_unfreed_mem(char *title)
{
#if defined(DMALLOC)
	extern int _dmalloc_outfile;
	extern char *_dmalloc_logpath;
	char buffer[SMALLBUFFER];

	if( _dmalloc_logpath && _dmalloc_outfile < 0 ){
		_dmalloc_outfile = open( _dmalloc_logpath,  O_WRONLY | O_CREAT | O_TRUNC, 0666);
	}
	plp_snprintf(buffer,sizeof(buffer),"*** Dump_unfreed_mem: %s, pid %d\n",
		title, getpid() );
	Write_fd_str(_dmalloc_outfile, buffer );
	if(Outbuf) free(Outbuf); Outbuf = 0;
	if(Inbuf) free(Inbuf); Inbuf = 0;
	Clear_tempfile_list();
	{
		struct line_list **l;
		for( l = Allocs; *l; ++l ) Free_line_list(*l);
	}
	Free_line_list(&Exit_list);
	Clear_all_host_information();
    Clear_var_list( Pc_var_list, 0 );
    Clear_var_list( DYN_var_list, 0 );
	dmalloc_log_unfreed();
	Write_fd_str(_dmalloc_outfile, "***\n" );
	exit(Errorcode);
#endif
}
