/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: accounting.c,v 5.1 1999/09/12 21:32:32 papowell Exp papowell $";


#include "lp.h"
#include "accounting.h"
#include "lpd.h"
#include "getqueue.h"
#include "errorcodes.h"
#include "child.h"
#include "linksupport.h"
#include "fileopen.h"
/**** ENDINCLUDE ****/

int Do_accounting( int end, char *command, struct job *job, int timeout )
{
	int i, err, filter, pid, n, errors[2], len, tempfd;
	char msg[SMALLBUFFER];
	char *s, *tempfile;
	struct line_list args;
	plp_status_t status;

	Init_line_list(&args);
	msg[0] = 0;

	DEBUG2("Do_accounting: command '%s'", command );

	Split(&args,command,Whitespace,0,0,0,0,0);
	if( args.count == 0 ) return(0);
	filter = (cval(args.list[0]) == '|');

	err = JSUCC;
	if( filter ){
		tempfd = 1;
		if( end == 0 && Accounting_check_DYN ){
			tempfd = Make_temp_fd( &tempfile );
		}
		if( pipe(errors) == -1 ){
			Errorcode = JFAIL;
			logerr_die(LOG_INFO,
				"Do_accounting: pipe() failed");
		}
		Free_line_list(&args);
		Check_max(&args,10);
		args.list[args.count++] = (void *) 0;
		args.list[args.count++] = (void *) tempfd;
		args.list[args.count++] = Cast_int_to_voidstar(errors[1]);
		if( (pid = Make_passthrough( command, Filter_options_DYN, &args, job, 0 )) < 0 ){
			logerr_die(LOG_INFO,
				"Do_accounting: could not create '%s'", command );
		}
		args.count = 0;
		Free_line_list(&args);
		close( errors[1] ); errors[1] = -1;

		n = 0;
		while( n < sizeof(msg)-1
			&& (len = read(errors[0],msg+n,sizeof(msg)-1-n)) > 0 ){
			msg[n+len] = 0;
			while( (s = safestrchr(msg,'\n')) ){
				*s++ = 0;
				setstatus(job,"ACCOUNTING error '%s'", msg );
				memmove(msg,s,strlen(s)+1);
			}
		}
		close( errors[0] ); errors[0] = -1;

		while( (n = plp_waitpid(pid,&status,0)) != pid );
		if( WIFEXITED(status) && (err = WEXITSTATUS(status)) ){
			DEBUG1("Do_accounting: process exited with status %d", err);
			if( err && err < 32 ) err += 31;
		} else if( WIFSIGNALED(status) ){
			n = WTERMSIG(status);
			DEBUG1( "Do_accounting: process died with signal %d, '%s'",
				n, Sigstr(n));
			err = JABORT;
		}
		msg[0] = 0;
		if( err == 0 && end == 0 && Accounting_check_DYN ){
			if( lseek(tempfd,0,SEEK_SET) < 0 ){
				logerr_die( LOG_INFO,"Do_accounting: lseek tempfile failed");
			}
			len = 0;
			while( len < sizeof(msg)-1
				&& (n = read(tempfd,msg+len,sizeof(msg)-1-len)) > 0 ){
				DEBUG1("Do_accounting: read %d, '%s'", n, msg );
			}
			if( strlen(msg) == 0 ) safestrncpy(msg,"accept");
			close(tempfd);
		}
	} else if( Accounting_port > 0 ){
		Fix_dollars(&args, job);
		command = Join_line_list_with_quotes( &args, " ");
		DEBUG1( "Do_accounting: expanded command is '%s'", command );
		s = command+strlen(command)-1;
		s[0] = '\n';
		err = 0;
		if( Write_fd_str( Accounting_port, command ) <  0 ){
			logerr( LOG_INFO, "Do_accounting: write failed" );
			plp_snprintf( msg, sizeof(msg), "accounting port write failed - %s",
				Errormsg(errno) );
			err = JFAIL;
		}
		if( command ) free( command ); command = 0;
		msg[0] = 0;
		if( err == 0 && end == 0 && Accounting_check_DYN ){
			i = sizeof(msg) - 1;
			err = Link_line_read( "ACCOUNTING SERVER", &Accounting_port,
				timeout, msg, &i );
			msg[i] = 0;
		}
	}
	if( err == 0 && Accounting_check_DYN ){
		Free_line_list(&args);
		s = "";
		Split(&args,msg,Whitespace,0,0,0,0,0);
		if( args.count ) s = args.list[0];
		if( !safestrcasecmp( s, "accept" ) ){
			err = JSUCC;
		} else if( !safestrcasecmp( s, "hold" ) ){
			err = JHOLD;
		} else if( !safestrcasecmp( s, "remove" ) ){
			err = JREMOVE;
		} else if( !safestrcasecmp( s, "fail" ) ){
			err = JFAIL;
		} else {
			plp_snprintf( msg, sizeof(msg),
				"accounting check failed - status message '%s'", s );
			err = JABORT;
		}
	}
	Free_line_list(&args);
	DEBUG2("Do_accounting: status %s", Server_status(err) );
	return( err );
}

int Setup_accounting( struct job *job )
{
	int n, p[2], pid, errors[2], error_pid;
	struct stat statb;
	char *command, *s;
	struct line_list args;
	int err;

	Init_line_list(&args);
	DEBUG2("Setup_accounting: '%s'", Accounting_file_DYN);
	if( !(command = Accounting_file_DYN ) ) return 0;
	if( cval(command) == '|' ){
		if( pipe(errors) == -1 ){
			logerr_die(LOG_INFO,
				"Setup_accounting: pipe() failed" );
		}
		Free_line_list(&args);
		Set_str_value(&args,NAME,"ACCT_ERR");
		Set_str_value(&args,CALL,LOG);
		Set_str_value(&args,PRINTER,Printer_DYN);
		s = Find_str_value(&job->info,IDENTIFIER,Value_sep);
		if(!s) s = Find_str_value(&job->info,TRANSFERNAME,Value_sep);
		Set_str_value(&args,IDENTIFIER,s);
		if( (error_pid = Start_worker(&args,errors[0])) < 0 ){
			Errorcode = JFAIL;
			logerr_die(LOG_INFO,
				"Setup_accounting: could not create ACCT_ERR error loggin process");
		}
		if( close(errors[0]) == -1 ){
			logerr_die(LOG_INFO,
				"Setup_accounting: close errors[1]=%d failed", errors[0] );
		}

		if( pipe(p) == -1 ){
			logerr_die(LOG_INFO,
				"Setup_accounting: pipe() failed" );
		}

		Free_line_list(&args);
		Check_max(&args,10);
		args.list[args.count++] = (void *)0;
		args.list[args.count++] = Cast_int_to_voidstar(p[1]);
		args.list[args.count++] = Cast_int_to_voidstar(errors[1]);
		if( (pid = Make_passthrough( command, Filter_options_DYN,
			&args, job, 0 )) < 0 ){
			logerr_die(LOG_INFO,
				"Setup_accounting: could not create '%s'", command );
		}
		args.count = 0;
		Free_line_list( &args );
		if( close(errors[1]) == -1 ){
			logerr_die(LOG_INFO,
				"Setup_accounting: close errors[1]=%d failed", errors[1] );
		}
		if( close(p[1]) == -1 ){
			logerr_die(LOG_INFO,
				"Setup_accounting: close p[1]=%d failed", p[1] );
		}
		Accounting_port = p[0];
	} else if( safestrchr( command, '%' ) ){
		char *host, *port;
		/* now try to open a connection to a server */
		host = safestrdup(command,__FILE__,__LINE__);
		port = safestrchr( host, '%' );
		*port++ = 0;
		
		DEBUG2("Setup_accounting: connecting to '%s'%'%s'",host,port);
		if( (n = Link_open(host,port,Connect_timeout_DYN,0)) < 0 ){
			err = errno;
			Errorcode= JFAIL;
			logerr_die(LOG_INFO,
				_("connection to accounting server '%s' failed '%s'"),
				Accounting_file_DYN, Errormsg(err) );
		}
		DEBUG2("Setup_accounting: socket %d", n );
		if( host ) free(host); host = 0;
		Accounting_port = n;
	} else {
		n = Checkwrite( Accounting_file_DYN, &statb, 0, Create_files_DYN, 0 );
		if( n > 0 ){
			Accounting_port = n;
		}
		DEBUG2("Setup_accounting: fd %d", n );
	}
	return(0);
}
