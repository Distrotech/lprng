/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: sendmail.c,v 5.1 1999/09/12 21:32:53 papowell Exp papowell $";

#include "lp.h"
#include "errorcodes.h"
#include "fileopen.h"
#include "getqueue.h"
#include "sendmail.h"
#include "child.h"
/**** ENDINCLUDE ****/

/*
 * sendmail --- tell people about job completion
 * 1. fork a sendmail process
 * 2. if successful, send the good news
 * 3. if unsuccessful, send the bad news
 */

void Sendmail_to_user( int retval, struct job *job )
{
	char buffer[LARGEBUFFER];
	int in[2], out[2], pid, n, len;
	char *id, *mailname, *s;
	plp_status_t status;
	struct line_list files;

	/*
	 * check to see if the user really wanted
	 * "your file was printed ok" message
	 */
	Init_line_list(&files);
	id = Find_str_value(&job->info,IDENTIFIER,Value_sep);
	if(!id) id = Find_str_value(&job->info,TRANSFERNAME,Value_sep);
	mailname = Find_str_value(&job->info,MAILNAME,Value_sep);
	DEBUG2("Sendmail_to_user: MAILNAME '%s' sendmail '%s'", mailname, Sendmail_DYN );
	if( mailname == 0 || safestrchr( mailname, '%' ) ){
		mailname = 0;
		if( retval != JSUCC ){
			mailname = Mail_operator_on_error_DYN;
		}
	}
	if( !mailname || !Sendmail_DYN ) return;
	DEBUG2("Sendmail_to_user: using '%s'", mailname );
	if( pipe(in) == -1 || pipe(out) == -1 ){
		logerr_die( LOG_ERR, _("Sendmail_to_user: pipe failed") );
	}

	Free_line_list(&files);
	Check_max(&files,10);
	files.list[files.count++] = Cast_int_to_voidstar(in[0]);
	files.list[files.count++] = Cast_int_to_voidstar(out[1]);
	files.list[files.count++] = Cast_int_to_voidstar(out[1]);
	pid = Make_passthrough( Sendmail_DYN, 0, &files, job, 0 );
	files.count = 0;
	Free_line_list(&files);

	close(in[0]);
	close(out[1]);

	plp_snprintf( buffer, sizeof(buffer),
		"To: %s\n", mailname );
	if( retval != JSUCC && Mail_operator_on_error_DYN
		&& mailname != Mail_operator_on_error_DYN  ){
		len = strlen(buffer);
		plp_snprintf(buffer+len,sizeof(buffer)-len,
			"CC: %s\n", Mail_operator_on_error_DYN );
	}
	len = strlen(buffer);
	plp_snprintf(buffer+len,sizeof(buffer)-len,
		"From: %s@%s\n",
		Mail_from_DYN ? Mail_from_DYN : Printer_DYN, FQDNHost_FQDN );
	len = strlen(buffer);
	plp_snprintf(buffer+len,sizeof(buffer)-len,
		"Subject: %s@%s job %s\n\n",
		Printer_DYN, FQDNHost_FQDN, id );

	/* now do the message */
	len = strlen(buffer);
	plp_snprintf(buffer+len,sizeof(buffer)-len,
		_("printer %s job %s"), Printer_DYN, id );

	len = strlen(buffer);
	switch( retval) {
	case JSUCC:
		plp_snprintf(buffer+len,sizeof(buffer)-len,
		_(" was successful.\n"));
		break;

	case JFAIL:
		plp_snprintf(buffer+len,sizeof(buffer)-len,
		_(" failed, and retry count was exceeded.\n") );
		break;

	case JABORT:
		plp_snprintf(buffer+len,sizeof(buffer)-len,
		_(" failed and could not be retried.\n") );
		break;

	default:
		plp_snprintf(buffer+len,sizeof(buffer)-len,
		_(" died a horrible death.\n"));
		break;
	}

	/*
	 * get the last status of the spooler
	 */
	if( (s = Get_file_image( Spool_dir_DYN, Queue_status_file_DYN, Max_status_size_DYN )) ){
		len = strlen(buffer);
		plp_snprintf(buffer+len,sizeof(buffer)-len, "\nStatus:\n\n%s", s);
		if(s) free(s); s = 0;
	}

	if( Status_file_DYN && (s = Get_file_image( Spool_dir_DYN,
		Status_file_DYN, Max_status_size_DYN )) ){
		len = strlen(buffer);
		plp_snprintf(buffer+len,sizeof(buffer)-len, "\nFilter Status:\n\n%s", s);
		if(s) free(s); s = 0;
	}
	
	Write_fd_str( in[1], buffer );
	close( in[1] );
	buffer[0] = 0;
	len = 0;
	while( len < sizeof(buffer)-1
		&& (n = read(out[0],buffer+len,sizeof(buffer)-len-1)) >0 ){
		buffer[n+len] = 0;
		while( (s = safestrchr(buffer,'\n')) ){
			*s++ = 0;
			setstatus(job,"mail: %s", buffer );
			memmove(buffer,s,strlen(s)+1);
		}
		len = strlen(buffer);
	}
	close(out[0]);
	while( (n = plp_waitpid(pid,&status,0)) != pid );
	DEBUG1("Sendmail_to_user: pid %d, exit status '%s'", pid,
		Decode_status(&status) );
	if( WIFEXITED(status) && (n = WEXITSTATUS(status)) ){
		setstatus(job,"mail exited with status %d", n);
	} else if( WIFSIGNALED(status) ){
		setstatus(job,"mail died with signal %d, '%s'",
			n, Sigstr(n));
	}
	DEBUG1("Sendmail_to_user: done");
	return;
}
