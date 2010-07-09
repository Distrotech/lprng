/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 ***************************************************************************/

/*
 * This code is, sadly,  a whimpy excuse for the dynamically loadable
 * modules.  The idea is that you can put your user code in here and it
 * will get included in various files.
 * 
 * Supported Sections:
 *   User Authentication
 * 
 *   DEFINES      FILE WHERE INCLUDED PURPOSE
 *   USER_RECEIVE  lpd_secure.c       define the user authentication
 *                                    This is an entry in a table
 *   USER_SEND     sendauth.c         define the user authentication
 *                                    This is an entry in a table
 *   RECEIVE       lpd_secure.c       define the user authentication
 *                            This is the code referenced in USER_RECEIVE
 *   SENDING       sendauth.c       define the user authentication
 *                            This is the code referenced in USER_SEND
 * 
 */

#include "lp.h"
#include "user_auth.h"
#include "krb5_auth.h"
#include "errorcodes.h"
#include "fileopen.h"
#include "linksupport.h"
#include "child.h"
#include "getqueue.h"
#include "lpd_secure.h"
#include "lpd_dispatch.h"
#include "permission.h"
#include "globmatch.h"

#ifdef SSL_ENABLE
/* The Kerberos 5 support is MIT-specific. */
#define OPENSSL_NO_KRB5
# include "ssl_auth.h"
#endif


/**************************************************************
 * Secure Protocol
 *
 * the following is sent on *sock:  
 * \REQ_SECUREprintername C/F user authtype \n        - receive a command
 *             0           1   2   3
 * \REQ_SECUREprintername C/F user authtype jobsize\n - receive a job
 *             0           1   2   3        4
 *          Printer_DYN    |   |   |        + jobsize
 *                         |   |   authtype 
 *                         |  user
 *                        from_server=1 if F, 0 if C
 *                         
 * The authtype is used to look up the security information.  This
 * controls the dispatch and the lookup of information from the
 * configuration and printcap entry for the specified printer
 *
 * The info line_list has the information, stripped of the leading
 * xxxx_ of the authtype name.
 * For example:
 *
 * forward_id=test      <- forward_id from configuration/printcap
 * id=test              <- id from configuration/printcap
 * 
 * If there are no problems with this information, a single 0 byte
 * should be written back at this point, or a nonzero byte with an
 * error message.  The 0 will cause the corresponding transfer
 * to be started.
 * 
 * The handshake and with the remote end should be done now.
 *
 * The client will send a string with the following format:
 * destination=test\n     <- destination ID (URL encoded)
 *                       (destination ID from above)
 * server=test\n          <- if originating from server, the server key (URL encoded)
 *                       (originator ID from above)
 * client=papowell\n      <- client id
 *                       (client ID from above)
 * input=%04t1\n          <- input or command
 * This information will be extracted by the server.
 * The 'Do_secure_work' routine can now be called,  and it will do the work.
 * 
 * ERROR MESSAGES:
 *  If you generate an error,  then you should log it.  If you want
 *  return status to be returned to the remote end,  then you have
 *  to take suitable precautions.
 * 1. If the error is detected BEFORE you send the 0 ACK,  then you
 *    can send an error back directly.
 * 2. If the error is discovered as the result of a problem with
 *    the encryption method,  it is strongly recommended that you
 *    simply send a string error message back.  This should be
 *    detected by the remote end,  which will then decide that this
 *    is an error message and not status.
 *
 **************************************************************/

/***************************************************************************
 * Pgp encode and decode a file
 ***************************************************************************/

static int Pgp_get_pgppassfd( char **pgppass, struct line_list *info, char *error, int errlen )
{
	char *s, *t;
	int pgppassfd = -1;
	struct stat statb;

	/* get the user authentication */
	error[0] = 0;
	if( !Is_server ){
		char *passphrasefile = Find_str_value(info,"passphrasefile");
		if( (s = getenv( "PGPPASS" )) ){
			DEBUG1("Pgp_get_pgppassfd: PGPPASS '%s'", s );
			*pgppass = s;
		} else if( (s = getenv( "PGPPASSFD" )) ){
			t = 0;
			pgppassfd = strtol(s,&t,10);
			if( pgppassfd <= 0 || !t || *t || fstat(pgppassfd, &statb)  ){
				Errorcode = JABORT;
				DIEMSG("PGPASSFD '%s' not active file descriptor", s);
			}
			/* we read the password and put into a file */
		} else if( (s = getenv( "PGPPASSFILE" ) ) ){
			if( (pgppassfd = Checkread( s, &statb )) < 0 ){
				Errorcode = JABORT;
				DIEMSG("PGP phrasefile '%s' not opened - %s\n",
					s, Errormsg(errno) );
			}
			DEBUG1("Pgp_get_pgppassfd: PGPPASSFILE file '%s', size %0.0f, fd %d",
				s, (double)statb.st_size, pgppassfd );
		} else if( (s = getenv("PGPPATH")) && passphrasefile ){
			char *path;
			s = safestrdup2(s,"/",__FILE__,__LINE__);
			path = Make_pathname( s, passphrasefile);
			if( s ) free(s); s = 0;
			if( (pgppassfd = Checkread( path, &statb )) < 0 ){
				Errorcode = JABORT;
				DIEMSG("passphrase file %s not readable - %s",
					path, Errormsg(errno));
			}
			DEBUG1("Pgp_get_pgppassfd: PGPPASSFD file '%s', size %0.0f, fd %d",
				path, (double)statb.st_size, pgppassfd );
			if( path ) free(path); path = 0;
		} else if( (s = getenv("HOME")) && passphrasefile ){
			char *path;
			s = safestrdup2(s,"/.pgp",__FILE__,__LINE__);
			path = Make_pathname( s, passphrasefile);
			if( s ) free(s); s = 0;
			if( (pgppassfd = Checkread( path, &statb )) < 0 ){
				Errorcode = JABORT;
				DIEMSG("passphrase file %s not readable - %s",
					path, Errormsg(errno));
			}
			DEBUG1("Pgp_get_pgppassfd: PGPPASSFD file '%s', size %0.0f, fd %d",
				path, (double)statb.st_size, pgppassfd );
			if( path ) free(path); path = 0;
		}
	} else {
		char *server_passphrasefile = Find_str_value(info,"server_passphrasefile");
		if(DEBUGL1)Dump_line_list("Pgp_get_pgppassfd: info - need server_passphrasefile", info);
		if( !server_passphrasefile ){
			plp_snprintf(error,errlen,
				"Pgp_get_pgppassfd: on server, no 'pgp_server_passphrasefile' value\n" );
		} else if( (pgppassfd =
			Checkread(server_passphrasefile,&statb)) < 0 ){
				plp_snprintf(error,errlen,
					"Pgp_get_pgppassfd: on server, cannot open '%s' - '%s'\n",
					server_passphrasefile, Errormsg(errno) );
		}
	}
	DEBUG1("Pgp_get_pgppassfd: pgppassfd %d", pgppassfd );
	return(pgppassfd);
}

static int Pgp_decode(int transfer_timeout, struct line_list *info, char *tempfile, char *pgpfile,
	struct line_list *pgp_info, char *buffer, int bufflen,
	char *error, int errlen, char *esc_to_id, struct line_list *from_info,
	int *pgp_exit_code, int *not_a_ciphertext )
{
	struct line_list env, files;
	plp_status_t procstatus;
	int pgppassfd = -1, error_fd[2], status, cnt, n, pid, i;
	char *s, *t;
	*pgp_exit_code = *not_a_ciphertext = 0;

	Init_line_list(&env);
	Init_line_list(&files);

	DEBUG1("Pgp_decode: esc_to_id '%s'", esc_to_id );

	error[0] = 0;
	status = 0;
	if( ISNULL(Pgp_path_DYN) ){
		plp_snprintf( error, errlen,
		"Pgp_decode: on %s, missing pgp_path info",
			Is_server?"server":"client"
			); 
		status = JFAIL;
		goto error;
	}

	status = 0;
	error_fd[0] = error_fd[1] = -1;

	error[0] = 0;
	s = 0;
	pgppassfd = Pgp_get_pgppassfd( &s, info, error, errlen );
	if( error[0] ){
		status = JFAIL;
		goto error;
	}
	Set_str_value(&env,"PGPPASSFILE",0);
	Set_str_value(&env,"PGPPASSFD",0);
	if( Is_server ){
		if( pgppassfd <= 0 ){
			plp_snprintf(error, errlen, "Pgp_decode: on %s, no server key file!",
				Is_server?"server":"client"
				);
			status = JFAIL;
			goto error;
		}
		Set_str_value(&env,"PGPPASS",0);
		if( (s= Find_str_value(info,"server_pgppath")) ){
			DEBUG1("Pgp_decode: server_pgppath - %s", s );
			Set_str_value(&env,"PGPPATH",s);
		}
	} else {
		if( s ) Set_str_value(&env,"PGPPASS",s);
		if( (s= getenv("PGPPATH")) ){
			Set_str_value(&env,"PGPPATH",s);
		}
	}

	/* run the PGP decoder */
	if( pipe(error_fd) == -1 ){
		Errorcode = JFAIL;
		logerr_die(LOG_INFO, "Pgp_decode: on %s, pipe() failed",
			Is_server?"server":"client"
			 );
	}
	Max_open(error_fd[0]); Max_open(error_fd[1]);
	Check_max(&files,10);
	files.list[files.count++] = Cast_int_to_voidstar(0);
	files.list[files.count++] = Cast_int_to_voidstar(error_fd[1]);
	files.list[files.count++] = Cast_int_to_voidstar(error_fd[1]);
	if( pgppassfd >= 0 ){
		Set_decimal_value(&env,"PGPPASSFD",files.count);
		files.list[files.count++] = Cast_int_to_voidstar(pgppassfd);
	}

	/* now we run the PGP code */

	plp_snprintf(buffer,bufflen,
		"%s +force +batch %s -u '$%%%s' -o '%s'",
		Pgp_path_DYN, pgpfile, esc_to_id, tempfile ); 

	if( (pid = Make_passthrough(buffer, 0, &files, 0, &env )) < 0 ){
		DEBUG1("Pgp_decode: fork failed - %s", Errormsg(errno));
		status = JFAIL;
		goto error;
	}

	files.count = 0;
	Free_line_list(&files);
	Free_line_list(&env);
	close(error_fd[1]); error_fd[1] = -1;
	if( pgppassfd >= 0) close(pgppassfd); pgppassfd = -1;

	/* now we read authentication info from pgp */

	n = 0;
	while( n < bufflen-1
		&& (cnt = Read_fd_len_timeout( transfer_timeout, error_fd[0], buffer+n, bufflen-1-n )) > 0 ){
		buffer[n+cnt] = 0;
		while( (s = safestrchr(buffer,'\n')) ){
			*s++ = 0;
			DEBUG1("Pgp_decode: pgp output '%s'", buffer );
			while( cval(buffer) && !isprint(cval(buffer)) ){
				memmove(buffer,buffer+1,safestrlen(buffer+1)+1);
			}
			/* rip out extra spaces */
			for( t = buffer; *t ; ){
				if( isspace(cval(t)) && isspace(cval(t+1)) ){
					memmove(t,t+1,safestrlen(t+1)+1);
				} else {
					++t;
				}
			}
			if( buffer[0] ){
				DEBUG1("Pgp_decode: pgp final output '%s'", buffer );
				Add_line_list( pgp_info, buffer, 0, 0, 0 );
			}
			memmove(buffer,s,safestrlen(s)+1);
		}
	}
	close(error_fd[0]); error_fd[0] = -1;

	/* wait for pgp to exit and check status */
	while( (n = waitpid(pid,&procstatus,0)) != pid ){
		int err = errno;
		DEBUG1("Pgp_decode: waitpid(%d) returned %d, err '%s'",
			pid, n, Errormsg(err) );
		if( err == EINTR ) continue; 
		Errorcode = JABORT;
		logerr_die(LOG_ERR, "Pgp_decode: on %s, waitpid(%d) failed",
			Is_server?"server":"client",
			pid);
	} 
	DEBUG1("Pgp_decode: pgp pid %d exit status '%s'",
		pid, Decode_status(&procstatus) );
	if( WIFEXITED(procstatus) && (n = WEXITSTATUS(procstatus)) ){
		plp_snprintf(error,errlen, "Pgp_decode: on %s, exit status %d",
			Is_server?"server":"client",
			n);
		DEBUG1("Pgp_decode: pgp exited with status %d on host %s", n, FQDNHost_FQDN );
		*pgp_exit_code = n;
		for( i = 0; (n = safestrlen(error)) < errlen - 2 && i < pgp_info->count; ++i ){
			s = pgp_info->list[i];
			plp_snprintf(error+n, errlen-n, "\n %s",s);
			if( !*not_a_ciphertext ){
				*not_a_ciphertext = (strstr(s, "not a ciphertext") != 0);
			}
		}
		status = JFAIL;
		goto error;
	} else if( WIFSIGNALED(procstatus) ){
		n = WTERMSIG(procstatus);
		DEBUG1( "Pgp_decode: pgp died with signal %d, '%s'",
			n, Sigstr(n));
		status = JFAIL;
		goto error;
	}

	for( i = 0; i < pgp_info->count; ++i ){
		s = pgp_info->list[i];
		if( !safestrncmp("Good",s,4) ){
			if( (t = safestrchr(s,'"')) ){
				*t++ = 0;
				if( (s = safestrrchr(t,'"')) ) *s = 0;
				DEBUG1( "Pgp_decode: FROM '%s'", t );
				Set_str_value(from_info,FROM,t);
			}
		}
	}

 error:
	DEBUG1( "Pgp_decode: error '%s'", error );
	if( error_fd[0] >= 0 ) close(error_fd[0]); error_fd[0] = -1;
	if( error_fd[1] >= 0 ) close(error_fd[1]); error_fd[1] = -1;
	if( pgppassfd >= 0) close(pgppassfd); pgppassfd = -1;
	Free_line_list(&env);
	files.count = 0;
	Free_line_list(&files);
	return( status );
}

static int Pgp_encode(int transfer_timeout, struct line_list *info, char *tempfile, char *pgpfile,
	struct line_list *pgp_info, char *buffer, int bufflen,
	char *error, int errlen, char *esc_from_id, char *esc_to_id,
	int *pgp_exit_code )
{
	struct line_list env, files;
	plp_status_t procstatus;
	int error_fd[2], status, cnt, n, pid, pgppassfd = -1, i;
	char *s, *t;

	Init_line_list(&env);
	Init_line_list(&files);
	*pgp_exit_code = 0;
	status = 0;
	if( ISNULL(Pgp_path_DYN) ){
		plp_snprintf( error, errlen,
		"Pgp_encode: missing pgp_path info"); 
		status = JFAIL;
		goto error;
	}
	DEBUG1("Pgp_encode: esc_from_id '%s', esc_to_id '%s'",
		esc_from_id, esc_to_id );

	status = 0;
	pgppassfd = error_fd[0] = error_fd[1] = -1;

	error[0] = 0;
	s = 0;
	pgppassfd = Pgp_get_pgppassfd( &s, info, error, errlen );
	if( error[0] ){
		status = JFAIL;
		goto error;
	}
	Set_decimal_value(&env,"PGPPASSFD",files.count);
	Set_str_value(&env,"PGPPASSFILE",0);
	Set_str_value(&env,"PGPPASSFD",0);
	if( Is_server ){
		if( pgppassfd <= 0 ){
			plp_snprintf(error, errlen, "Pgp_encode: no server key file!");
			status = JFAIL;
			goto error;
		}
		Set_str_value(&env,"PGPPASS",0);
		if( (s= Find_str_value(info,"server_pgppath")) ){
			DEBUG1("Pgp_decode: server_pgppath - %s", s );
			Set_str_value(&env,"PGPPATH",s);
		}
	} else if( s ){
		Set_str_value(&env,"PGPPASS",s);
		if( (s= getenv("PGPPATH")) ){
			Set_str_value(&env,"PGPPATH",s);
		}
	}

	pgpfile = safestrdup2(tempfile,".pgp",__FILE__,__LINE__);
	Check_max(&Tempfiles,1);
	if( !Debug ) Tempfiles.list[Tempfiles.count++] = pgpfile;

	if( pipe(error_fd) == -1 ){
		Errorcode = JFAIL;
		logerr_die(LOG_INFO, "Pgp_encode: pipe() failed" );
	}
	Max_open(error_fd[0]); Max_open(error_fd[1]);
	Check_max(&files,10);
	files.count = 0;
	files.list[files.count++] = Cast_int_to_voidstar(0);
	files.list[files.count++] = Cast_int_to_voidstar(error_fd[1]);
	files.list[files.count++] = Cast_int_to_voidstar(error_fd[1]);

	if( pgppassfd > 0 ){
		Set_decimal_value(&env,"PGPPASSFD",files.count);
		files.list[files.count++] = Cast_int_to_voidstar(pgppassfd);
	}

	/* now we run the PGP code */

	plp_snprintf(buffer,bufflen,
		"$- %s +armorlines=0 +verbose=0 +force +batch -sea '%s' '$%%%s' -u '$%%%s' -o %s",
		Pgp_path_DYN, tempfile, esc_to_id, esc_from_id, pgpfile );

	if( (pid = Make_passthrough(buffer, 0, &files, 0, &env )) < 0 ){
		Errorcode = JABORT;
		logerr_die(LOG_INFO, "Pgp_encode: fork failed");
	}

	DEBUG1("Pgp_encode: pgp pid %d", pid );
	files.count = 0;
	Free_line_list(&files);
	Free_line_list(&env);
	close(error_fd[1]); error_fd[1] = -1;
	if( pgppassfd >= 0) close(pgppassfd); pgppassfd = -1;

	/* now we read error info from pgp */

	n = 0;
	while( n < bufflen-1
		&& (cnt = Read_fd_len_timeout( transfer_timeout, error_fd[0], buffer+n, bufflen-1-n )) > 0 ){
		buffer[n+cnt] = 0;
		while( (s = safestrchr(buffer,'\n')) ){
			*s++ = 0;
			DEBUG1("Pgp_encode: pgp output '%s'", buffer );
			while( cval(buffer) && !isprint(cval(buffer)) ){
				memmove(buffer,buffer+1,safestrlen(buffer+1)+1);
			}
			/* rip out extra spaces */
			for( t = buffer; *t ; ){
				if( isspace(cval(t)) && isspace(cval(t+1)) ){
					memmove(t,t+1,safestrlen(t+1)+1);
				} else {
					++t;
				}
			}
			if( buffer[0] ){
				DEBUG1("Pgp_encode: pgp final output '%s'", buffer );
				Add_line_list( pgp_info, buffer, 0, 0, 0 );
			}
			memmove(buffer,s,safestrlen(s)+1);
		}
	}
	close(error_fd[0]); error_fd[0] = -1;

	/* wait for pgp to exit and check status */
	while( (n = waitpid(pid,&procstatus,0)) != pid ){
		int err = errno;
		DEBUG1("Pgp_encode: waitpid(%d) returned %d, err '%s', status '%s'",
			pid, n, Errormsg(err), Decode_status(&procstatus));
		if( err == EINTR ) continue; 
		Errorcode = JABORT;
		logerr_die(LOG_ERR, "Pgp_encode: waitpid(%d) failed", pid);
	} 
	DEBUG1("Pgp_encode: pgp pid %d exit status '%s'",
		pid, Decode_status(&procstatus) );
	if(DEBUGL1)Dump_line_list("Pgp_encode: pgp_info", pgp_info);
	if( WIFEXITED(procstatus) && (n = WEXITSTATUS(procstatus)) ){
		plp_snprintf(error,errlen,
			"Pgp_encode: on %s, pgp exited with status %d on host %s",
			Is_server?"server":"client",
			n, FQDNHost_FQDN );
		*pgp_exit_code = n;
		for( i = 0; (n = safestrlen(error)) < errlen - 2 && i < pgp_info->count; ++i ){
			s = pgp_info->list[i];
			plp_snprintf(error+n, errlen-n, "\n %s",s);
		}
		status = JFAIL;
		goto error;
	} else if( WIFSIGNALED(procstatus) ){
		n = WTERMSIG(procstatus);
		plp_snprintf(error,errlen,
		"Pgp_encode: on %s, pgp died with signal %d, '%s'",
			Is_server?"server":"client",
			n, Sigstr(n));
		status = JFAIL;
		goto error;
	}

 error:
	DEBUG1("Pgp_encode: status %d, error '%s'", status, error );
	if( error_fd[0] >= 0 ) close(error_fd[0]); error_fd[0] = -1;
	if( error_fd[1] >= 0 ) close(error_fd[1]); error_fd[1] = -1;
	if( pgppassfd >= 0) close(pgppassfd); pgppassfd = -1;
	Free_line_list(&env);
	files.count = 0;
	Free_line_list(&files);
	return( status );
}


/*
 * 
 * The following routines simply implement the encryption and transfer of
 * the files and/or values
 * 
 * By default, when sending a command,  the file will contain:
 *   key=value lines.
 *   KEY           PURPOSE
 *   client        client or user name
 *   from          originator - server if forwarding, client otherwise
 *   command       command to send
 * 
 */

/*************************************************************
 * PGP Transmission
 * 
 * Configuration:
 *   pgp_id            for client to server
 *   pgp_forward_id    for server to server
 *   pgp_forward_id    for server to server
 *   pgp_path          path to pgp program
 *   pgp_passphrasefile     user passphrase file (relative to $HOME/.pgp)
 *   pgp_server_passphrasefile server passphrase file
 * User ENVIRONMENT Variables
 *   PGPPASS           - passphrase
 *   PGPPASSFD         - passfd if set up
 *   PGPPASSFILE       - passphrase in this file
 *   HOME              - for passphrase relative to thie file
 * 
 *  We encrypt and sign the file,  then send it to the other end.
 *  It will decrypt it, and then send the data back, encrypted with
 *  our public key.
 * 
 *  Keyrings must contain keys for users.
 *************************************************************/

static int Pgp_send( int *sock, int transfer_timeout, char *tempfile,
	char *error, int errlen,
	const struct security *security UNUSED, struct line_list *info )
{
	char *pgpfile;
	struct line_list pgp_info;
	char buffer[LARGEBUFFER];
	int status, i, tempfd, len, n, fd;
	struct stat statb;
	char *from, *destination, *s, *t;
	int pgp_exit_code = 0;
	int not_a_ciphertext = 0;

	DEBUG1("Pgp_send: sending on socket %d", *sock );

	len = 0;
	error[0] = 0;
	from = Find_str_value( info, FROM);
	destination = Find_str_value( info, ID );

	tempfd = -1;

	Init_line_list( &pgp_info );
    pgpfile = safestrdup2(tempfile,".pgp",__FILE__,__LINE__); 
    Check_max(&Tempfiles,1);
    Tempfiles.list[Tempfiles.count++] = pgpfile;

	status = Pgp_encode( transfer_timeout, info, tempfile, pgpfile, &pgp_info,
		buffer, sizeof(buffer), error, errlen, 
        from, destination, &pgp_exit_code );

	if( status ){
		goto error;
	}
	if( !Is_server && Verbose ){
		for( i = 0; i < pgp_info.count; ++i ){
			if( Write_fd_str(1,pgp_info.list[i]) < 0
				|| Write_fd_str(1,"\n") < 0 ) cleanup(0);
		}
	}
	Free_line_list(&pgp_info);

	if( (tempfd = Checkread(pgpfile,&statb)) < 0 ){
		plp_snprintf(error,errlen,
			"Pgp_send: cannot open '%s' - %s", pgpfile, Errormsg(errno) );
		goto error;
	}

	DEBUG1("Pgp_send: encrypted file size '%0.0f'", (double)(statb.st_size) );
	plp_snprintf(buffer,sizeof(buffer), "%0.0f\n",(double)(statb.st_size) );
	Write_fd_str(*sock,buffer);

	while( (len = Read_fd_len_timeout( transfer_timeout, tempfd, buffer, sizeof(buffer)-1 )) > 0 ){
		buffer[len] = 0;
		DEBUG4("Pgp_send: file information '%s'", buffer );
		if( write( *sock, buffer, len) != len ){
			plp_snprintf(error,errlen,
			"Pgp_send: write to socket failed - %s", Errormsg(errno) );
			goto error;
		}
	}

	DEBUG2("Pgp_send: sent file" );
	close(tempfd); tempfd = -1;
	/* we close the writing side */
	shutdown( *sock, 1 );
	if( (tempfd = Checkwrite(pgpfile,&statb,O_WRONLY|O_TRUNC,1,0)) < 0){
		plp_snprintf(error,errlen,
			"Pgp_send: open '%s' for write failed - %s", pgpfile, Errormsg(errno));
		goto error;
	}
	DEBUG2("Pgp_send: starting read");
	len = 0;
	while( (n = Read_fd_len_timeout(transfer_timeout, *sock,buffer,sizeof(buffer)-1)) > 0 ){
		buffer[n] = 0;
		DEBUG4("Pgp_send: read '%s'", buffer);
		if( write(tempfd,buffer,n) != n ){
			plp_snprintf(error,errlen,
			"Pgp_send: write '%s' failed - %s", tempfile, Errormsg(errno) );
			goto error;
		}
		len += n;
	}
	close( tempfd ); tempfd = -1;

	DEBUG2("Pgp_send: total %d bytes status read", len );

	Free_line_list(&pgp_info);

	/* decode the PGP file into the tempfile */
	if( len ){
		status = Pgp_decode( transfer_timeout, info, tempfile, pgpfile, &pgp_info,
			buffer, sizeof(buffer), error, errlen, from, info,
			&pgp_exit_code, &not_a_ciphertext );
		if( not_a_ciphertext ){
			DEBUG2("Pgp_send: not a ciphertext" );
			if( (tempfd = Checkwrite(tempfile,&statb,O_WRONLY|O_TRUNC,1,0)) < 0){
				plp_snprintf(error,errlen,
				"Pgp_send: open '%s' for write failed - %s",
					tempfile, Errormsg(errno));
			}
			if( (fd = Checkread(pgpfile,&statb)) < 0){
				plp_snprintf(error,errlen,
				"Pgp_send: open '%s' for write failed - %s",
					pgpfile, Errormsg(errno));
			}
			if( error[0] ){
				Write_fd_str(tempfd,error);
				Write_fd_str(tempfd,"\n Contents -\n");
			}
			error[0] = 0;
			len = 0;
			buffer[0] = 0;
			while( (n = Read_fd_len_timeout(transfer_timeout, fd, buffer+len, sizeof(buffer)-len-1)) > 0 ){
				buffer[n] = 0;
				DEBUG2("Pgp_send: read '%s'", buffer );
				while( (s = strchr( buffer, '\n')) ){
					*s++ = 0;
					for( t = buffer; *t; ++t ){
						if( !isprint(cval(t)) ) *t = ' ';
					}
					plp_snprintf(error,errlen, "  %s\n", buffer);
					Write_fd_str(tempfd, error );
					DEBUG2("Pgp_send: wrote '%s'", error );
					memmove(buffer,s,safestrlen(s)+1);
				}
				len = safestrlen(buffer);
			}
			DEBUG2("Pgp_send: done" );
			error[0] = 0;
			close(fd); fd = -1;
			close(tempfd); tempfd = -1;
			error[0] = 0;
		}
	}

 error:
	if( error[0] ){
		char *s, *end;
		DEBUG2("Pgp_send: writing error to file '%s'", error );
		if( (tempfd = Checkwrite(tempfile,&statb,O_WRONLY|O_TRUNC,1,0)) < 0){
			plp_snprintf(error,errlen,
			"Pgp_send: open '%s' for write failed - %s",
				tempfile, Errormsg(errno));
		}
		for( s = error; !ISNULL(s); s = end ){
			if( (end = strchr( error, '\n')) ) *end++ = 0;
			plp_snprintf(buffer,sizeof(buffer), "%s\n", s);
			Write_fd_str(tempfd, buffer );
			DEBUG2("Pgp_send: wrote '%s'", buffer );
		}
		close( tempfd ); tempfd = -1;
		error[0] = 0;
	}
	Free_line_list(&pgp_info);
	return(status);
}

static int Pgp_receive( int *sock, int transfer_timeout,
	char *user UNUSED, char *jobsize, int from_server, char *authtype UNUSED,
	struct line_list *info,
	char *errmsg, int errlen,
	struct line_list *header_info,
	const struct security *security UNUSED, char *tempfile,
	SECURE_WORKER_PROC do_secure_work)
{
	char *pgpfile;
	int tempfd, status, n;
	char buffer[LARGEBUFFER];
	struct stat statb;
	struct line_list pgp_info;
	double len;
	char *id = Find_str_value( info, ID );
	char *from = 0;
	int pgp_exit_code = 0;
	int not_a_ciphertext = 0;

	Init_line_list(&pgp_info);
	tempfd = -1;
	errmsg[0] = 0;

	pgpfile = safestrdup2(tempfile,".pgp",__FILE__,__LINE__);
	Check_max(&Tempfiles,1);
	Tempfiles.list[Tempfiles.count++] = pgpfile;

	if( id == 0 ){
		status = JABORT;
		plp_snprintf( errmsg, errlen, "Pgp_receive: %s has no pgp_id or auth_id value",
			Is_server?"server":"client");
		goto error;
	}

	if( Write_fd_len( *sock, "", 1 ) < 0 ){
		status = JABORT;
		plp_snprintf( errmsg, errlen, "Pgp_receive: ACK 0 write error - %s",
			Errormsg(errno) );
		goto error;
	}


	if( (tempfd = Checkwrite(pgpfile,&statb,O_WRONLY|O_TRUNC,1,0)) < 0 ){
		status = JFAIL;
		plp_snprintf( errmsg, errlen,
			"Pgp_receive: reopen of '%s' for write failed - %s",
			pgpfile, Errormsg(errno) );
		goto error;
	}
	DEBUGF(DRECV4)("Pgp_receive: starting read from %d", *sock );
	while( (n = Read_fd_len_timeout(transfer_timeout, *sock, buffer,1)) > 0 ){
		/* handle old and new format of file */
		buffer[n] = 0;
		DEBUGF(DRECV4)("Pgp_receive: remote read '%d' '%s'", n, buffer );
		if( isdigit(cval(buffer)) ) continue;
		if( isspace(cval(buffer)) ) break;
		if( write( tempfd,buffer,1 ) != 1 ){
			status = JFAIL;
			plp_snprintf( errmsg, errlen,
				"Pgp_receive: bad write to '%s' - '%s'",
				tempfile, Errormsg(errno) );
			goto error;
		}
		break;
	}
	while( (n = Read_fd_len_timeout(transfer_timeout, *sock, buffer,sizeof(buffer)-1)) > 0 ){
		buffer[n] = 0;
		DEBUGF(DRECV4)("Pgp_receive: remote read '%d' '%s'", n, buffer );
		if( write( tempfd,buffer,n ) != n ){
			status = JFAIL;
			plp_snprintf( errmsg, errlen,
				"Pgp_receive: bad write to '%s' - '%s'",
				tempfile, Errormsg(errno) );
			goto error;
		}
	}
	if( n < 0 ){
		status = JFAIL;
		plp_snprintf( errmsg, errlen,
			"Pgp_receive: bad read from socket - '%s'",
			Errormsg(errno) );
		goto error;
	}
	close(tempfd); tempfd = -1;
	DEBUGF(DRECV4)("Pgp_receive: end read" );

	status = Pgp_decode(transfer_timeout, info, tempfile, pgpfile, &pgp_info,
		buffer, sizeof(buffer), errmsg, errlen, id, header_info,
		&pgp_exit_code, &not_a_ciphertext );
	if( status ) goto error;

	DEBUGFC(DRECV1)Dump_line_list("Pgp_receive: header_info", header_info );

	from = Find_str_value(header_info,FROM);
	if( from == 0 ){
		status = JFAIL;
		plp_snprintf( errmsg, errlen,
			"Pgp_receive: no 'from' information" );
		goto error;
	}

	status = do_secure_work( jobsize, from_server, tempfile, header_info );

	Free_line_list( &pgp_info);
 	status = Pgp_encode(transfer_timeout, info, tempfile, pgpfile, &pgp_info,
		buffer, sizeof(buffer), errmsg, errlen,
		id, from, &pgp_exit_code );
	if( status ) goto error;

	/* we now have the encoded output */
	if( (tempfd = Checkread(pgpfile,&statb)) < 0 ){
		status = JFAIL;
		plp_snprintf( errmsg, errlen,
			"Pgp_receive: reopen of '%s' for read failed - %s",
			tempfile, Errormsg(errno) );
		goto error;
	}
	len = statb.st_size;
	DEBUGF(DRECV1)( "Pgp_receive: return status encoded size %0.0f",
		len);
	while( (n = Read_fd_len_timeout(transfer_timeout, tempfd, buffer,sizeof(buffer)-1)) > 0 ){
		buffer[n] = 0;
		DEBUGF(DRECV4)("Pgp_receive: sending '%d' '%s'", n, buffer );
		if( write( *sock,buffer,n ) != n ){
			status = JFAIL;
			plp_snprintf( errmsg, errlen,
				"Pgp_receive: bad write to socket - '%s'",
				Errormsg(errno) );
			goto error;
		}
	}
	if( n < 0 ){
		status = JFAIL;
		plp_snprintf( errmsg, errlen,
			"Pgp_receive: read '%s' failed - %s",
			tempfile, Errormsg(errno) );
		goto error;
	}

 error:
	if( tempfd>=0) close(tempfd); tempfd = -1;
	Free_line_list(&pgp_info);
	return(status);
}

static const struct security pgp_auth =
	{ "pgp",       "pgp",	"pgp",      0,              0,           Pgp_send, 0, Pgp_receive };

static const struct security *SecuritySupported[] = {
	/* name, server_name, config_name, flags,
        client  connect, send, send_done
		server  accept, receive, receive_done
	*/
#if defined(KERBEROS)
	&kerberos5_auth,
	&k5conn_auth,
#endif
	&test_auth,
	&md5_auth,
	&pgp_auth,
#ifdef SSL_ENABLE
	&ssl_auth,
#endif
	NULL
};

char *ShowSecuritySupported( char *str, int maxlen )
{
	int i, len;
	const char *name;
	str[0] = 0;
	for( len = i = 0; SecuritySupported[i] != NULL; ++i ){
		name = SecuritySupported[i]->name;
		plp_snprintf( str+len,maxlen-len, "%s%s",len?",":"",name );
		len += strlen(str+len);
	}
	return( str );
}

const struct security *FindSecurity( const char *name ) {
	const struct security *s, **p;

	for( p = SecuritySupported ; (s = *p) != NULL ; p++ ) {
		if( !Globmatch(s->name, name ) )
			return s;
	}
	return NULL;
}
