/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: removejob.c
 * PURPOSE: remove a job
 **************************************************************************/

static char *const _id =
"$Id: removejob.c,v 3.1 1996/08/25 22:20:05 papowell Exp papowell $";

#include "lp.h"
#include "jobcontrol.h"

/***************************************************************************
Commentary:
Patrick Powell Sat May 13 08:24:43 PDT 1995

 ***************************************************************************/

int Remove_job( struct control_file *cfp )
{
	int i;
	struct data_file *df;
	char *path;
	int fail = 0;
	struct stat statb;


	DEBUG4("Remove_job: '%s', spooldir '%s'",cfp->name,Clear_path(SDpathname));
	if( Interactive ){
		logDebug( "Removing job '%s'", cfp->name );
	}
	if( Debug > 3 ){
		dump_control_file( "Remove_job", cfp );
	}
	cfp->remove_time = time( (void *) 0 );
	if( Set_job_control( cfp ) ){
		DEBUG4( "Cannot update hold file for '%s'", cfp->name );
		fail = 1;
	}
		
	/* remove all of the data files listed in the control file */

	df = (void *)cfp->data_file.list;
	for( i = 0; i < cfp->data_file.count; ++i ){
		path = Add_path( SDpathname, df[i].openname );
		DEBUG4("Remove_job: removing file '%s'", path );
		unlink( path );
		if( stat( path, &statb ) == 0 ){
			log( LOG_ERR, "Remove_job: unlink did not remove '%s'",
				path);
			fail = 1;
		}
	}
	path = Add_path( SDpathname, cfp->name );
	DEBUG8("Remove_job: removing file '%s'", path );
	unlink( path );
	if( stat( path, &statb ) == 0 ){
		logerr( LOG_ERR, "Remove_job: unlink '%s' failed", path );
		fail = 1;
	}
	SDpathname->pathname[SDpathname->pathlen] = 'b';
	if( stat( path, &statb ) == 0 ){
		unlink( path );
		if( stat( path, &statb ) == 0 ){
			logerr( LOG_ERR, "Remove_job: unlink '%s' failed", path );
			fail = 1;
		}
	}
	fail = Remove_job_control( cfp );
	if( fail == 0 ){
		setmessage( cfp, "TRACE", "%s@%s: job removed", Printer, FQDNHost );
	}
	return( fail );
}
