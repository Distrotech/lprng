/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

#include "lp.h"
#include "errorcodes.h"
#include "globmatch.h"
#include "gethostinfo.h"
#include "child.h"
#include "fileopen.h"
#include "getqueue.h"
#include "getprinter.h"
#include "lpd_logger.h"
#include "lpd_dispatch.h"
#include "lpd_jobs.h"
#include "linelist.h"
#include "lpd_worker.h"

/* this file contains code that was formerly in linelist.c but split
 * out as it is only needed in lpd and pulls much code with it */

static void Do_work( const char *name, struct line_list *args );

/*
 *  Support for non-copy on write fork as for NT
 *   1. Preparation for the fork is done by calling 'Setup_lpd_call'
 *      This establishes a default setup for the new process by setting
 *      up a list of parameters and file descriptors to be passed.
 *   2. The user then adds fork/process specific options
 *   3. The fork is done by calling Make_lpd_call which actually
 *      does the fork() operation.  If the lpd_path option is set,
 *      then a -X command line flag is added and an execv() of the program
 *      is done.
 *   4.A - fork()
 *        Make_lpd_call (child) will call Do_work(),  which dispatches
 *         a call to the required function.
 *   4.B - execv()
 *        The execv'd process checks the command line parameters for -X
 *         flag and when it finds it calls Do_work() with the same parameters
 *         as would be done for the fork() version.
 */

void Setup_lpd_call( struct line_list *passfd, struct line_list *args )
{
	Free_line_list( args );
	Check_max(passfd, 10 );
	passfd->count = 0;
	passfd->list[passfd->count++] = Cast_int_to_voidstar(0);
	passfd->list[passfd->count++] = Cast_int_to_voidstar(1);
	passfd->list[passfd->count++] = Cast_int_to_voidstar(2);
	if( Mail_fd > 0 ){
		Set_decimal_value(args,MAIL_FD,passfd->count);
		passfd->list[passfd->count++] = Cast_int_to_voidstar(Mail_fd);
	}
	if( Status_fd > 0 ){
		Set_decimal_value(args,STATUS_FD,passfd->count);
		passfd->list[passfd->count++] = Cast_int_to_voidstar(Status_fd);
	}
	if( Logger_fd > 0 ){
		Set_decimal_value(args,LOGGER,passfd->count);
		passfd->list[passfd->count++] = Cast_int_to_voidstar(Logger_fd);
	}
	if( Lpd_request > 0 ){
		Set_decimal_value(args,LPD_REQUEST,passfd->count);
		passfd->list[passfd->count++] = Cast_int_to_voidstar(Lpd_request);
	}
	Set_flag_value(args,DEBUG,Debug);
	Set_flag_value(args,DEBUGFV,DbgFlag);
#ifdef DMALLOC
	{
		extern int dmalloc_outfile_fd;
		if( dmalloc_outfile_fd > 0 ){
			Set_decimal_value(args,DMALLOC_OUTFILE,passfd->count);
			passfd->list[passfd->count++] = Cast_int_to_voidstar(dmalloc_outfile_fd);
		}
	}
#endif
}

/*
 * Make_lpd_call - does the actual forking operation
 *  - sets up file descriptor for child, can close_on_exec()
 *  - does fork() or execve() as appropriate
 *
 *  returns: pid of child or -1 if fork failed.
 */

int Make_lpd_call( const char *name, struct line_list *passfd, struct line_list *args )
{
	int pid, fd, i, n, newfd;
	struct line_list env;

	Init_line_list(&env);
	pid = dofork(1);
	if( pid ){
		return(pid);
	}
	Name = "LPD_CALL";

	if(DEBUGL2){
		LOGDEBUG("Make_lpd_call: name '%s', lpd path '%s'", name, Lpd_path_DYN );
		LOGDEBUG("Make_lpd_call: passfd count %d", passfd->count );
		for( i = 0; i < passfd->count; ++i ){
			LOGDEBUG(" [%d] %d", i, Cast_ptr_to_int(passfd->list[i]));
		}
		Dump_line_list("Make_lpd_call - args", args );
	}
	for( i = 0; i < passfd->count; ++i ){
		fd = Cast_ptr_to_int(passfd->list[i]);
		if( fd < i  ){
			/* we have fd 3 -> 4, but 3 gets wiped out */
			do{
				newfd = dup(fd);
				Max_open(newfd);
				if( newfd < 0 ){
					Errorcode = JABORT;
					LOGERR_DIE(LOG_INFO)"Make_lpd_call: dup failed");
				}
				DEBUG4("Make_lpd_call: fd [%d] = %d, dup2 -> %d",
					i, fd, newfd );
				passfd->list[i] = Cast_int_to_voidstar(newfd);
			} while( newfd < i );
		}
	}
	if(DEBUGL2){
		LOGDEBUG("Make_lpd_call: after fixing fd count %d", passfd->count);
		for( i = 0 ; i < passfd->count; ++i ){
			fd = Cast_ptr_to_int(passfd->list[i]);
			LOGDEBUG("  [%d]=%d",i,fd);
		}
	}
	for( i = 0; i < passfd->count; ++i ){
		fd = Cast_ptr_to_int(passfd->list[i]);
		DEBUG2("Make_lpd_call: fd %d -> %d",fd, i );
		if( dup2( fd, i ) == -1 ){
			Errorcode = JABORT;
			LOGERR_DIE(LOG_INFO)"Make_lpd_call: dup2(%d,%d) failed",
				fd, i );
		}
	}
	/* close other ones to simulate close_on_exec() */
	n = Max_fd+10;
	for( i = passfd->count ; i < n; ++i ){
		close(i);
	}
	passfd->count = 0;
	Free_line_list( passfd );
	Do_work( name, args );
	return(0);
}

static void Do_work( const char *name, struct line_list *args )
{
	WorkerProc *proc = NULL;
	Logger_fd = Find_flag_value(args, LOGGER);
	Status_fd = Find_flag_value(args, STATUS_FD);
	Mail_fd = Find_flag_value(args, MAIL_FD);
	Lpd_request = Find_flag_value(args, LPD_REQUEST);
	/* undo the non-blocking IO */
	if( Lpd_request > 0 ) Set_block_io( Lpd_request );
	Debug= Find_flag_value( args, DEBUG );
	DbgFlag= Find_flag_value( args, DEBUGFV );
#ifdef DMALLOC
	{
		extern int dmalloc_outfile_fd;
		dmalloc_outfile_fd = Find_flag_value(args, DMALLOC_OUTFILE);
	}
#endif
	if( !safestrcasecmp(name,"logger") ) proc = Logger;
	else if( !safestrcasecmp(name,"all") ) proc = Service_all;
	else if( !safestrcasecmp(name,"server") ) proc = Service_connection;
	else if( !safestrcasecmp(name,"queue") ) proc = Service_queue;
	else if( !safestrcasecmp(name,"printer") ) proc = Service_worker;
	DEBUG3("Do_work: '%s', proc 0x%lx ", name, Cast_ptr_to_long(proc) );
	(proc)(args);
	cleanup(0);
}

/*
 * Start_worker - general purpose dispatch function
 *   - adds an input FD
 */

int Start_worker( const char *name, struct line_list *parms, int fd )
{
	struct line_list passfd, args;
	int pid;

	Init_line_list(&passfd);
	Init_line_list(&args);
	if(DEBUGL1){
		DEBUG1("Start_worker: '%s' fd %d", name, fd );
		Dump_line_list("Start_worker - parms", parms );
	}
	Setup_lpd_call( &passfd, &args );
	Merge_line_list( &args, parms, Hash_value_sep,1,1);
	Free_line_list( parms );
	if( fd ){
		Check_max(&passfd,2);
		Set_decimal_value(&args,INPUT,passfd.count);
		passfd.list[passfd.count++] = Cast_int_to_voidstar(fd);
	}

	pid = Make_lpd_call( name, &passfd, &args );
	Free_line_list( &args );
	passfd.count = 0;
	Free_line_list( &passfd );
	DEBUG1("Start_worker: pid %d", pid );
	return(pid);
}
