/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpd_jobs.c
 * PURPOSE: process jobs in the queue
 **************************************************************************/

static char *const _id =
"lpd_jobs.c,v 3.36 1998/03/29 18:32:42 papowell Exp";

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

extern char *Update_servers_status( struct malloc_list *servers );
static char error[LINEBUFFER];

int Remote_job( struct control_file *cfp, struct printcap_entry *pc);

static int Copy_job( struct control_file *cfp, struct server_info *server);
static int Fork_subserver( struct server_info *server_info );
static struct server_info *Wait_for_subserver( struct malloc_list *servers );
static int Finished_order;
static void dump_servers( char *title, struct malloc_list *servers );
int Do_check_idle(struct printcap_entry *pc);

/***************************************************************************
 * Start_all()
 *  This tries to start all the queue servers,  and is called by the
 *  LPD server.  It simply will reset the servers needed list
 ***************************************************************************/

int Server_next;
int PipeSig[2];
int Need_to_start( char *pr );

int Start_all(void)
{
	Server_next = 0;
	return(1);
}

/***************************************************************************
 * Start_idle_server( int max_to_be_started )
 *  start up at most max_to_be_started idle servers
 ***************************************************************************/

int Start_idle_server( int max_to_be_started )
{
	int started = 0;
	int status, list_max, pid, len, err, n, errfd, debuglevel, debugflag;
 	struct printcap_entry *entries;
	char buffer[LINEBUFFER];
	char *pr, *end;

	/* we start up servers while we can */
 	if( All_list.count ){
		list_max = All_list.count;
	} else {
		list_max = Expanded_printcap_entries.count;
	}
	DEBUG0( "Start_idle_server: begin Server_next %d, max %d", Server_next, list_max );
	if( Server_next >= list_max ) return(0);
	while( Server_next < list_max && started < max_to_be_started ){
		/* first, we make pipe */
		if( pipe(PipeSig) == -1 ){
			fatal( LOG_INFO, _("Start_idle_server: pipe failed!") );
		}
		if( (pid = dofork(1)) < 0 ){
			logerr( LOG_INFO, _("Start_idle_server: fork failed!") );
			close(PipeSig[0]); PipeSig[0] = 0;
			close(PipeSig[1]); PipeSig[1] = 0;
			return( started );
		} else if( pid == 0 ){
			/* child process */
			close(PipeSig[0]); PipeSig[0] = 0;
			/* we save the error log FD */ 
			errfd = dup(2);
			debuglevel = Debug;
			debugflag = DbgFlag;
			if( All_list.count ){
				char **line_list;
				DEBUG0("Start_idle_server: using the All_list" );
				line_list = All_list.list;
				while( Server_next < All_list.count ){
					pr = line_list[Server_next];
					DEBUG0("Start_idle_server: checking '%s'", pr );
					plp_snprintf(buffer,sizeof(buffer),"%d %s\n",Server_next, pr);
					Write_fd_str(PipeSig[1],buffer);
					Do_queue_jobs(pr);
					dup2(errfd,2);
					Debug = debuglevel;
					DbgFlag = debugflag;
					++Server_next;
				}
			} else {
				DEBUG0("Start_idle_server: using the printcap list" );
				entries = (void *)Expanded_printcap_entries.list;
				while( Server_next < Expanded_printcap_entries.count ){
					pr = entries[Server_next].names[0];
					DEBUG0("Start_idle_server: checking '%s'", pr );
					plp_snprintf(buffer,sizeof(buffer),"%d %s\n",Server_next, pr);
					Write_fd_str(PipeSig[1],buffer);
					Do_queue_jobs(pr);
					Debug = debuglevel;
					DbgFlag = debugflag;
					dup2(errfd,2);
					++Server_next;
				}
			}
			plp_snprintf(buffer,sizeof(buffer),"%d\n",Server_next);
			Write_fd_str(PipeSig[1],buffer);
			close(PipeSig[1]); PipeSig[1] = 0;
			cleanup(0);
		}
		close(PipeSig[1]); PipeSig[1] = 0;
		DEBUG0( "Start_idle_server: created pid %d", pid );
		buffer[0] = 0;
		do{
			errno = 0;
			len = strlen(buffer);
			status = read(PipeSig[0],buffer+len,sizeof(buffer)-1-len);
			err = errno;
			if( status >= 0 ) buffer[len+status] = 0;
			DEBUG0( "Start_idle_server: read status %d, read '%s', errno '%s'",
				status, &buffer[len],Errormsg(err) );
			if( status <= 0 ){
				++started;
				++Server_next;
				DEBUG0( "Start_idle_server: Server_next %d, started %d",
					Server_next, started );
				break;
			}
			/* we split up read line and record information */
			while(( end = strchr(buffer,'\n') )){
				*end++ = 0;
				n = atoi(buffer);
				if( n >= Server_next ) Server_next = n;
				DEBUG0( "Start_idle_server: new Server_next %d", Server_next );
				strcpy( buffer, end );
			}
		} while( Server_next < list_max && started < max_to_be_started );
		close( PipeSig[0] ); PipeSig[0] = 0;
	}
	if( PipeSig[0] ){ close(PipeSig[0]); PipeSig[0] = 0; }
	if( PipeSig[1] ){ close(PipeSig[1]); PipeSig[1] = 0; }
	DEBUG0( "Start_idle_server: started %d printers", started );
	return(started);
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


/***************************************************************************
 * Do_queue_jobs: process the job queue
 ***************************************************************************/

void Do_queue_jobs( char *name )
{
	int lockfd = -1;			/* lock file fd */
	char subserver_for[LINEBUFFER];	/* flag set when a subserver */
	struct stat statb;			/* status of file */
	struct stat control_statb;	/* status of control file */
	pid_t pid;					/* process id */
	int unspoolerfd = -1;		/* server id */
	struct control_file *cfp, **cfpp;	/* control file to process */
	int status = 0;				/* status of the job */
	int i, j, c;				/* AJAX! for integers */
	char *pname, *strv, *end;	/* ACME, the very finest pointers */
	int err = 0;
	struct destination *destination = 0;	/* current destination */
	struct destination *dpp;		/* list */
	struct printcap_entry *pc;
	char *newdest;				/* new destination */
	int all_finished;			/* all servers done */
	int update_status;			/* we need to update status */
	plp_block_mask omask;		/* signal mask */
	static struct malloc_list servers;
	struct server_info *server_info;	/* server information */
	struct server_info *first_server, *finished_server;
	char orig_name[LINEBUFFER];
	char buffer[LINEBUFFER];
	int did_work = 0;				/* we did work */


	subserver_for[0] = 0;
	server_info = 0;

begin:
	servers.count = 0;
	safestrncpy(orig_name, name);
	Printer = orig_name;
	close( lockfd );
	close( unspoolerfd );

	/* when a subserver, we come here to re-do the work
		of reading the printcap file, etc. */
	/* process the job */

	(void) plp_signal(SIGUSR2,  (plp_sigfunc_t)Sigusr2);

	Name = "(Server)";
	if( subserver_for[0] ){
		Name = "(Subserver)";
	}
	proctitle( "lpd %s '%s'", Name, Printer );

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
	DEBUG0( "Do_queue_jobs: printer '%s', pr_disabled %d, pr_aborted %d",
		Printer, Printing_disabled, Printing_aborted );

	if( i ){
		if( i != 2 ){
			log( LOG_ERR, _("Do_queue_jobs: %s"), error );
		} else {
			DEBUG0( "Do_queue_jobs: '%s'", error );
		}
		/* see if we do checks */
		if( PipeSig[1] ) return;
		/* here we simply exit */
		cleanup(0);
	}

	DEBUG0( "Do_queue_jobs: Server_queue_name '%s' and subserver for '%s'",
		Server_queue_name, subserver_for );
	if( Server_queue_name && subserver_for[0] == 0 ){
		DEBUG0("Do_queue_jobs: subserver started without server");
		Errorcode = 0;
		if( PipeSig[1] ) return;
		cleanup( 0 );
	}

	if( subserver_for[0] ){
		if( Printing_disabled ){
			Errorcode = JNOPRINT;
			DEBUG0( "Do_queue_jobs: printing disabled" );
			cleanup(0);
		}
		if( Printing_aborted ){
			Errorcode = JNOPRINT;
			DEBUG0( "Do_queue_jobs: printing aborted" );
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
	lockfd = Checkwrite( pname, &statb, O_RDWR, 1, 0 );
	if( lockfd < 0 ){
		logerr_die( LOG_ERR, _("Do_queue_jobs: cannot open '%s'"), pname ); 
	}
	if( Do_lock( lockfd, pname, 0 ) < 0 ){
		pid = Read_pid( lockfd, (char *)0, 0 );
		DEBUG0( "Do_queue_jobs: server process '%d' may be active", pid );
		if( PipeSig[1] == 0 && pid > 0 ){
			kill( pid, SIGUSR2 );
		}
		if( PipeSig[1] ){
			DEBUG0("Do_queue_jobs: returning, active server" );
			return;
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
		logerr_die( LOG_ERR, _("Do_queue_jobs: cannot open '%s'"), pname );
	}

	/* we clear out all of the active process information -
	 * it is a hold over from a previous server.  If we do not do this,
	 * then we will not be able to get the correct job status
	 */
	cfpp = (void *)C_files_list.list;
	for(i=0; i < C_files_list.count; ++i ){
		cfp = cfpp[i];
		update_status = 0;
		if( cfp->hold_info.server || cfp->hold_info.subserver ){
			cfp->hold_info.server = 0;
			cfp->hold_info.subserver = 0;
			update_status = 1;
		}
		dpp = (void *)cfp->destination_list.list;
		for( j = 0; j < cfp->destination_list.count; ++j ){
			if( dpp[j].subserver ){
				dpp[j].subserver = 0;
				update_status = 1;
			}
		}
		if( update_status ) Set_job_control( cfp, 0);
	}
	update_status = 0;

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
			fatal( LOG_ERR, _("Do_queue_jobs: Mergesort failed") );
		}
		if(DEBUGL4 ){
			logDebug( "Do_queue_jobs: server order after sorting" );
			for( c = 0; c < servers.count; ++c ){
				logDebug( "[%d] %s, time %d",
					c, server_info[c].name, server_info[c].time );
			}
		}
		if( (name = Update_servers_status( &servers )) ){
			if( PipeSig[1] ){
				close(PipeSig[1] );
				PipeSig[1] = 0;
			}
			goto begin;
		}
		Printer = orig_name;
		memset(&control_statb, 0, sizeof(control_statb) );
		pc = 0;
		Setup_printer( Printer, error, sizeof(error),
			debug_vars, 0, &control_statb, &pc );
	} else {
		/* dummy one for our queue here */
		servers.count = 0;
		if( servers.max == 0 ){
			extend_malloc_list( &servers, sizeof( server_info[0] ), 1,__FILE__,__LINE__  );
		}
		server_info = (void *)servers.list;
		first_server = &server_info[servers.count++];
		memset( first_server, 0, sizeof( first_server[0] ) );
		first_server->initial = 1;
	}

	/*
	 * Inform the waiting process that we are going to print
	 * something
	 */

	cfp = 0;
	destination = 0;
	buffer[0] = 0;
	for( i = 0; cfp == 0 &&  i < C_files_list.count; ++i ){
		cfp = cfpp[i];
		/* see if you can print it */
		DEBUG0("Do_queue_jobs: checking '%s'%s, for printable",
			cfp->transfername,
			   (cfp->hold_info.routed_time ? " (routed)" : "") );
		status = Job_printable_status( cfp, &destination, buffer, sizeof(buffer) );
		if( status == JIGNORE ){
			cfp = 0;
		} else if( Printing_disabled || Printing_aborted ){
			/* we allow removed or moved jobs to be processed */
			if( status == JREMOVE || cfp->hold_info.redirect[0] ){
				break;
			}
		}
	}
	DEBUG0("Do_queue_jobs: PipeSig[1] %d, cfp 0x%x, status '%s', '%s'",
		PipeSig[1],cfp, Server_status(status), buffer );
	if( PipeSig[1] ){
		/* no work and no subservers then we are done another */
		i = Countpid(0);
		if( i == 0 && cfp == 0 ){
			DEBUG0("Do_queue_jobs: returning" );
			Write_pid( lockfd, 0, (char *)0 );
			close( lockfd );
			return;
		}
		/* we have work or we need to wait */
		close(PipeSig[1]);
		PipeSig[1] = 0;
	}
	DEBUG0("Do_queue_jobs: doing work" );

	/* we now invoke the Check_idle operation */
	if( Check_idle && !(Printing_disabled || Printing_aborted) ){
		status = Do_check_idle(pc);
		if( status ){
			exit(status);
		}
	}

	while(1){
		/* check for changes to spool control information */

		DEBUG0( "Do_queue_jobs: MAIN LOOP" );
		DEBUGF(DMEM1)( "Do_queue_jobs: memory allocation now %s",
			Brk_check_size());
		plp_block_all_signals( &omask );
		update_status = Susr2;
		Susr2 = 0;
		plp_unblock_all_signals( &omask );

		if( Get_spool_control( &control_statb, (void *)0 ) ){
			DEBUG0( "Do_queue_jobs: control file changed, restarting" );
			Fix_update( debug_vars, 0 );
			update_status = 1;
		}

		/* get the first free server */
		DEBUG0( "Do_queue_jobs: update_status %d", update_status );
		first_server = 0;
		finished_server = 0;
		server_info = (void *)servers.list;

		/* we may need to start subservers again */

		if( update_status ){
			if( Server_names && Server_names[0] ){
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
		DEBUG0( "Do_queue_jobs: total_cf_files '%d', pr_disabled %d, pr_aborted %d",
			C_files_list.count, Printing_disabled, Printing_aborted );
		cfpp = (void *)C_files_list.list;
		if(DEBUGL4){
			for(i=0; i < C_files_list.count; ++i ){
				cfp = cfpp[i];
				logDebug("[%d] '%s'", i, cfp->transfername );
			}
		}

		cfp = 0;
		destination = 0;
		for( i = 0; cfp == 0 &&  i < C_files_list.count; ++i ){
			cfp = cfpp[i];
			/* see if you can print it */
			DEBUG0("Do_queue_jobs: checking '%s'%s, for printable",
				cfp->transfername,
			       (cfp->hold_info.routed_time ? " (routed)" : "") );
			status = Job_printable_status( cfp, &destination, buffer, sizeof(buffer) );
			if( status == JREMOVE ){
				/* we have a job to be removed from queue */
				goto done;
			} else if( status ){
				cfp = 0;
			} else if( cfp->hold_info.done_time ){
				/* we have a completed job to be removed from queue */
				goto done;
			} else if( Printing_disabled || Printing_aborted ){
				/* we allow Moved jobs to be processed */
				if( cfp->hold_info.redirect[0] ){
					break;
				}
				cfp = 0;
			}
		}
		if( destination ){
			Destination_index = 1+ (destination - (struct destination *)(cfp->destination_list.list));
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
			newdest = 0;
			cfp->hold_info.server = getpid();
			cfp->hold_info.active_time = time((void *)0);
			safestrncpy( first_server->transfername, cfp->transfername );
			/* do this so you do not print job file multiple times */
			if( Set_job_control( cfp, 0) ){
				/* you cannot update hold file!! */
				setstatus( cfp, _("cannot update hold file for '%s'"),
					cfp->transfername );
				log( LOG_ERR,
					_("Do_queue_jobs: cannot update hold file for '%s'"), 
					cfp->transfername );
				status = JABORT;
				goto done;
			}
			/* fork the subserver and get its id  */
			if( did_work == 0 ){
				did_work = 1;
				setstatus(0,_("server starting"));
			}
			if( Fork_subserver( first_server ) == 0 ){
				pid = getpid();
				DEBUG0( "Do_queue_jobs: daughter %d", pid );
				Write_pid( unspoolerfd, pid, (char *)0 );
				cfp->hold_info.subserver = pid;
				(void) plp_signal (SIGUSR1,(plp_sigfunc_t)cleanup_USR1 );
				(void) plp_signal (SIGHUP,(plp_sigfunc_t)cleanup_HUP );
				(void) plp_signal (SIGINT,(plp_sigfunc_t)cleanup_INT );
				Name = "(Worker)";
				proctitle( "lpd %s '%s'", Name, Printer );
				if( Destination_index ){
					/* set up the destination */
					destination = Destination_cfp(cfp,Destination_index);
					newdest = destination->destination;
					strncpy( cfp->identifier, destination->identifier,
						sizeof( cfp->identifier ) );
					destination->subserver = pid;
					if( destination->copies ){
						int len;
						len = strlen( cfp->identifier );
						plp_snprintf( cfp->identifier+len,
							sizeof(cfp->identifier)-len,
							"C%d", destination->copy_done+1 );
					}
					DEBUG0( "Do_queue_jobs: destination id '%s', ID '%s'",
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
				if( Set_job_control( cfp, 0) ){
					/* you cannot update hold file!! */
					setstatus( cfp, _("cannot update hold file for '%s'"),
						cfp->transfername );
					log( LOG_ERR,
						_("Do_queue_jobs: cannot update hold file for '%s'"), 
						cfp->transfername );
					Errorcode = JABORT;
					cleanup(0);
				}
				if( newdest == 0 && first_server->name[0] ){
					/*
					 * we copy the job to the spool queue
					 */
					status = JSUCC;
					setstatus( cfp, _("copying job '%s' to '%s'"),
						cfp->identifier+1, first_server->name );
					DEBUG0("Do_queue_jobs: copying job %s", cfp->transfername );
					err = Copy_job( cfp, first_server );
					DEBUG0("Do_queue_jobs: copy status %d", err );
					if( err ){
						plp_snprintf( cfp->error, sizeof(cfp->error),
							_("error copying job to %s"),
							first_server->spooldir.pathname ); 
						first_server->status = JABORT;
						continue;
					}
					setmessage(cfp,"TRACE","%s@%s: job copied to '%s'",
						Printer,FQDNHost, first_server->name );
					setstatus( cfp, _("copy done '%s' to '%s'"),
						cfp->identifier+1, first_server->name );

					/* now restart as new print server */
					name = first_server->name;
					Printer = name;
					DEBUG0("Do_queue_jobs: restarting as '%s'", Printer); 
					goto begin;
				} else {
					/*
					 * The following code is implementing job handling as follows.
					 *  if newdest has a value then
					 *      newdest has format 'pr' or 'pr@host'
					 *      if it is pr@host then we set RemotePrinter and RemoteHost
					 *         else we set RemotePrinter, RemoteHost is then set
					 *         to Default or FQDN of server
					 *
					 *  else we look at Lp_device
					 *    - if have a :lp: entry,
					 *      then this can be 'lp=pr@host' and if it is,
					 *          we set the RemoteHost and
					 *      RemotePrinter appropriately.
					 */
					if( newdest ){
						RemoteHost = 0;
						RemotePrinter = 0;
						Lp_device = 0;
						Destination_port = 0;
						if( strchr(newdest, '@') ){
							Lp_device = newdest;
							Check_remotehost();
						}
						/* if we do not have a remote host,  then we
							send it to this one for handling */
						if( RemotePrinter == 0 ){
							RemotePrinter = newdest;
						}
						if( RemoteHost == 0 ){
							RemoteHost = Default_remote_host;
							if( RemoteHost == 0 ){
								RemoteHost = FQDNHost;
							}
						}
					} else if( Lp_device ){
						RemoteHost = 0;
						RemotePrinter = 0;
						if( strchr(Lp_device, '@') ){
							Check_remotehost();
						}
					}
/*
 * At this point, if RemotePrinter is set,  then we have RemotePrinter and
 * RemoteHost set, so we can send to the remote site.
 * If not,  then it must be a local printer.
 */
					if( RemotePrinter ){
						DEBUG0( "Do_queue_jobs: sending '%s' to '%s@%s'",
							cfp->transfername, RemotePrinter, RemoteHost );
						Name = "(Worker - Remote)";
						proctitle( "lpd %s '%s'", Name, Printer );
						if( Remote_support
							&& strchr( Remote_support, 'R' ) == 0 
							&& strchr( Remote_support, 'r' ) == 0 ){
							status = JABORT;
							setstatus( cfp,
							_("no remote support to `%s@%s'"),
							RemotePrinter, RemoteHost );
							cleanup(0);
						}
						status = Remote_job( cfp, pc );
					} else {
						DEBUG0( "Do_queue_jobs: printing '%s'",
							cfp->transfername, Printer );
						Name = "(Worker - Print)";
						proctitle( "lpd %s '%s'", Name, Printer );
						status = Print_job( cfp, pc, Send_job_rw_timeout);
					}
					Errorcode = status;
					cleanup(0);
				}
			} else if( first_server->pid < 0 ){
				/* we should suspend operations for a bit */
				first_server->pid = 0;
				sleep(1);
			} else {
				DEBUG0("Do_queue_jobs: job '%s' server %d", cfp->identifier+1,
					first_server->pid );
			}
		}

		/*
		 * wait for a process to finish
		 */

		DEBUG0( "Do_queue_jobs: waiting for subserver to finish" );
		if( (finished_server = Wait_for_subserver( &servers ) ) == 0 ){
			continue;
		}

		/* now we extract the job information */
		pid = finished_server->pid;
		finished_server->pid = 0;
		cfp = 0;
		destination = 0;
		DEBUG0(
"Do_queue_jobs: subserver pid '%d' for '%s' finished, status '%s', signal '%s'",
			pid, finished_server->transfername,
			Server_status(finished_server->status),
			Sigstr(finished_server->sigval) );
		status = finished_server->status;

		if( finished_server->transfername[0] ){
			for( i = 0; i < C_files_list.count; ++i ){
				if( strcmp( cfpp[i]->transfername,
					finished_server->transfername ) == 0 ){
					if( stat( cfpp[i]->transfername, &statb) == 0 ){
						cfp = cfpp[i];
					} else {
						/* the job was removed */
						finished_server->status = JSUCC;
					}
					break;
				}
			}
		}

		if( cfp == 0 ){
			if( finished_server->status != JSIGNAL){
				setstatus( cfp, _("subserver pid %d status '%s'"),
					pid, Server_status(status));
			} else {
				setstatus( cfp, _("subserver pid %d status '%s', signal '%s'"),
					pid,Server_status(status), Sigstr(finished_server->sigval));
			}
			DEBUG0("Do_queue_jobs: no status to update");
			continue;
		}

		/* force rereading the destination information */
		Get_job_control( cfp, 0 );
		dpp = ((struct destination *)cfp->destination_list.list);
		for( j = 0; j < cfp->destination_list.count; ++j ){
			if( dpp[j].subserver == pid ){
				destination = &dpp[j];
				break;
			}
		}
		DEBUG0(
		"Do_queue_jobs: subserver pid '%d' for '%s' dest 0x%x '%s'",
		pid, finished_server->transfername, destination,
		destination?destination->identifier:0 );

		if( destination ){
			if( destination->error[0] == 0 && status == JSIGNAL ){
				plp_snprintf( destination->error, sizeof(destination->error),
					"subserver process %d died with signal '%s'",pid,
					Sigstr( finished_server->sigval ) );
			}
			setstatus( cfp,
			_("subserver status '%s' for '%s', destination '%s' copy %d %s"),
				Server_status(status), cfp->identifier+1,
				destination->identifier, destination->copy_done+1,
				destination->error);
		} else {
			if( cfp->error[0] == 0 && status == JSIGNAL ){
				plp_snprintf( cfp->error, sizeof(cfp->error),
					"subserver process %d died with signal '%s'", pid,
					Sigstr( finished_server->sigval ) );
			}
			setstatus( cfp, _("subserver status '%s' for '%s' %s"),
				Server_status(status), cfp->identifier+1,cfp->error);
		}
		if( status == JSIGNAL ){
			status = JABORT;
		}

		/* and now we allow the server to continue */
		switch( status ){
		case JABORT:
			if( Stop_on_abort ){
				Printing_aborted = 1;
				Set_spool_control( 0, 0 );
				setstatus( cfp, _("stopping printing on filter JABORT exit code") );
				memset(&control_statb, 0, sizeof(control_statb) );
			} else {
				finished_server->status = 0;
			}
			break;
		case JNOPRINT:
			break;
		default:
			finished_server->status = 0;
			break;
		}

done:
		if( cfp == 0 ){
			DEBUG0("Do_queue_jobs: no status to update");
			continue;
		}
		if( destination ){
			destination->subserver = 0;
		}
		cfp->hold_info.server = cfp->hold_info.subserver = 0;
		cfp->hold_info.active_time = 0;

		/* now update the job status */
		switch( status ){
		case JHOLD:
			if( destination ){
				destination->hold_time = time( (void *) 0 );
			} else {
				cfp->hold_info.hold_time = time( (void *)0 );
				cfp->hold_info.priority_time = 0;
			}
			Set_job_control( cfp, (void *)0);
			break;

		case JSUCC:	/* successful, remove job */
			if( destination ){
				++destination->copy_done;
				if( destination->copy_done >= destination->copies ){
					destination->done_time = time( (void *) 0 );
				}
				Set_job_control( cfp, (void *)0);
			} else {
				cfp->hold_info.done_time = time( (void *)0 );
				cfp->hold_info.priority_time = 0;
				i = cfp->hold_info.redirect[0];
				cfp->hold_info.redirect[0] = 0;
				Set_job_control( cfp, (void *)0);
				if( i ){
					setmessage( cfp, "TRACE", "%s@%s: job moved", Printer, FQDNHost );
				} else {
					setmessage( cfp, "TRACE", "%s@%s: job printed", Printer, FQDNHost );
					Sendmail_to_user( status, cfp, pc );
				}
				if( !Save_when_done ){
					if( Remove_job( cfp ) ){
						setstatus( cfp, _("could not remove job '%s'"), cfp->identifier+1 );
						status = JABORT;
						Errorcode = JABORT;
					} else {
						setstatus( cfp, _("job '%s' removed"), cfp->identifier+1 );
					}
				} else {
					setmessage( cfp, "TRACE", "%s@%s: job NOT removed, save_when_done set",
						Printer, FQDNHost );
				}
			}
			break;

		case JFAIL:	/* failed, retry ?*/
			if( destination ){
				err = ++destination->attempt;
			} else {
				err = ++cfp->hold_info.attempt;
			}
			DEBUG0( "Do_queue_jobs: JFAIL - attempt %d, max %d",
				err, Send_try );
			Set_job_control( cfp, (void *)0);
			if( Send_try && err >= Send_try ){
				char buf[60];
				/* check to see what the failure action
				 *	should be - abort, failure; default is remove
				 */
				setstatus( cfp, _("job '%s', attempt %d, allowed %d"),
					cfp->identifier+1, err, Send_try );
				status = Decode_transfer_failure( err, cfp, status );
				switch( status ){
				case JSUCC:   strv = _("treating as successful"); break;
				case JFAIL:   strv = _("retrying job"); break;
				case JABORT:  strv = _("aborting server"); break;
				case JREMOVE: strv = _("removing job"); break;
				case JHOLD:   strv = _("holding job"); break;
				default:
					plp_snprintf( buf, sizeof(buf),
						_("unexpected status 0x%x"), status );
					strv = buf;
					break;
				}
				setstatus( cfp, _("job '%s', %s"), cfp->identifier+1, strv );
			} else {
				setstatus( cfp, _("job '%s' attempt %d of %d, retrying"),
					cfp->identifier+1, err, Send_try );
			}
			/* retry only if specified */
			if( status == JFAIL ){
				if( Connect_interval > 0 ){
					int n = (10 * (1 << (err - 1)));
					if( Max_connect_interval && n > Max_connect_interval ){
						n = Max_connect_interval;
					}
					setstatus( cfp,
                       _("sleeping %d secs before retry, starting sleep"), n );
					plp_sleep( n );
				}
			} else {
				goto done;
			}
			break;


		case JNOPRINT:	/* no printing */
			cfp->hold_info.priority_time = 0;
			Set_job_control( cfp, (void *)0);
			if( destination ){
				if( destination->error[0] ){
				setstatus( cfp, "job '%s', destination '%s', error '%s'",
					cfp->identifier+1,destination->identifier+1,destination->error );
				}
			} else {
				if( cfp->error[0] ){
				setstatus( cfp, "job '%s' error '%s'",cfp->identifier+1, cfp->error );
				}
			}
			setstatus( cfp, "job '%s' error 'JNOPRINT'",cfp->identifier+1 );
			Printing_disabled = 1;
			Set_spool_control( 0, 0 );
			break;

		case JFAILNORETRY:	/* do not try again */
			cfp->hold_info.priority_time = 0;
			if( destination ){
				if( destination->error[0] == 0 ){
					plp_snprintf( destination->error, sizeof(destination->error),
						_("failed, no retry") );
				}
				Set_job_control( cfp, (void *)0);
				setstatus( cfp, "job '%s', destination '%s', error '%s'",
					cfp->identifier+1,destination->identifier+1,destination->error );
			} else {
				if( cfp->error[0] == 0 ){
					plp_snprintf( cfp->error, sizeof(cfp->error), _("failed, no retry") );
				}
				Set_job_control( cfp, 0);
				setstatus( cfp, "job '%s' error '%s'",cfp->identifier+1, cfp->error );
				Sendmail_to_user( status, cfp, pc );
			}
			if( !Save_on_error ){
				setstatus( cfp, _("removing job '%s'"), cfp->identifier+1 );
				Remove_job( cfp );
			}
			break;

		default:
			/* fall through to JABORT */
		case JABORT:	/* abort, do not try again */
			cfp->hold_info.priority_time = 0;
			if( destination ){
				if( destination->error[0] == 0 ){
					plp_snprintf( destination->error, sizeof(destination->error),
						_("aborting operations") );
				}
				Set_job_control( cfp, (void *)0);
				setstatus( cfp, "job '%s', destination '%s', error '%s'",
					cfp->identifier+1,destination->identifier+1,destination->error );
			} else {
				if( cfp->error[0] == 0 ){
					plp_snprintf( cfp->error, sizeof(cfp->error), _("aborting operations") );
				}
				Set_job_control( cfp, 0);
				setstatus( cfp, "job '%s' error '%s'",cfp->identifier+1, cfp->error );
				Sendmail_to_user( status, cfp, pc );
			}
			if( !Save_on_error ){
				setstatus( cfp, _("removing job '%s'"), cfp->identifier+1 );
				Remove_job( cfp );
			}
			break;

		case JREMOVE:	/* failed, remove job */
			status = JSUCC;
			if( destination ){
				if( destination->error[0] == 0 ){
				plp_snprintf( destination->error,
					sizeof(destination->error),
					_("removing destination '%s'"), destination->identifier+1 );
				}
				destination->done_time = time( (void *)0 );
				Set_job_control( cfp, (void *)0);
			} else {
				/* get the error message */
				if( cfp->error[0] == 0 ){
					plp_snprintf( cfp->error,
						sizeof(cfp->error),
						_("removing job '%s'"), cfp->identifier+1 );
				}
				cfp->hold_info.done_time = time( (void *)0 );
				Set_job_control( cfp, (void *)0);
				Sendmail_to_user( status, cfp, pc );
				if( !Save_on_error ){
					setstatus( cfp, _("removing job '%s'"), cfp->identifier+1 );
					Remove_job( cfp );
				}
			}
			break;
		}
	}
	Errorcode = 0;
	DEBUG0("Do_queue_jobs: finished");
	/* now we write out the Server_order information */
	/* update the server order in the spool control file */
	if( Server_names && *Server_names ){
		int len;
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
		Server_order = buffer;
		Set_spool_control( 0, 0 );
	}
	if( did_work ) setstatus(0,_("server finished"));
	Errorcode = 0;
	cleanup(0);
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
	if( RemotePrinter && RemotePrinter[0] == 0 ) RemotePrinter = 0;
	status = Send_job( RemotePrinter?RemotePrinter:Printer, RemoteHost, cfp,
		SDpathname, Connect_timeout, Connect_interval,
		Max_connect_interval, Send_job_rw_timeout, pc );
	DEBUG0("Remote_job: %s, status '%s'", cfp->transfername,
		Link_err_str(status) );
	switch( status ){
	case JSUCC:
	case JABORT:
	case JFAIL:
	case JREMOVE:
		break;
	case LINK_ACK_FAIL:
		setstatus(cfp,_("link failure while sending job '%s'"),
			cfp->identifier+1 );
		status = JFAIL;
		break;
	case LINK_PERM_FAIL:
		setstatus( cfp, _("no permission to spool job '%s'"), cfp->identifier+1 );
		status = JREMOVE;
		break;
	default:
		setstatus( cfp, _("failed to send job '%s'"), cfp->identifier+1 );
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
	server_info->pid = dofork(1);
	if( server_info->pid < 0 ){
		logerr( LOG_ERR, _("Fork_subserver: fork failed") );
	}
	return( server_info->pid );
}

/***************************************************************************
 * Get_subserver_info()
 *  hack up the server information list into a list of servers
 ***************************************************************************/

void Get_subserver_info( struct malloc_list *servers, char *snames)
{
	char *end, *s;
	int c, cnt;
	struct server_info *server_info, *server;
	char names[LINEBUFFER];

	cnt = 0;
	safestrncpy( names, snames );
	for( s = names; s && *s; s = strpbrk( s, ":;, \t" ) ){
		++s;
		++cnt;
	}
	if( cnt+1 >= servers->max ){
		extend_malloc_list( servers, sizeof( server_info[0] ), cnt+1,__FILE__,__LINE__  );
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
			safestrncpy( server->name, s );
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
	int status, i, sigval = 0;
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

	DEBUG0( "Wait_for_subservers: checking" );

	/* now we check for processes that have exited
	 *  -1 - error (ECHILD means no children)
	 *   0 - when WNOHANG means no child status available
	 *   pid - pid of exited child
	 */

	/* unblock signals and wait for change */
	if( Susr2 ){
		goto done;
	}

	/* we need to unblock block and wait for change */
	pid = plp_waitpid( -1, &procstatus, WNOHANG );
	if( pid == 0 ){
		Setup_waitpid_break(0);
		Sig_break = 1;
		(void) plp_signal_break(SIGUSR2,  (plp_sigfunc_t)Sigusr2);
		DEBUG0( "Wait_for_subservers: blocking!" );
		plp_sigpause();
		DEBUG0( "Wait_for_subservers: got signal!" );
		pid = plp_waitpid( -1, &procstatus, WNOHANG );
	}
	err = errno;
	Sig_break = 0;
	Setup_waitpid();
	(void) plp_signal(SIGUSR2,  (plp_sigfunc_t)Sigusr2);

	DEBUG0( "Wait_for_subservers: pid %d, errno '%s', status '%s'",
		pid, Errormsg(err), Decode_status( &procstatus ) );

	if( pid == -1 ){
		if( err == ECHILD ){
			DEBUG0( "Wait_for_subservers: all servers have finished" );
		}
		goto done;
	} else if( pid ){
		if( WIFSIGNALED( procstatus ) ){
			sigval = WTERMSIG( procstatus );
			DEBUG0("Wait_for_subservers: pid %d terminated by signal '%s'",
				pid, Sigstr( sigval ) );
			switch( sigval ){
			/* generated by the program */
			case 0:
			case SIGINT:
			case SIGKILL:
			case SIGQUIT:
			case SIGTERM:
			case SIGUSR1:
			case SIGUSR2:
				status = JFAIL;
				break;
			default:
				status = JSIGNAL;
				break;
			}
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
				found->sigval = sigval;
				DEBUG0( "Wait_for_subservers: server '%s' for '%s' finished",
					found->name, server_info[i].transfername );
			}
		}
		if( found ){
			/* sort server order */
			if( Mergesort( server_info, servers->count,
				sizeof( server_info[0] ), cmp_server ) ){
				fatal( LOG_ERR, _("Wait_for_subservers: Mergesort failed") );
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
	int len, l;
	char line[LARGEBUFFER];

	if( stat( dest, &statb ) == 0 && unlink( dest ) == -1 ){
		logerr(LOG_ERR,
			_("link_or_copy: cannot unlink '%s'"),dest);
		return(1);
	}
	if( link( src, dest ) == -1 ){
		DEBUG0("link_or_copy: link failed '%s'", Errormsg(errno) );
		/* try copying then */
		srcfd = Checkread( src, &statb );
		if( srcfd <= 0 ){
			logerr( LOG_ERR, _("link_or_copy: cannot open for reading '%s'"),
				src);
			return(1);
		}
		destfd = Checkwrite( dest, &wstatb, O_RDWR|O_TRUNC, 1, 0 );
		if( destfd <= 0 ){
			logerr( LOG_ERR, _("link_or_copy: cannot open for writing '%s'"),
				dest );
			return(1);
		}
		for( l = len = statb.st_size;
			len > 0 && (l = read( srcfd, line, sizeof(line))) > 0;
			len -= l ){
			if( Write_fd_len( destfd, line, l ) < 0 ){
				logerr(LOG_ERR,
					_("link_or_copy: write failed to '%s'"),dest);
				return(1);
			}
		}
		if( l < 0  ){
			logerr(LOG_ERR,_("link_or_copy: read failed '%s'"),src);
			return(1);
		}
		close( destfd );
	}
	return(0);
}

/***************************************************************************
 * Copy_job:
 *  subserver gets a new job
 *  We have the server copy the job into the new location.
 *  When we do this copy,  we need to do the full job transfer protocol -
 *   1. check for duplicate job numbers
 *   2. rename the files if necessary
 *   3. fix up the control file
 *  We do this to handle the problem when an error occurred during a job
 *  and the body is still there.
 ***************************************************************************/

static int Copy_job(struct control_file *cfp, struct server_info *server )
{
	int i;						/* ACME Indexes! Gives you the finger! */
	struct data_file *data_file;	/* data files in job */
	struct stat statb;				/* status buffer */
	char **lines, *src, *dest;	/* source and destination file */
	struct control_file new_cfp;
	int err = 0;
	int hold_fd, fd;

	/* first,  we find the unique job number */
	new_cfp = *cfp;	/* we get the 'new' control file for the job */
	hold_fd = Find_non_colliding_job_number( &new_cfp, &server->controldir );
	DEBUG0("Copy_job: fd %d, hold file '%s'", hold_fd, new_cfp.hold_file );
	if( hold_fd < 0 ){
		Errorcode = JABORT;
		logerr_die( LOG_ERR, "Copy_job: cannot create hold file" );
	}
	if( Fix_data_file_info( &new_cfp ) ){
		Errorcode = JABORT;
		fatal( LOG_ERR, "Copy_job: %s", new_cfp.error );
	}
	/* we now can do the setup of the process */
	DEBUG0("Copy_job: printer '%s', job '%s' copying data files",
		Printer, cfp->identifier+1 );
	data_file = (void *)cfp->data_file_list.list;
	for( i = 0; i < cfp->data_file_list.count; ++i ){
		if( data_file[i].is_a_copy ) continue;
		src = data_file[i].openname;
		if( src[0] == 0 ) continue;
		if( stat( src, &statb ) == -1 ){
			Errorcode = JABORT;
			logerr_die( LOG_ERR, _("Copy_job: cannot stat '%s'"), src );
		}
		dest = Add_path( &server->spooldir, data_file[i].cfline+1 );
		DEBUG0("Copy_job: data file src '%s' dest '%s'", src, dest );
		if( (err = link_or_copy( src, dest )) ){
			Errorcode = JABORT;
			logerr_die( LOG_ERR, _("Copy_job: cannot transfer '%s' to '%s'"),
				src, dest );
		}
	}

	/* we now transfer the control file */
	dest = Add_path( &server->spooldir, new_cfp.transfername );
	DEBUG0("Copy_job: control file src '%s' dest '%s'", cfp->transfername,
		dest );
	fd = Checkwrite( dest, &statb, 0, 1, 0 );
	if( fd < 0 ){
		Errorcode = JABORT;
		logerr_die( LOG_ERR, _("Copy_job: cannot open '%s'"), dest );
	}
	DEBUG1("Copy_job: opened '%s' fd %d", dest, fd );
	if( ftruncate( fd, 0 ) == -1 ){
		Errorcode = JABORT;
		logerr_die( LOG_ERR, _("Copy_job: cannot truncate '%s'"), dest );
	}

	/* now we need to write the control file lines out, one by one.
	 * Note that we have already modified the data file lines above
	 */

	lines = cfp->control_file_lines.list;
	for( i = 0; i < cfp->control_file_lines.count; ++i ){
		if( lines[i] && lines[i][0] ){
			if( Write_fd_str( fd, lines[i] ) < 0
				|| Write_fd_str( fd, "\n" ) < 0 ){
				Errorcode = JABORT;
				logerr_die( LOG_ERR, _("Copy_job: cannot write fd %d, '%s'"),
					fd, dest );
			}
		}
	}
	close( fd );
	DEBUG0("Copy_job: finished writing control file to '%s'", dest);
	close( hold_fd );

	/* restore information */
	if( Fix_data_file_info( cfp ) ){
		Errorcode = JABORT;
		fatal( LOG_ERR, "Copy_job: %s", cfp->error );
	}

	setmessage(cfp,"TRACE","%s@%s: job copied to %s, id %d",
		Printer,FQDNHost, server->name, new_cfp.number );
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
				logerr_die( LOG_ERR, _("Decode_transfer: pipe failed") );
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
		 	status = Close_filter( 0, &As_fd_info, 0, "transfer failure" );
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
	} else {
		status = JREMOVE;
	}
	DEBUG0("Decode_transfer_failure: returning '%s'", Server_status(status) );
	return( status );
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
	int i, j, count;		/* AJAX! for integers */
	struct server_info *server_info;	/* server information */
	struct control_file *cfp, **cfpp;	/* pointer to control file */
	struct destination *dest;		/* destination */
	char msg[LINEBUFFER];

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
			DEBUG0("Update_servers_status: printer '%s' SD '%s' CD '%s'",
				Printer, Clear_path(SDpathname) );
			Init_path(&server_info[i].spooldir,
				Clear_path(SDpathname));
			Init_path(&server_info[i].controldir,
				Clear_path(CDpathname));
			count = 0;
			noprint = (Printing_disabled || Printing_aborted || Spooling_disabled);
			DEBUG0( "Update_servers_status: printing_disabled", noprint );
			if( noprint ){
				server_info[i].pid = -1;
				continue;
			}
			count = 0;
			cfpp = (void *)C_files_list.list;
			for( j = 0; j < C_files_list.count; ++j ){
				cfp = cfpp[j];
				if( Job_printable_status( cfp, &dest, msg, sizeof(msg) ) == 0 ){ 
					++count;
				}
			}
			DEBUG0( "Update_servers_status: job count %d, Check_idle '%s'",
				count, Check_idle );
			if( count == 0 && Check_idle == 0 ){
				continue;
			}
			DEBUG0( "Update_servers_status: update - starting '%s'",
				Printer );
			if( Fork_subserver( &server_info[i] ) == 0 ){
				return(Printer);
			}
		}
	}
	return(0);
}

/***************************************************************************
 * Do_check_idle()
 *  execute the idle check of the printer indicated by the
 *  check_idle printcap entry:
 *   check_idle=/script
 ***************************************************************************/

int Do_check_idle(struct printcap_entry *pc)
{
	struct control_file cfp;
	int status;
	
	DEBUG0( "Do_check_idle: Check_idle '%s'", Check_idle );

	memset( &cfp, 0, sizeof(cfp) );
	setstatus(0,_("checking for idle using %s"), Check_idle );
	if( Make_filter( 'f', &cfp, &XF_fd_info, Check_idle,
		0, /* no extra */
		0,	/* RW pipe */
		1, /* dup to fd 1 */
		pc, /* printcap information */
		0, 0, Logger_destination != 0, 0 ) ){
		Errorcode = JABORT;
		fatal( LOG_ERR, "Do_check_idle: failed '%s'", cfp.error );
	}
	status = Close_filter( 0, &XF_fd_info, 0, "check idle" );
	DEBUG0("Do_check_idle: exit status '%s'",
		Server_status(status) );
	switch( status ){
	case JSUCC:
	case JNOSPOOL:
	case JNOPRINT:
		break;
	default:
		status = JABORT;
		break;
	}
	DEBUG0("Do_check_idle: returning status '%s'",
		Server_status(status) );
	setstatus(0,_("idle status %s"), Server_status(status));
	return( status );
}
