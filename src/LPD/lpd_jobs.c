/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpd_jobs.c
 * PURPOSE: process jobs in the queue
 **************************************************************************/

static char *const _id =
"$Id: lpd_jobs.c,v 3.3 1996/08/31 21:11:58 papowell Exp papowell $";

#include "lpd.h"
#include "printcap.h"
#include "lp_config.h"
#include "permission.h"
#include "jobcontrol.h"
#include "pr_support.h"
#include "decodestatus.h"
#include "timeout.h"
#include "sendjob.h"

/***************************************************************************
Commentary:
Patrick Powell Thu May 11 09:26:48 PDT 1995

Job processing algorithm

1. Check to see if there is already a spooler process active.
   The active file will contain the PID of the active spooler process.
2. Build the job queue
3. For each job in the job queue, service them.

The work is divided up into two stages:
	1. finding the printer printcap (Process_jobs)
	2. performing the work for the spool queue specified by the printcap entry
		Do_spool_queue

The Do_spool_queue routine provides the high level structure to the
spool handling.  It expects to be run as the head of its process group,
and will start slave processes to actually do the work.  These
slave processes will in turn be the head of their process group.
When the Do_spool_queue routine starts up,  it records it PID in the
spool directory;  this is an indication that spooling is currently active.
If the spool server is to be killed off,  a signal is sent to the PID
and this signal in turn is propagated to the slave processes.


When a slave process dies,  it must in turn kill off all of its slave
processes.  This is done by calling the Print_abort routine,  which
will try to kill off all of the slave's slave processes in a sane manner,
and then calls the cleanup(signal) routine, which will try to finish
off the job in a vulgar manner.

It turns out to be rather difficult to make this work in a fail safe manner.
Here is the overall technique.
1. A signal handler for the SIGINT signal is set up, and will call cleanup()
2. The slave process is forked by Do_spool_queue.
3. If a SIGINT signal is received,  then the slave process is in turn
	sent a SIGINT signal.  This should cause it to call Print_abort()
	and cleanup().  The slave process should exit with a SIGINT signal
	indication.
4. The slave process records its PID as the spool server PID.  When the higher
   level software wants to abort the spool server,  it will kill off
   the spool server by sending a SIGINT.  This will cause the slave to
   exit with a SIGINT indication after calling Print_abort().

MULTIPLE Servers for a Single Queue.
In the printcap, the "sv" flag sets the Server_names variable with
the list of servers to be used for this queue.  The "ss"  flag sets
the Server_queue_name flag with the queue that this is a server for.

Under normal conditions, the following process hierarchy is used:

 server process - printer 'spool queue'
    subserver process - first printer in 'Server_name'
    subserver process - second printer in 'Server_name'
    ...

The server process does the following:
  for each printer in the Server_name list
   1. forks a subserver process
   2. reads the queue of things to do
   3. waits for a subserver process to exit.
   4. when the subserver process exits,  it will return:
       JSUCC: job completed, schedule another (see 
       JABORT: do not use this printer
       JACTIVE: there is already a server process active
       JFAIL: retry the same job
   5. JSUCC: get the next available job in the list,  and transfer
       it to the subserver process by copying it to the subserver
       spool directory.  Call the subserver
   6. JABORT: mark the printer as inactive, do not use again
   7. JACTIVE: mark the printer as active,  and set up a timeout
		to try again.

 ***************************************************************************/

void Do_queue_jobs( char *name );
static char *Start_subservers( struct stat *statb,
	struct pc_used *pc_used );
int Decode_transfer_failure( int status );

static char error[LINEBUFFER];

static struct control_file *src_cfp;	/* transfer from srcpathname */
int Remote_job( struct control_file *cfp, struct pc_used *pc_used );
static void Copy_job();
static char *Start_all();
static int Fork_subserver( struct server_info *server_info );
static int Check_printable( struct control_file *cfp );
static int Wait_for_subserver( struct malloc_list *servers,
	struct server_info **server_info_result );

/***************************************************************************
 * Process_jobs()
 *  This is the high level routine that is called directly by the
 *  lpd server code to handle the starting of print servers.
 *  It is passed a line with the queue name on it and will then
 *  start up all the other servers.
 *
 *  Note: The printer names passed should not be 'vulgar' and have funny
 *   metacharacters in it.
 ***************************************************************************/
void Process_jobs( int *socket, char *input, int maxlen )
{
	char *name, *s, *end;
	pid_t result;				/* process pid and exit status */
	plp_status_t status;
	int err;					/* saved errno */

	/* extract name of printer from input */
	Name = "Process_jobs";
	++input;
	if( (s = strchr( input, '\n' )) ) *s = 0;
	DEBUG3("Process_jobs: doing '%s'", input );

	/* send the ACK and then close the connection, wait only short time */
	if( socket ){
		(void)Link_send( ShortRemote, socket, 1, 0x100, 0, 0, 0 );
		close( *socket );
	}

	/* process the list of names on the command line */
	for( name = input; name && *name; name = end ){
		end = strpbrk( name, ",; \t" );
		if( end ) *end++ = 0;

		if( (s = Clean_name( name )) ){
			DEBUG4( "Process_jobs: bad printer name '%s'", name );
			continue;
		}
		setproctitle( "lpd %s '%s'", Name, name );
		/*
		 * if we are starting 'all' then we need to start subprocesses
		 */

		if( strcmp( name, "all" ) == 0){
			name = Start_all();
		}
		/* Call the routine to actually do the work */
		Do_queue_jobs( name );
	}
	do{
		result = plp_waitpid( -1, &status, 0 );
		err = errno;
		DEBUG8( "Process_jobs: result %d, status 0x%x", result, status );
		removepid( result );
	} while( result != -1 || err != ECHILD );

	DEBUG4( "Process_jobs: done" );
	cleanup( 0 );
	exit( 0 );
}



/***************************************************************************
 * Start_all()
 *  This tries to start all the queue servers,  and is called by the
 *  LPD server.  It will run down the printcap list,  checking for
 *  servers;  when it finds a valid entry,  it will then fork a server.
 *  In order to avoid overwhelming the system with forks,  it waits
 *  a while between forks;  this gives the system a chance to do other
 *  work during startup.
 ***************************************************************************/

static char *Start_all()
{
	int i, c;
	struct printcap *pc, *ppc;		/* printcap entry for printer */
	char *s, *end;
	pid_t pid, result;				/* process pid and exit status */
	plp_status_t status;
	int err;

	/* we work our way down the printcap list, checking for
		ones that have a spool queue */
	/* note that we have already tried to get the 'all' list */
	Name = "Start_all";
	setproctitle( "lpd %s", Name );
	if( All_list && *All_list ){
		static char *list;
		if( list ){
			free( list );
			list = 0;
		}
		list = safestrdup( All_list );
		for( i = 0, s = list; s && *s; s = end, ++i ){
			end = strpbrk( s, ", \t" );
			if( end ){
				*end++ = 0;
			}
			while( (c = *s) && isspace(c) ) ++s;
			if( c == 0 ) continue;
			/* give them a chance to run */
			if( i > 0 ) sleep(1);
			if( (pid = dofork()) < 0 ){
				logerr_die( LOG_INFO, "Start_all: fork failed!" );
			}
			if( pid == 0 ){
				Printer = s;
				return( s );
			}
		}
	} else {
		for( i = 0; i < Printcapfile.pcs.count; ++i ){
			ppc = (void *)Printcapfile.pcs.list;
			pc = &ppc[i];
			Printer = pc->pcf->lines.list[pc->name];
			DEBUG4( "Job_control: checking printcap entry '%s'",  Printer );

			/* this is a printcap entry with spool directory -
				we need to check it out
			 */
			s = Get_pc_option( "sd", pc );
			if( s ){
				/* do not overload the system by trying to do everything
					at once */
				if( i > 0 ) sleep(1);
				/* put it in its own process group */
				if( (pid = dofork()) < 0 ){
					logerr_die( LOG_INFO, "Start_all: fork failed!" );
				}
				if( pid == 0 ){
					return( Printer );
				}
			}
		}
	}
	DEBUG8( "Start_all: waiting for children" );
	
	do{
		result = plp_waitpid( -1, &status, 0 );
		err = errno;
		DEBUG8( "Start_all: result %d, status 0x%x", result, status );
		removepid( result );
	} while( result != -1 || err != ECHILD );
	exit( JSUCC );
}


/***************************************************************************
 Do_queue_jobs( char *printer )
 1. Set up indication that we are Master queue server
 2. Create slave
 3. Make slave work
 4. Keep slaves working until all work is done

Actually, it is a little more complex than that!
We first check to see if the printcap entry is valid; the Setup_printer()
routine covers a lot of ground in this area,  such as checking for
spool queues and directories.  After the return from Setup_printer()
we should be in a well established state, with the current directory
set to the spool directory.

We then check to see if we are a slave (sv) to a master spool queue;
if we are and we are not a child process of the 'master' server,
we exit.  This happens when all processes are started.
Note: if we spool something to a slave queue,  then we need to start
the master server to make the slave printer work.  The various startup
code things should take care of this.


Next we see if printing has been disabled or there is another server
active.  If not, we proceed to leap into action and check for jobs.
 
Part of this leaping is to establish some open file descriptors and
lock files.  These are opened once;  I suppose that they could be
opened each time I did a job, but who cares?

Note carefully that a master process will fork slaves;  the slaves
will not close the masters lock files;  this means a new master
cannot start serving the queue until all the slaves are dead.
Why this action do you ask?  The reason is that it is difficult
for a new master to inherit slaves from a dead master.

Now the code for starting slaves is rather ingenious. (well, I think it is)
If the master discovers it needs to create slaves,  it does so
by calling a subroutine;  the master will fork in the subroutine
(and never return);  the slave will return,  and then make a
goto (stop retching) to the beginning of the code.  In actual fact,
we are just implementing a finite state machine (FSM) here;  I could have
called the 'Do_queue_jobs' in the subroutine,  but I did this code
the first time based on an FSM design and never spotted the recursion.


A slave will repeat the same set of actions,  until it runs into the test
for 'src_cfp',  which is the job that it is supposed to handle.
It will transfer it to its own queue,  and then remove it from the
master's queue.

At this point,  the process is actually the print queue server process,
and will now happily start looking for work.

The first thing that happens it to check for spool control changes;
then you check to see if there are jobs to print;  if there are,
you then check to see if they are printable.

You then fork a process to do the actual printing. Why fork, you must
ask yourself.  It turns out that many implementations of some network
based databased systems and other network database routines are broken,
and have memory leaks or never close file descriptors.  Up to the point
where the loop for checking the queue starts,  there is a known number
of file descriptors open,  and dynamically allocated memory.
After this,  it is difficult to predict just what is going to happen.
By forking a 'subserver' process, we firewall the actual memory and
file descriptor screwups into the subserver process.

When the subserver exits, it returns an error code that the server
process then interprets.  This error code is used to either remove the job
or retry it.

Note that there are conditions under which a job cannot be removed.
We simply abort at that point and let higher level authority (admins)
deal with this.

 ***************************************************************************/

static char *subserver_for;	/* flag set when a subserver */
static struct pc_used pc_used;
static void cleanup_noerr();
static void cleanup_err();

void Do_queue_jobs( char *name )
{
	int lockfd, lock, create;	/* lock file fd */
	struct stat statb;			/* status of file */
	struct stat control_statb;	/* status of control file */
	pid_t pid;					/* process id */
	int unspoolerfd;			/* server id */
	int entry;					/* entry in queue processed */
	struct control_file *cfp, **cfpp;	/* control file to process */
	pid_t result = 0;			/* result pid and status */
	plp_status_t procstatus;
	int status;					/* status of the job */
	int i, j;
	char *pname, *s;				/* ACME, the very finest pointers */
	int err;
	struct destination *destination = 0;	/* current destination */
	int can_remove;				/* can we remove this */

	/* when a subserver, we come here to re-do the work
		of reading the printcap file, etc. */
	/* process the job */
	Name = "Do_queue_jobs";
	setproctitle( "lpd %s '%s'", Name, name );

newprinter:

	Errorcode = JABORT;

	error[0] = 0;
	memset(&control_statb, 0, sizeof(control_statb) );
	i = Setup_printer( name, error, sizeof(error), &pc_used,
		debug_vars, 0, &control_statb );
	if( i ){
		if( i != 2 ){
			log( LOG_ERR, "Do_queue_jobs: %s", error );
		} else {
			DEBUG2( "Do_queue_jobs: '%s'", error );
		}
		/* here we simply exit */
		exit( Errorcode );
	}

	if( Spool_dir == 0 || *Spool_dir == 0 ){
		exit( 0 );
	}

	DEBUG4( "Do_queue_jobs: Server_queue_name '%s' and subserver for '%s'",
		Server_queue_name, subserver_for );
	if( Server_queue_name && subserver_for == 0 ){
		DEBUG0( "Do_queue_jobs: subserver and not started by server" );
		/* here we simply exit */
		exit( JSUCC );
	}
	DEBUG4( "Do_queue_jobs: total_cf_files '%d'", C_files_list.count );
	cfpp = (void *)C_files_list.list;
	if(Debug>4){
		for(i=0; i < C_files_list.count; ++i ){
			cfp = cfpp[i];
			logDebug("[%d] '%s'", i, cfp->name );
			if( Debug>9 ){
				dump_control_file( "Do_queue_jobs", cfp );
			}
		}
	}

	if( Printing_disabled ){
		DEBUG0( "Do_queue_jobs: printing disabled" );
		/* here we simply exit */
		exit( Errorcode );
	}

	/* lock the printer lock file */

	pname = Add_path( CDpathname, Printer );
	lockfd = Lockf( pname, &lock, &create, &statb );
	if( lockfd < 0 ){
		fatal( LOG_ERR, "Do_queue_jobs: cannot open '%s'", pname ); 
	}
	if( lock <= 0 ){
		pid = Read_pid( lockfd, (char *)0, 0 );
		DEBUG2( "Do_queue_jobs: server process '%d' is active", pid );
		exit( Errorcode );
		return;
	}

	DEBUG2( "Do_queue_jobs: writing lockfile '%s' with pid '%d'",
		pname,getpid() );
	Write_pid( lockfd, getpid(), (char *)0 );

	pname = Add2_path( CDpathname, "unspooler.", Printer );
	if( (unspoolerfd = Checkwrite( pname, &statb, O_RDWR, 1, 0 )) < 0 ){
		logerr_die( LOG_ERR, "Do_queue_jobs: cannot open '%s'", pname );
	}

	/* now we see if we have subservers! */
	/* note that the main process will never return, only subprocesses */
	if( Server_names && *Server_names ){
		name = Start_subservers( &control_statb, &pc_used );
		/* we deliberately leave the file descriptors open
			to keep the server locked until all subservers die
		 */
		Name = "Subserver";
		setproctitle( "lpd %s '%s'", Name, name );
		goto newprinter;
	}

	/* check to see if we have to transfer a control file */
	if( src_cfp ){
		Copy_job();
	}

rescan:
	do{
		entry = 0;

		/* check for changes to spool control information */
		if( Get_spool_control( &control_statb ) ){
			DEBUG4( "Do_queue_jobs: control file changed, restarting" );
			Fix_update( debug_vars, 0 );
		}
		if( Printing_disabled ){
			DEBUG0( "Do_queue_jobs: printing disabled" );
			/* here we simply exit */
			exit( Errorcode );
		}

		Scan_queue( 1 );

		DEBUG4( "Do_queue_jobs: total_cf_files '%d'", C_files_list.count );
		cfpp = (void *)C_files_list.list;
		if(Debug>4){
			for(i=0; i < C_files_list.count; ++i ){
				cfp = cfpp[i];
				logDebug("[%d] '%s'", i, cfp->name );
			}
		}

		for( i = 0; i < C_files_list.count; ++i ){
			cfp = cfpp[i];

			/* see if you can print it */

			destination = 0;
			DEBUG4("Do_queue_jobs: checking '%s'",
				cfp->name );
			status = Check_printable( cfp );
			DEBUG8("Do_queue_jobs: Check_printable '%s', status = %d (%s)",
				cfp->name, status, Server_status(status) );
			if( status ) goto done;

			/*
			 * before you do anything serious,  check to see if
			 * status has changed. We do this here to avoid making
			 * lots of junk calls to the check code
			 */

			if( Get_spool_control( &control_statb ) ){
				DEBUG4( "Do_queue_jobs: control file changed, restarting" );
				Fix_update( debug_vars, 0 );
				goto rescan;
			}
			/* we may be processing an entry which is routed to a new
				destination */

next_destination:
			if( cfp->routed_time ){
				/* check to see if anything left */
				can_remove = 1;
				for( j = 0; j < cfp->destination_list.count; ++j ){
					destination = (void *)cfp->destination_list.list;
					destination = &destination[j];
					if( destination->error[0] ){
						can_remove = 0;
						continue;
					}
					if( destination->copies > 0 &&
							destination->copy_done >= destination->copies ){
						destination->done = time( (void *) 0 );
					}
					if( destination->done ) continue;
					break;
				}
				/* check to see if there is work to be done
				 * if not, we do the next job in the list
				 * Note that we may have a bounce queue destination
				 */
				if( j >= cfp->destination_list.count ){
					DEBUG4(
						"Do_queue_jobs: all destinations checked, remove %d",
							can_remove );
					destination = 0;
					cfp->done_time = time( (void *)0 );
					cfp->error[0] = 0;
					if( can_remove ){
						status = JREMOVE;
					} else {
						status = JIGNORE;
					}
					goto done;
				}
			}

			/*
			 * fork the process to do the actual dirty work
			 * this is in a separate process group so we can kill it off
			 * NOTE: why, do you ask, a separate process?  Well,
			 *  there were so many memory leaks with the spooler code
			 *  that it was easier to simply firewall the allocation
			 *  code in a separate process than try to track them down
			 * The network database access stuff was especially nasty on
			 *  some systems, i.e.- remote printer access
			 */

			if( (pid = dofork()) == 0 ){
				/*
				 * daughter process
				 */
				pid = getpid();
				DEBUG4( "Do_queue_jobs: daughter %d", pid );
				Write_pid( unspoolerfd, pid, (char *)0 );
				/*
				 * make sure that you can update the hold file
				 */
				cfp->active = pid;
				Destination = destination;
				(void) plp_signal (SIGUSR1,(plp_sigfunc_t)cleanup_noerr );
				(void) plp_signal (SIGHUP,(plp_sigfunc_t)cleanup_noerr );
				(void) plp_signal (SIGINT,(plp_sigfunc_t)cleanup_err );
				(void) plp_signal (SIGQUIT,(plp_sigfunc_t)cleanup_err );
				(void) plp_signal (SIGTERM,(plp_sigfunc_t)cleanup_err );
				s = 0;
				if( cfp->redirect[0] ){
					s = cfp->redirect;
					DEBUG4( "Do_queue_jobs: redirect %s to %s",
						cfp->name, s );
				} else if( destination ){
					char copyname[16];
					copyname[0] = 0;
					destination->active = pid;
					if( destination->copies ){
						plp_snprintf( copyname,
							sizeof(copyname),
							"C%d", destination->copy_done+1 );
					}
					DEBUG4( "Do_queue_jobs: destination id before '%s', dest id '%s'",
						cfp->identifier, destination->identifier );
					if( cfp->IDENTIFIER ){
						cfp->IDENTIFIER[0] = 0;
						cfp->IDENTIFIER = 0;
					}
					plp_snprintf( cfp->ident_buffer,
						sizeof( cfp->ident_buffer),
						"A%s%s", destination->identifier, copyname );
					DEBUG4( "Do_queue_jobs: ident_buffer '%s'",
						cfp->ident_buffer );
					cfp->IDENTIFIER = Prefix_job_line(cfp,cfp->ident_buffer);
					cfp->identifier = cfp->IDENTIFIER+1;
					s = destination->destination;
					DEBUG4( "Do_queue_jobs: destination id after '%s', ID '%s'",
						cfp->identifier, cfp->IDENTIFIER );
				} else if( Bounce_queue_dest ){
					s = Bounce_queue_dest;
					DEBUG4( "Do_queue_jobs: bounce queue %s to %s",
						cfp->name, s );
				}
				if( Set_job_control( cfp ) ){
					/* you cannot update hold file!! */
					setstatus( cfp,
						"cannot update hold file for '%s'", cfp->name );
					log( LOG_ERR,
						"Do_queue_jobs: cannot update hold file for '%s'", 
						cfp->name );
					exit( JREMOVE );
				}
				if( s ){
					DEBUG4( "Do_queue_jobs: sending %s job to %s",
						cfp->name, s );
					if( strchr( s, '@' ) == 0 ){
						/* append localhost if no host present */
						if( strchr( s, '%' ) ){
							Errorcode = JABORT;
							fatal( LOG_ERR,
							"Do_queue_jobs: '%s' new destination '%s' missing host",
								cfp->name, s );
						}
						/* use localhost as the default destination */
						plp_snprintf( error, sizeof(error), "%s@localhost",
							s );
						s = safestrdup( error );
					}
					RemoteHost = Orig_RemoteHost;
					RemotePrinter = Orig_RemotePrinter;
					Lp_device = s;
					Check_remotehost(1);
				}
				if( RemoteHost ){
					DEBUG4( "Do_queue_jobs: sending '%s' to '%s'",
						cfp->name, RemoteHost );
					status = Remote_job( cfp, &pc_used );
				} else {
					DEBUG4( "Do_queue_jobs: printing '%s' to '%s'",
						cfp->name, Printer );
					status = Print_job( cfp, &pc_used );
				}
				DEBUG4( "Do_queue_jobs: exit status %d", status );
				exit(status);
			} else if( pid < 0 ){
				logerr_die( LOG_ERR, "Do_queue_jobs: fork failed!" );
			}

			/*
			 * wait for the process to finish
			 */

			DEBUG4( "Do_queue_jobs: waiting for pid %d", pid );
			do{
				procstatus = 0;
				result = plp_waitpid( pid, &procstatus, 0 );
				err = errno;
				DEBUG4( "Do_queue_jobs: result %d, status '%s'", result,
					Decode_status( &procstatus ) );
				removepid( result );
			} while( (result == -1 && err != ECHILD) || WIFSTOPPED( procstatus ) );
			if( result == -1 ){
				logerr_die( LOG_ERR, "Do_queue_jobs: plp_waitpid failed!" );
			}
			if( WIFSIGNALED( procstatus ) ){
				status = WTERMSIG( procstatus );
				DEBUG4("Do_queue_jobs: pid %d terminated by signal %d",
					pid, WTERMSIG( procstatus ) );
			} else {
				status = WEXITSTATUS( procstatus );
			}

			if( destination ){
				char copyname[16];
				copyname[0] = 0;
				if( destination->copies ){
					plp_snprintf( copyname,
						sizeof(copyname),
						"C%d", destination->copy_done+1 );
				}
				setstatus( cfp, "subserver status '%s' for '%s%s' destination '%s' on attempt %d",
					Server_status(status), destination->identifier,copyname,
					destination->destination,
					destination->attempt+1 );
			} else {
				setstatus( cfp, "subserver status '%s' for '%s' on attempt %d",
					Server_status(status), cfp->identifier,
					cfp->print_attempts+1 );
			}

done:
			DEBUG4( "Do_queue_jobs: final status %d, '%s'",
				status, Server_status(status) );
			switch( status ){
			case JHOLD:
				if( destination ){
					plp_snprintf( destination->error, sizeof(destination->error),
						"error printing job, holding for retry" ); 
					destination->hold = time( (void *) 0 );
				} else {
					plp_snprintf( cfp->error, sizeof(cfp->error),
						"error printing job, holding for retry" ); 
					cfp->hold_time = time( (void *)0 );
				}
				Set_job_control( cfp );
				++entry;
				break;

			case JIGNORE:	/* ignore this job for now */
				if( destination ){
					destination->ignore = 1;
				}
				break;

			case JSUCC:	/* successful, remove job */
				if( destination ){
					if( destination->copies == 0 ||
						++destination->copy_done >= destination->copies ){
						destination->done = time( (void *) 0 );
					}
					Set_job_control( cfp );
				} else {
					cfp->done_time = time( (void *)0 );
					Set_job_control( cfp );
					setmessage( cfp, "TRACE", "%s@%s: job printed", Printer, FQDNHost );
					if( !Save_when_done && Remove_job( cfp ) ){
						setstatus( cfp, "could not remove job %s",
							cfp->name );
						status = JABORT;
						Errorcode = JABORT;
						cleanup( 0 );
					}
					Sendmail_to_user( status, cfp, &pc_used );
				}
				++entry;
				break;

			default:
				/* This is not good. You probably had a core dump */
				if( destination ){
					plp_snprintf( destination->error,
						sizeof(destination->error),
						"subserver pid %d died!!! %s",
						result, Decode_status( &procstatus ) );
				} else {
					plp_snprintf( cfp->error, sizeof(cfp->error),
						"subserver pid %d died!!! %s",
						result, Decode_status( &procstatus ) );
					setstatus( cfp, "%s", cfp->error );
				}
				Set_job_control( cfp );
				status = JABORT;
				Errorcode = JABORT;
				cleanup( 0 );
				break;

			case JFAIL:	/* failed, retry ?*/
				j = 0;
				if( destination ){
					++destination->attempt;
					j =  Send_try > 0
						&& destination->attempt >= Send_try;
				} else {
					++cfp->print_attempts;
					j =  Send_try > 0
						&& cfp->print_attempts >= Send_try;
				}
				if( j ){
					char buf[60];
					/* check to see what the failure action
					 *	should be - abort, failure; default is remove
					 */
					status = Decode_transfer_failure( status );
					if( status == JFAIL ) status = JREMOVE;
					switch( status ){
					case JABORT:  s = "aborting server"; break;
					case JREMOVE:  s = "removing job"; break;
					case JIGNORE:  s = "going to next job"; break;
					case JHOLD:  s = "holding job"; break;
					default:
						plp_snprintf( buf, sizeof(buf),
							"unexpected status 0x%x", status );
						s = buf;
						break;
					}
					setstatus( cfp, "too many failures on job '%s', %s",
						cfp->identifier, s );
					Sendmail_to_user( status, cfp, &pc_used );
					goto done;
				}
				if( Connect_interval > 0 ){
					setstatus( cfp, "sleeping %d secs before retry",
						Connect_interval );
					sleep( Connect_interval );
				}
				++entry;
				break;

			case JABORT:	/* abort, do not try again */
				if( destination ){
					plp_snprintf( destination->error,
						sizeof(destination->error),
						"aborting operations on job %s, destination %s",
							destination->identifier, destination->destination );
				} else {
					plp_snprintf( cfp->error,
						sizeof(cfp->error),
						"aborting operations on job %s",
						cfp->identifier );
					setstatus( cfp, "%s", cfp->error );
					Sendmail_to_user( status, cfp, &pc_used );
				}
				Set_job_control( cfp );
				Errorcode = JABORT;
				cleanup( 0 );
				break;

			case JREMOVE:	/* failed, remove job */
				if( destination ){
					plp_snprintf( destination->error,
						sizeof(destination->error),
						"removing job entry due to failures" );
					destination->done = time( (void *)0 );
					Set_job_control( cfp );
					break;
				}
				plp_snprintf( cfp->error,
					sizeof(cfp->error),
					"removing job due to failures" );
				cfp->done_time = time( (void *)0 );
				Set_job_control( cfp );
				Sendmail_to_user( status, cfp, &pc_used );
				if( Remove_job( cfp ) ){
					setstatus( cfp, "could not remove job %s",
						cfp->identifier );
					status = JABORT;
					Errorcode = JABORT;
					cleanup( 0 );
				};
				++entry;
				break;
			}
			/* now we do the next destinations */
			if( destination ){
				goto next_destination;
			}
		}
	} while( entry );
	setstatus( NORMAL, "finished operations" );
	Errorcode = JSUCC;
	cleanup( 0 );
}

/***************************************************************************
 * Remote_job()
 * Send a job to a remote server.  This code is actually trickier
 *  than it looks, as the Send_job code takes most of the heat.
 *
 ***************************************************************************/
int Remote_job( struct control_file *cfp, struct pc_used *pc_used )
{
	int status;

	DEBUG8("Remote_job: %s", cfp->name );
	status = Send_job( RemotePrinter?RemotePrinter:Printer, RemoteHost, cfp,
		SDpathname, Connect_try, Connect_timeout,
		Connect_interval, Send_timeout, pc_used );
	DEBUG8("Remote_job: %s, status '%s'", cfp->name, Link_err_str(status) );
	switch( status ){
	case JSUCC:
	case JABORT:
	case JFAIL:
	case JREMOVE:
	case JIGNORE:
		break;
	case LINK_ACK_FAIL:
		setstatus( cfp, "link failure while sending job '%s'", cfp->identifier );
		status = JABORT;
		break;
	case LINK_PERM_FAIL:
		setstatus( cfp, "no permission to spool job '%s'", cfp->identifier );
		status = JREMOVE;
		break;
	default:
		setstatus( cfp, "failed to send job '%s'", cfp->identifier );
		status = JFAIL;
		break;
	}
	return( status );
}

/***************************************************************************
Start_subservers()

We check to see if we already were a subserver.  If we do, then we
 have an error in the printcap database
We then set the subserver_for variable so that subservers will know
they are 'slaves' to the master spool queue.

The work scanning is very straight forward:
  we scan the master queue, checking to see if there are entries.
  if there is an entry, we see if there is a server available.
    if there is an server available, then we assign the job
  we wait for a server to die.
  if no servers and no entries, we are done.

 ***************************************************************************/
static int cmp_server( const void *l, const void *r );

static char *Start_subservers( struct stat *control_statb,
	struct pc_used *pc_used )
{
	struct malloc_list servers;
	struct server_info *server_info;	/* server information */
	int c, i, j, status;				/* ACME! integers that COUNT! */
	int entry;
	struct control_file *cfp, **cfpp;	/* control file to process */
	char *s, *end;						/* ACME */
	int len;							/* length of some string */

	Name = "Start_subservers";
	setproctitle( "lpd %s '%s'", Name, Printer );

	/* initialize values */
	memset( &servers, 0, sizeof(servers) );
	Server_pid = getpid();

	Errorcode = JABORT;
	if( subserver_for ){
		logerr_die( LOG_ERR,
			"Start_subserver: '%s' is already subserver for '%s'",
			Printer, subserver_for );
	}
	subserver_for = Printer;

	/* now we allocate the subserver information. */
	Get_subserver_info( &servers, Server_names );
	/* now we check to see if there is an initial server order */
	server_info = (void *)servers.list;
	if( Server_order ){
		/* we get the values from list and update the order */
		for( i = 1, s = Server_order; s && *s; ++i, s = end ){
			end = strpbrk( s, ":;, \t" );
			if( end ){
				*end++ = 0;
			}
			while( (c = *s) && isspace(c) ) ++s;
			if( c == 0 ) continue;
			/* now find the server in list */ 
			for( j = 0; j < servers.count; ++j ){
				if( strcmp( server_info[j].name, s ) == 0 ){
					server_info[j].time = i;
					break;
				}
			}
		}
	}
	qsort( server_info, servers.count, sizeof( server_info[0] ),
		cmp_server );
	if( Debug > 4 ){
		logDebug( "Get_subserver_info:after sorting" );
		server_info = (void *)servers.list;
		for( c = 0; c < servers.count; ++c ){
			logDebug( "[%d] %s, time %d",
				c, server_info[c].name, server_info[c].time );
		}
	}

	/* start up the subservers */
rescan:
	server_info = (void *)servers.list;
	for( i = 0; !Printing_disabled && i < servers.count; ++i ){
		if( server_info[i].pid == 0 ){
			if( Fork_subserver( &server_info[i] ) == 0 ){
				return( server_info[i].name );
			} else {
				sleep(1);
			}
		}
	}
	do{
		status = Wait_for_subserver( &servers, &server_info );
		DEBUG4("Start_subservers: server_info 0x%x (%s)",
			server_info, server_info?server_info->name:"<NULL>" );
	} while( status > 0 );

	do{
		DEBUG4( "Start_subservers: RESCAN" );
		/* check on the spool queue status */
		if( Get_spool_control( control_statb ) ){
			DEBUG4( "Start_subservers: control file changed, restarting" );
			Fix_update( debug_vars, 0 );
		}

		entry = 0;

		Scan_queue( 1 );

		DEBUG4( "Start_subservers: total_cf_files '%d', Spool_dir '%s'",
			C_files_list.count, Clear_path(SDpathname) );
		cfpp = (void *)C_files_list.list;
		if(Debug>4){
			for(i=0; i < C_files_list.count; ++i ){
				cfp = cfpp[i];
				logDebug("[%d] '%s'", i, cfp->name );
			}
		}

		for( i = 0; !Printing_disabled && i < C_files_list.count; ){

			/* see if status has changed */
			if( Get_spool_control( control_statb ) ){
				DEBUG4( "Start_subservers: control file changed, restarting" );
				Fix_update( debug_vars, 0 );
				goto rescan;
			}

			cfp = cfpp[i];
			status =  Check_printable( cfp );
			switch( status ){
			case JSUCC: /* printable */
				break;
			case JREMOVE:	/* remove job */
				plp_snprintf( cfp->error, sizeof(cfp->error),
					"job not printable, holding for retry" ); 
				setstatus( cfp, "%s", cfp->error );
				cfp->hold_time = time( (void *)0 );
				(void)Set_job_control( cfp );
				if( !Save_when_done && Remove_job( cfp ) ){
					setstatus( cfp, "could not remove job %s",
						cfp->identifier );
					status = JABORT;
					Errorcode = JABORT;
					cleanup( 0 );
				};
				++i;
				continue;

			default:	/* any other status - ignore */
				++i;
				continue;
			}

			/* check to see if there is a printer you can feed the
				file to */

			/*
			 * we will do a round robin search in order of the last
			 * one finished
			 * we sort the entries according to the one that
			 * last finished
			 */

			/* get the servers that have died */
			do{
				status = Wait_for_subserver( &servers, &server_info );
				DEBUG4("Start_subservers: server_info 0x%x (%s)",
					server_info, server_info?server_info->name:"<NULL>" );
			} while( status > 0 );

			/* sort the list */
			server_info = (void *)servers.list;
			qsort( server_info, servers.count, sizeof( server_info[0] ),
				cmp_server );
			if( Debug > 4 ){
				logDebug( "Start_subservers: printers after sorting" );
				for( j = 0; j < servers.count; ++j ){
					logDebug( "printer [%d] %s time %d",
						j, server_info[j].name, server_info[j].time );
				}
			}
			/* look for a free server */
			for( j = 0; j < servers.count ; ++j ){
				DEBUG8("Start_subservers: server '%s' pid %d, status %s",
					server_info[j].name, server_info[j].pid,
					Server_status( server_info[j].status ) );

				if( server_info[j].pid == 0 ){
					if( server_info[j].status == JSUCC ){
						break;
					} else if( server_info[j].status == JFAIL ){
						DEBUG4("Start_subservers: restarting %s",
							server_info[j].name );
						if( Fork_subserver( &server_info[j] ) == 0 ){
							return( server_info[j].name );
						}
					}
				}
			}
			if( j < servers.count ){
				/*
				 * we now have a free printer handler for the job.
				 * we need to send the job to the printer.  This
				 * is done in the child, not parent; parent vars
				 * never get modified
				 */
				cfp->move_time = Server_pid;
				if( Set_job_control( cfp ) ){
					/* you cannot update hold file!! */
					setstatus( cfp,
						"cannot update hold file for '%s'", cfp->identifier );
					log( LOG_ERR,
						"Start_subservers: cannot update hold file for '%s'", 
						cfp->identifier );
					/* force it to be ignored and removed */
					cfp->flags = 1;
				} else {
					server_info = &server_info[j];

					/* give the child a job to copy */
					if( Fork_subserver( server_info ) == 0 ){
						s = Clear_path(SDpathname);
						cfp->spool_dir =
							add_buffer( &cfp->control_file, strlen(s)+1);
						strcpy( cfp->spool_dir, s );
						src_cfp = cfp;
						setstatus( cfp,
							"sending job '%s' to subserver '%s'",
								cfp->identifier, server_info->name );
						return( server_info->name );
					}
				}
				++entry;
				++i;
				continue;
			}

			/*
			 * at this point we have arrived with a job to print
			 * and no servers available. We check to see
			 * if we have any server working, and we may just need to
			 * sleep.
			 */
			DEBUG4("Start_subservers: checking for server working" );
			server_info = (void *)servers.list;
			for( j = 0; j < servers.count && server_info[j].pid; ++j );
			if( j < servers.count ){
				DEBUG4("Start_subservers: all completed, none available" );
				setstatus( cfp,"no subserver available to print jobs" );
				exit(0);
			} else {
				sleep(10);
			}
		}
		/* 
		 * You arrive here when there are no more jobs to print from
		 * the previous scan.  You need to rescan the spool queue and
		 * and check for work
		 */
		if( entry == 0 ){
			/* you arrived here because during the last scan you discovered
			 * no print jobs.  You want to wait for either some jobs
			 * to be put in the queue or a server to finish.
			 * Rescan when either of these events occurs 
			 */
			status = Wait_for_subserver( &servers, &server_info );
			if( status == 0 ){
				sleep(10);
			}
		}
	} while( entry || status != -1 );

	/* now we write out the Server_order information */

	server_info = (void *)servers.list;
	qsort( server_info, servers.count, sizeof( server_info[0] ),
		cmp_server );
	len = 0;
	for( i = 0; i < servers.count; ++i ){
		len += strlen( server_info[i].name ) + 1;
	}
	++len;
	malloc_or_die( s, len );
	s[0] = 0;
	for( i = 0; i < servers.count; ++i ){
		if( s[0] ) strcat( s, "," );
		strcat( s, server_info[i].name );
	}
	DEBUG4("Start_subservers: final order '%s'", s );
	(void)Get_spool_control( control_statb);
	Server_order = s;
	Set_spool_control();

	setstatus( NORMAL, "finished operations" );
	Errorcode = JSUCC;
	cleanup(0);
	/* not reached, make LINT happy? */
	return( 0 );
}


/***************************************************************************
 * cmp_server: comparison function for server list
 * 
 ***************************************************************************/

static int cmp_server( const void *l, const void *r )
{
	const struct server_info *ls = l;
	const struct server_info *rs = r;
	int cmp = 0;
	cmp = ls->status - rs->status;
	if( cmp == 0 ) cmp = ls->pid - rs->pid;
	if( cmp == 0 ) cmp = ls->time - rs->time;
	return( cmp );
}

static int Fork_subserver( struct server_info *server_info )
{
	server_info->status = 0;	/* no status yet */
	server_info->pid = dofork();
	if( server_info->pid < 0 ){
		logerr_die( LOG_ERR, "Fork_subserver: fork failed" );
	}
	return( server_info->pid );
}

/***************************************************************************
 * Get_subserver_info()
 *  hack up the server information list into a list of servers
 ***************************************************************************/

void Get_subserver_info( struct malloc_list *servers, char *s)
{
	char *end;
	int c;
	struct server_info *server_info;

	clear_malloc_list( servers, 0 );

	for( ; s && *s; s = end ){
		end = strpbrk( s, ":;, \t" );
		if( end ){
			*end++ = 0;
		}
		while( (c = *s) && isspace(c) ) ++s;
		if( c == 0 ) continue;
		/* we now can allocate a new server name */ 
		if( servers->count >= servers->max ){
			extend_malloc_list( servers, sizeof( server_info[0] ), 10 );
		}
		server_info = (void *)servers->list;
		server_info = &server_info[servers->count++];
		memset( server_info, 0, sizeof( server_info[0] ) );
		server_info->name = s;
	}
	if( Debug > 4 ){
		logDebug( "Get_subserver_info: %d servers", servers->count );
		server_info = (void *)servers->list;
		for( c = 0; c < servers->count; ++c ){
			logDebug( "[%d] %s", c, server_info[c].name );
		}
	}
}

/***************************************************************************
 * struct server_info *Wait_for_subserver( struct malloc_list *servers )
 *  wait for a server process to exit
 *  if none present return 0
 *  look up the process in the process table
 *  update the process table status
 *  return the process table entry
 ***************************************************************************/
static int Wait_for_subserver( struct malloc_list *servers,
	struct server_info **server_info_result )
{
	pid_t result;
	int status, i;
	plp_status_t procstatus;
	struct server_info *server_info = 0;
	int err;				/* saved errno */
	static int t = 1000;

	/*
	 * wait for the process to finish
	 */
	*server_info_result = 0;

	DEBUG4( "Wait_for_subservers: checking" );

	do {
		procstatus = 0;
		result = plp_waitpid( -1, &procstatus, WNOHANG );
		err = errno;

		DEBUG4( "Wait_for_subservers: result %d, status '%s'", result,
			Decode_status( &procstatus ) );
		removepid( result );
	} while( (result == -1 && err != ECHILD) || WIFSTOPPED( procstatus ) );

	if( result == -1 ){
		if( err == ECHILD ){
			DEBUG4( "Wait_for_subservers: all servers have finished" );
			errno = err;
		} else {
			logerr( LOG_INFO, "Wait_for_subservers: waitpid error" );
		}
		return( -1 );
	} else if( result == 0 ){
		return( result );
	}
	if( WIFSIGNALED( procstatus ) ){
		status = WTERMSIG( procstatus );
		DEBUG4("Wait_for_subservers: pid %d terminated by signal %d",
			result, WTERMSIG( procstatus ) );
	} else {
		status = WEXITSTATUS( procstatus );
	}
	switch( status ){
		case SIGINT:
			status = JFAIL;
			break;
		default:
			break;
	}
	DEBUG4( "Wait_for_subservers: pid %d final status %s",
				result, Server_status(status) );

	server_info = (void *)servers->list;
	for( i = 0; i < servers->count ; ++i, ++server_info ){
		if( server_info->pid == result ){
			server_info->pid = 0;
			server_info->status = status;
			server_info->time = ++t;
			DEBUG4( "Wait_for_subservers: server '%s' finished",
				server_info->name );
			*server_info_result = server_info;
		}
	}
	return( result );
}

/***************************************************************************
 * link_or_copy - make a copy of a job file in another spool queue
 *  If the spool queues are on the same file system,  then you can link
 *  otherwise you must copy.  Linking is better idea.
 * void link_or_copy( char *src, char *dest, int cf_len )
 *  char *src, *dest - source and destination file name
 *  int cf_len - start of control file name
 *    - we replace the 'c' in control file name with '_`, and then
 *  we rename
 ***************************************************************************/

void link_or_copy( char *src, char *dest, int cf_len )
{
	int srcfd, destfd;
	struct stat statb, wstatb;
	int len, l;
	char line[LARGEBUFFER];
	struct dpathname dpath;		/* pathname */

	if( link( src, dest ) == -1 ){
		DEBUG7("link_or_copy: link failed '%s'", Errormsg(errno) );
		/* try copying then */
		srcfd = Checkread( src, &statb );
		if( srcfd <= 0 ){
			fatal( LOG_ERR, "link_or_copy: cannot open for reading '%s'",
				src);
		}
		if( cf_len ){
			strcpy( dpath.pathname, dest );
			dest[cf_len] = '_';
		}
		destfd = Checkwrite( dest, &wstatb, O_WRONLY|O_TRUNC, 1, 0 );
		if( destfd <= 0 ){
			fatal( LOG_ERR, "link_or_copy: cannot open for writing '%s'",
				dest );
		}
		l = 1;
		for( len = statb.st_size;
			len > 0 && (l = read( srcfd, line, sizeof(line))) > 0;
			len -= l ){
			if( Write_fd_len( destfd, line, l ) < 0 ){
				logerr_die(LOG_ERR,
					"link_or_copy: write failed to '%s'",dest);
			}
		}
		if( l <= 0  ){
			logerr_die(LOG_ERR,"link_or_copy: read failed '%s'",src);
		}
		if( cf_len && rename( dest, dpath.pathname ) < 0 ){
			logerr_die(LOG_ERR,
				"link_or_copy: rename failed, '%s' to '%s'",
					dest, dpath.pathname);
		}
	}
}

/***************************************************************************
 * Copy_job:
 *  subserver gets a new job
 *  We have the subserver copy the job into the new location.
 *  Note that the server will fork the subserver FIRST,
 *   the subserver will update the information,  and then
 *   clobber all variables.  This order is important,  as the
 *   only link between the server and subserver are the src_cfp and
 *   srcdpath variables.
 ***************************************************************************/
static void Copy_job()
{
	int i;
	struct data_file *data_file;	/* data files in job */
	struct stat statb;				/* status buffer */
	char *s, *src, *dest;				/* source and destination file */
	struct dpathname srcdpath;		/* job for a queue */

	/* we now can do the setup of the process */
	Errorcode = JABORT;

	DEBUG7("Copy_job: printer '%s', job '%s' copying data files",
		Printer, src_cfp->identifier );
		
	Init_path( &srcdpath, src_cfp->spool_dir );
	data_file = (void *)src_cfp->data_file.list;
	for( i = 0; i < src_cfp->data_file.count; ++i ){
		s = data_file[i].openname;
		src = Add_path( &srcdpath, s );
		dest = Add_path( SDpathname, s );
		DEBUG7("Copy_job: data file src '%s' dest '%s'", src, dest );
		/* try renaming */
		link_or_copy( src, dest, 0 );
	}
	s = src_cfp->identifier;
	src = Add_path( &srcdpath, s );
	dest = Add_path( SDpathname, s );
	DEBUG7("Copy_job: control file src '%s' dest '%s'", src, dest );
	link_or_copy( src, dest, SDpathname->pathlen );

	/* now unlink all the files */
	for( i = 0; i < src_cfp->data_file.count; ++i ){
		s = data_file[i].openname;
		src = Add_path( &srcdpath, s );
		DEBUG7("Copy_job: unlink src '%s'", src );
		if( stat( src, &statb ) == 0 && unlink( src ) == -1 ){
			logerr( LOG_ERR, "Copy_job: cannot unlink '%s'", src );
		}
	}
	s = src_cfp->identifier;
	src = Add_path( &srcdpath, s );
	DEBUG7("Copy_job: unlink src '%s'", src );
	if( stat( src, &statb ) == 0 && unlink( src ) == -1 ){
		logerr( LOG_ERR, "Copy_job: cannot unlink '%s'", src );
	}
	if( stat( src_cfp->hold_file, &statb ) == 0
		&& unlink( src_cfp->hold_file ) ){
		logerr( LOG_ERR, "Copy_job: cannot unlink '%s'", src );
	}
	src_cfp = 0;
}

/***************************************************************************
 * int Decode_transfer_failure( int status )
 * When you get a job failure more than a certain number of times,
 *  you check the 'Send_failure_action' variable
 *  This can be abort, retry, or remove
 * If retry, you keep retrying; if abort you shut the queue down;
 *  if remove, you remove the job and try again.
 ***************************************************************************/

static struct keywords keys[] = {
	{"abort", INTEGER_K, (void *)0, JABORT},
	{"hold", INTEGER_K, (void *)0, JHOLD},
	{"ignore", INTEGER_K, (void *)0, JIGNORE},
	{"remove", INTEGER_K, (void *)0, JREMOVE},
	{ (char *)0 }
};

int Decode_transfer_failure( int status )
{
	struct keywords *key;

	DEBUG4("Decode_transfer_failure: '%s'", Send_failure_action );
	if( Send_failure_action ){
		for( key = keys; key->keyword; ++key ){
			DEBUG4("Decode_transfer_failure: comparing '%s' to '%s'",
				Send_failure_action, key->keyword );
			if( strcasecmp( key->keyword, Send_failure_action ) == 0 ){
				status = key->maxval;
				break;
			}
		}
	}
	DEBUG4("Decode_transfer_failure: returning '%s'", Server_status(status) );
	return( status );
}


/***************************************************************************
 * Check_printable()
 * Check to see if the job is printable
 ***************************************************************************/

static int Check_printable( struct control_file *cfp )
{
	/* check to see if it is being held or not printed */
	if( cfp->flags
		|| cfp->error[0]
		|| (cfp->move_time == Server_pid)
		|| cfp->hold_time
		|| cfp->remove_time
		|| cfp->done_time
		){
		return(JIGNORE);
	}

	/*
	 * make sure that you can update the hold file
	 */
	cfp->active = 0;
	if( Set_job_control( cfp ) ){
		/* you cannot update hold file!! */
		setstatus( cfp,
			"cannot update hold file for '%s'", cfp->identifier );
		log( LOG_ERR,
			"Check_printable: cannot update hold file for '%s'", 
			cfp->identifier );
		/* force it to be ignored and removed */
		cfp->flags = 1;
		return( JREMOVE );
	}

	/*
	 * check to see if you have permissions to print/process
	 * the job
	 */
	memset( &Perm_check, 0, sizeof( Perm_check ) );
	Perm_check.service = 'R';
	Perm_check.printer = Printer;
	if( cfp->LOGNAME && cfp->LOGNAME[1] ){
		Perm_check.user = cfp->LOGNAME+1;
		Perm_check.remoteuser = Perm_check.user;
	}
	if( cfp->FROMHOST && cfp->FROMHOST[1] ){
		Perm_check.host = Get_realhostname( cfp );
		Perm_check.remotehost = Perm_check.host;
		Perm_check.ip = Perm_check.remoteip = ntohl(Find_ip(Perm_check.host));
	}

	Init_perms_check();
	if( Perms_check( &Perm_file, &Perm_check, cfp ) == REJECT
		|| Perms_check( &Local_perm_file, &Perm_check, cfp ) == REJECT
		|| Last_default_perm == REJECT ){
		setstatus( cfp,
			"no permission to print job %s", cfp->identifier );
		return( JREMOVE );
	}
	return( JSUCC );
}


/***************************************************************************
 * cleanup_noerr()
 *  Called when sent a SIGHUP: this is a reasonable way to for a subserver
 *  to exit when it's job is removed.
 ***************************************************************************/

static void cleanup_noerr()
{
	DEBUG6("cleanup_noerr: signal %d, Errorcode %d", signal, Errorcode);
	Errorcode = 0;
	cleanup(0);
	exit(0);
}

/***************************************************************************
 * cleanup_noerr()
 *  Called when sent a fatal stop signal: we want to kill off all processes
 ***************************************************************************/

static void cleanup_err( int signal )
{
	DEBUG6("cleanup_err: signal %d, Errorcode %d", signal, Errorcode);
	Print_abort();
	cleanup(signal);
	exit(signal);
}
