/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: removejob.c
 * PURPOSE: remove a job
 **************************************************************************/

static char *const _id =
"$Id: removejob.c,v 3.4 1997/01/30 21:15:20 papowell Exp $";

#include "lp.h"
#include "removejob.h"
#include "dump.h"
#include "jobcontrol.h"
#include "fileopen.h"
#include "setstatus.h"
/**** ENDINCLUDE ****/

/***************************************************************************
Commentary:
Patrick Powell Sat May 13 08:24:43 PDT 1995

 ***************************************************************************/

int Remove_file( char *path )
{
	struct stat statb;

	DEBUG4("Remove_file: removing file '%s'", path );
	if( path[0] && stat( path, &statb ) == 0 && unlink( path ) == -1 ){
		log( LOG_ERR, "Remove_file: unlink did not remove '%s'", path);
		return( 1 );
	}
	return(0);
}

int Remove_job( struct control_file *cfp )
{
	int i;
	struct data_file *df;
	int fail = 0;
	int fd = -1;
	struct control_file *old_cfp;
	struct stat statb;


	DEBUG3("Remove_job: '%s'",cfp->transfername);
	if( Interactive ){
		logDebug( "Removing job '%s'", cfp->transfername );
	}
	if(DEBUGL3 ){
		dump_control_file( "Remove_job", cfp );
	}


	if( cfp->hold_file[0] &&
		stat( cfp->hold_file, &statb ) == 0 ){
		cfp->hold_info.remove_time = time( (void *) 0 );
		Set_job_control( cfp, (void *) 0, 0 );
	}
	/* remove all of the data files listed in the control file */

	df = (void *)cfp->data_file_list.list;
	for( i = 0; i < cfp->data_file_list.count; ++i ){
		fail |= Remove_file( df->openname );
	}
	fail |= Remove_file( cfp->openname );
	fail |= Remove_file( cfp->hold_file );

	/* remove temp files, if any */
	old_cfp = Cfp_static;
	Cfp_static = cfp;
	Remove_tempfiles();
	Cfp_static = old_cfp;
	close(fd);

	if( fail == 0 ){
		setmessage( cfp, "TRACE", "%s@%s: job removed", Printer, FQDNHost );
	} else {
		setmessage( cfp, "TRACE", "%s@%s: job removal FAILED", Printer, FQDNHost );
	}
	return( fail );
}
