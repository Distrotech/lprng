/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpd_jobs.c
 * PURPOSE: process jobs in the queue
 **************************************************************************/

static char *const _id =
"$Id: lpd_jobs.c,v 3.8 1997/01/30 21:15:20 papowell Exp $";

#include "lp.h"
#include "printcap.h"
#include "checkremote.h"
#include "cleantext.h"
#include "decodestatus.h"
#include "dump.h"
#include "errorcodes.h"
#include "fileopen.h"
#include "gethostinfo.h"
#include "getqueue.h"
#include "jobcontrol.h"
#include "killchild.h"
#include "linksupport.h"
#include "lockfile.h"
#include "malloclist.h"
#include "merge.h"
#include "pathname.h"
#include "permission.h"
#include "pr_support.h"
#include "printjob.h"
#include "removejob.h"
#include "sendjob.h"
#include "serverpid.h"
#include "setstatus.h"
#include "setup_filter.h"
#include "setupprinter.h"
#include "waitchild.h"
/**** ENDINCLUDE ****/

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
       JNOPRINT: printing disabled, do not give me jobs
       JNOSPOOL: spooling disabled, do not give me jobs
   5. The server will sit in a major loop,  waiting for an event:
       - subserver exit
       - new job added to queue
      The server will start up a subserver to service each job
      when this happens. 

NOTE:  the event handling is complicated by the fact that the
  server may be in the following state:
   - all subservers busy; waiting for subserver
     If a new job arrives,  it should be ignored.
     If a subserver exits,  it should be restarted with new job
   - some subservers busy; waiting for subserver or new job
     If a new job arrives, it should cause a subserver to be started
     If a subserver exits,  it should be restarted with new job

 ***************************************************************************/

static int Decode_transfer_failure( int attempt, struct control_file *cfp,
	int status );

static int cmp_server( const void *l, const void *r );

int Check_printable( struct control_file *cfp, struct destination *dest );
char *Update_servers_status( struct malloc_list *servers );
static char error[LINEBUFFER];

int Remote_job( struct control_file *cfp, struct printcap_entry *pc);

static int Copy_job( struct control_file *cfp, struct dpathname *spooldir);
static int Fork_subserver( struct server_info *server_info );
static struct server_info *Wait_for_subserver( struct malloc_list *servers );
static int Finished_order;
static void dump_servers( char *title, struct malloc_list *servers );

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
	pid_t pid;				/* process pid and exit status */
	plp_status_t status;
	int err;					/* saved errno */

	/* extract name of printer from input */
	Name = "Process_jobs";
	++input;
	if( (s = strchr( input, '\n' )) ) *s = 0;
	DEBUG0("Process_jobs: doing '%s'", input );

	/* send the ACK and then close the connection, wait only short time */
	if( socket ){
		(void)Link_ack( ShortRemote, socket, 1, 0x100, 0 );
		Link_close( socket );
	}

	/* process the list of names on the command line */
	for( name = input; name && *name; name = end ){
		end = strpbrk( name, ",; \t" );
		if( end ) *end++ = 0;

		if( (s = Clean_name( name )) ){
			DEBUG0( "Process_jobs: bad printer name '%s'", name );
			continue;
		}
		setproctitle( "lpd %s '%s'", Name, name );
		/*
		 * if we are starting 'all' then we need to start subprocesses
		 */
		if( strcmp( name, "all" ) == 0){
			/* Start_all will return the name to use */
			Start_all();
		} else {
			/* Call the routine to actually do the work */
			Do_queue_jobs( name );
		}
	}
	while(1){
		pid = plp_waitpid( -1, &status, 0 );
		err = errno;
		if( ( pid == -1 && errno != EINTR) || pid == 0 ){
			break;
		}
		DEBUG0( "Process_jobs: pid %d, status 0x%x", pid, status );
	}

	DEBUG0( "Process_jobs: done" );
	Errorcode = 0;
	cleanup( 0 );
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

static int Need_to_start( void );

void Start_all(void)
{
	int i, j, c;
	struct printcap_entry *pc, *entries;

	/* we work our way down the printcap list, checking for
		ones that have a spool queue */
	/* note that we have already tried to get the 'all' list */
	Name = "Start_all";
	setproctitle( "lpd %s", Name );
	j = 0;
	if( All_list.count ){
		char **line_list;
		DEBUG0("Start_all: using the All_list" );
		line_list = All_list.list;
		for( i = 0; i < All_list.count; ++i ){
			Printer = line_list[i];
			if( Need_to_start() ){
				DEBUG0("Start_all: starting '%s'", Printer );
			}
		}
	} else if( Expanded_printcap_entries.count > 0 ){
		DEBUG0("Start_all: using the printcap list" );
		entries = (void *)Expanded_printcap_entries.list;
		c = Expanded_printcap_entries.count;
		for( i = 0; i < c; ++i ){
			pc = &entries[i];
			Printer = pc->names[0];
			DEBUG0("Start_all: printcap entry [%d of %d] '%s'",
				i, c,  Printer );
			if( Need_to_start() ){
				DEBUG0("Start_all: starting '%s'", Printer );
			}
		}
	}
	Printer = 0;
	return;
}


/***************************************************************************
 * int Need_to_start
 *  returns: 0 if no server needed, non-zero otherwise
 ***************************************************************************/
static int Need_to_start( void )
{
	struct printcap_entry *pc;
	struct stat control_statb;	/* status of control file */
	int  hc, cnt, pid, jobcount;
	char orig_name[LINEBUFFER];
	char subservers[LINEBUFFER];
	char *s, *end;
	int noprinting;

	pc = 0;
	jobcount = 0;
	safestrncpy( orig_name, Printer );
	Printer = Find_printcap_entry( Printer, &pc );
	if( Printer == 0 || *Printer == 0 || ispunct( *Printer ) ){
		return(0);
	}
	error[0] = 0;
	memset(&control_statb, 0, sizeof(control_statb) );
	/* see if there are jobs */
	if( Setup_printer( Printer, error, sizeof(error), 0, 1, 0, 0 ) ){
		DEBUG0( "Need_to_start: bad queue info '%s'", Printer );
		return(0);
	}
	if( Server_queue_name && *Server_queue_name ){
		DEBUG0( "Need_to_start: '%s' is server for '%s'",
			Printer, Server_queue_name );
		return(0);
	}

	noprinting = Printing_disabled;
	Job_count( &hc, &cnt );
	jobcount += cnt;

	DEBUG0( "Need_to_start: '%s' no printing %d, job count %d",
		Printer, noprinting, jobcount );

	subservers[0] = 0;
	if( Server_names && *Server_names ){
		safestrncpy( subservers, Server_names );
	}
	for( s = subservers; s && *s; s = end ){
		while( isspace( *s ) ) ++s;
		end = strpbrk( s, ";,\t " );
		if( end ) *end++ = 0;
		if( *s == 0 ) continue;
		cnt = 0;
		if( Setup_printer( s, error, sizeof(error), 0, 1, 0, 0 ) == 0 ){
			Job_count( &hc, &cnt );
			jobcount += cnt;
		}
		DEBUG0( "Need_to_start: '%s' server, no printing %d, job count %d",
			Printer, Printing_disabled, cnt );
		if( Printing_disabled == 0 ) noprinting = 0;
	}

	Printer = orig_name;
	DEBUG0( "Need_to_start: '%s' final no printing %d, job count %d",
		Printer, noprinting, jobcount );
	if( noprinting || jobcount == 0 ){
		return(0);
	}

	/* we can start up the process */
	if( (pid = dofork()) < 0 ){
		logerr_die( LOG_INFO, "Start_all: fork failed!" );
	} else if( pid ){
		plp_usleep(100);
		return( pid );
	}
	Do_queue_jobs( orig_name );
	Errorcode = 0;
	cleanup(0);
	return(0);
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
/*
 * Do nothing signal handler
 */
static int Susr2;
static int Sig_break;

static void Sigusr2()
{
	DEBUG0( "Sigusr2: INTERRUPT" );
	++Susr2;
	if( Sig_break ){
		(void) plp_signal_break(SIGUSR2,  (plp_sigfunc_t)Sigusr2);
	} else {
		(void) plp_signal(SIGUSR2,  (plp_sigfunc_t)Sigusr2);
	}
	return;
}

void Do_queue_jobs( char *name )
{
	int lockfd = -1, lock, create;	/* lock file fd */
	char subserver_for[LINEBUFFER];	/* flag set when a subserver */
	struct stat statb;			/* status of file */
	struct stat control_statb;	/* status of control file */
	pid_t pid;					/* process id */
	int unspoolerfd = -1;		/* server id */
	struct control_file *cfp, **cfpp;	/* control file to process */
	plp_status_t procstatus;
	int status = 0;				/* status of the job */
	int i, j, c;				/* AJAX! for integers */
	char *pname, *strv, *end;	/* ACME, the very finest pointers */
	int err = 0;
	struct destination *destination = 0;	/* current destination */
	struct destination *dpp;		/* list */
	int can_remove;				/* can we remove this */
	struct printcap_entry *pc;
	char *newdest;				/* new destination */
	int all_finished;			/* all servers done */
	int update_status;			/* we need to update status */
	plp_block_mask omask;		/* signal mask */
	struct malloc_list servers;
	struct server_info *server_info;	/* server information */
	struct server_info *first_server;	/* server information */
	char orig_name[LINEBUFFER];
	char buffer[LINEBUFFER];


	memset( &servers, 0, sizeof(servers) );
	subserver_for[0] = 0;
	server_info = 0;

begin:
	servers.count = 0;
	safestrncpy(orig_name, name);
	close( lockfd );
	close( unspoolerfd );

	/* when a subserver, we come here to re-do the work
		of reading the printcap file, etc. */
	/* process the job */

	/* we will explicitly collect children */
	(void) signal(SIGCHLD,  SIG_IGN );
	(void) plp_signal(SIGUSR2,  (plp_sigfunc_t)Sigusr2);

	Printer = orig_name;
	Name = "(Server)";
	if( subserver_for[0] ){
		Name = "(Subserver)";
	}
	setproctitle( "lpd %s '%s'", Name, Printer );

	Errorcode = JABORT;
	if( lockfd != -1 ){
		close( lockfd );
		lockfd = -1;
	}

	error[0] = 0;
	memset(&control_statb, 0, sizeof(control_statb) );
	pc = 0;
	i = Setup_printer( orig_name, error, sizeof(error),
		debug_vars, 0, &control_statb, &pc );
	DEBUG0( "Do_queue_jobs: printer '%s', printing_disabled %d", Printer,
		Printing_disabled );

	if( i ){
		if( i != 2 ){
			log( LOG_ERR, "Do_queue_jobs: %s", error );
		} else {
			DEBUG0( "Do_queue_jobs: '%s'", error );
		}
		/* here we simply exit */
		cleanup(0);
	}

	DEBUG0( "Do_queue_jobs: Server_queue_name '%s' and subserver for '%s'",
		Server_queue_name, subserver_for );
	if( Server_queue_name && subserver_for[0] == 0 ){
		DEBUG0("Do_queue_jobs: subserver started without server");
		Errorcode = 0;
		cleanup( 0 );
	}

	if( subserver_for[0] ){
		if( Printing_disabled ){
			Errorcode = JNOPRINT;
			DEBUG0( "Do_queue_jobs: printing disabled" );
			cleanup(0);
		}
		if( Spooling_disabled ){
			Errorcode = JNOSPOOL;
			DEBUG0( "Do_queue_jobs: subserver and spooling disabled" );
			cleanup(0);
		}
	}

	/* lock the printer lock file */

	pname = Add_path( CDpathname, Printer );
	lockfd = Lockf( pname, &lock, &create, &statb );
	if( lockfd < 0 ){
		logerr_die( LOG_ERR, "Do_queue_jobs: cannot open '%s'", pname ); 
	}
	if( lock <= 0 ){
		pid = Read_pid( lockfd, (char *)0, 0 );
		DEBUG0( "Do_queue_jobs: server process '%d' may be active", pid );
		if( Signal_server && pid > 0 ){
			kill( pid, SIGUSR2 );
		}
		Errorcode = 0;
		cleanup(0);
		return;
	}

	pid = getpid();
	DEBUG0( "Do_queue_jobs: writing lockfile '%s' with pid '%d'",
		pname,pid );
	Write_pid( lockfd, pid, (char *)0 );

	pname = Add2_path( CDpathname, "unspooler.", Printer );
	if( (unspoolerfd = Checkwrite( pname, &statb, O_RDWR, 1, 0 )) < 0 ){
		logerr_die( LOG_ERR, "Do_queue_jobs: cannot open '%s'", pname );
	}

	/* note that the sub servers will fork and branch to the start */
	if( Server_names && *Server_names ){
		safestrncpy(subserver_for,Printer);
		Get_subserver_info( &servers, Server_names );
		/* now we check to see if there is an initial server order */
		server_info = (void *)servers.list;
		if( Server_order ){
			/* we get the values from list and update the order */
			safestrncpy( buffer, Server_order );
			for( strv = buffer; strv && *strv; strv = end ){
				while( isspace( *strv ) ) ++strv;
				end = strpbrk( strv, ":;, \t" );
				if( end ){
					*end++ = 0;
				}
				if( *strv == 0 ) continue;
				/* now find the server in list */ 
				for( j = 0; j < servers.count; ++j ){
					if( strcmp( server_info[j].name, strv ) == 0 ){
						server_info[j].time = ++Finished_order;
						break;
					}
				}
			}
		}
		/* set this up as an initial server */
		for( c = 0; c < servers.count; ++c ){
			server_info[c].initial = 1;
		}

		if( Mergesort( server_info, servers.count, sizeof( server_info[0] ),
			cmp_server ) ){
			fatal( LOG_ERR, "Do_queue_jobs: Mergesort failed" );
		}
		if(DEBUGL4 ){
			logDebug( "Do_queue_jobs: after sorting" );
			for( c = 0; c < servers.count; ++c ){
				logDebug( "[%d] %s, time %d",
					c, server_info[c].name, server_info[c].time );
			}
		}
	} else {
		/* dummy one for our queue here */
		servers.count = 0;
		if( 1 > servers.max ){
			extend_malloc_list( &servers, sizeof( server_info[0] ), 1 );
		}
		server_info = (void *)servers.list;
		first_server = &server_info[servers.count++];
		memset( first_server, 0, sizeof( first_server[0] ) );
	}

	if( Server_names && *Server_names ){
		Susr2 = 1;
	}

	/* we clear out all of the active process information - it is a hold over from a previous
	 * server.  If we do not do this,  then we will not be able to get the correct job status
	 */
	cfpp = (void *)C_files_list.list;
	for(i=0; i < C_files_list.count; ++i ){
		cfp = cfpp[i];
		update_status = 0;
		logDebug("clearing [%d] '%s'", i, cfp->transfername );
		if( cfp->hold_info.active ){
			cfp->hold_info.active = 0;
			update_status = 1;
		}
		dpp = (void *)cfp->destination_list.list;
		for( j = 0; j < cfp->destination_list.count; ++j ){
			if( dpp[j].active ){
				dpp[j].active = 0;
				update_status = 1;
			}
		}
		if( update_status ) Set_job_control( cfp, 0, 0 );
	}

	while(1){
		/* check for changes to spool control information */

		plp_block_all_signals( &omask );
		update_status = Susr2;
		Susr2 = 0;
		plp_unblock_all_signals( &omask );

		DEBUG0( "Do_queue_jobs: Printing_disabled before %d", Printing_disabled );
		if( Get_spool_control( &control_statb, (void *)0 ) ){
			DEBUG0( "Do_queue_jobs: control file changed, restarting" );
			Fix_update( debug_vars, 0 );
			update_status = 1;
		}
		DEBUG0( "Do_queue_jobs: Printing_disabled after %d", Printing_disabled );
		/*********************************************************************
		 * We now start any subserver that needs it.  Note that we will
		 *  fork and branch to the program start
		 *******************************************************************/
		/* get the first free server */
		DEBUG0( "Do_queue_jobs: update_status %d", update_status );
		first_server = 0;
		server_info = (void *)servers.list;

		if( update_status ){
			if( (name = Update_servers_status( &servers )) ){
				goto begin;
			}
			Printer = orig_name;
			memset(&control_statb, 0, sizeof(control_statb) );
			pc = 0;
			Setup_printer( Printer, error, sizeof(error),
				debug_vars, 0, &control_statb, &pc );
		} else {
			Scan_queue( 1, 0 );
		}

		if(DEBUGL0 ) dump_servers( "Do_queue_jobs", &servers );
		/* now find first available printer */
		all_finished = 1;
		first_server = 0;
		for( i = 0; i < servers.count; ++i ){
			if( first_server == 0 && server_info[i].pid == 0
				&& server_info[i].status == 0 ){
				first_server = &server_info[i];
			}
			if( server_info[i].pid > 0 ){
				all_finished = 0;
			}
		}

		DEBUG0("Do_queue_jobs: first_server 0x%x, all_finished %d",
			first_server, all_finished );
		DEBUG0( "Do_queue_jobs: total_cf_files '%d', Printing_disabled %d",
			C_files_list.count, Printing_disabled );
		cfpp = (void *)C_files_list.list;
		if(DEBUGL4){
			for(i=0; i < C_files_list.count; ++i ){
				cfp = cfpp[i];
				logDebug("[%d] '%s'", i, cfp->transfername );
			}
		}

		cfp = 0;
		destination = 0;
		if( !Printing_disabled ) for( i = 0;
			cfp == 0 &&  i < C_files_list.count; ++i ){
			cfp = cfpp[i];
			destination = 0;
			/* see if you can print it */
			DEBUG0("Do_queue_jobs: checking '%s', for printable",
				cfp->transfername );
			if( cfp->hold_info.routed_time == 0 ){
				status = Check_printable(cfp, 0 );
				DEBUG0("Do_queue_jobs: checking '%s', not_printable %d",
					cfp->transfername, status );
				if( status == JREMOVE ){
					goto done;
				} else if( status ){
					cfp = 0;
					continue;
				}
			}
			/* now we check for routed jobs */
			if( cfp && cfp->hold_info.routed_time ){
				/* check to see if anything left */
				can_remove = 1;
				dpp = (void *)cfp->destination_list.list;
				status = 0;
				for( j = 0;
					destination == 0 && j < cfp->destination_list.count;
					++j ){
					destination = &dpp[j];
					/* status will be JREMOVE or JIGNORE */
					status = Check_printable( cfp, destination );
					if( status == 0 ){
						if( destination->done == 0 && destination->copies
							&& destination->copy_done >= destination->copies ){
							destination->done = time( (void *) 0 );
							Set_job_control( cfp, 0, 0 );
						}
						if( destination->done ){
							destination = 0;
						}
					} else if( status == JREMOVE ){
						/* the job is unprintable */
						destination = 0;
						DEBUG0( "Do_queue_jobs: job unprintable '%s'",
							cfp->transfername );
						goto done;
						break;
					} else {
						/* we ignore the job - simply not printable */
						destination = 0;
						can_remove = 0;
					}
				}
				/* if no destination, then we may be done or have an error */
				if( destination == 0 ){
					if( can_remove ){
						DEBUG0( "Do_queue_jobs: job finished '%s'",
							cfp->transfername );
						cfp->hold_info.done_time = time( (void *)0 );
						Set_job_control( cfp, 0, 0 );
						status = JSUCC;
						goto done;
					}
					/* if no destination, on to the next job */
					cfp = 0;
				}
			}
		}

		/*********************************************************
		 * We now have reached the following conditions:
		 *  cfp == 0 : no printable jobs
		 *  cfp != 0 : printable job waiting
         *  first_server == 0 : no server available
		 *  first_server != 0 : server available
         *  all_finished == 1    -> all servers finished
         *  all_finished == 0    -> a server is still printing
		 *
		 *        first   all
		 *  cfp  server finished
         *   0      0      0  all servers active, no jobs - wait
         *   0      0      1  all subservers aborted - terminate
         *   0      1      0  some server is active - wait
         *   0      1      1  all servers have finished - terminate
		 *   1      0      0  some server is active - wait
         *   1      0      1  all subservers aborted - terminate
		 *   1      1      0  free server and a job - start job
		 *   1      1      1  free server and a job - start job
         *
         *
         *   X      0      1  all subservers aborted - terminate
         *   0      1      1  all servers have finished - terminate
		 *   1      1      X  free server and a job - start job
		 *   - default        wait
		 *********************************************************/

		DEBUG0("Do_queue_jobs: cfp 0x%x, first_server 0x%x, all_finished %d",
			cfp, first_server, all_finished );
		if(DEBUGL0 ) dump_servers( "Do_queue_jobs: before checks",&servers);

		if( first_server == 0 && all_finished ){
			DEBUG0("Do_queue_jobs: all finished and no server available");
			break; /* we exit main loop */
		}
		if( cfp == 0 && first_server && all_finished ){
			DEBUG0("Do_queue_jobs: all jobs done");
			break; /* we exit main loop */
		}
		if( cfp && first_server ){
			DEBUG0("Do_queue_jobs: starting job '%s'", cfp->identifier+1);
			/*
			 * first, check to see if various non-server operations
			 * are called for
			 */
			first_server->transfername[0] = 0;
			newdest = 0;
			Destination = destination;
			if( destination ){
				/* set up the destination */
				newdest = destination->destination;
				strncpy( cfp->identifier, destination->identifier,
					sizeof( cfp->identifier ) );
				destination->active = pid;
				if( destination->copies ){
					int len;
					len = strlen( cfp->identifier );
					plp_snprintf( cfp->identifier+len,
						sizeof(cfp->identifier)-len,
						"C%d", destination->copy_done+1 );
				}
				cfp->copynumber = destination->sequence_number;
				DEBUG0( "Do_queue_jobs: destination id after '%s', ID '%s'",
					cfp->identifier+1, cfp->IDENTIFIER );
			} else if( cfp->hold_info.redirect[0] ){
				newdest = cfp->hold_info.redirect;
				DEBUG0( "Do_queue_jobs: redirect %s to %s",
					cfp->transfername, newdest );
			} else if( Forwarding ){
				newdest = Forwarding;
			} else if( Bounce_queue_dest ){
				newdest = Bounce_queue_dest;
				DEBUG0( "Do_queue_jobs: bounce queue %s to %s",
					cfp->transfername, newdest );
			}

			/* do this so you do not print job file multiple times */
			cfp->hold_info.active = getpid();
			if( Set_job_control( cfp, 0, 0 ) ){
				/* you cannot update hold file!! */
				setstatus( cfp, "cannot update hold file for '%s'",
					cfp->transfername );
				log( LOG_ERR,
					"Do_queue_jobs: cannot update hold file for '%s'", 
					cfp->transfername );
				status = JABORT;
				goto done;
			}
			/* force us to reread hold file */
			memset( &cfp->hstatb, 0 , sizeof(cfp->hstatb) );
			if( newdest == 0 && first_server->name ){
				/*
				 * we copy the job to the spool queue
				 */
				status = JSUCC;
				setstatus( cfp, "copying job '%s' to '%s'",
					cfp->identifier+1, first_server->name );
				DEBUG0("Do_queue_jobs: copying job %s", cfp->transfername );
				err = Copy_job( cfp, &first_server->spooldir );
				DEBUG0("Do_queue_jobs: copy status %d", err );
				if( err ){
					plp_snprintf( cfp->error, sizeof(cfp->error),
						"error copying job to %s", first_server->spooldir ); 
					first_server->status = JABORT;
					continue;
				} else if( destination == 0 ){
					/* we need to remove the job and then rescan the queue */
					Remove_job( cfp );
				} else {
					++destination->copy_done;
					destination->active = 0;
					if( Set_job_control( cfp, 0, 1 ) ){
						/* you cannot update hold file!! */
						setstatus( cfp, "cannot update hold file for '%s'",
							cfp->transfername );
						log( LOG_ERR,
							"Do_queue_jobs: cannot update hold file for '%s'", 
							cfp->transfername );
						Errorcode = JABORT;
						cleanup(0);
					}
				}
				/* fork the subserver and have it put in its id  */
				DEBUG0("Do_queue_jobs: status %s", Server_status(status) );
				if( Fork_subserver( first_server ) == 0 ){
					name = first_server->name;
					Printer = name;
					DEBUG0("Do_queue_jobs: after fork"); 
					goto begin;
				}
			} else {
				/*
				 * fork the process to do the actual dirty work
				 */
				safestrncpy( first_server->transfername, cfp->transfername );
				if( Fork_subserver( first_server ) == 0 ){
					Name = "(Worker)";
					setproctitle( "lpd %s '%s'", Name, Printer );

					/*
					 * daughter process
					 */
					pid = getpid();
					DEBUG0( "Do_queue_jobs: daughter %d", pid );
					Write_pid( unspoolerfd, pid, (char *)0 );
					cfp->hold_info.active = pid;
					(void) plp_signal (SIGUSR1,(plp_sigfunc_t)cleanup );
					(void) plp_signal (SIGHUP,(plp_sigfunc_t)cleanup );
					(void) plp_signal (SIGINT,(plp_sigfunc_t)cleanup );
					(void) plp_signal (SIGQUIT,(plp_sigfunc_t)cleanup );
					(void) plp_signal (SIGTERM,(plp_sigfunc_t)cleanup );
					DEBUG0( "Do_queue_jobs: printing '%s' to '%s'",
						cfp->transfername, Printer );
					if( destination ){
						destination->active = pid;
					}
					if( Set_job_control( cfp, 0, 0 ) ){
						/* you cannot update hold file!! */
						setstatus( cfp, "cannot update hold file for '%s'",
							cfp->transfername );
						log( LOG_ERR,
							"Do_queue_jobs: cannot update hold file for '%s'", 
							cfp->transfername );
						Errorcode = JABORT;
						cleanup(0);
					}
					if( newdest ){
						RemoteHost = Orig_RemoteHost;
						RemotePrinter = Orig_RemotePrinter;
						Lp_device = newdest;
						Check_remotehost(1);
						/* if we do not have a remote host,  then we
							send it to this one for handling */
						if( RemotePrinter == 0 || *RemotePrinter == 0 ){
							RemotePrinter = newdest;
						}
						if( RemoteHost == 0 || *RemoteHost == 0 ){
							RemoteHost = Default_remote_host;
						}
						if( RemoteHost == 0 || *RemoteHost == 0 ){
							RemoteHost = FQDNHost;
						}
					}
					if( RemoteHost ){
						DEBUG0( "Do_queue_jobs: sending '%s' to '%s@%s'",
							cfp->transfername, RemotePrinter, RemoteHost );
						Name = "(Worker - Remote)";
						setproctitle( "lpd %s '%s'", Name, Printer );
						status = Remote_job( cfp, pc );
					} else {
						Name = "(Worker - Print)";
						setproctitle( "lpd %s '%s'", Name, Printer );
						status = Print_job( cfp, pc );
					}
					Errorcode = status;
					cleanup(0);
				} else if( first_server->pid < 0 ){
					Errorcode = JABORT;
					logerr_die( LOG_ERR,"Do_queue_jobs: fork failed");
				}
			}
			continue;
		}

		/*
		 * wait for the process to finish
		 */

		DEBUG0( "Do_queue_jobs: waiting for subserver to finish" );
		if( (first_server = Wait_for_subserver( &servers ) ) == 0 ){
			continue;
		}

		/* now we extract the job information */
		pid = first_server->pid;
		first_server->pid = 0;
		cfp = 0;
		destination = 0;
		DEBUG0( "Do_queue_jobs: subserver pid '%d' for '%s' finished",
			pid, first_server->transfername );
		status = first_server->status;

		if( first_server->transfername[0] ){
			for( i = 0; i < C_files_list.count; ++i ){
				if( strcmp( cfpp[i]->transfername,
					first_server->transfername ) == 0 ){
					cfp = cfpp[i];
					break;
				}
			}
			if( cfp ){
				struct destination *dpp;
				/* we need to get and lock the destination information */

				/* force us to reread status */
				memset( &cfp->hstatb, 0 , sizeof(cfp->hstatb) );
				Get_job_control( cfp, 0 );
				dpp = ((struct destination *)cfp->destination_list.list);
				for( j = 0; j < cfp->destination_list.count; ++j ){
					if( dpp[j].active == pid ){
						destination = &dpp[j];
						break;
					}
				}
			}
		}
		if( cfp == 0 ){
			DEBUG0("Do_queue_jobs: no status to update");
			continue;
		}

done:
		DEBUG0( "Do_queue_jobs: DONE- final status %d, '%s'",
			status, Server_status(status) );
		if( destination ){
			destination->active = 0;
		}
		if( cfp ){
			cfp->hold_info.active = 0;
		}
		/* get the job control information - you may need it! */
		if( cfp ) switch( status ){
		case JHOLD:
			if( destination ){
				plp_snprintf(destination->error,sizeof(destination->error),
					"error printing job, holding for retry" ); 
				destination->hold = time( (void *) 0 );
			} else {
				plp_snprintf( cfp->error, sizeof(cfp->error),
					"error printing job, holding for retry" ); 
				cfp->hold_info.hold_time = time( (void *)0 );
			}
			Set_job_control( cfp, (void *)0, 0 );
			status = JSUCC;
			break;

		case JSUCC:	/* successful, remove job */
			if( destination ){
				++destination->copy_done;
				if( destination->copy_done >= destination->copies ){
					destination->done = time( (void *) 0 );
				}
				Set_job_control( cfp, (void *)0, 0 );
			} else {
				cfp->hold_info.done_time = time( (void *)0 );
				Set_job_control( cfp, (void *)0, 0 );
				setmessage( cfp, "TRACE", "%s@%s: job printed", Printer, FQDNHost );
				if( !Save_when_done && Remove_job( cfp ) ){
					setstatus( cfp, "could not remove job %s",
						cfp->transfername );
					status = JABORT;
					Errorcode = JABORT;
					cleanup( 0 );
				}
				Sendmail_to_user( status, cfp, pc );
			}
			status = JSUCC;
			break;

		case JFAIL:	/* failed, retry ?*/
			if( destination ){
				err = ++destination->attempt;
			} else {
				err = ++cfp->hold_info.attempt;
			}
			DEBUG0( "Do_queue_jobs: JFAIL - attempt %d, max %d",
				err, Send_try );
			Set_job_control( cfp, (void *)0, 0 );
			if( err >= Send_try ){
				char buf[60];
				/* check to see what the failure action
				 *	should be - abort, failure; default is remove
				 */
				setstatus( cfp, "job '%s', attempt %d, allowed %d",
					cfp->identifier+1, err, Send_try );
				status = Decode_transfer_failure( err, cfp, status );
				switch( status ){
				case JSUCC:   strv = "treating as successful"; break;
				case JFAIL:   strv = "retrying job"; break;
				case JABORT:  strv = "aborting server"; break;
				case JREMOVE: strv = "removing job"; break;
				case JHOLD:   strv = "holding job"; break;
				default:
					plp_snprintf( buf, sizeof(buf),
						"unexpected status 0x%x", status );
					strv = buf;
					break;
				}
				setstatus( cfp, "job '%s', %s", cfp->identifier+1, strv );
			} else {
				setstatus( cfp, "job '%s' attempt %d of %d, retrying",
					cfp->identifier+1, err, Send_try );
			}
			/* retry only if specified */
			if( status == JFAIL ){
				if( Connect_interval > 0 ){
					setstatus( cfp, "sleeping %d secs before retry",
						Connect_interval );
					sleep( Connect_interval );
				}
				status = JSUCC;
			} else {
				goto done;
			}
			break;


		default:
			/* This is not good. You probably had a core dump */
			if( destination ){
				plp_snprintf( destination->error,
					sizeof(destination->error),
					"subserver pid %d died!!! %s",
					pid, Decode_status( &procstatus ) );
			} else {
				plp_snprintf( cfp->error, sizeof(cfp->error),
					"subserver pid %d died!!! %s",
					pid, Decode_status( &procstatus ) );
				setstatus( cfp, "%s", cfp->error );
			}
			Set_job_control( cfp, (void *)0, 0 );
			/* fall through to JABORT */
		case JABORT:	/* abort, do not try again */
			{
				int fd = -1;
				Get_job_control( cfp, &fd );
				if( cfp->error[0] == 0 ){
					plp_snprintf( cfp->error,
						sizeof(cfp->error),
						"aborting operations on job %s",
						cfp->identifier+1 );
				}
				Set_job_control( cfp, &fd, 0 );
				setstatus( cfp, "%s", cfp->error );
				Sendmail_to_user( status, cfp, pc );
			}
			status = JABORT;
			break;

		case JREMOVE:	/* failed, remove job */
			status = JSUCC;
			if( destination ){
				plp_snprintf( destination->error,
					sizeof(destination->error),
					"removing job due to failures" );
				destination->done = time( (void *)0 );
				Set_job_control( cfp, (void *)0, 0 );
			} else {
				/* get the error message */
				if( cfp->error[0] == 0 ){
					plp_snprintf( cfp->error,
						sizeof(cfp->error),
						"removing job due to failures" );
				}
				cfp->hold_info.done_time = time( (void *)0 );
				Set_job_control( cfp, (void *)0, 0 );
				Sendmail_to_user( status, cfp, pc );
				if( !Save_on_error ){
					Remove_job( cfp );
				}
			}
			break;
		}
		first_server->status = status;
	}
	Errorcode = 0;
	DEBUG0("Do_queue_jobs: finished");
	/* now we write out the Server_order information */
	/* update the server order in the spool control file */
	if( Server_names && *Server_names ){
		int len, fd;
		buffer[0] = 0;
		for( i = 0; i < servers.count; ++i ){
			if( server_info[i].name == 0 ) continue;
			len = strlen(buffer);
			plp_snprintf( buffer+len, sizeof(buffer)-len,
				"%s%s",
				i == 0 ? "" : ",",
				server_info[i].name );
		}
		DEBUG3("Do_queue_jobs: final order '%s'", buffer );
		fd = 0;
		memset( &statb, 0, sizeof( statb ) );
		(void)Get_spool_control( &statb, &fd );
		Server_order = buffer;
		Set_spool_control( &fd, 0 );
	}

	return;
}

/***************************************************************************
 * Remote_job()
 * Send a job to a remote server.  This code is actually trickier
 *  than it looks, as the Send_job code takes most of the heat.
 *
 ***************************************************************************/
int Remote_job( struct control_file *cfp, struct printcap_entry *pc )
{
	int status;

	DEBUG0("Remote_job: %s", cfp->transfername );
	status = Send_job( RemotePrinter?RemotePrinter:Printer, RemoteHost, cfp,
		SDpathname, Connect_try, Connect_timeout,
		Connect_interval, Send_timeout, pc );
	DEBUG0("Remote_job: %s, status '%s'", cfp->transfername,
		Link_err_str(status) );
	switch( status ){
	case JSUCC:
	case JABORT:
	case JFAIL:
	case JREMOVE:
		break;
	case LINK_ACK_FAIL:
		setstatus(cfp,"link failure while sending job '%s'",
			cfp->identifier+1 );
		status = JFAIL;
		break;
	case LINK_PERM_FAIL:
		setstatus( cfp, "no permission to spool job '%s'", cfp->identifier+1 );
		status = JREMOVE;
		break;
	default:
		setstatus( cfp, "failed to send job '%s'", cfp->identifier+1 );
		status = JFAIL;
		break;
	}
	return( status );
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
	DEBUG0( "Fork_subserver: starting '%s'", server_info->name );
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

void Get_subserver_info( struct malloc_list *servers, char *names)
{
	char *end, *s;
	int c, cnt;
	struct server_info *server_info, *server;

	cnt = 0;
	for( s = names; s && *s; s = strpbrk( s, ":;, \t" ) ){
		++s;
		++cnt;
	}
	if( cnt+1 >= servers->max ){
		extend_malloc_list( servers, sizeof( server_info[0] ), cnt+1 );
	}
	servers->count = 0;
	server_info = (void *)servers->list;
	for( s = names; s && *s; s = end ){
		end = strpbrk( s, ":;, \t" );
		if( end ){
			*end++ = 0;
		}
		if( strlen( s ) ){
			/* we now can allocate a new server name */ 
			server = &server_info[servers->count++];
			memset( server, 0, sizeof( server[0] ) );
			server->name = s;
		}
	}
	if(DEBUGL4 ){
		logDebug( "Get_subserver_info: %d servers", servers->count );
		for( c = 0; c < servers->count; ++c ){
			logDebug( "[%d] %s", c, server_info[c].name );
		}
	}
}

/***************************************************************************
 * struct server_info *Wait_for_subserver( struct malloc_list *servers,
 *     int block, plp_block_mask *omask )
 *  wait for a server process to exit
 *  if none present return 0
 *  look up the process in the process table
 *  update the process table status
 *  return the process table entry
 ***************************************************************************/
static struct server_info *Wait_for_subserver( struct malloc_list *servers )
{
	pid_t pid;
	int status, i;
	plp_status_t procstatus;
	struct server_info *server_info = (void *)servers->list;
	int err;				/* saved errno */
	plp_block_mask omask;
	struct server_info *found = 0;

	/*
	 * wait for the process to finish
	 */

	errno = 0;
	plp_block_all_signals( &omask );

begin:
	DEBUG0( "Wait_for_subservers: checking" );

	/* now we check for processes that have exited */
	/* waitpid returns:
	 *  -1 - error (ECHILD means no children)
	 *   0 - when WNOHANG means no child status available
	 *   pid - pid of exited child
	 */
	Child_exit = 0;

	procstatus = 0;
	pid = plp_waitpid( -1, &procstatus, WNOHANG );
	err = errno;

	DEBUG0( "Wait_for_subservers: pid %d, errno '%s', status '%s'",
		pid, Errormsg(err), Decode_status( &procstatus ) );

	if( pid == -1 && err != ECHILD ){
		logerr_die( LOG_ERR, "Wait_for_subservers: waitpid error" );
	} else if( pid == -1 ){
		DEBUG0( "Wait_for_subservers: all servers have finished" );
		goto done;
	} else if( pid ){
		if( WIFSIGNALED( procstatus ) ){
			status = WTERMSIG( procstatus );
			DEBUG0("Wait_for_subservers: pid %d terminated by signal %d",
				pid, WTERMSIG( procstatus ) );
			status = JFAIL;
		} else {
			status = WEXITSTATUS( procstatus );
			if( status > 0 && status < 32 ) status += JFAIL-1;
		}
		DEBUG0( "Wait_for_subservers: pid %d final status %s",
					pid, Server_status(status) );
		for( i = 0; found == 0 && i < servers->count; ++i ){
			if( server_info[i].pid == pid ){
				found = &server_info[i];
				if( found->initial ){
					found->initial = 0;
				} else {
					found->time = ++Finished_order;
				}
				found->status = status;
				DEBUG0( "Wait_for_subservers: server '%s' for '%s' finished",
					found->name, server_info[i].transfername );
			}
		}
		if( found ){
			/* sort server order */
			if( Mergesort( server_info, servers->count,
				sizeof( server_info[0] ), cmp_server ) ){
				fatal( LOG_ERR, "Wait_for_subservers: Mergesort failed" );
			}
			if(DEBUGL4 ){
				dump_servers( "Wait_for_subservers: after sorting", servers );
			}
			/* find the data in the reordered list */
			for( i = 0; i < servers->count; ++i ){
				if( server_info[i].pid == pid ){
					found = &server_info[i];
					break;
				}
			}
		}
	}

	/* unblock signals and wait for change */
	if( found == 0 && Susr2 == 0 ){
		/* we need to unblock block and wait for change */
		Sig_break = 1;
		(void) plp_signal_break(SIGUSR2,  (plp_sigfunc_t)Sigusr2);
		Setup_waitpid_break();
		DEBUG0( "Wait_for_subservers: blocking!" );
		plp_sigpause();
		DEBUG0( "Wait_for_subservers: got signal!" );
		Sig_break = 0;
		Setup_waitpid();
		(void) plp_signal(SIGUSR2,  (plp_sigfunc_t)Sigusr2);
		if( Child_exit ) goto begin;
	}

done:
	plp_unblock_all_signals( &omask );
	return( found );
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

int link_or_copy( char *src, char *dest )
{
	int srcfd, destfd;
	struct stat statb, wstatb;
	int len, l, lock, create;
	char line[LARGEBUFFER];

	if( stat( dest, &statb ) == 0 && unlink( dest ) == -1 ){
		logerr(LOG_ERR,
			"link_or_copy: cannot unlink '%s'",dest);
		return(1);
	}
	if( link( src, dest ) == -1 ){
		DEBUG0("link_or_copy: link failed '%s'", Errormsg(errno) );
		/* try copying then */
		srcfd = Checkread( src, &statb );
		if( srcfd <= 0 ){
			logerr( LOG_ERR, "link_or_copy: cannot open for reading '%s'",
				src);
			return(1);
		}
		destfd = Lockf( dest, &lock, &create, &wstatb );
		if( destfd <= 0 ){
			logerr( LOG_ERR, "link_or_copy: cannot lock for writing '%s'",
				dest );
			return(1);
		}
		if( ftruncate( destfd, 0 ) == -1 ){
			logerr( LOG_ERR, "link_or_copy: cannot truncate '%s'",
				dest );
			return(1);
		}
		for( l = len = statb.st_size;
			len > 0 && (l = read( srcfd, line, sizeof(line))) > 0;
			len -= l ){
			if( Write_fd_len( destfd, line, l ) < 0 ){
				logerr(LOG_ERR,
					"link_or_copy: write failed to '%s'",dest);
				return(1);
			}
		}
		if( l < 0  ){
			logerr(LOG_ERR,"link_or_copy: read failed '%s'",src);
			return(1);
		}
		close( destfd );
	}
	return(0);
}

/***************************************************************************
 * Copy_job:
 *  subserver gets a new job
 *  We have the subserver copy the job into the new location.
 *  Note that the server will fork the subserver FIRST,
 *   the subserver will update the information.
 ***************************************************************************/
static int Copy_job(struct control_file *cfp, struct dpathname *spooldir  )
{
	int i;						/* ACME Indexes! Gives you the finger! */
	struct data_file *data_file;	/* data files in job */
	struct stat statb;				/* status buffer */
	char *src, *dest;				/* source and destination file */
	int err = 0;

	/* we now can do the setup of the process */
	DEBUG0("Copy_job: printer '%s', job '%s' copying data files",
		Printer, cfp->identifier+1 );
		
	data_file = (void *)cfp->data_file_list.list;
	for( i = 0; i < cfp->data_file_list.count; ++i ){
		src = data_file[i].openname;
		if( src[0] == 0 ) continue;
		if( stat( src, &statb ) == -1 ){
			logerr( LOG_ERR, "Copy_job: cannot stat '%s'", src );
			err = 1;
			break;
		}
		dest = Add_path( spooldir, data_file[i].original );
		DEBUG0("Copy_job: data file src '%s' dest '%s'", src, dest );
		if( (err = link_or_copy( src, dest )) ) break;
	}
	if( err == 0 ){
		src = cfp->openname;
		if( stat( src, &statb ) == -1 ){
			logerr( LOG_ERR, "Copy_job: cannot stat '%s'", src );
			err = 1;
		} else {
			dest = Add_path( spooldir, cfp->original );
			DEBUG0("Copy_job: control file src '%s' dest '%s'", src, dest );
			err = link_or_copy( src, dest );
		}
	}
	return(err);
}

/***************************************************************************
 * int Decode_transfer_failure( int attempt, struct control_file *cfp, int status )
 * When you get a job failure more than a certain number of times,
 *  you check the 'Send_failure_action' variable
 *  This can be abort, retry, or remove
 * If retry, you keep retrying; if abort you shut the queue down;
 *  if remove, you remove the job and try again.
 ***************************************************************************/

static struct keywords keys[] = {
	{"succ", INTEGER_K, (void *)0, JSUCC},
	{"jsucc", INTEGER_K, (void *)0, JSUCC},
	{"success", INTEGER_K, (void *)0, JSUCC},
	{"jsuccess", INTEGER_K, (void *)0, JSUCC},
	{"abort", INTEGER_K, (void *)0, JABORT},
	{"jabort", INTEGER_K, (void *)0, JABORT},
	{"hold", INTEGER_K, (void *)0, JHOLD},
	{"jhold", INTEGER_K, (void *)0, JHOLD},
	{"remove", INTEGER_K, (void *)0, JREMOVE},
	{"jremove", INTEGER_K, (void *)0, JREMOVE},
	{ (char *)0 }
};

static int Decode_transfer_failure( int attempt, struct control_file *cfp,
	int status )
{
	struct keywords *key;
	int p[2], count;
	char line[SMALLBUFFER];

	DEBUG0("Decode_transfer_failure: send_failure_action '%s'",
		Send_failure_action );
	if( Send_failure_action ){
		/* check to see if it is a filter */
		if( Send_failure_action[0] == '|' ){
			/* open a pipe to read output from */
			if( pipe(p) < 0 ){
				logerr_die( LOG_ERR, "Decode_transfer: pipe failed" );
			}
			DEBUG0("Decode_transfer_failure: pipe fd [%d, %d]", p[0], p[1] );
			if( Make_filter( 'f', cfp, &As_fd_info, Send_failure_action, 0, 0,
				p[1], (void *)0, (void *)0, 0, Logger_destination != 0, 0 ) ){
				fatal( LOG_ERR, "%s", cfp->error );
			}
			plp_snprintf( line, sizeof(line), "%d", attempt );
			(void)close(p[1]);
			p[1] = -1;
			/* write attempt count to filter */
			if( As_fd_info.input > 0 ){
				Write_fd_str( As_fd_info.input, line );
				close( As_fd_info.input );
				As_fd_info.input = -1;
			}
			while( (count = read( p[0], line, sizeof(line)-1 )) > 0 ){
				line[count] = 0;
				DEBUG0("Decode_transfer_failure: read '%s'", line );
			}
			close( p[0] );
			/* now we get the exit status */
		 	status = Close_filter( &As_fd_info, 0, "transfer failure" );
			DEBUG0("Decode_transfer_failure: exit status '%s'",
				Server_status(status) );
		} else {
			for( key = keys; key->keyword; ++key ){
				DEBUG0("Decode_transfer_failure: comparing '%s' to '%s'",
					Send_failure_action, key->keyword );
				if( strcasecmp( key->keyword, Send_failure_action ) == 0 ){
					status = key->maxval;
					break;
				}
			}
		}
	}
	DEBUG0("Decode_transfer_failure: returning '%s'", Server_status(status) );
	return( status );
}

/***************************************************************************
 * Check_printable()
 * Check to see if the job is printable
 ***************************************************************************/

int Check_printable( struct control_file *cfp, struct destination *dest )
{
	int permission;
	struct perm_check perm_check;
	int not_printable = 0;

	DEBUG0("Check_printable: '%s'", cfp->transfername );
	memset( &perm_check, 0, sizeof(perm_check) );
	/* check to see if it is being held or not printed */
	cfp->hold_info.not_printable = 0;
	if( cfp->flags
		|| cfp->error[0]
		|| cfp->hold_info.active
		|| cfp->hold_info.hold_time
		|| cfp->hold_info.remove_time
		|| cfp->hold_info.done_time
		|| cfp->hold_info.held_class
		){
		not_printable = cfp->hold_info.not_printable = JIGNORE;
	}
	if( not_printable == 0 && dest ){
		if( dest->error[0]
			|| dest->active ){
			not_printable = dest->not_printable = JIGNORE;
		}
	}
	DEBUG0("Check_printable: job '%s' not_printable %d, dest '%s', not_printable %d",
		cfp->transfername, cfp->hold_info.not_printable,
		dest?dest->destination:"", dest?dest->not_printable:0);
	if( not_printable ) return(not_printable);

	/*
	 * check to see if you have permissions to print/process
	 * the job
	 */
	memset( &perm_check, 0, sizeof( perm_check ) );
	perm_check.service = 'R';
	perm_check.printer = Printer;
	if( cfp->LOGNAME && cfp->LOGNAME[1] ){
		perm_check.user = cfp->LOGNAME+1;
		perm_check.remoteuser = perm_check.user;
	}

	if( cfp->FROMHOST && cfp->FROMHOST[1]
		&& Find_fqdn( &PermcheckHostIP, &cfp->FROMHOST[1], 0 ) ){
		Perm_check.host = &PermcheckHostIP;
		Perm_check.remotehost = &PermcheckHostIP;
	}

	Init_perms_check();
	if( (permission = Perms_check( &Perm_file, &perm_check, cfp )) == REJECT
		|| (permission == 0
			&& (permission = Perms_check(
					&Local_perm_file, &perm_check, cfp )) == REJECT)
		|| (permission == 0 && Last_default_perm == REJECT) ){
		plp_snprintf( cfp->error, sizeof(cfp->error),
			"no permission to print job %s", cfp->identifier+1 );
		not_printable = cfp->hold_info.not_printable = JREMOVE;
	}
	DEBUG0("Check_printable: '%s' - permissions -> %d",
		cfp->transfername, cfp->hold_info.not_printable );
	return( not_printable );
}

static void dump_servers( char *title, struct malloc_list *servers )
{
	struct server_info *list, *entry;
	int i;

	if( title ) logDebug( "*** %s ***", title );
	if( servers ){
		logDebug( "server count %d", servers->count );
		list = (void *)servers->list;
		for( i = 0; i < servers->count; ++i ){
			entry = &list[i];
			logDebug( "  [%d] name '%s', pid %d, status %d, initial %d",
				i, entry->name, entry->pid, entry->status, entry->initial );

			logDebug( "       pr_disbld %d, time %ld, transfername '%s'",
				entry->printing_disabled, entry->time, entry->transfername );
			logDebug( "       spooldir '%s', len %d",
				entry->spooldir.pathname, entry->spooldir.pathlen );
		}
	}
}


/***************************************************************************
 * Update_server_status( struct malloc_list *servers )
 *  update the server information by seeing if you need
 ***************************************************************************/

char *Update_servers_status( struct malloc_list *servers )
{
	int noprint;
	int i, hc, cnt;		/* AJAX! for integers */
	struct server_info *server_info;	/* server information */

	server_info = (void *)servers->list;
	for( i = 0; i < servers->count; ++i ){
		if( server_info[i].pid <= 0 ){
			server_info[i].status = 0;
			server_info[i].pid = 0;
			if( server_info[i].name == 0 ){
				/* no subservers */
				continue;
			}
			Printer = server_info[i].name;
			DEBUG0( "Update_servers_status: update - checking '%s'", Printer );
			if( Setup_printer( Printer, error, sizeof(error),
				0, 1, 0, 0 ) ){
				server_info[i].pid = -1;
			}
			DEBUG0("Update_servers_status: printer '%s' SD '%s'",
				Printer, Clear_path(SDpathname) );
			if( server_info[i].spooldir.pathname[0] == 0 ){
				Init_path(&server_info[i].spooldir,
					Clear_path(SDpathname));
			}
			cnt = 0;
			noprint = (Printing_disabled | Spooling_disabled);
			DEBUG0( "Update_servers_status: printing_disabled", noprint );
			if( noprint ){
				server_info[i].pid = -1;
				continue;
			}
			/* get pathname of destination */
			Job_count( &hc, &cnt );
			DEBUG0( "Update_servers_status: job count %d", cnt );
			if( cnt == 0 ){
				continue;
			}
			DEBUG0( "Update_servers_status: update - starting '%s'",
				Printer );
			if( Fork_subserver( &server_info[i] ) == 0 ){
				return(Printer);
			}
			/* sleep a short time */
			plp_usleep( 10000 );
		}
	}
	return(0);
}
