/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: lpd_secure.c,v 5.2 1999/10/09 20:48:44 papowell Exp papowell $";


#include "lp.h"
#include "lpd.h"
#include "lpd_secure.h"

#include "getopt.h"
#include "getqueue.h"
#include "proctitle.h"
#include "permission.h"
#include "linksupport.h"
#include "errorcodes.h"
#include "fileopen.h"
#include "lpd_rcvjob.h"
#include "child.h"
#include "globmatch.h"
#include "lpd_jobs.h"
#include "krb5_auth.h"
#include "sendauth.h"

/**** ENDINCLUDE ****/

/***************************************************************************
 * Commentary:
 * Patrick Powell Mon Apr 17 05:43:48 PDT 1995
 * 
 * The protocol used to send a secure job consists of the following
 * following:
 * 
 * Client                                   Server
 * \REQ_SECUREprintername C/F user\n - receive a command
 *             0           1   2
 * \REQ_SECUREprintername C/F user controlfile\n - receive a job
 *             0           1   2
 * 
 * The server will return an ACK, and then start the authentication
 * process.  See README.security for details.
 * 
 ***************************************************************************/

int Receive_secure( int *sock, char *input )
{
	char *printername;
	char error[SMALLBUFFER];	/* error message */
	char buffer[SMALLBUFFER];
	char *authtype;
	char *cf, *s;
	char *jobsize = 0;
	char *user = 0;
	int pipe_fd[2], report_fd[2], error_fd[2], pid, error_pid;
	int temp_fd = -1;
	int len, err, ack, status, read_len, n, from_server;
	struct stat statb;
	char *tempfile;
	struct line_list args, options, files;

	Init_line_list(&args);
	Init_line_list(&options);
	Init_line_list(&files);

	Name = "RCVSEC";
	error[0] = 0;
	pid = error_pid = 0;

	pipe_fd[0] = pipe_fd[1] = report_fd[0] = report_fd[1]
		= error_fd[0] = error_fd[1] = -1;
	ack = 0;
	status = 0;

	DEBUGF(DRECV1)("Receive_secure: input line '%s'", input );
	Split(&args,input+1,Whitespace,0,0,0,0,0);
	DEBUGFC(DRECV1)Dump_line_list("Receive_secure - input", &args);
	if( args.count != 5 && args.count != 4 ){
		plp_snprintf( error, sizeof(error),
			_("bad command line '%s'"), input );
		ack = ACK_FAIL;	/* no retry, don't send again */
		status = JFAIL;
		goto error;
	}
	Check_max(&args,1);
	args.list[args.count] = 0;

	/*
     * \REQ_SECUREprintername C/F user authtype jobsize\n - receive a job
     *              0           1   2  3        4
	 */
	printername = args.list[0];
	cf = args.list[1];
	user = args.list[2];	/* user is escape encoded */
	Unescape(user);
	authtype = args.list[3];
	Unescape(authtype);
	jobsize = args.list[4];

	setproctitle( "lpd %s '%s'", Name, printername );

	Perm_check.authfrom = CLIENT;
	from_server = 0;
	if( *cf == 'F' ){
		Perm_check.authfrom = SERVER;
		from_server = 1;
	}
	Perm_check.authtype = authtype;

	if( Clean_name( printername ) ){
		plp_snprintf( error, sizeof(error),
			_("bad printer name '%s'"), input );
		ack = ACK_FAIL;	/* no retry, don't send again */
		status = JFAIL;
		goto error;
	}

	Set_DYN(&Printer_DYN,printername);

	if( Setup_printer( printername, error, sizeof(error) ) ){
		if( jobsize ){
			plp_snprintf( error, sizeof(error),
				_("bad printer '%s'"), printername );
			ack = ACK_FAIL;	/* no retry, don't send again */
			status = JFAIL;
			goto error;
		}
	} else {
		int db, dbf;

		db = Debug;
		dbf = DbgFlag;
		s = Find_str_value(&Spool_control,DEBUG,Value_sep);
		if(!s) s = New_debug_DYN;
		Parse_debug( s, 0 );

		if( !(DRECVMASK & DbgFlag) ){
			Debug = db;
			DbgFlag = dbf;
		} else {
			dbf = Debug;
			Debug = db;
			if( Log_file_DYN ){
				temp_fd = Checkwrite( Log_file_DYN, &statb,0,0,0);
				if( temp_fd > 0 && temp_fd != 2 ){
					dup2(temp_fd,2);
					close(temp_fd);
				}
				temp_fd = -1;
			}
			Debug = dbf;
			logDebug("Receive_secure: socket fd %d", *sock );
			Dump_line_list("Receive_secure - input", &args);
		}
#ifndef NODEBUG 
		DEBUGF(DRECV1)("Receive_job: debug '%s', Debug %d, DbgFlag 0x%x", s, Debug, DbgFlag );
#endif
	}
	Fix_auth(0,0);



	DEBUGF(DRECV1)("Receive_secure: Auth_DYN '%s', Auth_id_DYN '%s', Auth_receive_filter_DYN '%s'",
		Auth_DYN, Auth_id_DYN, Auth_receive_filter_DYN );

	if( jobsize ){
		read_len = strtol(jobsize,0,10);

		DEBUGF(DRECV2)("Receive_secure: spooling_disabled %d",
			Sp_disabled(&Spool_control) );
		if( Sp_disabled(&Spool_control) ){
			plp_snprintf( error, sizeof(error),
				_("%s: spooling disabled"), Printer_DYN );
			ack = ACK_RETRY;	/* retry */
			status = JFAIL;
			goto error;
		}
		if( Max_job_size_DYN > 0 && (read_len+1023)/1024 > Max_job_size_DYN ){
			plp_snprintf( error, sizeof(error),
				_("%s: job size %d is larger than %d K"),
				Printer_DYN, read_len, Max_job_size_DYN );
			ack = ACK_RETRY;
			status = JFAIL;
			goto error;
		} else if( !Check_space( read_len, Minfree_DYN, Spool_dir_DYN ) ){
			plp_snprintf( error, sizeof(error),
				_("%s: insufficient file space"), Printer_DYN );
			ack = ACK_RETRY;
			status = JFAIL;
			goto error;
		}
	}

#if defined(HAVE_KRB5_H)
	/* check to see if built in */
	if( safestrcasecmp( authtype,KERBEROS5) == 0
		|| safestrcasecmp( authtype,KERBEROS) == 0 ){
		DEBUGF(DRECV1)( "Receive_secure: kerberos keytab '%s'",
			Kerberos_keytab_DYN );
		if( Kerberos_keytab_DYN
			&& stat(Kerberos_keytab_DYN,&statb) == 0 ){
			status = Link_send( ShortRemote_FQDN, sock,
					Send_query_rw_timeout_DYN,"",1,0 );
			if( !status ) status = Krb5_receive( sock, authtype, user, jobsize, from_server );
			goto error;
		} else {
			plp_snprintf( error, sizeof(error),
				_("kerberos keytab '%s' not present"), Kerberos_keytab_DYN );
			ack = ACK_RETRY;
			status = JFAIL;
		}
		goto error;
	}
#endif

	
	DEBUGF(DRECV1)( "Receive_secure: pgp path '%s', passphrasefile '%s'",
		Pgp_path_DYN, Pgp_server_passphrasefile_DYN );
	if( (safestrcasecmp( authtype,PGP ) == 0) ){
		int fd;
		if( Pgp_path_DYN == 0 ){
			plp_snprintf( error, sizeof(error),
				_("missing pgp program path") );
			ack = ACK_RETRY;
			status = JFAIL;
			goto error;
		}
		if( Pgp_server_passphrasefile_DYN == 0 ){
			logerr( LOG_INFO, "Receive_secure: no Pgp_server_passphrasefile value");
			plp_snprintf( error, sizeof(error),
				_("no pgp server passphrasefile") );
			ack = ACK_RETRY;
			status = ACK_RETRY;
			status = JFAIL;
			goto error;
		}
		if( (fd = Checkread(Pgp_server_passphrasefile_DYN, &statb) ) < 0 ){
			logerr( LOG_INFO,
				_("Receive_secure: cannot open passphrasefile '%s' on server"),
				Pgp_server_passphrasefile_DYN );
			plp_snprintf( error, sizeof(error),
				_("bad pgp server passphrasefile") );
			ack = ACK_RETRY;
			status = ACK_RETRY;
			status = JFAIL;
			goto error;
		}
		close(fd);
		status = Link_send( ShortRemote_FQDN, sock,
			Send_query_rw_timeout_DYN,"",1,0 );
		if( !status ) status = Pgp_receive( sock, user, jobsize, from_server );
		goto error;
	}

	if( safestrcasecmp( authtype, Auth_DYN ) || Auth_receive_filter_DYN == 0 ){
		plp_snprintf( error, sizeof(error),
			_("authentication type '%s' not supported"), authtype );
		ack = ACK_RETRY;
		status = JFAIL;
		goto error;
	}

	DEBUGF(DRECV1)( "Receive_secure: sending 0 ack value");
	status = Link_send( ShortRemote_FQDN, sock,
			Send_query_rw_timeout_DYN,"",1,0 );
	if( status ){
		goto error;
	}

	/* make tempfile */
	temp_fd = Make_temp_fd( &tempfile );
	close( temp_fd );
	temp_fd = -1;

	plp_snprintf( buffer, sizeof(buffer),
		"%s -S -P%s -n%s -A%s -R%s -T%s",
		Auth_receive_filter_DYN, Printer_DYN, esc_Auth_id_DYN,
		authtype, user, tempfile );

	DEBUGF(DRECV1)( "Receive_secure: rcv authenticator '%s'", buffer);

	/* now set up the file descriptors:
	 *   FD Options Purpose
	 *   0  R/W     socket connection to remote host (R/W)
	 *   1  W       for status report about authentication
	 *   2  W       error log
	 *   3  R       for server status to be sent to client
	 */

	if( pipe(pipe_fd) == -1 || pipe(report_fd) == -1 || pipe(error_fd) == -1 ){
		Errorcode = JFAIL;
		logerr_die( LOG_INFO, _("Receive_secure: pipe failed") );
	}

	Free_line_list(&options);
	Set_str_value(&options,PRINTER,Printer_DYN);
	Set_str_value(&options,NAME,"RCVSEC_ERR");
	Set_str_value(&options,CALL,LOG);
	if( (error_pid = Start_worker(&options,error_fd[0])) < 0 ){
		Errorcode = JFAIL;
		logerr_die(LOG_INFO,"Receive_secure: fork failed");
	}
	Free_line_list(&options);

	files.count = 0;
	Check_max(&files,10);
	files.list[files.count++] = Cast_int_to_voidstar(*sock);
	files.list[files.count++] = Cast_int_to_voidstar(pipe_fd[1]);
	files.list[files.count++] = Cast_int_to_voidstar(error_fd[1]);
	files.list[files.count++] = Cast_int_to_voidstar(report_fd[0]);

	/* create the secure process */
	if((pid = Make_passthrough( buffer, 0, &files, 0, 0 )) < 0 ){
		Errorcode = JFAIL;
		logerr_die( LOG_INFO, _("Receive_secure: fork failed") );
	}
	files.count = 0;
	Free_line_list(&files);

	/* we now send all information to the authenticator */
	DEBUGF(DRECV1)("Receive_secure: report_fd %d dup to socket %d",
		report_fd[1], *sock );
	if( dup2( report_fd[1], *sock ) == -1 ){
		err = errno;
		plp_snprintf( error, sizeof( error),
			_("Receive_secure: dup of %d to %d failed - %s"),
			report_fd[1], *sock, Errormsg(err));
		status = JFAIL;
		goto error;
	}
	close( pipe_fd[1] ); pipe_fd[1] = -1;
	close( error_fd[1] ); error_fd[1] = -1;
	close( error_fd[0] ); error_fd[0] = -1;
	close( report_fd[0] ); report_fd[0] = -1;
	close( report_fd[1] ); report_fd[1] = -1;

	/* now we wait for the authentication info */

	n = 0;
	while( n < sizeof(buffer)-1
		&& (len = read( pipe_fd[0], buffer+n, sizeof(buffer)-1-n )) > 0 ){
		buffer[n+len] = 0;
		DEBUGF(DRECV1)("Receive_secure: read authentication '%s'", buffer );
		while( (s = safestrchr(buffer,'\n')) ){
			*s++ = 0;
			if( strlen(buffer) == 0 ){
				break;
			}
			Add_line_list(&options,buffer,Value_sep,1,1);
			memmove(buffer,s,strlen(s)+1);
		}
	}
	DEBUGFC(DRECV1)Dump_line_list("Receive_secure - received", &options );

	close(pipe_fd[0]); pipe_fd[0] = -1;
	if( (status = Check_secure_perms( &options, from_server)) ){
		DEBUGF(DRECV1)("Receive_secure: no permission");
		goto error;
	}

	/* now we do the dirty work */
	if( (s = Find_str_value(&options,INPUT,Value_sep)) ){
		DEBUGF(DRECV1)("Receive_secure: undecoded command '%s'", s );
		Unescape(s);
		DEBUGF(DRECV1)("Receive_secure: decoded command '%s'", s );
		Dispatch_input( sock, s );
		status = 0;
		goto error;
	} else if( jobsize ){
		if( (temp_fd = Checkread(tempfile, &statb) ) < 0 ){
			err = errno;
			plp_snprintf( error, sizeof( error),
				_("Receive_secure: reopen of '%s' failed - %s"),
				tempfile, Errormsg(err));
			status = JFAIL;
			goto error;
		}
		if( (status = Scan_block_file( temp_fd, error, sizeof(error)-4 )) ){
			goto error;
		}
	}

	/* we have just received the job with no errors */

 error:

	DEBUGF(DRECV1)("Receive_secure: status %d, ack %d, error '%s'",
		status, ack, error );
	if( status ){
		if( ack == 0 ) ack = ACK_FAIL;
		buffer[0] = ack;
		buffer[1] = 0;
		(void)Link_send( ShortRemote_FQDN, sock,
			Send_query_rw_timeout_DYN, buffer, 1, 0 );
		if( error[0] ){
			safestrncat( error, "\n" );
			(void)Link_send( ShortRemote_FQDN, sock,
				Send_query_rw_timeout_DYN, error, strlen(error), 0 );
		}
		Errorcode = JFAIL;
	}

	if( pipe_fd[1] >=0 )close( pipe_fd[1]  ); pipe_fd[1] = -1;
	if( pipe_fd[0] >=0 )close( pipe_fd[0]  ); pipe_fd[0] = -1;
	if( error_fd[1] >=0 )close( error_fd[1]  ); error_fd[1] = -1;
	if( error_fd[0] >=0 )close( error_fd[0]  ); error_fd[0] = -1;
	if( report_fd[1] >=0 )close( report_fd[1]  ); report_fd[1] = -1;
	if( report_fd[0] >=0 )close( report_fd[0]  ); report_fd[0] = -1;
	close( *sock ); *sock = -1;

	/* we are done */
	DEBUGF(DRECV1)("Receive_secure: done - status %d, ack %d, jobsize '%s'",
		status, ack, jobsize );
	while( pid > 0 || error_pid >0 ){
		plp_status_t procstatus;
		memset(&procstatus,0,sizeof(procstatus));
		n = plp_waitpid(-1,&procstatus,0);
		DEBUGF(DRECV1)("Receive_secure: pid %d, exit status '%s'",
			n, Decode_status(&procstatus) );
		if( n == pid ) pid = 0;
		if( n == error_pid ) error_pid = 0;
	}
	Remove_tempfiles();

	if( status == 0 && jobsize ){
		/* start a new server */
		DEBUGF(DRECV1)("Receive_secure: starting server");
		if( Server_queue_name_DYN ){
			Do_queue_jobs( Server_queue_name_DYN, 0, 0 );
		} else {
			Do_queue_jobs( Printer_DYN, 0, 0 );
		}
	}
	cleanup(0);
	return(0);
}

int Pgp_receive( int *sock, char *user, char *jobsize, int from_server )
{
	char *tempfile, *s, *cmdstr, *t;
	int tempfd, error_fd[2], pipe_fd[2], status, cnt, n, pid;
	char buffer[LARGEBUFFER];
	struct line_list env, files, args, pgp_info, file_info;
	struct stat statb;
	plp_status_t procstatus;
	double len;

	Init_line_list(&env);
	Init_line_list(&args);
	Init_line_list(&files);
	Init_line_list(&pgp_info);
	Init_line_list(&file_info);
	pipe_fd[0] = pipe_fd[1] = error_fd[0] = error_fd[1] = -1;
	

	DEBUGF(DRECV1)( "Pgp_receive: path '%s', key '%s'",
		Pgp_path_DYN, Pgp_server_passphrasefile_DYN );
	cnt = sizeof(buffer)-1;
	status = Link_line_read(ShortRemote_FQDN,sock,
		Send_job_rw_timeout_DYN,buffer,&cnt);
	DEBUGF(DRECV1)( "Pgp_receive: read status %d, cnt %d, '%s'",
		status, cnt, buffer );
	tempfd = Make_temp_fd(&tempfile);
	if( status || cnt == 0 ){
		DEBUGF(DRECV1)( "Pgp_receive: bad read from socket" );
		goto error;
	}
	buffer[cnt] = 0;
	len = strtod(buffer,0);
	while( len  > 0 ){
		cnt = sizeof(buffer)-1;
		if( cnt > len ) cnt = len;
		if( (n = read(*sock, buffer,cnt) != cnt ) ){
			DEBUGF(DRECV1)("Pgp_receive: bad read from socket" );
			goto error;
		}
		buffer[cnt] = 0;
		DEBUGF(DRECV2)("Pgp_receive: remote sent '%s'", buffer );
		if( write( tempfd,buffer,cnt ) != cnt ){
			DEBUGF(DRECV1)( "Ppg_receive: bad write to '%s' - '%s'",
				tempfile, Errormsg(errno) );
			goto error;
		}
		len -= cnt;
	}
	close(tempfd); tempfd = -1;

	if( pipe(error_fd) == -1 ){
		DEBUGF(DRECV1)("Pgp_receive: pipe failed - '%s'", Errormsg(errno) );
		goto error;
	}

	Check_max(&files,10);
	files.list[files.count++] = Cast_int_to_voidstar(0);
	files.list[files.count++] = Cast_int_to_voidstar(error_fd[1]);
	files.list[files.count++] = Cast_int_to_voidstar(error_fd[1]);
	if( (tempfd = Checkread(Pgp_server_passphrasefile_DYN,&statb)) < 0 ){
		DEBUGF(DRECV1)("Pgp_receive: cannot open '%s' - '%s'", Errormsg(errno));
		goto error;
	}
	Set_decimal_value(&env,"PGPPASSFD",files.count);
	files.list[files.count++] = Cast_int_to_voidstar(tempfd);

	/* now we run the PGP code */

	plp_snprintf(buffer,sizeof(buffer),
		"%s +force +batch %s -u $%%%s -o %s",
		Pgp_path_DYN, tempfile, esc_Auth_id_DYN, tempfile ); 

	if( (pid = Make_passthrough(buffer, 0, &files, 0, &env )) < 0 ){
		DEBUGF(DRECV1)("Pgp_receive: fork failed - %s", Errormsg(errno));
		goto error;
	}

	files.count = 0;
	Free_line_list(&env);
	close(tempfd); tempfd = -1;
	close(error_fd[1]);

	/* now we read authentication info from pgp */

	n = 0;
	while( n < sizeof(buffer)-1
		&& (cnt = read( error_fd[0], buffer+n, sizeof(buffer)-1-n )) > 0 ){
		buffer[n+cnt] = 0;
		while( (s = safestrchr(buffer,'\n')) ){
			*s++ = 0;
			DEBUGF(DRECV1)("Pgp_receive: pgp output '%s'", buffer );
			if( !safestrncmp("Good",buffer,4) ){
				if( (t = safestrrchr(buffer,'"')) ) *t = 0;
				if( (t = safestrchr(buffer,'"')) ){
					*t++ = 0;
					if( *t ){
						Set_str_value(&pgp_info,FROM,t);
					}
				}
			}
			memmove(buffer,s,strlen(s)+1);
		}
	}
	close(error_fd[0]); error_fd[0] = -1;

	/* wait for pgp to exit and check status */
	while( pid > 0 ){
		n = waitpid(pid,&procstatus,0);
		DEBUGF(DRECV1)("Pgp_receive: pid %d, exit status '%s'",
			n, Decode_status(&procstatus));
		if( n == pid ) pid = 0;
	}
	if( WIFEXITED(procstatus) && (n = WEXITSTATUS(procstatus)) ){
		DEBUGF(DRECV1)("Pgp_receive: pgp exited with status %d", n);
		goto error;
	} else if( WIFSIGNALED(procstatus) ){
		n = WTERMSIG(procstatus);
		DEBUGF(DRECV1)( "Pgp_receive: pgp died with signal %d, '%s'",
			n, Sigstr(n));
		goto error;
	}

	DEBUGFC(DRECV1)Dump_line_list("Receive_secure - pgp reports", &env );
	if( !(s = Find_str_value(&pgp_info,FROM,Value_sep)) ){
		logmsg(LOG_INFO,"pgp: no from information");
		goto error;
	}
	if( (tempfd = Checkread( tempfile, &statb )) < 0 ){
		DEBUGF(DRECV1)("Pgp_receive: open '%s' failed - %s",
			tempfile, Errormsg(errno));
		goto error;
	}
	n = 0;
	while( n < sizeof(buffer)-1
		&& (cnt = read( tempfd, buffer+n, sizeof(buffer)-1-n )) > 0 ){
		buffer[n+cnt] = 0;
		while( (s = safestrchr(buffer,'\n')) ){
			*s++ = 0;
			if( strlen(buffer) == 0 ){
				break;
			}
			DEBUGF(DRECV1)("Pgp_receive: decoded output '%s'", buffer );
			Add_line_list(&pgp_info,buffer,Value_sep,1,1);
			memmove(buffer,s,strlen(s)+1);
		}
	}
	close(tempfd); tempfd = -1;

	DEBUGFC(DRECV1)Dump_line_list("Pgp_receive - header", &pgp_info );

	if( Check_secure_perms( &pgp_info, from_server ) ){
		DEBUGF(DRECV1)("Pgp_receive: Check_secure_perms failed");
		goto error;
	}
	buffer[0] = 0;
	if( (cmdstr = Find_str_value(&pgp_info,INPUT,Value_sep)) ){
		DEBUGF(DRECV1)("Pgp_receive: encoded cmd - '%s'", cmdstr );
		Unescape(cmdstr);
		DEBUGF(DRECV1)("Pgp_receive: decoded cmd - '%s'", cmdstr );
		Set_DYN(&esc_Auth_received_id_DYN, (s = Escape(Auth_received_id_DYN,1,1)));
		if(s) free(s); s = 0;
		if( pipe(error_fd) == -1 || pipe(pipe_fd) == -1 ){
			DEBUGF(DRECV1)("Pgp_receive: pipe failed - '%s'", Errormsg(errno) );
			goto error;
		}

		Free_line_list(&args);
		Set_str_value(&args,NAME,"PGP_FILTER");
		Set_str_value(&args,CALL,LOG);
		Set_str_value(&args,PRINTER,Printer_DYN);
		if( (pid = Start_worker(&args,error_fd[0])) < 0 ){
			logerr(LOG_INFO, "Printer_open: could not create LP_FILTER error loggin process");
			goto error;
		}
		Free_line_list(&args);
		Register_exit( "Wait_for_child", (exit_ret)Wait_for_child, Cast_int_to_voidstar(pid) );

		/* now we do the dirty work */
		Check_max(&files,10);
		files.count = 0;
		files.list[files.count++] = Cast_int_to_voidstar(pipe_fd[0]);
		files.list[files.count++] = Cast_int_to_voidstar(*sock);
		files.list[files.count++] = Cast_int_to_voidstar(error_fd[1]);
		if( (tempfd = Checkread(Pgp_server_passphrasefile_DYN,&statb)) < 0 ){
			logerr(LOG_INFO,"Pgp_send: cannot open '%s'", Errormsg(errno));
			goto error;
		}   
		Set_decimal_value(&env,"PGPPASSFD",files.count);
		files.list[files.count++] = Cast_int_to_voidstar(tempfd);  

		/* now we run the PGP code */

		plp_snprintf(buffer,sizeof(buffer),
			"$- %s +force +batch -sef $%%%s -u $%%%s",
			Pgp_path_DYN, esc_Auth_received_id_DYN, esc_Auth_id_DYN );
		DEBUGF(DRECV2)("Pgp_send: cmd '%s'", buffer );
		if( (pid = Make_passthrough(buffer, 0, &files, 0, &env )) < 0 ){
			logerr(LOG_INFO,"Pgp_send: '%s' failed", buffer);
			goto error;
		}
		files.count = 0;

		close(tempfd); tempfd = -1;
		if( dup2( pipe_fd[1], *sock ) == -1 ){
			logerr( LOG_INFO, "Pgp_send: dup of %d to %d failed", pipe_fd[1], *sock );
			goto error;
		}
		if( pipe_fd[1] >=0 )close( pipe_fd[1]  ); pipe_fd[1] = -1;
		if( pipe_fd[0] >=0 )close( pipe_fd[0]  ); pipe_fd[0] = -1;
		if( error_fd[1] >=0 )close( error_fd[1]  ); error_fd[1] = -1;
		if( error_fd[0] >=0 )close( error_fd[0]  ); error_fd[0] = -1;
		Free_line_list(&env);
		Free_line_list(&files);
		Dispatch_input( sock, cmdstr );
		if( *sock >= 0 ) close(*sock); *sock = -1;
		while( pid > 0 ){
			n = waitpid(pid,&procstatus,0);
			DEBUGF(DRECV1)("Pgp_receive: pid %d, exit status '%s'",
				n, Decode_status(&procstatus));
			if( n == pid ) pid = 0;
		}
	} else if( jobsize ){
		if( (tempfd = Checkread(tempfile, &statb) ) < 0 ){
			DEBUGF(DRECV1)("Pgp_receive: reopen of '%s' failed - %s",
				tempfile, Errormsg(errno));
			goto error;
		}
		if( (status = Scan_block_file( tempfd, buffer, sizeof(buffer)-4 )) ){
			DEBUGF(DRECV1)("Pgp_receive: '%s'", buffer );
			goto error;
		}
	}

	status = 0;
	goto finished;

 error:
	status = 1;
 finished:
	if( pipe_fd[1] >=0 )close( pipe_fd[1]  ); pipe_fd[1] = -1;
	if( pipe_fd[0] >=0 )close( pipe_fd[0]  ); pipe_fd[0] = -1;
	if( error_fd[1] >=0 )close( error_fd[1]  ); error_fd[1] = -1;
	if( error_fd[0] >=0 )close( error_fd[0]  ); error_fd[0] = -1;
	if( tempfd>=0) close(tempfd); tempfd = -1;
	Free_line_list(&args);
	Free_line_list(&env);
	Free_line_list(&pgp_info);
	Free_line_list(&file_info);
	files.count = 0;
	Free_line_list(&files);
	return(status);
}

int Krb5_receive( int *sock, char *authtype, char *user,
	char *jobsize, int from_server )
{
	char buffer[SMALLBUFFER], *client;
	char *tempfile, *s, *cmdstr;
	int fd = -1, n, len;
	int status = 0;
	struct line_list header_info;
	struct stat statb;
	int linecount = 0, done = 0;

	client = 0;
	Init_line_list(&header_info);

	fd = Make_temp_fd(&tempfile);
	close(fd);

	DEBUGF(DRECV1)("Krb5_receive: starting, jobsize '%s'", jobsize );
	if( server_krb5_auth( Kerberos_keytab_DYN, Kerberos_service_DYN, *sock,
		&client, buffer, sizeof(buffer), tempfile ) ){
		DEBUGF(DRECV1)("Krb5_receive: receive error '%s'\n", buffer );
		goto done;
	}
	DEBUGF(DRECV1)("Krb5_receive: user '%s'", client );

	if( (fd = Checkread(tempfile,&statb)) < 0 ){ 
		DEBUGF(DRECV1)("Krb5_receive: reopen of '%s' failed - %s",
			tempfile, Errormsg(errno));
		goto done;
	}
	buffer[0] = 0;
	n = 0;
	done = 0;
	linecount = 0;

	while( !done && n < sizeof(buffer)-1
		&& (len = read( fd, buffer+n, sizeof(buffer)-1-n )) > 0 ){
		buffer[n+len] = 0;
		while( !done && (s = safestrchr(buffer,'\n')) ){
			*s++ = 0;
			if( strlen(buffer) == 0 ){
				done = 1;
				break;
			}
			DEBUGF(DRECV1)("Krb5_receive: line [%d] '%s'", linecount, buffer );
			switch( linecount ){
				case 0:
					if( jobsize ){
						if( from_server ){
							Set_str_value(&header_info,CLIENT,buffer);
						}
						done = 1;
					} else {
						Set_str_value(&header_info,INPUT,buffer); break;
					}
					break;
				case 1:
					Set_str_value(&header_info,CLIENT,buffer);
					done = 1;
					break;
			}
			++linecount;
			memmove(buffer,s,strlen(s)+1);
			n = strlen(buffer);
		}
	}

	close(fd); fd = -1;

	Set_str_value(&header_info,FROM,client);
	if( client ) free( client ); client = 0;
	DEBUGFC(DRECV1)Dump_line_list("Krb5_receive - header", &header_info );

	if( Check_secure_perms( &header_info, from_server ) ){
		DEBUGF(DRECV1)("Krb5_receive: Check_secure_perms failed");
		goto error;
	}

	buffer[0] = 0;
	if( jobsize ){
		if( (fd = Checkread(tempfile, &statb) ) < 0 ){
			DEBUGF(DRECV1)("Krb5_receive: reopen of '%s' failed - %s",
				tempfile, Errormsg(errno));
			goto error;
		}
		if( (status = Scan_block_file( fd, buffer, sizeof(buffer)-4 )) ){
			DEBUGF(DRECV1)("Krb5_receive: '%s'", buffer );
			goto error;
		}
	} else if( (cmdstr = Find_str_value(&header_info,INPUT,Value_sep)) ){
		if( (fd = Checkwrite(tempfile,&statb,O_WRONLY|O_TRUNC,1,0)) < 0 ){
			DEBUGF(DRECV1)("Krb5_receive: reopen of '%s' failed - %s",
				tempfile, Errormsg(errno));
			goto done;
		}
		Dispatch_input( &fd, cmdstr );
		close(fd); fd = -1;
		if( server_krb5_status( *sock, buffer,sizeof(buffer), tempfile ) ){
			DEBUGF(DRECV1)("Krb5_receive: status send failed - '%s'",
				buffer );
			goto done;
		}
	}

 done:
	status = 0;
	goto finished;

 error:
	status = 1;
 finished:
	if( fd>=0) close(fd); fd = -1;
	Free_line_list(&header_info);
	return(status);
}

int Check_secure_perms( struct line_list *options, int from_server )
{
	/*
	 * line 1 - CLIENT=xxxx   - client authentication
	 * line 2 - SERVER=xxxx   - server authentication
	 * ...    - FROM=xxxx     - from
	 * line 3 - INPUT=\00x  - command line
	 */
	Set_DYN(&Auth_received_id_DYN, Find_str_value(options,FROM,Value_sep));
	if( from_server ){
		Set_DYN(&Auth_client_id_DYN, Find_str_value(options,CLIENT,Value_sep));
		Perm_check.auth_client_id = Auth_client_id_DYN;
		Perm_check.auth_from_id = Auth_received_id_DYN;
	} else {
		Set_DYN(&Auth_client_id_DYN, Auth_received_id_DYN );
		Perm_check.auth_client_id = Auth_received_id_DYN;
		Perm_check.auth_from_id = 0;
	}
	return( Auth_client_id_DYN == 0 );
}

void Wait_for_child( void *p )
{
	plp_status_t procstatus;
	int pid = Cast_ptr_to_int(p), n;
	DEBUGF(DRECV1)("Wait_for_child: waiting for %d", pid );
	while( pid > 0 ){
		n = waitpid(pid,&procstatus,0);
		DEBUGF(DRECV1)("Wait_for_child: pid %d, '%s'",
			n, Decode_status(&procstatus));
		if( pid == n ) pid = 0;
	}
}


/***************************************************************************
 * Commentary:
 * MIT Athena extension  --mwhitson@mit.edu 12/2/98
 * 
 * The protocol used to send a krb4 authenticator consists of:
 * 
 * Client                                   Server
 * kprintername\n - receive authenticator
 *                                          \0  (ack)
 * <krb4_credentials>
 *                                          \0
 * 
 * The server will note validity of the credentials for future service
 * requests in this session.  No capacity for TCP stream encryption or
 * verification is provided.
 * 
 ***************************************************************************/


/* A bunch of this has been ripped from the Athena lpd, and, as such,
 * isn't as nice as Patrick's code.  I'll clean it up eventually.
 *   -mwhitson
 *   Cleaned up,  run through the drunk tank and detox center,
 *    and cleaned the dog's teeth.
 *     -papowell ("sigh...") Powell
 */

#if defined(HAVE_KRB_H) && defined(MIT_KERBEROS4)
# include <krb.h>
# include <des.h>
#if !defined(HAVE_KRB_AUTH_DEF)
 extern int krb_recvauth(
	long options,            /* bit-pattern of options */
	int fd,              /* file descr. to read from */
	KTEXT ticket,            /* storage for client's ticket */
	char *service,           /* service expected */
	char *instance,          /* inst expected (may be filled in) */
	struct sockaddr_in *faddr,   /* address of foreign host on fd */
	struct sockaddr_in *laddr,   /* local address */
	AUTH_DAT *kdata,         /* kerberos data (returned) */
	char *filename,          /* name of file with service keys */
	Key_schedule schedule,       /* key schedule (return) */
	char *version);           /* version string (filled in) */
#endif

#endif

int Receive_k4auth( int *sock, char *input )
{
	int status = 0;
	char error_msg[LINEBUFFER];
	char cmd[LINEBUFFER];
	struct line_list values;
	int ack = ACK_SUCCESS;
	int k4error=0;

#if defined(HAVE_KRB_H) && defined(MIT_KERBEROS4)

	int sin_len = sizeof(struct sockaddr_in);
	struct sockaddr_in faddr;
	int len;
	uid_t euid;
	KTEXT_ST k4ticket;
	AUTH_DAT k4data;
	char k4principal[ANAME_SZ];
	char k4instance[INST_SZ];
	char k4realm[REALM_SZ];
	char k4version[9];
	char k4name[ANAME_SZ + INST_SZ + REALM_SZ + 3];

	Init_line_list(&values);
	error_msg[0] = '\0';
	DEBUG1("Receive_k4auth: doing '%s'", ++input);

	k4principal[0] = k4realm[0] = '\0';
	memset(&k4ticket, 0, sizeof(k4ticket));
	memset(&k4data, 0, sizeof(k4data));
	if (getpeername(*sock, (struct sockaddr *) &faddr, &sin_len) <0) {
	  	status = JFAIL;
		plp_snprintf( error_msg, sizeof(error_msg), 
			      "Receive_k4auth: couldn't get peername" );
		goto error;
	}
    DEBUG1("Receive_k4auth: remote host IP '%s'",
        inet_ntoa( faddr.sin_addr ) );
	status = Link_send( ShortRemote_FQDN, sock,
			Send_query_rw_timeout_DYN,"",1,0 );
	if( status ){
		ack = ACK_FAIL;
		plp_snprintf( error_msg, sizeof(error_msg),
			      "Receive_k4auth: sending ACK 0 failed" );
		goto error;
	}
	strcpy(k4instance, "*");
	euid = geteuid();
	if( UID_root ) (void)To_root();  /* Need root here to read srvtab */
	k4error = krb_recvauth(0, *sock, &k4ticket, KLPR_SERVICE,
			      k4instance,
			      &faddr,
			      (struct sockaddr_in *)NULL,
			      &k4data, "", NULL,
			      k4version);
	if( UID_root ) (void)To_uid( euid );
	DEBUG1("Receive_k4auth: krb_recvauth returned %d, '%s'",
		k4error, krb4_err_str(k4error) );
	if (k4error != KSUCCESS) {
		/* erroring out here if the auth failed. */
	  	status = JFAIL;
		plp_snprintf( error_msg, sizeof(error_msg),
		    "kerberos 4 receive authentication failed - '%s'",
			krb4_err_str(k4error) );
	  	goto error;
	}

	strncpy(k4principal, k4data.pname, ANAME_SZ);
	strncpy(k4instance, k4data.pinst, INST_SZ);
	strncpy(k4realm, k4data.prealm, REALM_SZ);

	/* Okay, we got auth.  Note it. */

	if (k4instance[0]) {
		plp_snprintf( k4name, sizeof(k4name), "%s.%s@%s", k4principal,
			      k4instance, k4realm );
	} else {
		plp_snprintf( k4name, sizeof(k4name), "%s@%s", k4principal, k4realm );
	}
	DEBUG1("Receive_k4auth: auth for %s", k4name);
	Perm_check.authtype = "kerberos4";
	Set_str_value(&values,FROM,k4name);
	/* we will only use this for client to server authentication */
	if( Check_secure_perms( &values, 0 ) ){
		DEBUGF(DRECV1)("Krb5_receive: Check_secure_perms failed");
		plp_snprintf(error_msg,sizeof(error_msg),
			"kerberos 4 authentication sent bad protocol info");
		goto error;
	}

	/* ACK the credentials  */
	status = Link_send( ShortRemote_FQDN, sock,
			Send_query_rw_timeout_DYN,"",1,0 );
	if( status ){
		ack = ACK_FAIL;
		plp_snprintf(error_msg, sizeof(error_msg),
			     "Receive_k4auth: sending ACK 0 failed" );
		goto error;
	}
	len = sizeof(cmd)-1;
    status = Link_line_read(ShortRemote_FQDN,sock,
        Send_job_rw_timeout_DYN,cmd,&len);
	if( len >= 0 ) cmd[len] = 0;
    DEBUG1( "Receive_k4auth: read status %d, len %d, '%s'",
        status, len, cmd );
    if( len == 0 ){
        DEBUG3( "Receive_k4auth: zero length read" );
		cleanup(0);
    }
    if( status ){
        logerr_die( LOG_DEBUG, "Service_connection: cannot read request" );
    }
    if( len < 3 ){
        fatal( LOG_INFO, "Service_connection: bad request line '%s'", cmd );
    }
	Free_line_list(&values);
    Dispatch_input(sock,cmd);

	status = ack = 0;

#else
	/* not supported */
	Init_line_list(&values);
	ack = ACK_FAIL;
	plp_snprintf(error_msg,sizeof(error_msg),
		"kerberos 4 not supported");
	goto error;

#endif

 error:	
	DEBUG1("Receive_k4auth: error - status %d, ack %d, k4error %d, error '%s'",
		status, ack, k4error, error_msg );
	if( status || ack ){
		cmd[0] = ack;
		if(ack) Link_send( ShortRemote_FQDN, sock,
			Send_query_rw_timeout_DYN, cmd, 1, 0 );
		if( status == 0 ) status = JFAIL;
		/* shut down reception from the remote file */
		if( error_msg[0] ){
			safestrncat( error_msg, "\n" );
			Write_fd_str( *sock, error_msg );
		}
	}

	DEBUG1("Receive_k4auth: done");
	Free_line_list(&values);
	return(status);
}
