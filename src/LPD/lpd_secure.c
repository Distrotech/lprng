/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpd_secure.c
 * PURPOSE: receive command over a secure/authenticated connection
 **************************************************************************/

static char *const _id =
"lpd_secure.c,v 3.14 1998/03/24 02:43:22 papowell Exp";

#include "lp.h"
#include "cleantext.h"
#include "errorcodes.h"
#include "fileopen.h"
#include "killchild.h"
#include "linksupport.h"
#include "malloclist.h"
#include "pathname.h"
#include "permission.h"
#include "setup_filter.h"
#include "setupprinter.h"
#include "waitchild.h"
#include "lockfile.h"
#include "jobcontrol.h"
#include "krb5_auth.h"
/**** ENDINCLUDE ****/

/***************************************************************************
Commentary:
Patrick Powell Mon Apr 17 05:43:48 PDT 1995

The protocol used to send a secure job consists of the following
following:

Client                                   Server
\REQ_SECUREprintername C/F user\n - receive a command
            0           1   2
\REQ_SECUREprintername C/F user controlfile\n - receive a job
            0           1   2

The server will return an ACK, and then start the authentication
process.  See README.security for details.

 ***************************************************************************/

int Receive_secure( int *socket, char *input, int maxlen, int transfer_timeout )
{
	char *orig_name;		/* line buffer for input */
	char tempbuf[LINEBUFFER];	/* line buffer for output */
	char linecopy[LINEBUFFER];	/* copy of input */
	char *authtype;				/* authentication type */
	char *fields[10];			/* fields in the line */
	int max_fields;				/* fields in the line */
	char *s, *end;
	char *filename = 0;
	char *user = 0;
	int fd[10];					/* file descriptor list */
	int pipe_fd[2];				/* status from authenticator */
	int report_fd[2];			/* status to authenticator */
	int temp_fd = -1;
	int hold_fd = -1;			/* hold file fd */
	struct printcap_entry *pc_entry = 0;
	int i, len, err, ack, status;
	struct stat statb;
	char tempfilename[MAXPATHLEN];	/* line buffer for input */
	char authentication[LINEBUFFER];	/* authentication information */
	int permission;
#if defined(HAVE_KRB5_H)
	int use_kerberos = 0;			/* use built-in kerberos */
#endif

	if( Cfp_static == 0 ){
		Cfp_static = malloc_or_die( sizeof(Cfp_static[0]) );
		memset(Cfp_static, 0, sizeof( Cfp_static[0] ) );
	}
	Clear_control_file( Cfp_static );
	Cfp_static->error[0] = 0;
	Cfp_static->remove_on_exit = 1;

	pipe_fd[0] = pipe_fd[1] = -1;
	report_fd[0] = report_fd[1] = -1;
	ack = 0;
	status = 0;


	/* clean up the printer name a bit */
	++input;
	trunc_str(input);
	safestrncpy( linecopy, input );
	/* get the fields on the input line */
	for( max_fields = 0, s = linecopy;
		max_fields < sizeof(fields)/sizeof(fields[0]) && s && *s;
			s = end, ++max_fields ){
		while( isspace(*s) ) ++s;
		end = strpbrk( s, " \t" );
		if( end ) *end++ = 0;
		fields[max_fields] = s;
	}
	fields[max_fields] = 0;
	if( max_fields < 4 || max_fields > 5 ){
		plp_snprintf( Cfp_static->error, sizeof(Cfp_static->error),
			_("bad command line '%s'"), input );
		ack = ACK_FAIL;	/* no retry, don't send again */
		goto error;
	}
	/*
     * \REQ_SECUREprintername C/F user authtype controlfile\n - receive a job
     *              0           1   2  3        4
	 */
	orig_name = fields[0];

	/* set the authentication information appropriately */
	Auth_from = 1;
	if(  fields[1][0] == 'F' ){
		Auth_from = 2;
	}
	user = fields[2];
	authtype = fields[3];
	Perm_check.authtype = authtype;
	filename = fields[4];
	if( Clean_name( orig_name ) ){
		plp_snprintf( Cfp_static->error, sizeof(Cfp_static->error),
			_("bad command '%s'"), input );
		ack = ACK_FAIL;	/* no retry, don't send again */
		goto error;
	}
	Name = "Receive_secure";
	proctitle( "lpd %s '%s'", Name, orig_name );

	/* set up cleanup and do initialization */
	register_exit( Remove_files, 0 );

	/* now you need to check about the secure option */

#if defined(HAVE_KRB5_H)
	/* check to see if built in */
	if( strcasecmp( authtype,"kerberos" ) == 0 ){
		DEBUG0("Receive_secure: using built-in kerberos" );
		use_kerberos = 1;
	} else
#endif
	if( Server_authentication_command == 0
		|| *Server_authentication_command == 0 ){
		plp_snprintf( Cfp_static->error, sizeof(Cfp_static->error),
			_("secure command not supported"), input );
		ack = ACK_FAIL;	/* no retry, don't send again */
		goto error;
	}

	if( filename ){
		/* try to get spool directory */
		if( Setup_printer( orig_name, Cfp_static->error,
			sizeof(Cfp_static->error), debug_vars,!DEBUGFSET(DAUTH1), (void *)0,
			&pc_entry ) ){
			DEBUGF(DRECV2)(
				"Receive_secure: Setup_printer failed '%s'",
				Cfp_static->error );
			ack = ACK_FAIL;	/* no retry, don't send again */
			goto error;
		}
		DEBUGF(DRECV2)("Receive_secure: spooling_disabled %d",
			Spooling_disabled );
		if( Spooling_disabled ){
			plp_snprintf( Cfp_static->error, sizeof(Cfp_static->error),
				_("%s: spooling disabled"), Printer );
			ack = ACK_RETRY;	/* retry */
			goto error;
		}

		/* check the filename format for consistency,
		 *	and make sure that the datafiles are the same as well
		 * We try at this point to make sure that there are no duplicate
		 *  job numbers in the queue
		 */
		safestrncpy( Cfp_static->original, filename );
		status = Check_format( CONTROL_FILE, filename, Cfp_static );
		if( status ){
			char buffer[LINEBUFFER];
			safestrncpy(buffer,Cfp_static->error);
			plp_snprintf( Cfp_static->error, sizeof(Cfp_static->error),
				_("%s: file '%s' name problems - %s"),
					Printer, filename, buffer );
			ack = ACK_FAIL;
			goto error;
		}
		/*
		 * get the non colliding job number
		 */
		if( (hold_fd
			= Find_non_colliding_job_number(Cfp_static,CDpathname)) < 0 ){
			goto error;
		}
		plp_snprintf( Cfp_static->transfername,
			sizeof(Cfp_static->transfername),
			"cf%c%0*d%s",
			Cfp_static->priority,
			Cfp_static->number_len,
			Cfp_static->number, Cfp_static->filehostname );
	} else {
		Setup_printer( orig_name, Cfp_static->error,
			sizeof(Cfp_static->error), debug_vars, !DEBUGFSET(DAUTH1), (void *)0,
			&pc_entry );
	}

	status = Link_ack( ShortRemote, socket, transfer_timeout, 0x100, 0 );
	if( status ){
		ack = ACK_RETRY;
		plp_snprintf( Cfp_static->error, sizeof(Cfp_static->error),
			_("%s: Receive_secure: sending ACK 0 failed"), Printer );
		goto error;
	}
	if( Server_user == 0 || *Server_user == 0 ){
		Server_user = Daemon_user;
	}
	/* make tempfile */
	temp_fd = Make_temp_fd( tempfilename, sizeof(tempfilename) );
	if( temp_fd < 0 ){
		err = errno;
		plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
			_("Receive_secure: cannot create temp file") );
		status = JFAIL;
		goto error;
	}
	close( temp_fd );
	temp_fd = -1;

#if defined(HAVE_KRB5_H)
	if( use_kerberos ){
		if( server_krb5_auth( Kerberos_keytab, Kerberos_service,
		*socket, authentication, sizeof(authentication)-1,
		Cfp_static->error, sizeof(Cfp_static->error), tempfilename ) ){
			status = JFAIL;
			goto error;
		}
		DEBUGF(DRECV1)("Receive_secure: Auth_from %d, authenticate '%s'",
			Auth_from, authentication );
	} else
#endif
	{
		plp_snprintf( tempbuf, sizeof(tempbuf),
			"%s -S -P%s -n%s -A%s -R%s -T%s",
			Server_authentication_command,
			Printer, user, authtype, Server_user, tempfilename );

		/* now set up the file descriptors:
		 *   FD Options Purpose
		 *   0  R/W     socket connection to remote host (R/W)
		 *   1  W       for status report about authentication
		 *   2  W       error log
		 *   3  R       for server status to be sent to client
		 */

		if( pipe(pipe_fd) <  0 ){
			Errorcode = JFAIL;
			logerr_die( LOG_INFO, _("Send_files: pipe failed") );
		};
		if( pipe(report_fd) <  0 ){
			Errorcode = JFAIL;
			logerr_die( LOG_INFO, _("Send_files: pipe failed") );
		}
		i = 0;
		fd[i++] = *socket;
		fd[i++] = pipe_fd[1];
		fd[i++] = 0;
		fd[i++] = report_fd[0];

		/* create the secure process */
		if( Make_passthrough( &Passthrough_receive, tempbuf, fd, 4,
			0, Cfp_static, pc_entry ) < 0 ){
			goto error;
		}
		/* set up closing */
		Passthrough_receive.input = *socket;
		Passthrough_receive.output = report_fd[1];

		close( pipe_fd[1] );	/* close writing side */
		pipe_fd[1] = -1;
		close( report_fd[0] );  /* close reading side */
		report_fd[0] = -1;

		/* now we wait for the status */

		len = read( pipe_fd[0], authentication, sizeof(authentication)-1 );
		if( len <= 0 ){
			status = JFAIL;
			plp_snprintf( Cfp_static->error, sizeof(Cfp_static->error),
				_("Receive_secure: transfer process failed") );
			goto error;
		}

		/* we have the authenticated name information */
		authentication[len] = 0;
		trunc_str(authentication);
		DEBUGF(DRECV1)("Receive_secure: Auth_from %d, authenticate '%s'",
			Auth_from, authentication );
		/* we now send all information to the authenticator */
		DEBUGF(DRECV1)("Receive_secure: report_fd %d dup to socket %d",
			report_fd[1], *socket );
		if( dup2( report_fd[1], *socket ) == -1 ){
			err = errno;
			plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
				_("Receive_secure: dup of %d to %d failed - %s"),
				report_fd[1], *socket, Errormsg(err));
			status = JFAIL;
			goto error;
		}
		close( report_fd[1] );
		report_fd[1] = -1;
	}

	if( (temp_fd = Checkwrite(tempfilename, &statb, O_RDWR, 0, 0 ) ) < 0 ){
		err = errno;
		plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
			_("Receive_secure: reopen of '%s' failed - %s"),
			tempfilename, Errormsg(err));
		status = JFAIL;
		goto error;
	}
	if( lseek( temp_fd, 0, SEEK_SET ) < 0 ){
		err = errno;
		plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
			_("Receive_secure: lseek of temp_fd failed - %s"),
			Errormsg(err));
		status = JFAIL;
		goto error;
	}
	if( filename ){
		if( (status = Scan_block_file( temp_fd, Cfp_static )) ){
			goto error;
		}
		/* now we check the permissions for LPR job */

		if( Auth_from == 1 ){
			plp_snprintf( Cfp_static->auth_id, sizeof( Cfp_static->auth_id),
				"_%s", authentication );
		} else {
			if( Cfp_static->auth_id[0] == 0 ){
				plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
					_("Receive_secure: authenication missing in forwarded job"));
				status = JFAIL;
				goto error;
			}
			plp_snprintf( Cfp_static->forward_id,
				sizeof( Cfp_static->forward_id), "_%s", authentication );
		}
		safestrncpy( Cfp_static->authtype, authtype );
		DEBUGF(DRECV1)("Receive_secure: auth_id '%s'", Cfp_static->auth_id );
		DEBUGF(DRECV1)("Receive_secure: forward_id '%s'",Cfp_static->forward_id);
		DEBUGF(DRECV1)("Receive_secure: authtype '%s'",Cfp_static->authtype);

		if( (status = Do_perm_check( Cfp_static )) ){
			DEBUGF(DRECV1)("Receive_secure: Do_perm_check failed '%d'",status);
			goto error;
		}

		/* now we process the job */

		status = Check_for_missing_files( Cfp_static, &Data_files,
			orig_name, authentication, &hold_fd, pc_entry );

	} else {
		/* we read the command */

		int size;
		char buffer[LARGEBUFFER];
		char *command;
		char *forward = 0;

		if( fstat( temp_fd, &statb ) == -1 ){
			plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
				"Receive_secure: fstat of '%s' failed - %s",
				tempfilename, Errormsg(errno) );
			status = JFAIL;
			goto error;
		}
		size = statb.st_size;
		len = statb.st_size;
		DEBUGF(DRECV1)("Receive_secure: file '%s' size '%d'",
			tempfilename, len );
		if( size >= sizeof(buffer) ){
			plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
				_("Receive_secure: command file too large") );
			status = JFAIL;
			goto error;
		}
		for( s = buffer, i = len = size;
			len > 0 && (i = read( temp_fd, s, len ) ) > 0;
			len -= i, s += i );
		*s = 0;
		DEBUGF(DRECV1)("Receive_secure: contents '%s'", buffer );
		if( i <= 0 ){
			err = errno;
			plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
				_("Receive_secure: read of temp_fd failed - %s"),
				Errormsg(err));
			status = JFAIL;
			goto error;
		}
		/* we now need to process the command.  First, check to see if
		 *	there is a forwarded indication
		 */
		trunc_str( buffer );
		command = buffer;
		forward = strchr( command, '\n' );
		if( forward ){
			*forward++ = 0;
			if( *forward == 0 ){
				forward = 0;
			}
			if( forward && (s = strchr( forward, '\n' )) ){
				*s++ = 0;
				if( *s ){
					plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
						_("Receive_secure: wrong number of lines in file") );
					status = JFAIL;
					goto error;
				}
			}
		}
		DEBUGF(DRECV1)("Receive_secure: Auth_from %d, command '%s', forward '%s'",
			Auth_from, command, forward );
		if( Auth_from == 2 && forward == 0 ){
			plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
				_("Receive_secure: missing forwarded authentication") );
			status = JFAIL;
			goto error;
		}
		if( forward ){
			if( Auth_from != 2 ){
				plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
					_("Receive_secure: forward information and not from server") );
				status = JFAIL;
				goto error;
			}
			plp_snprintf( Cfp_static->forward_id,
				sizeof( Cfp_static->forward_id), "_%s", forward );
		}
		if( Auth_from == 1 ){
			plp_snprintf( Cfp_static->auth_id, sizeof( Cfp_static->auth_id),
				"_%s", authentication );
		} else {
			plp_snprintf( Cfp_static->forward_id,
				sizeof( Cfp_static->forward_id), "_%s", authentication );
		}
		safestrncpy( Cfp_static->authtype, authtype );
		DEBUGF(DRECV1)("Receive_secure: auth_id '%s'", Cfp_static->auth_id );
		DEBUGF(DRECV1)("Receive_secure: forward_id '%s'",Cfp_static->forward_id);
		DEBUGF(DRECV1)("Receive_secure: authtype '%s'",Cfp_static->authtype);
		/* now we simply farm the command off to the command parser */
		DEBUGF(DRECV1)("Receive_secure: command '%s' id '0x%x'",
			command, command[0] );
		len = strlen(command)+1;
		switch( command[0] ){
		default:
			plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
				_("Receive_secure: bad request line '%s'"), command );
			status = JFAIL;
			goto error;
			break;
		case REQ_START:
			Perm_check.service = 'S';
			Perm_check.printer = command+1;
			if( (s = strchr( command+1, '\n' )) ) *s = 0;
			Init_perms_check();
			if( (permission = Perms_check( &Perm_file, &Perm_check,
				Cfp_static )) == REJECT
				|| (permission == 0 && Last_default_perm == REJECT) ){
				
				plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
					_("Receive_secure: no permission to start printer %s"),
					command+1 );
				goto error;
			}
			command[0] = '!';
			Write_fd_str( Lpd_pipe[1], command );
			break;
		case REQ_DSHORT:
		case REQ_DLONG:
			Job_status( socket, command, len );
			break;
		case REQ_REMOVE:
			Job_remove( socket, command, len );
			break;
		case REQ_CONTROL:
			Job_control( socket, command, len );
			break;
		}
		status = 0;
		goto done;
	}

	/* we have just received the job with no errors */

error:
	DEBUGF(DRECV1)("Receive_secure: error - status %d, ack %d, error '%s'",
		status, ack, Cfp_static->error );
	if( status || ack ){
		if( ack ) (void)Link_ack( ShortRemote, socket, transfer_timeout, ack, 0 );
		if( status == 0 ) status = JFAIL;
		DEBUGF(DRECV1)("Receive_secure: sending ACK %d, msg '%s'",
			ack, Cfp_static->error );
		/* shut down reception from the remote file */
		if( Cfp_static->error[0] ){
			safestrncat( Cfp_static->error, "\n" );
			Write_fd_str( *socket, Cfp_static->error );
		}
	}

	/* we are done */
done:
	DEBUGF(DRECV1)("Receive_secure: done - status %d, ack %d, filename '%s'",
		status, ack, filename );
	Link_close( socket );
	if( pipe_fd[0] > 0 ) close( pipe_fd[0] );
	if( pipe_fd[1] > 0 ) close( pipe_fd[1] );
	if( report_fd[0] > 0 ) close( report_fd[0] );
	if( report_fd[1] > 0 ) close( report_fd[1] );
	if( hold_fd > 0 ){
		close( hold_fd );
		hold_fd = -1;
	}

	if( status == 0 && ack == 0 && filename ){
		/* start a new server */
		DEBUGF(DRECV1)("Receive_secure: starting server");
		Cfp_static->remove_on_exit = 0;
		Start_new_server();
		Errorcode = 0;
	}
	return(status);
}
