/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: removejob.c
 * PURPOSE: remove a job
 **************************************************************************/

static char *const _id =
"removejob.c,v 3.7 1997/12/16 15:06:32 papowell Exp";

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
	int err;

	DEBUG4("Remove_file: removing file '%s'", path );
	errno = 0;
	if( path[0] && stat( path, &statb ) == 0 && unlink( path ) == -1 ){
		err = errno;
		log( LOG_ERR, "Remove_file: unlink did not remove '%s'", path);
		errno = err;
		return( 1 );
	}
	return(0);
}

int Remove_job( struct control_file *cfp )
{
	int i, len;
	struct data_file *df;
	int fail = 0, jf;
	struct stat statb;
	char buffer[LARGEBUFFER];


	DEBUG3("Remove_job: '%s'",cfp->transfername);
	buffer[0] = 0;
	if( Interactive ){
		logDebug( "Removing job '%s'", cfp->transfername );
	}
	if(DEBUGL3 ){
		dump_control_file( "Remove_job", cfp );
	}


	if( cfp->hold_file[0] &&
		stat( cfp->hold_file, &statb ) == 0 ){
		cfp->hold_info.remove_time = time( (void *) 0 );
		Set_job_control( cfp, (void *) 0 );
	}
	/* remove all of the data files listed in the control file */

	len = strlen(buffer);
	if( cfp->identifier[0] ){
		plp_snprintf(buffer,sizeof(buffer)-len,
			"job '%s'\n", cfp->identifier+1 );
	}
	len = strlen(buffer);
	plp_snprintf(buffer,sizeof(buffer)-len,
		" control file '%s'\n", cfp->transfername );
	df = (void *)cfp->data_file_list.list;
	for( i = 0; i < cfp->data_file_list.count; ++i ){
		len = strlen(buffer);
		plp_snprintf(buffer,sizeof(buffer)-len,
			" data file '%s'\n", df[i].openname );
	}
	setmessage( cfp, "TRACE", "%s@%s: Remove_job\n%s", Printer, FQDNHost,
		buffer );
	for( i = 0; i < cfp->data_file_list.count; ++i ){
		jf = Remove_file( df[i].openname );
		if( jf ) setmessage( cfp, "TRACE",
			"%s@%s: removal of '%s' failed - %s", Printer, FQDNHost,
			df[i].openname, Errormsg(errno));
		fail |= jf;
	}
	jf = Remove_file( cfp->openname );
	if( jf ) setmessage( cfp, "TRACE",
		"%s@%s: removal of '%s' failed - %s", Printer, FQDNHost,
		cfp->openname, Errormsg(errno));
	fail |= jf;
	jf = Remove_file( cfp->hold_file );
	if( jf ) setmessage( cfp, "TRACE",
		"%s@%s: removal of '%s' failed - %s", Printer, FQDNHost,
		cfp->hold_file, Errormsg(errno));
	fail |= jf;

	/* remove temp files, if any */
	Remove_tempfiles();

	if( fail == 0 ){
		setmessage( cfp, "TRACE", "%s@%s: job removed - Remove_job", Printer, FQDNHost );
	} else {
		setmessage( cfp, "TRACE", "%s@%s: job removal FAILED - Remove_job", Printer, FQDNHost );
	}
	return( fail );
}
