/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpd_sendmail.c
 * PURPOSE: send mail to users on job completion
 **************************************************************************/

static char *const _id =
"$Id: lpd_sendmail.c,v 3.2 1996/08/31 21:11:58 papowell Exp papowell $";
#include "lpd.h"
#include "lp_config.h"
#include "printcap.h"
#include "pr_support.h"

/*
 * sendmail --- tell people about job completion
 * 1. fork a sendmail process
 * 2. if successful, send the good news
 * 3. if unsuccessful, send the bad news
 */

void Sendmail_to_user( int status, struct control_file *cfp,
	struct pc_used *pc_used )
{
	FILE *mail = 0;
	int fd;
	struct stat statb;
	char *pname;
	/*
	 * check to see if the user really wanted
	 * "your file was printed ok" message
	 */
	if( cfp->MAILNAME == 0
		|| cfp->MAILNAME[0] == 0
		|| cfp->MAILNAME[1] == 0 ){
		DEBUG3("Sendmail: no mail wanted");
		return;
	}
	if( !Sendmail || !*Sendmail) {
		DEBUG3("Sendmail: mail is turned off");
		return;
	}
	DEBUG3("Sendmail: '%s'", Sendmail );

	/* create the sendmail process */
	Make_filter( 'f', cfp, &Pr_fd_info, Sendmail, 1, 1, 1,pc_used, (void*)0, 0, 0);

	mail = fdopen( Pr_fd_info.input, "a+" );
	if( mail == 0 ){
		logerr_die( LOG_ERR, "Sendmail: fdopen failed" );
	}

	(void) fprintf( mail, "To: %s\n", cfp->MAILNAME+1 );
	if( status != JSUCC && Mail_operator_on_error ){
		fprintf( mail, "CC: %s\n", Mail_operator_on_error );
	}
	(void) fprintf( mail, "From: %s@%s\n", Printer, FQDNHost );
	(void) fprintf( mail, "Subject: %s@%s job %s\n\n",
		Printer, FQDNHost, cfp->name );

	/* now do the message */
	(void) fprintf( mail, "printer %s job %s", Printer, cfp->name );
	if( cfp->JOBNAME ){
		(void) fprintf( mail, " (%s)", cfp->JOBNAME+1 );
	}
	switch( status) {
	case JSUCC:
		(void) fprintf( mail, " was successful.\n");
		break;

	case JFAIL:
		(void) fprintf( mail, " failed, and retry count was exceeded.\n" );
		break;

	case JABORT:
		(void) fprintf( mail, " failed and could not be retried.\n" );
		break;

	default:
		(void) fprintf( mail, " died a horrible death.\n");
		break;
	}

	/*
	 * get the last status of the spooler
	 */
	pname = Add2_path( CDpathname, "status.", Printer );
	if( (fd = Checkread( pname, &statb ) ) >= 0 ){
		FILE *sfile;
		char msg[LINEBUFFER];

		sfile = fdopen( fd, "r" );
		if( sfile == 0 ){
			logerr_die( LOG_ERR, "Sendmail: fdopen failed" );
		}
		/*
		 * we read the file,writing each line out
		 */
		while( fgets( msg, sizeof(msg), sfile ) ){
			fprintf( mail, "   Status: %s", msg );
		}
		fclose( sfile );
	}
	close(fd);

	/*
	 * open the status file and copy the last set of status
	 * information to the user
	 */

	(void) fflush( mail );
	(void) fclose( mail );
	/* give the mail a chance */
	Close_filter( &Pr_fd_info, 2 );
}
