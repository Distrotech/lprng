/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: lpd_jobs.c,v 5.5 1999/10/16 22:06:46 papowell Exp papowell $";

#include "lp.h"
#include "lpd.h"

#include "accounting.h"
#include "child.h"
#include "errorcodes.h"
#include "fileopen.h"
#include "gethostinfo.h"
#include "getopt.h"
#include "getprinter.h"
#include "getqueue.h"
#include "linksupport.h"
#include "lockfile.h"
#include "lpd_remove.h"
#include "merge.h"
#include "permission.h"
#include "printjob.h"
#include "proctitle.h"
#include "sendjob.h"
#include "sendmail.h"
#include "stty.h"

#include "lpd_jobs.h"

/**** ENDINCLUDE ****/

/***************************************************************************
 * Commentary:
 * Patrick Powell Thu May 11 09:26:48 PDT 1995
 * 
 * Job processing algorithm
 * 
 * 1. Check to see if there is already a spooler process active.
 *    The active file will contain the PID of the active spooler process.
 * 2. Build the job queue
 * 3. For each job in the job queue, service them.
 * 
 * MULTIPLE Servers for a Single Queue.
 * In the printcap, the "sv" flag sets the Server_names_DYN variable with
 * the list of servers to be used for this queue.  The "ss"  flag sets
 * the Server_queue_name_DYN flag with the queue that this is a server for.
 * 
 * Under normal conditions, the following process hierarchy is used:
 * 
 *  server process - printer 'spool queue'
 *     subserver process - first printer in 'Server_name'
 *     subserver process - second printer in 'Server_name'
 *     ...
 * 
 * The server process does the following:
 *   for each printer in the Server_name list
 *      sort them by the last order that you had in the control file
 *   for each printer in the Server_name list
 *      check the status of the queue, and gets the control file
 *      information and the numbers of jobs waiting.
 *   for each printer in the Server_name list
 *      if printable jobs and printing enabled
 *      start up a subserver
 * 
 *   while(1){
 * 	for all printable jobs do
 * 		check to see if there is a queue for them to be
 * 		printed on and the the queue is not busy;
 * 	if( job does not need a server ) then
 * 		do whatever is needed;
 * 		update job status();
 * 	else if( no jobs to print and no servers active ) then
 * 		break;
 * 	else if( no jobs to print or server active ) then
 * 		wait for server to exit();
 * 		if( pid is for server printing job)
 * 			update job status();
 * 			update server status();
 * 	else
 * 		dofork(0) a server process;
 * 		record pid of server doing work;
 * 	endif
 *   }
 *     
 * We then check to see if we are a slave (sv) to a master spool queue;
 * if we are and we are not a child process of the 'master' server,
 * we exit.
 * 
 * Note: if we spool something to a slave queue,  then we need to start
 * the master server to make the slave printer work.
 * 
 * Note: the slave queue processes
 * will not close the masters lock files;  this means a new master
 * cannot start serving the queue until all the slaves are dead.
 * Why this action do you ask?  The reason is that it is difficult
 * for a new master to inherit slaves from a dead master.
 * 
 * It turns out that many implementations of some network
 * based databased systems and other network database routines are broken,
 * and have memory leaks or never close file descriptors.  Up to the point
 * where the loop for checking the queue starts,  there is a known number
 * of file descriptors open,  and dynamically allocated memory.
 * After this,  it is difficult to predict just what is going to happen.
 * By forking a 'subserver' process, we firewall the actual memory and
 * file descriptor screwups into the subserver process.
 * 
 * When the subserver exits, it returns an error code that the server
 * process then interprets.  This error code is used to either remove the job
 * or retry it.
 * 
 * Note that there are conditions under which a job cannot be removed.
 * We simply abort at that point and let higher level authority (admins)
 * deal with this.
 * 
 ***************************************************************************/


/*
 * Signal handler to set flags and terminate system calls
 */

 static int Susr1, Chld;

 static void Sigusr1(void)
{
	++Susr1;
	(void) plp_signal_break(SIGUSR1,  (plp_sigfunc_t)Sigusr1);
	return;
}

 static void Sigchld(void)
{
	++Chld;
	signal( SIGCHLD, SIG_DFL );
	return;
}


/***************************************************************************
 * Update_spool_info()
 *  get updated spool control file information
 ***************************************************************************/


void Update_spool_info( struct line_list *sp )
{
	struct line_list info;
	char *sd, *pr, *hf_name, *sc;
	int pid, done_time;

	Init_line_list(&info);
	Move_line_list(&info,sp);

	pr = Find_str_value(&info,PRINTER,Value_sep);
	DEBUG1("Update_spool_info: printer '%s'", pr );
	sd = Find_str_value(&info,SPOOLDIR,Value_sep);
	sc = Find_str_value(&info,QUEUE_CONTROL_FILE,Value_sep);
	pr = Find_str_value(&info,PRINTER,Value_sep);
	hf_name = Find_str_value(&info,HF_NAME,Value_sep);
	pid = Find_flag_value(&info,SERVER,Value_sep);
	done_time = Find_flag_value(&info,DONE_TIME,Value_sep);
	Free_line_list(sp);
	Get_spool_control(sd,sc,pr,sp);
	Set_str_value(sp,SPOOLDIR,sd);
	Set_str_value(sp,QUEUE_CONTROL_FILE,sc);
	Set_str_value(sp,PRINTER,pr);
	Set_str_value(sp,HF_NAME,hf_name);
	Set_decimal_value(sp,SERVER,pid);
	Set_flag_value(sp,DONE_TIME,done_time);
	Free_line_list(&info);
}

int cmp_server( const void *left, const void *right )
{   
    struct line_list *l, *r;
	int tr, tl;
	l = ((struct line_list **)left)[0];
	r = ((struct line_list **)right)[0];
	tl = Find_flag_value(l,DONE_TIME,Value_sep);
	tr = Find_flag_value(r,DONE_TIME,Value_sep);
	if(DEBUGL5)Dump_line_list("cmp_server - l",l);
	if(DEBUGL5)Dump_line_list("cmp_server - r",r);
	DEBUG5("cmp_server: tl %d, tr %d, cmp %d", tl, tr, tl - tr );
	return( tl - tr );
}


int Get_subserver_pc( char *printer, struct line_list *l, int done_time )
{
	char *pr, *sd, *s, *scf;
	struct stat statb;
	struct line_list entry, alias;
	int printable, held, move, status, idle;
	char *save_printer;

	save_printer = Printer_DYN;
	Printer_DYN = 0;

	pr = sd = 0;
	printable = held = move = status = idle = 0;
	Init_line_list(&entry);
	Init_line_list(&alias);

	Set_DYN(&Printer_DYN,printer);
	DEBUG1("Get_subserver_pc: '%s'", printer );
	if( !(pr = Select_pc_info(printer,&alias,&entry, &PC_names_line_list,
			&PC_order_line_list, &PC_info_line_list, 0)) ){
		logmsg(LOG_ERR,"Get_subserver_pc: cannot find load balance queue '%s'", printer );
		status = 1;
		goto error;
	}
	Set_DYN(&Printer_DYN, pr);
	Set_str_value(l,PRINTER,pr);
	if( done_time == 0 ){
		done_time = time((void *)0);
	}
	Set_flag_value(l,DONE_TIME,done_time);
	if( (s = Find_str_value(&entry,"check_idle",Value_sep))
    	|| (s = Find_str_value(&Config_line_list,"check_idle",Value_sep)) ){
		idle = 1;
		DEBUG1("Get_subserver_pc: check_idle '%s'", s);
	}

	/* get Spool_dir */
	sd = Find_str_value(&entry,SD,Value_sep);
	if( !sd ) sd = Find_str_value(&Config_line_list,SD,Value_sep);
	if( !sd ) sd = Find_default_var_value( &Spool_dir_DYN );
	/* expand the spool queue string */
	sd = safestrdup(sd,__FILE__,__LINE__);
	Expand_percent( &sd );

	DEBUG1("Get_subserver_pc: spool dir '%s'", sd );
	/* get the spool queue */
	if( sd == 0 || stat(sd,&statb) == -1 || !S_ISDIR(statb.st_mode) ){
		status = 1;
		logmsg(LOG_ERR,"Get_subserver_pc: no spool queue for '%s'", sd );
		goto error;
	}
	Set_str_value(l,SPOOLDIR,sd);

	scf = Find_str_value(&entry,QUEUE_CONTROL_FILE,Value_sep);
	if( !scf ) scf = Find_str_value(&Config_line_list,QUEUE_CONTROL_FILE,Value_sep);
	if( !scf ) scf = Find_default_var_value( &Queue_control_file_DYN );
	scf = safestrdup(scf,__FILE__,__LINE__);
	Expand_percent( &scf );
	Set_str_value(l,QUEUE_CONTROL_FILE,scf);

	Update_spool_info( l );

	printable = held = move = 0;
	Free_line_list(&entry);
	DEBUG1("Get_subserver_pc: scanning '%s'", sd );
	Scan_queue(sd, l, &entry, &printable, &held, &move, 1);
	Set_flag_value(l,PRINTABLE,printable);
	Set_flag_value(l,HELD,held);
	Set_flag_value(l,MOVE,move);
	Set_flag_value(l,IDLE,idle);
	DEBUG1("Get_subserver_pc: printable %d, held %d, move %d",
		printable, held, move );


 error:

	Free_line_list(&alias);
	Free_line_list(&entry);
	if(sd)free(sd); sd = 0;
	if(scf)free(scf); scf = 0;
	if( Printer_DYN ) free(Printer_DYN); Printer_DYN = save_printer;
	return(status);
}

/***************************************************************************
 * Get_subserver_info()
 *  hack up the server information list into a list of servers
 ***************************************************************************/


void Dump_subserver_info( char *title, struct line_list *l)
{
	char buffer[LINEBUFFER];
	int i;
	logDebug("*** Dump_subserver_info: '%s' - %d subservers",
		title,	l->count );
	for( i = 0; i < l->count; ++i ){
		plp_snprintf(buffer,sizeof(buffer),"server %d",i);
		Dump_line_list_sub(buffer,(struct line_list *)l->list[i]);
	}
}

void Get_subserver_info( struct line_list *order,
	char *list, char *old_order)
{
	struct line_list server_order, server, *pl;
	int i, j;
	char *s;

	Init_line_list(&server);
	Init_line_list(&server_order);

	DEBUG1("Get_subserver_info: servers '%s'",list);
	DEBUG1("Get_subserver_info: order '%s'",old_order);
	Split(&server,list,           File_sep,1,File_sep,    1,1,0);
	Split(&server_order,old_order,File_sep,0,0,           0,1,0);

	/* get the info of printers */
	for( i = 0; i < server.count; ++i ){
		s = server.list[i];
		for( j = 0; j < server_order.count; ++j ){
			if( !safestrcasecmp(s,server_order.list[j]) ){
				break;
			}
		}
		++j;
		Check_max(order,1);
		pl = malloc_or_die(sizeof(pl[0]),__FILE__,__LINE__);
		memset(pl, 0, sizeof(pl[0]));
		if( Get_subserver_pc( s, pl, j ) ){
			free(pl); pl = 0;
		} else {
			order->list[order->count++] = (char *)pl;
			pl = 0;
		}
		++j;
	}

	Free_line_list(&server);
	Free_line_list(&server_order);
	if( Mergesort( order->list+1, order->count-1,
		sizeof( order->list[0] ), cmp_server ) ){
		fatal( LOG_ERR,
			_("Get_subserver_info: Mergesort failed") );
	}

	if(DEBUGL1)Dump_subserver_info("Get_subserver_info - starting order",order);
}

/***************************************************************************
 * int Copy_or_link - move or link files
 ***************************************************************************/

int Copy_or_link( char *srcfile, char *destfile )
{
	char buffer[LARGEBUFFER];
	char *path = 0;
	struct stat statb;
	int srcfd, destfd, fail, n, len, count;

	fail = 0;
	srcfd = destfd = -1;

	DEBUG3("Copy_or_link: '%s' to '%s'", srcfile, destfile);
	if( link( srcfile, destfile ) == -1 ){
		DEBUG3("Copy_or_link: link '%s' to '%s' failed, '%s'",
			srcfile, destfile, Errormsg(errno) );
		path = safestrdup2(destfile,".x",__FILE__,__LINE__);
		destfd = Checkwrite(path,&statb,0,1,0);
		if( destfd < 0 ){
			logerr(LOG_INFO,"Copy_or_link: open '%s' failed", path );
			fail = 1;
		}
		srcfd = Checkread(srcfile, &statb );
		if( srcfd < 0 ){
			logerr(LOG_INFO,"Copy_or_link: open '%s' failed", srcfile );
			fail = 1;
		}
		while( !fail && (n = read(srcfd,buffer,sizeof(buffer))) > 0 ){
			for( count = len = 0; len < n
				&& (count = write(destfd, buffer+len,n-len)) > 0;
				len += count );
			if( count < 0 ) fail = 1;
		}
		if( srcfd >= 0 ) close(srcfd); srcfd = -1;
		if( destfd >= 0 ) close(destfd); destfd = -1;

		if( !fail && link( path, destfile ) == -1 ){
			logerr(LOG_INFO,"Copy_or_link: link '%s' to '%s' failed, '%s'",
			path, destfile, Errormsg(errno) );
			fail = 1;
		}
	}
	if( path ) free(path); path = 0;
	return( fail );
}

/***************************************************************************
 * Do_queue_jobs: process the job queue
 ***************************************************************************/

int Do_queue_jobs( char *name, int subserver, int idle_check )
{
	int master = 0;		/* this is the master */
	int lock_fd;	/* fd for files */
	char buffer[SMALLBUFFER];
	char *path, *s, *id, *tempfile, *transfername, *openname,
		*new_dest, *move_dest, *pr, *hf_name, *sd, *from;
	struct stat statb;
	int i, j, mod, fd, pid, printable, held, move, destinations,
		destination, use_subserver, job_to_do, working, printing_enabled,
		all_done, fail, idle, job_index;
	struct line_list servers, tinfo, *sp, *datafile;
	plp_block_mask oblock;
	struct job job;

	Init_line_list(&tinfo);

	lock_fd = -1;

	Init_job(&job);
	Init_line_list(&servers);
	id = transfername = 0;

	Name = "(Server)";
	Set_DYN(&Printer_DYN,name);
	DEBUG1("Do_queue_jobs: called with name '%s', subserver %d, idle_check %d",
		Printer_DYN, subserver, idle_check );
	name = Printer_DYN;

	if(DEBUGL4){ int fdx; fdx = dup(0); logDebug("Do_queue_jobs: start next fd %d",fdx); close(fdx); };

 begin:
	Set_DYN(&Printer_DYN,name);
	DEBUG1("Do_queue_jobs: begin name '%s'", Printer_DYN );
	tempfile = 0;
	Free_listof_line_list( &servers );
	if( lock_fd != -1 ) close( lock_fd ); lock_fd = -1;

	/* block signals */
	plp_block_one_signal(SIGCHLD, &oblock);
	plp_block_one_signal(SIGUSR1, &oblock);
	(void) plp_signal(SIGCHLD,  SIG_DFL);
	(void) plp_signal(SIGUSR1,  (plp_sigfunc_t)Sigusr1);

	Errorcode = JABORT;
	/* you need to have a spool queue */
	if( Setup_printer( Printer_DYN, buffer, sizeof(buffer) ) ){
		cleanup(0);
	}
	if(DEBUGL4){ int fdx; fdx = dup(0);
	logDebug("Do_queue_jobs: after Setup_printer next fd %d",fdx); close(fdx); };

	setproctitle( "lpd %s '%s'", Name, Printer_DYN );
	path = Make_pathname( Spool_dir_DYN, Queue_lock_file_DYN );
	DEBUG1( "Do_queue_jobs: checking lock file '%s'", path );
	lock_fd = Checkwrite( path, &statb, O_RDWR, 1, 0 );
	if( lock_fd < 0 ){
		logerr_die( LOG_ERR, _("Do_queue_jobs: cannot open lockfile '%s'"),
			path ); 
	}
	pid = Read_pid( lock_fd, (char *)0, 0 );
	DEBUG1( "Do_queue_jobs: last server '%d'", pid );
	if( Do_lock( lock_fd, 0 ) <= 0 ){
		DEBUG1( "Do_queue_jobs: server process '%d' may be active", pid );
		Errorcode = 0;
		if( pid > 0 ) kill( pid, SIGUSR1 );
		if(path) free(path); path = 0;
		cleanup(0);
	}
	pid = getpid();
	DEBUG1( "Do_queue_jobs: writing lockfile with pid '%d'", pid );
	Write_pid( lock_fd, pid, (char *)0 );
	DEBUG1("Do_queue_jobs: lock_fd fd %d", lock_fd );
	DEBUG1( "Do_queue_jobs: got lock file '%s'", path );
	if(path) free(path); path = 0;

 again:

	if( Log_file_DYN ){
		fd = Trim_status_file( Log_file_DYN, Max_log_file_size_DYN,
			Min_log_file_size_DYN );
		if( fd > 0 && fd != 2 ){
			dup2(fd,2);
			close(fd);
		}
	}
	s = Find_str_value(&Spool_control,DEBUG,Value_sep);
	if(!s) s = New_debug_DYN;
	Parse_debug( s, 0);

	if(DEBUGL4){ int fdx; fdx = dup(0);
		logDebug("Do_queue_jobs: after logfile next fd %d",fdx); close(fdx); };


	if( Server_queue_name_DYN ){
		if( subserver == 0 ){
			/* you really need to start up the master queue */
			name = Server_queue_name_DYN;
			DEBUG1("Do_queue_jobs: starting up master queue '%s'", name );
			goto begin;
		}
		Name = "(Sub)";
	}


	if(DEBUGL4){ int fdx; fdx = dup(0);
		logDebug("Do_queue_jobs: after unspooler next fd %d",fdx); close(fdx); };

	/* we now do our idle check */
	idle = 0;
	DEBUG1("Do_queue_jobs: Check_idle_DYN '%s', idle_check %d",
		Check_idle_DYN, idle_check );
	if( Check_idle_DYN || idle_check ){
		idle = Do_check_idle();
		/* exit if not idle */
		if( idle_check || idle ){
			Errorcode = idle;
			cleanup(0);
		}
	}

	/* set up the server name information */
	Check_max(&servers,1);
	sp = malloc_or_die(sizeof(sp[0]),__FILE__,__LINE__);
	memset(sp,0,sizeof(sp[0]));
	Set_str_value(sp,PRINTER,Printer_DYN);
	Set_str_value(sp,SPOOLDIR,Spool_dir_DYN);
	Set_str_value(sp,QUEUE_CONTROL_FILE,Queue_control_file_DYN);
	servers.list[servers.count++] = (char *)sp;
	Update_spool_info(sp);

	master = 0;
	if( Server_names_DYN ){
		DEBUG1( "Do_queue_jobs: Server_names_DYN '%s', Server_order '%s'",
			Server_names_DYN, Srver_order(&Spool_control) );
		if( Server_queue_name_DYN ){
			Errorcode = JABORT;
			fatal(LOG_ERR,"Do_queue_jobs: serving '%s' and subserver for '%s'",
				Server_queue_name_DYN, Server_names_DYN );
		}
		Get_subserver_info( &servers,
			Server_names_DYN, Srver_order(&Spool_control) );
		master = 1;
		/* start the queues that need it */
		for( i = 1; i < servers.count; ++i ){
			sp = (void *)servers.list[i];
			pr = Find_str_value(sp,PRINTER,Value_sep);
			DEBUG1("Do_queue_jobs: subserver '%s' checking for independent action", pr );
			if( (s = Find_str_value(sp,SERVER,Value_sep)) ){
				DEBUG1("Do_queue_jobs: subserver '%s' active server '%s'", pr,s );
			}
			printable = !Pr_disabled(sp) && !Pr_aborted(sp);
			move = Find_flag_value(sp,MOVE,Value_sep);
			DEBUG1("Do_queue_jobs: subserver '%s', printable %d, move %d, idle %d",
				pr, printable, move );
			if( printable || move ){
				pid = Fork_subserver( &servers, i, 0 );
			}
		}
	}


	if(DEBUGL3)Dump_subserver_info("Do_queue_jobs - after setup",&servers);

	if(DEBUGL4){ int fdx; fdx = dup(0);
		logDebug("Do_queue_jobs: after subservers next fd %d",fdx);close(fdx);};
	/* get new job values */
	if( Scan_queue( Spool_dir_DYN, &Spool_control, &Sort_order,
			&printable, &held, &move, 1) ){
		logerr_die( LOG_ERR,"Do_queue_jobs: cannot read queue '%s'",
			Spool_dir_DYN );
	}

	DEBUG1( "Do_queue_jobs: printable %d, held %d, move %d",
		printable, held, move );

	if(DEBUGL1){ i = dup(0);
		logDebug("Do_queue_jobs: after Scan_queue next fd %d", i); close(i); }

	/* remove junk fields from job information */
	for( i = 0; i < Sort_order.count; ++i ){
		/* fix up the sort stuff */
		Free_job(&job);
		if( (s = safestrchr(Sort_order.list[i], ';')) ){
			Split(&job.info,s+1,";",1,Value_sep,1,1,1);
		}
		if(DEBUGL3)Dump_job("Do_queue_jobs - info", &job);

		/* debug output */
		mod = 0;
		if( Find_flag_value(&job.info,SERVER,Value_sep ) ){
			Set_decimal_value(&job.info,SERVER,0);
			mod = 1;
		}
		if( !Find_str_value(&job.info,OPENNAME,Value_sep ) ){
			transfername = Find_str_value(&job.info,TRANSFERNAME,Value_sep);
			Set_str_value(&job.info,OPENNAME,transfername);
			mod = 1;
		}
		if((destinations = Find_flag_value(&job.info,DESTINATIONS,Value_sep))){
			if( Find_str_value(&job.info,DESTINATION,Value_sep ) ){
				Set_str_value(&job.info,DESTINATION,0);
				mod = 1;
			}
			for( j = 0; j < destinations; ++j ){
				Get_destination(&job,j);
				if( Find_flag_value(&job.destination,SERVER,Value_sep) ){
					mod = 1;
					Set_decimal_value(&job.destination,SERVER,0);
					Update_destination(&job);
				}
			}
		}
		if( !(openname = Find_str_value(&job.info,HF_NAME,Value_sep ))
			|| stat(openname,&statb) == -1 ){
			mod = 1;
		}
		if( mod ) Set_hold_file(&job, 0);
	}

	Free_job(&job);
	
	/* get the permissions for this queue */
	Free_line_list(&Perm_line_list);
	Merge_line_list(&Perm_line_list,&RawPerm_line_list,0,0,0);
	if( Perm_filters_line_list.count ){
		Filterprintcap( &Perm_line_list, &Perm_filters_line_list,
		Printer_DYN);
	}


	while(1){
		DEBUG1( "Do_queue_jobs: MAIN LOOP" );
		if(DEBUGL4){ int fdx; fdx = dup(0);
		logDebug("Do_queue_jobs: MAIN LOOP next fd %d",fdx); close(fdx); };
		Unlink_tempfiles();

		/* check for changes to spool control information */
		plp_unblock_all_signals( &oblock);
		plp_set_signal_mask( &oblock, 0 );

		DEBUG1( "Do_queue_jobs: Susr1 before scan %d", Susr1 );
		while( Susr1 ){
			DEBUG1( "Do_queue_jobs: rescanning" );

			Get_spool_control(Spool_dir_DYN, Queue_control_file_DYN,
				Printer_DYN, &Spool_control);
			if( Scan_queue( Spool_dir_DYN, &Spool_control, &Sort_order,
					&printable, &held, &move, 1 ) ){
				logerr_die( LOG_ERR,"Do_queue_jobs: cannot read queue '%s'",
					Spool_dir_DYN );
			}
			DEBUG1( "Do_queue_jobs: printable %d, held %d, move %d",
				printable, held, move );

			for( i = 0; i < servers.count; ++i ){
				sp = (void *)servers.list[i];
				Update_spool_info( sp );
			}
			if(DEBUGL1) Dump_subserver_info( "Do_queue_jobs - rescan",
				&servers );

			Susr1 = 0;
			/* check for changes to spool control information */
			plp_unblock_all_signals( &oblock);
			plp_set_signal_mask( &oblock, 0);
			DEBUG1( "Do_queue_jobs: Susr1 at end of scan %d", Susr1 );
		}

		if(DEBUGL4) Dump_line_list("Do_queue_jobs - sort order printable",
			&Sort_order );

		/* make sure you can print */
		printing_enabled
			= !(Pr_disabled(&Spool_control) || Pr_aborted(&Spool_control));
		DEBUG3("Do_queue_jobs: printing_enabled '%d'", printing_enabled );

		Free_job(&job);
		openname = transfername = hf_name = id = move_dest = new_dest = 0;
		destination = use_subserver = job_to_do = -1;
		working =  destinations = 0;

		for( job_index = 0; job_to_do < 0 && job_index < Sort_order.count;
			++job_index ){
			Free_job(&job);
			openname = transfername = hf_name = id = move_dest = new_dest = 0;
			destination = use_subserver = job_to_do = -1;
			working = destinations = 0;

			if( !(s = Sort_order.list[job_index]) ) continue;
			path = Find_str_in_str(s,HF_NAME,";");
			DEBUG3("Do_queue_jobs: checking path '%s'", path );
			Get_file_image_and_split(0,path,0,0,&job.info,
				Line_ends,1,Value_sep,1,1,1);
			if( path ) free(path); path = 0;
			if( job.info.count == 0 ){
				/* no hold file in spool queue */
				if((s = safestrchr(s,';')) ){
					Split(&job.info,s+1,";",1,Value_sep,1,1,1);
				}
			}

			if( !Find_str_value(&job.info,OPENNAME,Value_sep) ){
				path = Find_str_in_str(s,OPENNAME,";");
				Set_str_value(&job.info,OPENNAME,path);
				if( path ) free(path); path = 0;
			}

			if( Setup_cf_info( Spool_dir_DYN, 0, &job ) ){
				id = Find_str_value(&job.info,OPENNAME,Value_sep);
				DEBUG3("Do_queue_jobs: missing control file '%s'",id);
				free( Sort_order.list[job_index] ); Sort_order.list[job_index] = 0;
				continue;
			}

			if(DEBUGL4)Dump_job("Do_queue_jobs: current hold file",&job);

			/* check to see if active */
			if( (pid = Find_flag_value(&job.info,SERVER,Value_sep)) ){
				DEBUG3("Do_queue_jobs: [%d] active %d", job_index, pid );
				continue;
			}
			/* get printable status */
			Job_printable(&job,&Spool_control,&printable,&held,&move);
			if( (!(printable && printing_enabled) && !move) || held ){
				DEBUG3("Do_queue_jobs: [%d] not processable", job_index );
				free( Sort_order.list[job_index] ); Sort_order.list[job_index] = 0;
				continue;
			}
			if( Check_print_perms(&job) == P_REJECT ){
				Set_str_value(&job.info,ERROR,"no permission to print");
				if( Set_hold_file( &job, 0 ) ){
					/* you cannot update hold file!! */
					setstatus( &job, _("cannot update hold file for '%s'"),
						id);
					fatal( LOG_ERR,
						_("Do_queue_jobs: cannot update hold file for '%s'"), 
						id);
				}
				if( !Save_on_error_DYN ){
					setstatus( &job, _("removing job '%s' - no permissions"), id);
					Remove_job( &job );
				}
				free( Sort_order.list[job_index] ); Sort_order.list[job_index] = 0;
				continue;
			}

			/* get destination information */
			destinations = Find_flag_value(&job.info,DESTINATIONS,Value_sep);
			if( !destinations ){
				move_dest = new_dest = Find_str_value(&job.info,MOVE,Value_sep);
				if( !new_dest ) new_dest = Frwarding(&Spool_control);
			} else {
				all_done = 0;
				for( j = 0; !new_dest && j < destinations; ++j ){
					Get_destination(&job,j);
					if( Find_flag_value(&job.destination,SERVER,Value_sep) ){
						break;
					}
					if( Find_flag_value(&job.destination,DONE_TIME,Value_sep ) ){
						++all_done;
						continue;
					}
					if( Find_str_value(&job.destination,ERROR,Value_sep)
						|| Find_flag_value(&job.destination,HOLD_TIME,Value_sep) ){
						continue;
					}
					if( (move_dest = new_dest = Find_str_value( &job.destination,
						MOVE,Value_sep)) ){
						destination = j;
						break;
					}
					if( Find_flag_value(&job.destination, PRINTABLE, Value_sep )
						&& printing_enabled ){
						new_dest = Find_str_value( &job.destination,
							DEST,Value_sep );
						destination = j;
						break;
					}
				}
				if( !new_dest ){
					printable = 0;
				}
				if( all_done == destinations ){
					DEBUG3("Do_queue_jobs: destinations %d, done %d",
						destinations, all_done );
					Update_status( &job, JSUCC );
					continue;
				}
			}

			DEBUG3("Do_queue_jobs: new_dest '%s', printable %d, master %d, destinations %d, destination %d",
				new_dest, printable, master, destinations, destination );
			if( new_dest ){
				sp = (void *)servers.list[0];
				if( Find_flag_value(sp,SERVER,Value_sep) ){
					continue;
				}
				use_subserver = 0;
				job_to_do = job_index;
			} else if( printing_enabled && printable ){
				/*
				 * find the subserver with a class that will print this job
				 * if master = 1, then we start with 1
				 */
				for( j = master; use_subserver < 0 && j < servers.count; ++j ){
					sp = (void *)servers.list[j];
					if( Pr_disabled(sp)
						|| Pr_aborted(sp)
						|| Sp_disabled(sp)
						|| Find_flag_value(sp,SERVER,Value_sep)){
						DEBUG1("Do_queue_jobs: cannot use [%d] '%s'",
							j, Find_flag_value(sp,PRINTER,Value_sep) );
						continue;
					}
					if( Get_hold_class(&job.info,sp) ){
						DEBUG1("Do_queue_jobs: cannot use [%d] '%s' class conflict",
							j, Find_flag_value(sp,PRINTER,Value_sep) );
						/* we found a server that was available */
						continue;
					}
					use_subserver = j;
					job_to_do = job_index;
				}
			}
		}

		if(DEBUGL2) Dump_subserver_info("Do_queue_jobs- checking for server",
			&servers );
		for( j = 0; j < servers.count; ++j ){
			sp = (void *)servers.list[j];
			if( Find_flag_value(sp,SERVER,Value_sep) ){
				++working;
			}
		}

		/* first, we see if there is no work and no server active */
		DEBUG1("Do_queue_jobs: job_to_do %d, use_subserver %d, working %d",
			job_to_do, use_subserver, working );

		if( job_to_do < 0 && !working ){
			DEBUG1("Do_queue_jobs: nothing to do");
			break;
		}

		/* now we see if we have to wait */ 
		if( working && use_subserver < 0 ){
			DEBUG1("Do_queue_jobs: waiting for process");
			if( servers.count > 1 ){
				setstatus(0, "waiting for subserver to finish" );
			}
			Wait_for_subserver( &servers, &Sort_order );
			continue;
		}

		/*
		 * get the complete job status
		 */
		hf_name = Find_str_value(&job.info,HF_NAME,Value_sep);

		/*
		 * set the hold file information
		 */

		if( destination >= 0 ){
			plp_snprintf(buffer,sizeof(buffer),"DEST%d",destination );
			Set_str_value(&job.info,DESTINATION,buffer);
		}

		Set_decimal_value(&job.info,SERVER,getpid());
		Set_flag_value(&job.info,START_TIME,time((void *)0));
		sp = (void *)servers.list[use_subserver];
		pr = Find_str_value(sp,PRINTER,Value_sep);

		DEBUG1("Do_queue_jobs: starting job '%s' on '%s'", id, pr );

		if( Set_hold_file( &job, 0 ) ){
			/* you cannot update hold file!! */
			setstatus( &job, _("cannot update hold file '%s'"), hf_name);
			fatal( LOG_ERR,
				_("Do_queue_jobs: cannot update hold file '%s'"), hf_name);
		}

		if( use_subserver > 0 ){
			/* we now copy the data and/or control files */
			sd = Find_str_value(sp,SPOOLDIR,Value_sep);
			name = Find_str_value(sp,PRINTER,Value_sep);
			id = Find_str_value(&job.info,IDENTIFIER,Value_sep);
			if(!id) id = Find_str_value(&job.info,TRANSFERNAME,Value_sep);
			DEBUG1("Do_queue_jobs: subserver '%s', spool dir '%s' for job '%s'",
				name, sd, id );
			setstatus(&job, "transferring '%s' to subserver '%s'", id, name );
			fail = 0;
			for( i = 0; i < job.datafiles.count; ++i ){
				datafile = (void *)job.datafiles.list[i];
				if(DEBUGL3)Dump_line_list("Do_queue_jobs - copying datafiles",
					datafile);
				from = Find_str_value(datafile,TRANSFERNAME,Value_sep);
				path = Make_pathname(sd,from);
				DEBUG3("Do_queue_jobs: sd '%s', from '%s', path '%s'",
					sd, from, path );
				fail |= Copy_or_link( from, path );
				if(path) free(path); path = 0;
			}
			from = Find_str_value(&job.info,TRANSFERNAME,Value_sep);
			path = Make_pathname(sd,from);
			DEBUG3("Do_queue_jobs: sd '%s', from '%s', path '%s'",
				sd, from, path );
			fail |= Copy_or_link( from, path );
			if(path) free(path); path = 0;

			if( fail ){
				setstatus(&job, "transfer '%s' to subserver '%s' failed", id, name );
				plp_snprintf(buffer,sizeof(buffer),
					"could not copy to destination '%s'", name );
				Set_str_value(&job.info,ERROR,buffer);
				Set_hold_file(&job, 0 );
				setstatus(&job,"could not copy to destination '%s', spool dir '%s'",
					name, sd );
				continue;
			}
			Set_flag_value(&job.info,DONE_TIME,time((void *)0));
			Set_hold_file(&job, 0 );
			setstatus(&job, "transfer '%s' to subserver '%s' finished", id, name );
			setmessage(&job,STATE,"COPYTO %s",name);
			if( !Save_when_done_DYN ){
				if( Remove_job( &job ) ){
					setstatus( &job, _("could not remove job '%s'"), id);
				} else {
					setstatus( &job, _("job '%s' removed"), id );
				}
			} else {
				setstatus( &job, _("job '%s' saved"), id );
			}
			setstatus(&job, "starting subserver '%s'", name );
			pid = Fork_subserver( &servers, use_subserver, 0 );
		} else {
			Free_line_list(&tinfo);
			Set_str_value(&tinfo,HF_NAME,hf_name);
			Set_str_value(&tinfo,NEW_DEST,new_dest);
			Set_str_value(&tinfo,MOVE_DEST,move_dest);
			if( (pid = Fork_subserver( &servers, 0, &tinfo )) < 0 ){
				Set_decimal_value(&job.info,SERVER,0);
				if( Set_hold_file( &job, 0 ) ){
					/* you cannot update hold file!! */
					setstatus( &job, _("cannot update hold file '%s'"),
						hf_name );
					fatal( LOG_ERR,
						_("Do_queue_jobs: cannot update hold file '%s'"), 
						hf_name );
				}
				/* see if a delay improves the process situation */
				setstatus( &job, _("sleeping, waiting for processes to exit"));
				sleep(1);
			} else {
				Set_str_value(sp,HF_NAME,hf_name);
			}
		}
	}

	/* now we reset the server order */
	Errorcode = JSUCC;
	Free_job(&job);
	Free_line_list(&tinfo);
	if( Server_names_DYN ){
		setstatus( 0, "no more jobs to process in load balance queue" );
		for( i = 1; i < servers.count; ++i ){
			sp = (void *)servers.list[i];
			s = Find_str_value(sp,PRINTER,Value_sep);
			Add_line_list(&tinfo,s,0,0,0);
		}
		s = Join_line_list(&tinfo,",");
		Set_str_value(&Spool_control,SERVER_ORDER,s);
		Set_spool_control(0, Spool_dir_DYN,Queue_control_file_DYN,
			Printer_DYN,&Spool_control);
		if(s) free(s); s = 0;
		Free_line_list(&tinfo);
	}
	Free_listof_line_list(&servers);

	DEBUG1("Do_queue_jobs: done, Errorcode %d, '%s'", Errorcode,
		Server_status(Errorcode) );

	/* check for changes to spool control information */
	plp_unblock_all_signals( &oblock);
	plp_set_signal_mask( &oblock, 0);
	DEBUG1( "Do_queue_jobs: Susr1 at end %d", Susr1 );
	if( Susr1 ){
		DEBUG1("Do_queue_jobs: SIGUSR1 just before exit" );
		Susr1 = 0;
		goto again;
	}
	cleanup(0);
	return(0);
}

/***************************************************************************
 * Remote_job()
 * Send a job to a remote server.  This code is actually trickier
 *  than it looks, as the Send_job code takes most of the heat.
 *
 ***************************************************************************/

int Remote_job( struct job *job, char *id )
{
	int status, tempfd, of_error[2], pid, len, n, i;
	double job_size;
	char buffer[SMALLBUFFER], *s, *tempfile, *pgm;
	struct line_list *lp, *firstfile;
	struct job jcopy;
	struct stat statb;
	plp_status_t procstatus;

	DEBUG1("Remote_job: %s", id );
	setmessage(job,STATE,"SENDING");
	status = 0;
	Init_job(&jcopy);

	Set_str_value(&job->info,PRSTATUS,0);
	Set_str_value(&job->info,ERROR,0);

	Setup_user_reporting(job);

	if( Accounting_remote_DYN && Accounting_file_DYN ){
		Setup_accounting( job );
		if( Accounting_start_DYN ){
			status = Do_accounting( 0,
				Accounting_start_DYN, job, Send_job_rw_timeout_DYN );
		}
		DEBUG1("Remote_job: accounting status %s", Server_status(status) );
		if( status ){
			plp_snprintf(buffer,sizeof(buffer),
				"accounting check failed '%s'", Server_status(status));
			setstatus(job,"%s", buffer );
			switch(status){
	case JFAIL: break;
	case JHOLD: Set_flag_value(&job->info,HOLD_TIME,time((void *)0)); break;
	case JREMOVE: Set_flag_value(&job->info,REMOVE_TIME,time((void *)0)); break;
	default: Set_str_value(&job->info,ERROR,buffer); break;
			}
			Set_hold_file(job, 0 );
			Errorcode = status;
			cleanup(0);
		}
	}

	status = 0;

	Copy_job(&jcopy,job);
	if(DEBUGL2)Dump_job("Remote_job - jcopy", &jcopy );
	if( Bounce_queue_dest_DYN || Lpd_bounce_DYN ){
		if(DEBUGL2) Dump_job( "Remote_job - before filtering", &jcopy );
		tempfd = Make_temp_fd(&tempfile);
		Print_job( tempfd, &jcopy, 0 );
		if( fstat( tempfd, &statb ) ){
			logerr_die(LOG_INFO,"Remote_job: fstatb failed" );
		}
		if( (close(tempfd) == -1 ) ){
			Errorcode = JFAIL;
			logerr_die( LOG_INFO,"Remote_job: close(%d) failed",
				tempfd);
		}
		job_size = statb.st_size;
		Free_listof_line_list(&jcopy.datafiles);
		lp = malloc_or_die(sizeof(lp[0]),__FILE__,__LINE__);
		memset(lp,0,sizeof(lp[0]));
		Check_max(&jcopy.datafiles,1);
		jcopy.datafiles.list[jcopy.datafiles.count++] = (void *)lp;
		Set_str_value(lp,OPENNAME,tempfile);
		Set_str_value(lp,TRANSFERNAME,tempfile);
		if( !(s = Bounce_queue_format_DYN) || strlen(s) > 1 || !islower(cval(s))  ){
			Set_str_value( &jcopy.info,ERROR,"bad bq_format value");
		}
		Set_str_value(lp,FORMAT,Bounce_queue_format_DYN);
		s = 0;
		if( (firstfile = (void *)(job->datafiles.list[0])) ){
			s = Find_str_value( firstfile, "N", Value_sep );
		}
		Set_str_value(lp,"N",s?s:"(lpd_filter)");
		Set_flag_value(lp,COPIES,1);
		Set_double_value(lp,SIZE,job_size);
	} else if( Generate_banner_DYN ){
		struct line_list files;
		int banner_place, do_banner;

		s = Find_str_value(&jcopy.info, BNRNAME, Value_sep );
		do_banner = Always_banner_DYN || s;
		if( do_banner && s == 0 ){
			s = Find_str_value( &jcopy.info,LOGNAME,Value_sep);
			if( s == 0 ) s = "ANONYMOUS";
			Set_str_value(&jcopy.info,BNRNAME,s);
		}

		pgm = Banner_end_DYN;
		if( !pgm ) pgm = Banner_start_DYN;
		if( !pgm ) pgm = Banner_printer_DYN;
		if( !pgm ){
			fatal( LOG_ERR,
				_("Remote_job: generate_banner and no banner_printer defined") );
		}
		if( do_banner ){
			setstatus(job,"generating banner using '%s'", pgm);
			Init_line_list(&files);
			tempfd = Make_temp_fd(&tempfile);

			if( pipe( of_error ) == -1 ){
				Errorcode = JFAIL;
				logerr_die( LOG_INFO,"Remote_job: pipe() failed");
			}
			DEBUG3("Remote_job: fd of_error[%d,%d]", of_error[0], of_error[1] );

			Check_max(&files, 10 );
			files.list[files.count++] = Cast_int_to_voidstar(0);		/* stdin */
			files.list[files.count++] = Cast_int_to_voidstar(tempfd);	/* stdout */
			files.list[files.count++] = Cast_int_to_voidstar(of_error[1]);	/* stderr */
			if( (pid = Make_passthrough( pgm, Filter_options_DYN, &files, &jcopy, 0 )) < 0 ){
				Errorcode = JFAIL;
				logerr_die(LOG_INFO,"Remote_job: could not make banner process");
			}
			files.count = 0;
			Free_line_list(&files);

			if( (close(of_error[1]) == -1 ) ){
				Errorcode = JFAIL;
				logerr_die( LOG_INFO,"Remote_job: close(%d) failed",
					of_error[1]);
			}
			len = 0;
			while( len < sizeof(buffer)-1
				&& (n = read(of_error[0],buffer+len,sizeof(buffer)-len-1)) >0 ){
				buffer[n+len] = 0;
				while( (s = safestrchr(buffer,'\n')) ){
					*s++ = 0;
					setstatus(job, "Remote_job: BANNER_ERROR: '%s'", buffer );
					memmove(buffer,s,strlen(s)+1);
				}
				len = strlen(buffer);
			}
			if( len ){
				setstatus(job, "Remote_job: BANNER_ERROR: '%s'", buffer );
			}
			if( (close(of_error[0]) == -1 ) ){
				Errorcode = JFAIL;
				logerr_die( LOG_INFO,"Remote_job: close(%d) failed",
					of_error[0]);
			}
			while( (n = plp_waitpid(pid,&procstatus,0)) != pid );
			DEBUG1("Remote_job: pid %d, exit status '%s'", pid,
				Decode_status(&procstatus) );
			if( WIFEXITED(procstatus) && (n = WEXITSTATUS(procstatus)) ){
				plp_snprintf(buffer,sizeof(buffer),
					"Remote_job: banner printer '%s' exited with status %d", pgm, n);
				Set_str_value(&jcopy.info,ERROR,buffer);
			} else if( WIFSIGNALED(procstatus) ){
				n = WTERMSIG(procstatus);
				plp_snprintf(buffer,sizeof(buffer),
					"Remote_job: banner printer '%s' died with signal %d, '%s'",
						pgm, n, Sigstr(n));
				Set_str_value(&jcopy.info,ERROR,buffer);
			}
			if( fstat( tempfd, &statb ) ){
				logerr_die(LOG_INFO,"Remote_job: fstatb failed" );
			}
			job_size = statb.st_size;

			if( (close(tempfd) == -1 ) ){
				Errorcode = JFAIL;
				logerr_die( LOG_INFO,"Remote_job: close(%d) failed",
					tempfd);
			}

			Check_max(&jcopy.datafiles,2);
			banner_place = 0;
			if( Banner_last_DYN || Banner_end_DYN ){
				banner_place = jcopy.datafiles.count;
			} else {
				for( i = jcopy.datafiles.count; i >= 0; --i ){
					jcopy.datafiles.list[i+1] = jcopy.datafiles.list[i];
				}
			}
			++jcopy.datafiles.count;

			lp = malloc_or_die(sizeof(lp[0]),__FILE__,__LINE__);
			memset(lp,0,sizeof(lp[0]));
			jcopy.datafiles.list[banner_place] = (void *)lp;
			Set_str_value(lp,OPENNAME,tempfile);
			Set_str_value(lp,TRANSFERNAME,tempfile);
			Set_str_value(lp,FORMAT,"f");
			Set_str_value(lp,"N","(banner)");
			Set_flag_value(lp,COPIES,1);
			Set_double_value(lp,SIZE,job_size);
		}
	}
	if( Fix_control( &jcopy, Control_filter_DYN ) ){
		status = JFAIL;
		if( !(s = Find_str_value(&jcopy.info,ERROR,Value_sep)) ){
			plp_snprintf(buffer,sizeof(buffer), _("control filter error") );
			Set_str_value(&jcopy.info,ERROR,buffer);
		}
	} else {
		if(DEBUGL3)Dump_job("Send_job - after Fix_control", &jcopy );
		status = Send_job( &jcopy, Connect_timeout_DYN, Connect_interval_DYN,
			Max_connect_interval_DYN, Send_job_rw_timeout_DYN );
		DEBUG1("Remote_job: %s, status '%s'", id, Link_err_str(status) );
	}
	buffer[0] = 0;

	if(DEBUGL2)Dump_job("Remote_job - final jcopy value", &jcopy );

	s = 0;
	if( status ){
		s = Find_str_value(&jcopy.info,ERROR,Value_sep);
	}
	Set_str_value(&job->info,ERROR,s);
	s = 0;

	Free_job(&jcopy);

	switch( status ){
	case JSUCC:
	case JABORT:
	case JFAIL:
	case JREMOVE:
		break;
	case LINK_ACK_FAIL:
		plp_snprintf(buffer,sizeof(buffer),
			_("link failure while sending job '%s'"), id );
		s = buffer;
		status = JFAIL;
		break;
	case LINK_PERM_FAIL:
		plp_snprintf(buffer,sizeof(buffer),
			 _("no permission to spool job '%s'"), id );
		s = buffer;
		status = JREMOVE;
		break;
	default:
		plp_snprintf(buffer,sizeof(buffer),
			_("failed to send job '%s'"), id );
		s = buffer;
		status = JFAIL;
		break;
	}
	if( s && !Find_str_value(&job->info,ERROR,s) ){
		Set_str_value(&job->info,ERROR,s);
	}

	Set_str_value(&job->info,PRSTATUS,Server_status(status));

	Set_hold_file(job, 0 );

	if( Accounting_remote_DYN && Accounting_file_DYN  ){
		if( Accounting_end_DYN ){
			Do_accounting( 1, Accounting_end_DYN, job,
				Send_job_rw_timeout_DYN );
		}
	}
	return( status );
}

/***************************************************************************
 * Local_job()
 * Send a job to a local printer.
 ***************************************************************************/

int Local_job( struct job *job, char *id )
{
	int status, fd, pid, errorpid, n, len;
	plp_status_t procstatus;
	char buffer[SMALLBUFFER], *s;

	DEBUG1("Local_job: starting %s", id );
	setmessage(job,STATE,"PRINTING");
	status = 0;
	Set_str_value(&job->info,PRSTATUS,0);
	Set_str_value(&job->info,ERROR,0);

	Setup_user_reporting(job);

	setstatus(job,"subserver pid %d starting", getpid());
	if( Accounting_file_DYN && Local_accounting_DYN ){
		setstatus(job,"accounting at start");
		Setup_accounting( job );
		if( Accounting_start_DYN ){
			status = Do_accounting( 0,
				Accounting_start_DYN, job, Send_job_rw_timeout_DYN );
		}
		DEBUG1("Local_job: accounting status %s", Server_status(status) );
		if( status ){
			plp_snprintf(buffer,sizeof(buffer),
				"accounting check failed '%s'", Server_status(status));
			setstatus(job,"%s", buffer );
			switch(status){
	case JFAIL: break;
	case JHOLD: Set_flag_value(&job->info,HOLD_TIME,time((void *)0)); break;
	case JREMOVE: Set_flag_value(&job->info,REMOVE_TIME,time((void *)0)); break;
	default: Set_str_value(&job->info,ERROR,buffer); break;
			}
			Set_hold_file(job, 0 );
			Errorcode = status;
			cleanup(0);
		}
	}
 	status = Errorcode = 0;

	setstatus(job,"opening device '%s'", Lp_device_DYN);
	pid = errorpid = 0;
	fd = Printer_open(Lp_device_DYN,job,
		Send_try_DYN, Connect_interval_DYN, Max_connect_interval_DYN,
		Connect_grace_DYN, Connect_timeout_DYN, &pid, &errorpid );

	DEBUG1("Local_job: fd %d", fd );
	if( fd < 0 ){
		status = JFAIL;
	} else {
		setstatus(job,"printing job '%s'", id );
		Print_job( fd, job, Send_job_rw_timeout_DYN );
		status = Errorcode;
		/* we do shutdown on socket, close otherwise */
		DEBUG1("Local_job: shutting down fd %d", fd );
		if( (fd = Shutdown_or_close(fd)) > 0 ){
			if( Send_job_rw_timeout_DYN > 0 ){
				Set_linger( fd, Send_job_rw_timeout_DYN );
			} else if( Exit_linger_timeout_DYN > 0 ){
				Set_linger( fd, Exit_linger_timeout_DYN );
			}
		}
		DEBUG1("Local_job: after shutdown fd %d", fd );
		while( pid > 0 || errorpid > 0 ){
			setstatus(job,"waiting for printer filter to exit");
			n = plp_waitpid(-1,&procstatus,0);
			if( n  == pid ){
				pid = 0;
				if( WIFEXITED(procstatus) ){
					status = WEXITSTATUS(procstatus);
					setstatus(job,"printer filter exit status '%s'",
						Server_status(status) );
					DEBUG1("Local_job: filter exit status '%s'",
						Server_status(status) );
				} else if( WIFSIGNALED(procstatus) ){
					n = WTERMSIG(procstatus);
					DEBUG1("Local_job: signal %d, '%s'",
						n, Sigstr(n));
					setstatus(job,"printer filter died with signal '%s'",
						Sigstr(n));
					status = JABORT;
				}
			} else if( n == errorpid ){
				errorpid = 0;
			}
		}
		/* we wait for nothing to read */
		if( fd > 0 && Wait_for_eof_DYN ){
			DEBUG1("Local_job: wait for eof on %d", fd );
			buffer[0] = 0;
			for(len = 0;
				len < sizeof(buffer)-1 && (n = read(fd, buffer+len, sizeof(buffer)-1-len)) > 0;
				len = strlen(buffer) ){
				buffer[len+n] = 0;
				DEBUG1("Local_job: read '%s'", buffer);
				while( (s = strchr(buffer,'\n')) ){
					*s++ = 0;
					setstatus(job, "IO_DEVICE message '%s'", buffer);
					memmove(buffer,s,strlen(s)+1);
				}
			}
			DEBUG1("Local_job: eof on %d", fd );
		}
		close( fd );
	}
	DEBUG1("Local_job: status %s", Server_status(status) );

	Set_str_value(&job->info,PRSTATUS,Server_status(status));
	if( Accounting_file_DYN && Local_accounting_DYN ){
		setstatus(job,"accounting at end");
		if( Accounting_end_DYN ){
			Do_accounting( 1, Accounting_end_DYN, job,
				Send_job_rw_timeout_DYN );
		}
	}
	setstatus(job,"finished '%s', status '%s'", id, Server_status(status));
	return( status );
}

int Fork_subserver( struct line_list *server_info, int use_subserver,
	struct line_list *parms )
{
	char *pr;
	struct line_list *sp;
	int pid;
	struct line_list pl;

	Init_line_list(&pl);
	if( parms == 0 ) parms = &pl;
	sp = (void *)server_info->list[use_subserver];
	Set_str_value(sp,PRSTATUS,0);
	Set_decimal_value(sp,SERVER,0);

	pr = Find_str_value(sp,PRINTER,Value_sep);
	Set_str_value(parms,PRINTER,pr);
	Set_flag_value(parms,SUBSERVER,use_subserver);
	Set_flag_value(parms,IDLE,Find_flag_value(sp,IDLE,Value_sep) );
	if( use_subserver > 0 ){
		Set_str_value(parms,CALL,QUEUE);
	} else {
		Set_str_value(parms,CALL,PRINTER);
	}

	DEBUG1( "Fork_subserver: starting '%s'", pr );

	pid = Start_worker( parms, 0 );
	if( pid > 0 ){
		Set_decimal_value(sp,SERVER,pid);
	} else {
		logerr( LOG_ERR, _("Fork_subserver: fork failed") );
	}
	Free_line_list(parms);
	return( pid );
}

/***************************************************************************
 * struct server_info *Wait_for_subserver( struct line_list *servers,
 *     int block, plp_block_mask *omask )
 *  wait for a server process to exit
 *  if none present return 0
 *  look up the process in the process table
 *  update the process table status
 *  return the process table entry
 ***************************************************************************/

void Wait_for_subserver( struct line_list *servers, struct line_list *order )
{
	pid_t pid;
	plp_status_t procstatus;
	int found, sigval, status, i, done;
	struct line_list *sp = 0;
	struct job job;
	char buffer[SMALLBUFFER], *pr, *hf_name;

	/*
	 * wait for the process to finish or a signal to be delivered
	 */

	Init_job(&job);
	sigval = errno = 0;

	/* we need to unblock signals and wait for event */
	(void) plp_signal(SIGCHLD,  (plp_sigfunc_t)Sigchld);
	done = 0;
	while( (pid = plp_waitpid( -1, &procstatus, WNOHANG )) > 0 ){
		++done;
		DEBUG1("Wait_for_subserver: pid %d, status '%s'", pid,
			Decode_status(&procstatus));
		if( WIFSIGNALED( procstatus ) ){
			sigval = WTERMSIG( procstatus );
			DEBUG1("Wait_for_subserver: pid %d terminated by signal '%s'",
				pid, Sigstr( sigval ) );
			switch( sigval ){
			/* generated by the program */
			case 0:
			case SIGINT:
			case SIGKILL:
			case SIGQUIT:
			case SIGTERM:
			case SIGUSR1:
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
		DEBUG1( "Wait_for_subserver: pid %d final status %s",
			pid, Server_status(status) );

		if( status != JSIGNAL ){
			plp_snprintf(buffer,sizeof(buffer),
				 _("subserver pid %d exit status '%s'"),
				pid, Server_status(status));
		} else {
			plp_snprintf(buffer,sizeof(buffer),
				_("subserver pid %d died with signal '%s'"),
				pid, Sigstr(sigval));
			status = JABORT;
		}
		setstatus( &job, "%s", buffer );
		if(DEBUGL4) Dump_subserver_info("Wait_for_subserver", servers );

		for( found = i = 0; !found && i < servers->count; ++i ){
			sp = (void *)servers->list[i];
			if( pid == Find_flag_value(sp,SERVER,Value_sep) ){
				DEBUG3("Wait_for_subserver: found %d", pid );
				found = 1;
				++done;

				Set_decimal_value(sp,SERVER,0);
				Set_flag_value(sp,DONE_TIME,time((void *)0));
				hf_name = Find_str_value(sp,HF_NAME,Value_sep);
				pr = Find_str_value(sp,PRINTER,Value_sep);
				DEBUG1( "Wait_for_subserver: server pid %d for '%s' for '%s' finished",
					pid, pr, hf_name );

				/* see if you can get the hold file and update the status */
				Free_job(&job);
				Get_file_image_and_split( 0, hf_name, 0, 0,
					&job.info, Line_ends,1,Value_sep,1,1,1);

				if( job.info.count ){
					Update_status( &job, status);
				}

				Set_str_value(sp,HF_NAME,0);
				Update_spool_info(sp);
				if( i == 0 ){
					Get_spool_control(Spool_dir_DYN,Queue_control_file_DYN,
					Printer_DYN,&Spool_control );
				}
			}
		}
		/* sort server order */
		if( Mergesort( servers->list+1, servers->count-1,
			sizeof( servers->list[0] ), cmp_server ) ){
			fatal( LOG_ERR,
				_("Wait_for_subserver: Mergesort failed") );
		}
		if(DEBUGL4) Dump_subserver_info(
			"Wait_for_subserver: after sorting", servers );
	}
	if( !done ){
		plp_sigpause();
	}
	signal( SIGCHLD, SIG_DFL );

	Free_job(&job);
}

/***************************************************************************
 * int Decode_transfer_failure( int attempt, struct job *job, int status )
 * When you get a job failure more than a certain number of times,
 *  you check the 'Send_failure_action_DYN' variable
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

int Decode_transfer_failure( int attempt, struct job *job, char *id )
{
	struct keywords *key;
	int in[2], out[2], count, result, n, pid, len;
	char line[SMALLBUFFER], *outstr;
	plp_status_t status;
	struct line_list fd;

	DEBUG1("Decode_transfer_failure: send_failure_action '%s'",
		Send_failure_action_DYN );
	Init_line_list(&fd);
	result = JREMOVE;
	outstr = 0;
	if( Send_failure_action_DYN ){
		/* check to see if it is a filter */
		if( Send_failure_action_DYN[0] == '|' ){
			/* open a pipe to read output from */
			if( pipe(in) < 0 || pipe(out) < 0 ){
				logerr_die( LOG_ERR, _("Decode_transfer: pipe failed") );
			}
			DEBUG1("Decode_transfer_failure: pipe in[%d,%d], out[%d,%d]",
				in[0], in[1], out[0], out[1] );

			Check_max(&fd,10);
			fd.list[fd.count++] = Cast_int_to_voidstar(in[0]);
			fd.list[fd.count++] = Cast_int_to_voidstar(out[1]);
			fd.list[fd.count++] = Cast_int_to_voidstar(2);
			pid = Make_passthrough( Send_failure_action_DYN+1,
				Filter_options_DYN, &fd,job, 0 );
			fd.count = 0;
			Free_line_list(&fd);

			(void)close(in[0]);
			(void)close(out[1]);
			plp_snprintf( line, sizeof(line), "%d", attempt );
			Write_fd_str( in[1], line );
			close(in[1]);
			len = 0;
			while( len < sizeof(line)-1
				&& (count = read( out[0], line+len, sizeof(line)-len-1 )) > 0){
				line[len+count] = 0;
				DEBUG1("Decode_transfer_failure: read '%s'", line );
				len += count;
			}
			line[len] = 0;
			close( out[0] );
			while( (n = plp_waitpid(pid,&status,0)) != pid );
			if( WIFSIGNALED(status) ){
				n = WTERMSIG(status);
				DEBUG1("Decode_transfer_failure: signal %d, '%s'",
					n, Sigstr(n));
				result = JABORT;
			} else if( WIFEXITED(status) ){
				result = WEXITSTATUS(status);
				if( result == 0 ){
					DEBUG1("Decode_transfer_failure: exit status %d", result);
					outstr = line;
					if( outstr && *outstr ){
						char *s;
						while( strchr(Whitespace,cval(outstr)) ) ++outstr;
						if( (s = strpbrk(outstr,Whitespace)) ) *s = 0;
					}
					setstatus( job, "job '%s', send_failure_action filter returned '%s'",
						id, outstr );
				} else {
					setstatus( job, "job '%s', send_failure_action filter exit '%s'",
						id, Server_status(result) );
				}
			}
		} else {
			outstr = Send_failure_action_DYN;
			setstatus( job, "job '%s', send_failure_action '%s'",
				id, outstr );
		}
		if( outstr && *outstr ){
			for( key = keys; key->keyword; ++key ){
				DEBUG1("Decode_transfer_failure: comparing '%s' to '%s'",
					outstr, key->keyword );
				if( safestrcasecmp( key->keyword, outstr ) == 0 ){
					result = key->maxval;
					break;
				}
			}
		}
	}
	DEBUG1("Decode_transfer_failure: returning '%s'", Server_status(result) );
	return( result );
}

void Update_status( struct job *job, int status )
{
	char buffer[SMALLBUFFER];
	char *id, *did, *strv;
	struct line_list *destination;
	int copy, copies, attempt, destinations, n, done = 0;
	
	did = 0;
	destinations = 0;
	destination = 0;
	Set_decimal_value(&job->info,SERVER,0);

	id = Find_str_value(&job->info,IDENTIFIER,Value_sep);
	if(!id) id = Find_str_value(&job->info,TRANSFERNAME,Value_sep);

	if( (destinations = Find_flag_value(&job->info,DESTINATIONS,Value_sep)) ){
		did = Find_str_value(&job->info,DESTINATION,Value_sep );
		if( !Get_destination_by_name( job, did ) ){
			destination = &job->destination;
			did = Find_str_value(destination,IDENTIFIER,Value_sep);
			if(!did) did = Find_str_value(destination,TRANSFERNAME,Value_sep);
			Set_decimal_value(destination,SERVER,0);
		}
	}
	setmessage(job,STATE,"EXITSTATUS %s", Server_status(status));

 again:
	DEBUG1("Update_status: again - status '%s', id '%s', dest id '%s'",
		Server_status(status), id, did );

	setmessage(job,STATE,"PROCESSSTATUS %s", Server_status(status));
	switch( status ){
		/* hold the destination stuff */
	case JHOLD:
		if( destination ){
			Set_flag_value(destination,HOLD_TIME,time((void *)0) );
			Update_destination(job);
		} else {
			Set_flag_value(&job->info,HOLD_TIME, time((void *)0) );
			Set_flag_value(&job->info,PRIORITY_TIME, 0 );
		}
		Set_hold_file( job, 0 );
		break;

	case JSUCC:	/* successful, remove job */
		if( destination ){
			done = 0;
			copies = Find_flag_value(&job->info,SEQUENCE,Value_sep);
			Set_flag_value(&job->info,SEQUENCE,copies+1);
			copies = Find_flag_value(destination,COPIES,Value_sep);
			copy = Find_flag_value(destination,COPY_DONE,Value_sep);
			n = Find_flag_value(destination,DESTINATION,Value_sep);
			if( Find_str_value(destination,MOVE,Value_sep) ){
				Set_flag_value(destination,DONE_TIME,time((void *)0));
				setstatus( job, "%s@%s: route job '%s' moved",
					Printer_DYN, FQDNHost_FQDN, did );
				done = 1;
			} else {
				++copy;
				Set_flag_value(destination,COPY_DONE,copy);
				if( copies ){
					setstatus( job,
					"%s@%s: route job '%s' printed copy %d of %d",
					Printer_DYN, FQDNHost_FQDN, id, copy, copies );
				}
				if( copy >= copies ){
					Set_flag_value(destination,DONE_TIME,time((void *)0));
					done = 1;
					++n;
				}
			}
			Update_destination(job);
			if( done && n >= destinations ){
				Set_flag_value(&job->info,DONE_TIME,time((void *)0));
				setstatus( job, "%s@%s: job '%s' printed",
					Printer_DYN, FQDNHost_FQDN, id );
				goto done_job;
			}
			Set_hold_file( job, 0 );
			break;
		} else {
			copies = Find_flag_value(&job->info,COPIES,Value_sep);
			copy = Find_flag_value(&job->info,COPY_DONE,Value_sep);
			if( Find_str_value(&job->info,MOVE,Value_sep) ){
				Set_flag_value(&job->info,DONE_TIME,time((void *)0));
				setstatus( job, "%s@%s: job '%s' moved",
					Printer_DYN, FQDNHost_FQDN, id );
			} else {
				++copy;
				Set_flag_value(&job->info,COPY_DONE,copy);
				if( copies ){
				setstatus( job, "%s@%s: job '%s' printed copy %d of %d",
					Printer_DYN, FQDNHost_FQDN, id, copy, copies );
				}
				if( copy >= copies ){
					Set_flag_value(&job->info,DONE_TIME,time((void *)0));
					Sendmail_to_user( status, job );
					setstatus( job, "%s@%s: job '%s' printed",
						Printer_DYN, FQDNHost_FQDN, id );
				} else {
					Set_hold_file( job, 0 );
					break;
				}
			}
		done_job:
			Set_hold_file( job, 0 );
			if(DEBUGL3)Dump_job("Update_status - done_job", job );
			if( !Save_when_done_DYN ){
				if( Remove_job( job ) ){
					setstatus( job, _("could not remove job '%s'"), id);
				} else {
					setstatus( job, _("job '%s' removed"), id );
				}
			} else {
				setstatus( job, _("job '%s' saved"), id );
			}
		}
		break;

	case JFAIL:	/* failed, retry ?*/
		if( destination ){
			attempt = Find_flag_value(destination,ATTEMPT,Value_sep);
			++attempt;
			Set_flag_value(destination,ATTEMPT,attempt);
			Update_destination(job);
		} else {
			attempt = Find_flag_value(&job->info,ATTEMPT,Value_sep);
			++attempt;
			Set_flag_value(&job->info,ATTEMPT,attempt);
		}
		DEBUG1( "Do_queue_jobs: JFAIL - attempt %d, max %d",
			attempt, Send_try_DYN );
		Set_hold_file( job, 0 );

		if( Send_try_DYN && attempt >= Send_try_DYN ){
			char buf[60];

			/* check to see what the failure action
			 *	should be - abort, failure; default is remove
			 */
			setstatus( job, _("job '%s', attempt %d, allowed %d"),
				id, attempt, Send_try_DYN );
			status = Decode_transfer_failure( attempt, job, id );
			switch( status ){
			case JSUCC:   strv = _("treating as successful"); break;
			case JFAIL:   strv = _("retrying job"); break;
			case JFAILNORETRY:   strv = _("no retry"); break;
			case JABORT:  strv = _("aborting server"); break;
			case JREMOVE: strv = _("removing job - status JREMOVE"); break;
			case JHOLD:   strv = _("holding job"); break;
			default:
				plp_snprintf( buf, sizeof(buf),
					_("unexpected status 0x%x"), status );
				strv = buf;
				status = JABORT;
				break;
			}
			setstatus( job, _("job '%s', %s"), id, strv );
		}
		if( status == JFAIL ){
			setstatus( job, _("job '%s' attempt %d of %d, retrying"),
				id, attempt, Send_try_DYN );
		} else {
			goto again;
		}
		break;

	case JFAILNORETRY:	/* do not try again */
		plp_snprintf( buffer, sizeof(buffer), _("failed, no retry") );
		if( destination ){
			attempt = Find_flag_value(destination,ATTEMPT,Value_sep);
			++attempt;
			if( !Find_str_value(destination,ERROR,Value_sep) ){
				Set_str_value(destination,ERROR,buffer);
			}
			Set_flag_value(destination,ATTEMPT,attempt);
			Update_destination(job);
			Set_hold_file( job, 0 );
		} else {
			attempt = Find_flag_value(&job->info,ATTEMPT,Value_sep);
			++attempt;
			Set_flag_value(&job->info,ATTEMPT,attempt);
			if( !Find_str_value(&job->info,ERROR,Value_sep) ){
				Set_str_value(&job->info,ERROR,buffer);
			}
			Set_hold_file( job, 0 );
			Sendmail_to_user( status, job );
		}
		if( destination == 0 && !Save_on_error_DYN ){
			setstatus( job, _("removing job '%s' - failed, no retry"), id);
			Remove_job( job );
		}
		break;

	default:
	case JABORT:	/* abort, do not try again */
		plp_snprintf(buffer,sizeof(buffer), _("aborting operations") );
		Set_flag_value(&job->info,PRIORITY_TIME,0);
		if( destination ){
			if( !Find_str_value(destination,ERROR,Value_sep) ){
				Set_str_value(destination,ERROR,buffer);
			}
			strv = Find_str_value(destination,ERROR,Value_sep);
			Update_destination(job);
			setstatus( job, "job '%s', destination '%s', error '%s'",
				id,did,strv );
		} else {
			if( !Find_str_value(&job->info,ERROR,Value_sep) ){
				Set_str_value(&job->info,ERROR,buffer);
			}
			strv = Find_str_value(&job->info,ERROR,Value_sep);
			setstatus( job, "job '%s' error '%s'",id, strv);
			Sendmail_to_user( status, job );
		}
		Set_hold_file( job, 0 );
		if( destination == 0 && !Save_on_error_DYN ){
			setstatus( job, _("removing job '%s' - ABORT"), id);
			Remove_job( job );
		}
		if( Stop_on_abort_DYN ){
			setstatus( job, _("stopping printing on filter JABORT exit code") );
			Set_flag_value( &Spool_control,PRINTING_ABORTED,1 );
			Set_spool_control(0, Spool_dir_DYN, Queue_control_file_DYN,
				Printer_DYN,&Spool_control);
		}
		break;

	case JREMOVE:	/* failed, remove job */
		if( destination ){
			if( !Find_str_value(destination,ERROR,Value_sep) ){
				plp_snprintf( buffer, sizeof(buffer),
					_("removing destination due to errors"), did );
				Set_str_value(destination,ERROR,buffer);
			}
			Set_flag_value(destination,DONE_TIME, time( (void *)0) );
			Set_flag_value(destination,REMOVE_TIME, time( (void *)0) );
			Update_destination(job);
			Set_hold_file( job, 0 );
		} else {
			if( !Find_str_value(&job->info,ERROR,Value_sep) ){
				plp_snprintf( buffer, sizeof(buffer),
					_("removing job due to errors"), id );
				Set_str_value(&job->info,ERROR,buffer);
			}
			Set_flag_value(&job->info,DONE_TIME, time( (void *)0) );
			Set_flag_value(&job->info,REMOVE_TIME, time( (void *)0) );
			Set_hold_file( job, 0 );
			Sendmail_to_user( status, job );
			if( !Save_on_error_DYN ){
				setstatus( job, _("removing job '%s' - JREMOVE"), id );
				Remove_job( job );
			}
		}
		break;
	}
	if(DEBUGL3)Dump_job("Update_status: exit result", job );
}

/***************************************************************************
 * int Check_print_perms
 *  check the printing permissions
 ***************************************************************************/

int Check_print_perms( struct job *job )
{
	struct perm_check perm;
	char *s;
	int permission;

	memset( &perm, 0, sizeof(perm) );
	perm.service = 'P';
	perm.printer = Printer_DYN;
	perm.user = Find_str_value(&job->info,LOGNAME,Value_sep);
	perm.remoteuser = perm.user;
	s = Find_str_value(&job->info,FROMHOST,Value_sep);
	if( s && Find_fqdn( &PermHost_IP, s ) ){
		perm.host = &PermHost_IP;
		perm.remotehost = perm.host;
	}
	permission = Perms_check( &Perm_line_list,&perm, job, 1 );
	DEBUG3("Check_print_perms: permission '%s'", perm_str(permission) );
	return( permission );
}


void Setup_user_reporting( struct job *job )
{
	char *host = Find_first_letter(&job->controlfile,'M',0);
	char *port = 0, *protocol = "UDP", *s;
	int prot_num = SOCK_DGRAM;

	DEBUG1("Setup_user_reporting: Allow_user_logging %d, host '%s'",
		Allow_user_logging_DYN, host );
	if( !Allow_user_logging_DYN || host==0
		|| safestrchr(host,'@') || !safestrchr(host,'%') ){
		return;
	}

	host = safestrdup(host,__FILE__,__LINE__);
	/* OK, we try to open a connection to the logger */
	if( (s = safestrchr( host, '%')) ){
		*s++ = 0;
		port = s;
	}
	if( (s = safestrchr( port, ',')) ){
		*s++ = 0;
		protocol = s;
	}

	if( protocol && safestrcasecmp( protocol, "TCP" ) == 0 ){
		protocol = "UDP";
		prot_num = SOCK_STREAM;
	}
		
	DEBUG3("setup_logger_fd: host '%s', port '%s', protocol %d",
		host, port, prot_num );
	Mail_fd = Link_open_type(host, port, 10, prot_num, 0 );
	DEBUG3("Setup_user_reporting: Mail_fd '%d'", Mail_fd );

	if( Mail_fd > 0 && prot_num == SOCK_STREAM ){
		if( Send_job_rw_timeout_DYN > 0 ){
			Set_linger( Mail_fd, Send_job_rw_timeout_DYN );
		} else if( Exit_linger_timeout_DYN > 0 ){
			Set_linger( Mail_fd, Exit_linger_timeout_DYN );
		}
	}
	if( host ) free(host); host = 0;
}



/***************************************************************************
 * Do_check_idle()
 *  execute the idle check of the printer indicated by the
 *  check_idle printcap entry:
 *   check_idle=/script
 ***************************************************************************/

int Do_check_idle(void)
{
	int status, n, pid, error_fd[2], len, count;
	char line[SMALLBUFFER];
	struct line_list fd;
	plp_status_t procstatus;

	Init_line_list(&fd);

	DEBUG1( "Do_check_idle: Check_idle_DYN '%s'", Check_idle_DYN );
	setstatus(0,_("checking for idle using %s"), Check_idle_DYN );

	Init_line_list(&fd);
	Check_max(&fd,10);
	if( pipe(error_fd) == -1 ){
		Errorcode = JABORT;
		logerr_die( LOG_ERR, _("Do_check_idle: pipe failed"));
	}

	fd.list[fd.count++] = Cast_int_to_voidstar(0);
	fd.list[fd.count++] = Cast_int_to_voidstar(1);
	fd.list[fd.count++] = Cast_int_to_voidstar(error_fd[1]);
	pid = Make_passthrough(Check_idle_DYN, 0, &fd, 0, 0 );
	fd.count = 0;
	Free_line_list(&fd);
	close(error_fd[1]);
	/* now read the error messages */
	len = 0;
	while( len < sizeof(line)-1
		&& (count = read( error_fd[0], line+len, sizeof(line)-len-1 )) > 0){
		char *s;
		line[len+count] = 0;
		DEBUG1("Do_check_idle: read '%s'", line );
		if( (s = safestrchr(line,'\n')) ){
			*s++ = 0;
			setstatus(0,_("CHECKIDLE filter msg - '%s'"), line );
			memmove(line,s,strlen(s)+1);
		}
		len = strlen(line);
	}
	close(error_fd[0]);
	while( (n = plp_waitpid(pid,&procstatus,0)) != pid );
	DEBUG1("Do_check_idle: pid %d, status '%s'", pid,
		Decode_status(&procstatus));
	if( WIFSIGNALED(procstatus) ){
		Errorcode = JABORT;
		n = WTERMSIG(procstatus);
		fatal(LOG_INFO,"Do_check_idle: process died with signal %d, '%s'",
			n, Sigstr(n));
	}
	status = WEXITSTATUS(procstatus);

	DEBUG1("Do_check_idle: exit status '%d'", status );
	switch( status ){
	case JSUCC:
	case JNOSPOOL:
	case JNOPRINT:
		break;
	default:
		status = JABORT;
		break;
	}
	DEBUG1("Do_check_idle: returning status '%s'",
		Server_status(status) );
	setstatus(0,_("idle status %s"), Server_status(status));
	return(status);
}

void Service_worker( struct line_list *args )
{
	int pid, unspooler_fd, destinations;
	char *s, *path, *hf_name, *new_dest, *move_dest,
		*id, *did;
	struct stat statb;
	char buffer[SMALLBUFFER];
	struct job job;

	Name="(Worker)";

	Init_job(&job);

	Set_DYN(&Printer_DYN, Find_str_value(args,PRINTER,Value_sep));
	setproctitle( "lpd %s '%s'", Name, Printer_DYN );

	DEBUG1("Service_worker: begin");

	(void) plp_signal(SIGUSR1, (plp_sigfunc_t)cleanup_USR1);
	Errorcode = JABORT;

	/* you need to have a spool queue */
	if( Setup_printer( Printer_DYN, buffer, sizeof(buffer) ) ){
		cleanup(0);
	}

	if(DEBUGL4){ int fd; fd = dup(0);
	logDebug("Service_worker: after Setup_printer next fd %d",fd); close(fd); };

	pid = getpid();
	DEBUG1( "Service_worker: pid %d", pid );
	path = Make_pathname( Spool_dir_DYN, Queue_unspooler_file_DYN );
	if( (unspooler_fd = Checkwrite( path, &statb, O_RDWR, 1, 0 )) < 0 ){
		logerr_die( LOG_ERR, _("Service_worker: cannot open lockfile '%s'"),
			path );
	}
	if(path) free(path); path = 0;
	Write_pid( unspooler_fd, pid, (char *)0 );
	close(unspooler_fd); unspooler_fd = -1;

	DEBUG3("Service_worker: checking path '%s'", path );

	hf_name = Find_str_value(args,HF_NAME,Value_sep);
	Get_file_image_and_split(0,hf_name,0,0, &job.info, Line_ends,1,Value_sep,1,1,1);
	if( job.info.count == 0 ){
		DEBUG3("Service_worker: missing hold file '%s'", hf_name);
		Errorcode = 0;
		cleanup(0);
	}
	hf_name = Find_str_value(&job.info,HF_NAME,Value_sep);


	if( Setup_cf_info( Spool_dir_DYN, 0, &job ) ){
		DEBUG3("Service_worker: missing control file");
		Errorcode = 0;
		cleanup(0);
	}
	Set_str_value(&job.info,NEW_DEST, Find_str_value(args,NEW_DEST,Value_sep));
	Set_str_value(&job.info,MOVE_DEST, Find_str_value(args,MOVE_DEST,Value_sep));
	Set_decimal_value(&job.info,SERVER,getpid());

	Free_line_list(args);

	if( Set_hold_file( &job, 0 ) ){
		/* you cannot update hold file!! */
		setstatus( &job, _("cannot update hold file for '%s'"),
			hf_name );
		fatal( LOG_ERR,
			_("Service_worker: cannot update hold file for '%s'"), 
			hf_name );
	}
	id = Find_str_value(&job.info,IDENTIFIER,Value_sep);
	if(!id) id = Find_str_value(&job.info,TRANSFERNAME,Value_sep);

	if( (destinations = Find_flag_value(&job.info,DESTINATIONS,Value_sep)) ){
		did = Find_str_value(&job.info,DESTINATION,Value_sep );
		Get_destination_by_name( &job, did );
	}
	new_dest = Find_str_value(&job.info,NEW_DEST,Value_sep);
	move_dest = Find_str_value(&job.info,MOVE_DEST,Value_sep);

	/*
	 * The following code is implementing job handling as follows.
	 *  if new_dest has a value then
	 *    new_dest has format 'pr' or 'pr@host'
	 *    if pr@host then
	 *       set RemotePrinter_DYN and RemoteHost_DYN
	 *   else
	 *       set RemotePrinter_DYN to pr
	 *       set RemoteHost_DYN to FQDNHost_FQDN
	 */

	if( new_dest || Bounce_queue_dest_DYN ){
		/* turn off filtering to output queues */
		if( move_dest ){
			Set_DYN(&Bounce_queue_dest_DYN,0);
		} else if( !new_dest ){
			new_dest = Bounce_queue_dest_DYN;
		}
		Set_DYN( &RemoteHost_DYN, 0);
		Set_DYN( &RemotePrinter_DYN, 0);
		Set_DYN( &Lp_device_DYN, 0);

		Set_DYN( &Lpd_port_DYN,
			Find_str_value(&Config_line_list,LPD_PORT,Value_sep) );

		Set_DYN( &RemotePrinter_DYN, new_dest );
		if( (s = safestrchr(RemotePrinter_DYN, '@')) ){
			*s++ = 0;
			Set_DYN( &RemoteHost_DYN, s );
			if( (s = safestrchr(s,'%')) ){
				*s++ = 0;
				Set_DYN( &Lpd_port_DYN,s );
			}
		}
		if( !RemoteHost_DYN ){
			Set_DYN( &RemoteHost_DYN, FQDNHost_FQDN);
		}
	}

	if( RemotePrinter_DYN ){
		Name = "(Worker - Remote)";
		DEBUG1( "Service_worker: sending '%s' to '%s@%s'",
			id, RemotePrinter_DYN, RemoteHost_DYN );
		setproctitle( "lpd %s '%s'", Name, Printer_DYN );
		if( Remote_support_DYN
			&& safestrchr( Remote_support_DYN, 'R' ) == 0 
			&& safestrchr( Remote_support_DYN, 'r' ) == 0 ){
			Errorcode = JABORT;
			setstatus( &job, "no remote support to `%s@%s'",
			RemotePrinter_DYN, RemoteHost_DYN );
		} else {
			Errorcode = Remote_job( &job, id );
		}
	} else {
		Name = "(Worker - Print)";
		DEBUG1( "Service_worker: printing '%s'",
			id, Printer_DYN );
		setproctitle( "lpd %s '%s'", Name, Printer_DYN );
		Errorcode = Local_job( &job, id );
	}
	cleanup(0);
}

/***************************************************************************
 * int Printer_open: opens the Printer_DYN
 ***************************************************************************/

/*
 * int Printer_open(
 *   lp_device        - open this device
 *   char error, int errlen - record errors
 *   int max_attempts - max attempts to open device or socket
 *   int interval,    - interval between attempts
 *   int max_interval, - maximum interval
 *   int grace,       - minimum time between attempts
 *   int connect_timeout - time to wait for connection success
 *   int *pid         - if we have a filter program, return pid
 */

int Printer_open( char *lp_device, struct job *job,
	int max_attempts, int interval, int max_interval, int grace,
	int connect_tmout, int *filterpid, int *errorpid )
{
	int attempt = 0, err = 0, n;
	int errors[2], in[2], pid, status;
	struct stat statb;
	int device_fd = -1, c;
	time_t tm;
	char tm_str[32];
	char *s, *host, *port, *iodevice, *filter;
	struct line_list args;

	Init_line_list(&args);
	host = port = iodevice = filter = 0;
	if( filterpid ) *filterpid = 0;
	if( errorpid ) *errorpid = 0;
	DEBUG1( "Printer_open: device '%s', max_attempts %d, grace %d, interval %d, max_interval %d",
		lp_device, max_attempts, grace, interval, max_interval );
	time( &tm );
	tm_str[0] = 0;
	if( lp_device == 0 ){
		fatal(LOG_ERR, "Printer_open: printer '%s' missing lp_device value",
			Printer_DYN );
	}
	c = lp_device[0];
	if( c == '|' ){
		filter = lp_device + 1;
	} else if( c == '/' ){
		iodevice = lp_device;
	} else if( safestrchr( lp_device, '%' ) ){
		/* we have a host%port form */
		host = safestrdup(lp_device, __FILE__,__LINE__);
		s = safestrchr(host,'%');
		*s++ = 0;
		port = s;
	} else {
		Errorcode = JABORT;
		fatal(LOG_ERR, "Printer_open: printer '%s', bad 'lp' entry '%s'", 
			Printer_DYN, lp_device );
	}


	/* set flag for closing on timeout */
	do{
		if( grace ) plp_sleep(grace);
		switch( c ){
		case '|':
			if( pipe( errors ) == -1 || pipe( in ) == -1 ){
				Errorcode = JFAIL;
				logerr_die( LOG_INFO,"Printer_open: pipe() failed");
			}
			DEBUG3("Printer_open: fd errors[%d,%d], in[%d,%d]",
				errors[0], errors[1], in[0], in[1] );
			/* set up file descriptors */
			Free_line_list(&args);
			Set_str_value(&args,NAME,"LP_FILTER");
			Set_str_value(&args,CALL,LOG);
			Set_str_value(&args,PRINTER,Printer_DYN);
			s = Find_str_value(&job->info,IDENTIFIER,Value_sep);
			if(!s) s = Find_str_value(&job->info,TRANSFERNAME,Value_sep);
			Set_str_value(&args,IDENTIFIER,s);
			if( (pid = Start_worker(&args,errors[0])) < 0 ){
				Errorcode = JFAIL;
				logerr_die(LOG_INFO,
					"Printer_open: could not create LP_FILTER error loggin process");
			}
			if( errorpid ) *errorpid = pid;

			Free_line_list(&args);
			Check_max(&args,10);
			args.list[args.count++] = Cast_int_to_voidstar(in[0]);	/* stdin */
			args.list[args.count++] = Cast_int_to_voidstar(1);	/* stdout */
			args.list[args.count++] = Cast_int_to_voidstar(errors[1]);	/* stderr */
			if( (pid = Make_passthrough( filter, Filter_options_DYN, &args,
					job, 0 )) < 0 ){
				Errorcode = JFAIL;
				logerr_die(LOG_INFO,
					"Printer_open: could not create LP_FILTER process");
			}
			args.count = 0;
			Free_line_list(&args);

			if( filterpid ) *filterpid = pid;
			if( (close( in[0] ) == -1 ) ){
				logerr_die( LOG_INFO,"Printer_open: close(%d) failed", in[0]);
			}
			if( (close( errors[0] ) == -1 ) ){
				logerr_die( LOG_INFO,"Printer_open: close(%d) failed", errors[0]);
			}
			if( (close( errors[1] ) == -1 ) ){
				logerr_die( LOG_INFO,"Printer_open: close(%d) failed", errors[1]);
			}
			device_fd = in[1];
			break;

		case '/':
			device_fd = Checkwrite_timeout( connect_tmout, lp_device, &statb, 
				(Lock_it_DYN || Read_write_DYN) ?(O_RDWR):(O_APPEND|O_WRONLY),
				0, Nonblocking_open_DYN );
			err = errno;
			if( Lock_it_DYN && device_fd >= 0 ){
				/*
				 * lock the device so that multiple servers can
				 * use it
				 */
				status = 0;
				if( isatty( device_fd ) ){
					status = LockDevice( device_fd, 0 );
				} else if( S_ISREG(statb.st_mode ) ){
					status = Do_lock( device_fd, 0 );
				}
				if( status <= 0 ){
					err = errno;
					setstatus( job,
						"lock '%s' failed - %s", lp_device, Errormsg(errno) );
					close( device_fd );
					device_fd = -1;
				}
			}
			break;
		default:
			DEBUG1( "Printer_open: doing link open '%s'", lp_device );
			device_fd = Link_open( host, port, connect_tmout, 0 );
			err = errno;
			break;
		}

		if( device_fd < 0 ){
			++attempt;
			DEBUG1( "Printer_open: open '%s' failed, attempt '%d' - %s",
				lp_device, attempt, Errormsg(err) );
			if( max_attempts && attempt >= max_attempts ){
				break;
			}
			n = 8;
			if( attempt < n ) n = attempt;
			n = interval*( 1 << (n-1) );
			if( max_interval > 0 && n > max_interval ) n = max_interval;
			DEBUG1( "Printer_open: cannot open device '%s' - '%s'", lp_device, Errormsg(err) );
			setstatus( job, "cannot open '%s' - '%s', attempt %d, sleeping %d",
					lp_device, Errormsg( err), attempt, n );
			if( n > 0 ){
				plp_sleep(n);
			}
		}
	} while( device_fd < 0 );
	if( device_fd >= 0 && isatty(device_fd ) ) Do_stty( device_fd );
	if( host ) free( host ); host = 0;

	DEBUG1 ("Printer_open: '%s' is fd %d", lp_device, device_fd);
	return( device_fd );
}
