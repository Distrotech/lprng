/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: sendauth.c,v 5.4 1999/10/23 02:37:06 papowell Exp papowell $";


#include "lp.h"
#include "lpd.h"
#include "sendauth.h"
#include "getqueue.h"
#include "errorcodes.h"
#include "krb5_auth.h"
#include "linksupport.h"
#include "fileopen.h"
#include "child.h"
#include "gethostinfo.h"
/**** ENDINCLUDE ****/

void Put_in_auth( int tempfd, const char *key, char *value )
{
	char *v = Escape(value,0,1);
	char *s = safestrdup4(key,"=",v,"\n",__FILE__,__LINE__);
	DEBUG1("Put_in_auth: fd %d, '%s'",tempfd, s );
	if( Write_fd_str(tempfd,s) < 0 ){
		Errorcode = JFAIL;
		logerr_die(LOG_INFO,"Put_in_auth: cannot write '%s' to file", s );
	}
	if( s ) free(s); s = 0;
	if( v ) free(v); v = 0;
}

void Setup_auth_info( int tempfd, char *cmd )
{
	char *s;

#if defined(HAVE_KRB5_H)
	if( safestrcasecmp( Auth_DYN, KERBEROS5 ) == 0
		|| safestrcasecmp( Auth_DYN, KERBEROS ) == 0 ){
	        DEBUG2("Setup_auth_info: cmd = '%s'", cmd);
		if(cmd) {
			if( Write_fd_str(tempfd,cmd) < 0 ||
				Write_fd_str(tempfd,"\n") < 0 ){
				Errorcode = JFAIL;
				logerr_die(LOG_INFO,"Put_in_auth: cannot write to tempfilefile" );
			}
		}
		if( Is_server ){
			if( Write_fd_str(tempfd,Auth_client_id_DYN) < 0 ||
				Write_fd_str(tempfd,"\n\n") < 0 ){
				Errorcode = JFAIL;
				logerr_die(LOG_INFO,"Put_in_auth: cannot write to tempfilefile" );
			}
		}
		return;
	}
#else
	if( safestrcasecmp( Auth_DYN, KERBEROS5 ) == 0
		|| safestrcasecmp( Auth_DYN, KERBEROS ) == 0 ){
		Errorcode = JFAIL;
		fatal(LOG_INFO,"Setup_auth_info: no kerberos support");
	}
#endif

	Put_in_auth(tempfd,DESTINATION,Auth_id_DYN);
	if( Is_server ){
		Put_in_auth(tempfd,SERVER,Auth_sender_id_DYN);
	}
	Put_in_auth(tempfd,CLIENT,Auth_client_id_DYN);

	if( cmd ){
		if( (s = safestrrchr(cmd,'\n')) ) *s = 0;
		Put_in_auth(tempfd,INPUT,cmd);
	}
	if( Write_fd_str(tempfd,"\n") < 0 ){
		Errorcode = JFAIL;
		logerr_die(LOG_INFO,"Setup_auth_info: cannot write to file");
	}

}

/*
 * Send_auth_transfer
 *  1. we send the command line and wait for ACK of 0
 *  \REQ_SEQUREprinter C/F sender_id authtype [jobsize]
 *  2. if authtype == kerberos we do kerberos
 *  4. if otherwise,  we start a process with command line options
 *       fd 0 -  sock
 *       fd 1 -  for reports
 *       fd 2 -  for errors
 *    /filter -C -P printer -n sender_id -A authtype -R remote_id -Ttempfile
 *  5. The tempfile will be sent to the remote end and status
 *     written back on fd 2
 *   RETURN:
 *     0 - no error
 *     !=0 - error
 */

int Send_auth_transfer( int *sock, int transfer_timeout, char *tempfile, int printjob )
{
	char tempbuf[SMALLBUFFER];	/* buffer */
	char size[LINEBUFFER];
	struct stat statb;
	int ack, pid, len, n, fd;	/* ACME! The best... */
	int status = -1;			/* job status */
	char *key, *s;
#if defined(HAVE_KRB5_H)
	char *keyfile = 0;
#endif
	int pipe_fd[2], error_fd[2];
	struct line_list args, files;
	int found = 0;

	Init_line_list(&args);
	Init_line_list(&files);
	DEBUG1("Send_auth_transfer: tempfile '%s'", tempfile );

	size[0] = 0;
	pipe_fd[0] = pipe_fd[1] = error_fd[0] = error_fd[1] = -1;

	DEBUGF(DRECV1)(
		"Send_auth_transfer: auth '%s', auth_id '%s' pgp(path '%s',key '%s')",
		Auth_DYN, Auth_id_DYN, Pgp_path_DYN, Pgp_server_passphrasefile_DYN );

	errno = 0;
	if( !Auth_DYN ){
		logmsg( LOG_INFO, "no auth (authentication) information");
		goto error;
	}
	if( !Auth_id_DYN ){
		logmsg( LOG_INFO, "no auth_id (server id) information");
		goto error;
	}

#if defined(HAVE_KRB5_H)
	if( safestrcasecmp( Auth_DYN, KERBEROS ) == 0
		|| safestrcasecmp( Auth_DYN, KERBEROS5 ) == 0 ){
		found = 1;
		if( Is_server ){
			if( !(keyfile = Kerberos_keytab_DYN)
				|| (stat(keyfile,&statb) == -1 ) ){
				logmsg( LOG_INFO, "no server keytab file name" );
				goto error;
			}
		}
	}
#endif

	if( (safestrcasecmp( Auth_DYN,PGP ) == 0) ){
		found = 1;
		errno = 0;
		if( Is_server
			&& (!Pgp_server_passphrasefile_DYN
				|| stat(Pgp_server_passphrasefile_DYN,&statb) == -1) ){
			logerr( LOG_INFO, "bad pgp_server_key '%s' file",
				Pgp_server_passphrasefile_DYN);
			goto error;
		}
		if( !Pgp_path_DYN || stat(Pgp_path_DYN,&statb) == -1 ){
			logerr( LOG_INFO, _("bad pgp_path '%s'"), Pgp_path_DYN );
			goto error;
		}
	}

	errno = 0;
	if( !found
		&& (Auth_filter_DYN == 0 || stat(Auth_filter_DYN, &statb) == -1) ){
		logerr( LOG_INFO, _("bad auth_send_filter '%s'"), Auth_filter_DYN );
		goto error;
	}

	key = "F";
	if( !Is_server ){
		key = "C";
	}

	if( printjob ){
		errno = 0;
		if( stat(tempfile,&statb) ){
			logerr(LOG_INFO, "stat '%s' failed", tempfile);
			goto error;
		}
		plp_snprintf( size,sizeof(size)," %0.0f",(double)(statb.st_size) );
	}

	DEBUG3("Send_auth_transfer: size '%s'", size );

	plp_snprintf( tempbuf, sizeof(tempbuf),
		"%c%s %s %s %s%s\n",
		REQ_SECURE,RemotePrinter_DYN,key,
		esc_Auth_sender_id_DYN, esc_Auth_DYN, size );
	DEBUG1("Send_auth_transfer: sending '%s'", tempbuf );
	status = Link_send( RemoteHost_DYN, sock, transfer_timeout,
		tempbuf, strlen(tempbuf), &ack );
	DEBUG3("Send_auth_transfer: status '%s'", Link_err_str(status) );
	if( status ){
		if( (s = safestrchr(tempbuf,'\n')) ) *s = 0;
		logerr( LOG_INFO, "error '%s' sending '%s' to %s@%s",
		Link_err_str(status), tempbuf, RemotePrinter_DYN, RemoteHost_DYN );
		len = 0;
		tempbuf[0] = 0;
		while( len < sizeof(tempbuf)-1
			&& (n = read(*sock,tempbuf+len,sizeof(tempbuf)-1-len)) > 0 ){
			tempbuf[len+n] = 0;
			while( (s = safestrchr(tempbuf,'\n')) ){
				*s++ = 0;
				logerr( LOG_INFO, "error msg '%s'", tempbuf );
				memmove(tempbuf,s,strlen(s)+1);
			}
			len = strlen(tempbuf);
		}
		if(strlen(tempbuf)){
			logerr( LOG_INFO, "error msg '%s'", tempbuf );
		}
		goto error;
	}

	/*
	 * we now use kerberos if we have it built in
	 */

#if defined(HAVE_KRB5_H)
	if( safestrcasecmp( Auth_DYN, KERBEROS ) == 0
		|| safestrcasecmp( Auth_DYN, KERBEROS5 ) == 0 ){
		tempbuf[0] = 0;
		DEBUG1("Send_auth_transfer: starting kerberos authentication" );
		/* DANGER DANGER - we are doing this because client_krb5_auth
		 * uses RUID
		 */
		if( To_ruid_user() ){
			Errorcode = JABORT;
			logerr_die( LOG_INFO,
			"Send_auth_transfer: kerberos required setruid failed" );
		}
		status= client_krb5_auth( keyfile, Kerberos_service_DYN,
			RemoteHost_DYN, /* remote host */
			Kerberos_dest_id_DYN,	/* principle name of the remote server */
			0,	/* options */
			Kerberos_life_DYN,	/* lifetime of server ticket */
			Kerberos_renew_DYN,	/* renewable time of server ticket */
			*sock, tempbuf, sizeof(tempbuf), tempfile );
		To_user();
		if( status ){
			/* don;t talk to the server any more - ignore errors... :-) */
			shutdown( *sock, 1);
			logmsg(LOG_INFO, "authentication failed '%s'", tempbuf);
		}
		goto error;
	}
#endif

	if( (safestrcasecmp( Auth_DYN,PGP ) == 0) ){
		DEBUG1("Send_auth_transfer: starting pgp authentication" );
		status = Pgp_send( sock, transfer_timeout, tempfile );
		goto error;
	}

	/* now we use the user specified method */
	DEBUG1("Send_auth_transfer: starting '%s' authentication", Auth_DYN );
	plp_snprintf( tempbuf, sizeof(tempbuf),
		"%s -C -P%s -n$%%%s -A$%%%s -R$%%%s -T%s",
		Auth_filter_DYN,RemotePrinter_DYN,esc_Auth_sender_id_DYN,
		esc_Auth_DYN, esc_Auth_id_DYN, tempfile );
	DEBUG3("Send_auth_transfer: '%s'", tempbuf );

	/* now set up the file descriptors:
	 *   FD  Options Purpose
	 *    0  R/W     sock connection to remote host (R/W)
	 *    1  W       pipe or file descriptor,  for responses to client programs
	 *    2  W       error log
	 */

	if( pipe(pipe_fd) <  0 ){
		logerr( LOG_INFO, "Send_auth_transfer: pipe failed" );
		goto error;
	}

	if( Is_server ){
		if( pipe(error_fd) <  0 ){
			logerr( LOG_INFO, "Send_auth_transfer: pipe failed" );
			goto error;
		}
		Free_line_list(&args);
		Set_str_value(&args,NAME,"SEND_FILTER");
		Set_str_value(&args,CALL,LOG);
		if( (pid = Start_worker(&args,error_fd[0])) < 0 ){
			Errorcode = JFAIL;
			logerr_die(LOG_INFO,
				"Printer_open: could not create SEND_FILTER error loggin process");
		}
		Free_line_list(&args);
		close(error_fd[0]); error_fd[0] = -1;
		fd = error_fd[1];
	} else {
		fd = 2;
	}

	Free_line_list(&files);
	Check_max(&files,10);
	files.list[files.count++] = Cast_int_to_voidstar(*sock);
	files.list[files.count++] = Cast_int_to_voidstar(pipe_fd[1]);
	files.list[files.count++] = Cast_int_to_voidstar(fd);
	if( (pid = Make_passthrough( tempbuf, 0, &files, 0, 0 )) < 0){
		logerr( LOG_INFO, "Send_auth_transfer: could not execute '%s'",
			Auth_filter_DYN );
		goto error;
	}
	files.count = 0;
	Free_line_list(&files);

	/* now we wait for the status */
	DEBUG3("Send_auth_transfer: sock %d, pipe_fd %d", *sock, pipe_fd[0]);
	if( dup2( pipe_fd[0], *sock ) < 0 ){
		Errorcode = JFAIL;
		logerr( LOG_INFO, "Send_auth_transfer: dup2 failed" );
		goto error;
	}
	status = 0;

 error:
	if( pipe_fd[0] >= 0 ) close(pipe_fd[0]); pipe_fd[0] = -1;
	if( pipe_fd[1] >= 0 ) close(pipe_fd[1]); pipe_fd[1] = -1;
	if( error_fd[0] >= 0 ) close(error_fd[0]); error_fd[0] = -1;
	if( error_fd[1] >= 0 ) close(error_fd[1]); error_fd[1] = -1;
	DEBUG3("Send_auth_transfer: exit status %d", status );
	Free_line_list(&args);
	Free_line_list(&files);
	return( status );
}

int Pgp_send( int *sock, int transfer_timeout, char *tempfile )
{
	int pipe_fd[2], error_fd[2], tempfd = -1, n, len, pid, i;
	int status = 1;
	struct line_list passfd, env;
	char buffer[SMALLBUFFER], *s;
	char *pgpfile, *path = 0;
	struct stat statb;
	plp_status_t procstatus;
	int pgppassfd = -1;
	double jobsize;
	struct line_list info;

	Init_line_list(&env);
	Init_line_list(&passfd);
	Init_line_list(&info);
	s = getenv("PGPPASS");
	DEBUG1("Pgp_send: starting, passphrasefile '%s', PGPASS '%s'",
		Pgp_passphrasefile_DYN, s );
	if( !Is_server && s == 0 ){
		s = getenv("PGPPASSFD");
		DEBUG1("Pgp_send: PGPPASSFD '%s'", s);
		if( s ){
			pgppassfd = atoi(s);
			if( pgppassfd <= 0 || fstat(pgppassfd, &statb ) ){
				Errorcode = JFAIL;
				Diemsg("PGPASSFD '%s' not file", s);
			}
		}
		if( pgppassfd < 0 ){
			if( (s = getenv("PGPPASSFILE")) ){
				path = Make_pathname( 0, s );
				pgppassfd = Checkread( path, &statb );
				DEBUG1("Pgp_send: PGPPASSFD file '%s', size %0.0f, fd %d",
					path, (double)statb.st_size, pgppassfd );
				if( Verbose ){
					plp_snprintf(buffer,sizeof(buffer),"phrasefile '%s' %s\n",
						path, pgppassfd>0?"opened":"not opened");
				}
			} else {
				s = getenv("HOME");
				if( s ) s = safestrdup2(s,"/.pgp",__FILE__,__LINE__);
				path = Make_pathname( s, Pgp_passphrasefile_DYN);
				if( s ) free(s); s = 0;
				pgppassfd = Checkread( path, &statb );
				DEBUG1("Pgp_send: PGPPASSFD file '%s', size %0.0f, fd %d",
					path, (double)statb.st_size, pgppassfd );
				if( pgppassfd < 0 ){
					Errorcode = JFAIL;
					Diemsg("PGPASSFD not set and passphrase file %s not readable",
						Pgp_passphrasefile_DYN);
				}
				if( Verbose ){
					plp_snprintf(buffer,sizeof(buffer),"phrasefile '%s' %s\n",
						path, pgppassfd>0?"opened":"not opened");
				}
				if( path ) free(path); path = 0;
			}
		}
	}
	pipe_fd[0] = pipe_fd[1] = error_fd[0] = error_fd[1] = -1;
	pgpfile = safestrdup2(tempfile,".pgp",__FILE__,__LINE__);
	Check_max(&Tempfiles,1);
	if( !Debug ) Tempfiles.list[Tempfiles.count] = pgpfile;

	if( pipe(error_fd) <  0 ){
		logerr( LOG_INFO, "Send_auth_transfer: pipe failed" );
		goto error;
	}
	Check_max(&passfd,10);
	passfd.count = 0;
	passfd.list[passfd.count++] = Cast_int_to_voidstar(0);
	passfd.list[passfd.count++] = Cast_int_to_voidstar(error_fd[1]);
	passfd.list[passfd.count++] = Cast_int_to_voidstar(error_fd[1]);

	if( Is_server ){
		if( (tempfd = Checkread(Pgp_server_passphrasefile_DYN,&statb)) < 0 ){
			logerr(LOG_INFO,"Pgp_send: cannot open '%s'", Errormsg(errno));
			goto error;
		}
		Set_decimal_value(&env,"PGPPASSFD",passfd.count);
		passfd.list[passfd.count++] = Cast_int_to_voidstar(tempfd);  
	} else {
		if( pgppassfd > 0 ){
			Set_decimal_value(&env,"PGPPASSFD",passfd.count);
			passfd.list[passfd.count++] = Cast_int_to_voidstar(pgppassfd);
		}
	}

    /* now we run the PGP code */
    
    plp_snprintf(buffer,sizeof(buffer),
        "%s +armorlines=0 +verbose=0 +force +batch -se %s $%%%s -u $%%%s",
        Pgp_path_DYN, tempfile, esc_Auth_id_DYN, esc_Auth_sender_id_DYN);
	DEBUG2("Pgp_send: cmd '%s'", buffer );
	Add_line_list(&info,buffer,0,0,0);
    if( (pid = Make_passthrough(buffer, 0, &passfd, 0, &env )) < 0 ){
        logerr(LOG_INFO,"Pgp_send: '%s' failed", buffer);
        goto error;
    }
	Free_line_list(&env);
	passfd.count = 0;
	close(error_fd[1]); error_fd[1] = -1;
	close(tempfd); tempfd = -1;

	n = 0;
	while( n < sizeof(buffer)-1
		&& (len = read( error_fd[0], buffer+n, sizeof(buffer)-1-n )) > 0 ){
		buffer[n+len] = 0;
		while( (s = safestrchr(buffer,'\n')) ){
			*s++ = 0;
			DEBUG1("Pgp_send: error '%s'", buffer );
			if( buffer[0] ) Add_line_list(&info,buffer,0,0,0);
			memmove(buffer,s,strlen(s)+1);
		}
	}
	close(error_fd[0]); error_fd[0] = -1;
	DEBUG1("Pgp_send: EOF on error fd" );

	while( (n = plp_waitpid(pid,&procstatus,0)) != pid );
    DEBUG1("Ppg_send: pid %d, exit status '%s'", pid,
        Decode_status(&procstatus) );

	if( WIFEXITED(procstatus) && (n = WEXITSTATUS(procstatus)) ){
		logmsg(LOG_INFO,"Pgp_send: pgp sending process exit status %d", n);
		status = JFAIL;
		goto error;
	} else if( WIFSIGNALED(procstatus) ){
		logmsg(LOG_INFO,"Pgp_send: pgp sending process died with signal %d, '%s'",
			n, Sigstr(n));
		status = JFAIL;
		goto error;
	}
	if( !Is_server && (Verbose || Debug) ){
		for( i = 0; i < info.count; ++i ){
			if( Write_fd_str(1,info.list[i]) < 0
				|| Write_fd_str(1,"\n") < 0 ) cleanup(0);
		}
	}
	Free_line_list(&info);

	if( (tempfd = Checkread(pgpfile,&statb)) < 0 ){
		logerr(LOG_INFO,"Pgp_send: cannot open '%s'", tempfile );
		goto error;
	}
	jobsize = statb.st_size;
	plp_snprintf(buffer,sizeof(buffer),"%0.0f\n",jobsize);
	DEBUG2("Pgp_send: sending file size '%s'", buffer );
	if( Write_fd_str(*sock,buffer) < 0 ){
		logerr(LOG_INFO,"Pgp_send: write to sock failed" );
		goto error;
	}
	errno = 0;
	while( jobsize > 0 ){
		len = sizeof(buffer)-1;
		if( len > jobsize ) len = jobsize;
		if( read( tempfd, buffer, len) != len ){
			logerr(LOG_INFO,"Pgp_send: read from '%s' failed", tempfile );
			goto error;
		}
		buffer[len] = 0;
		DEBUG2("Pgp_send: file information '%s'", buffer );
		if( write( *sock, buffer, len) != len ){
			logerr(LOG_INFO,"Pgp_send: write to sock failed" );
			goto error;
		}
		jobsize -= len;
	}
	DEBUG2("Pgp_send: sent file" );
	close(tempfd); tempfd = -1;

	if( (tempfd = Checkwrite(pgpfile,&statb,O_WRONLY|O_TRUNC,1,0)) < 0){
		logerr( LOG_INFO, "Send_auth_transfer: open '%s' for write failed",
			tempfile);
		goto error;
	}
	DEBUG2("Send_auth_transfer: starting read");
	len = 0;
	while( (n = read(*sock,buffer,sizeof(buffer)-1)) > 0 ){
		buffer[n] = 0;
		DEBUG2("Send_auth_transfer: read '%s'", buffer);
		if( write(tempfd,buffer,n) != n ){
			logerr( LOG_INFO, "Send_auth_transfer: write '%s' failed",
				tempfile );
			goto error;
		}
		len += n;
	}
	close( tempfd ); tempfd = -1;

	DEBUG2("Send_auth_transfer: total %d bytes status read", len );
	if( len > 0 ){
		passfd.count = 0;
		Check_max(&passfd,10);
		if( pipe(error_fd) <  0 ){
			logerr( LOG_INFO, "Send_auth_transfer: pipe failed" );
			goto error;
		}
		passfd.list[passfd.count++] = Cast_int_to_voidstar(0);
		passfd.list[passfd.count++] = Cast_int_to_voidstar(error_fd[1]);
		passfd.list[passfd.count++] = Cast_int_to_voidstar(error_fd[1]);
		if( Is_server ){
			if( (tempfd = Checkread(Pgp_server_passphrasefile_DYN,&statb)) < 0 ){
				logerr(LOG_INFO,"Pgp_send: cannot open '%s'", Errormsg(errno));
				goto error;
			}   
			Set_decimal_value(&env,"PGPPASSFD",passfd.count);
			passfd.list[passfd.count++] = Cast_int_to_voidstar(tempfd);  
		} else {
			if( pgppassfd > 0 ){
				if( lseek(pgppassfd,0,SEEK_SET) ){
					Errorcode = JFAIL;
					logerr_die(LOG_INFO,"Pgp_send: lseek pgppassfd failed");
				}
				Set_decimal_value(&env,"PGPPASSFD",passfd.count);
				passfd.list[passfd.count++] = Cast_int_to_voidstar(pgppassfd);
			}
		}

		/* now we run the PGP code */
		
		plp_snprintf(buffer,sizeof(buffer),
			"%s +verbose=0 +force +batch %s -u $%%%s -o %s",
			Pgp_path_DYN, pgpfile, esc_Auth_sender_id_DYN, tempfile );
		DEBUG2("Pgp_send: receive cmd '%s'", buffer );
		if( (pid = Make_passthrough(buffer, 0, &passfd, 0, &env )) < 0 ){
			logerr(LOG_INFO,"Pgp_send: fork for '%s' failed", buffer);
			goto error;
		}
		close(error_fd[1]); error_fd[1] = -1;
		close(tempfd); tempfd = -1;
		passfd.count = 0;

		n = 0;
		buffer[0] = 0;
		while( n < sizeof(buffer)-1
			&& (len = read( error_fd[0], buffer+n, sizeof(buffer)-1-n )) > 0 ){
			buffer[n+len] = 0;
			while( (s = safestrchr(buffer,'\n')) ){
				*s++ = 0;
				DEBUG4("Pgp_send: error '%s'", buffer );
				if( buffer[0] ) Add_line_list(&info,buffer,0,0,0);
				memmove(buffer,s,strlen(s)+1);
			}
		}
		close(error_fd[0]); error_fd[0] = -1;
		while( (n = plp_waitpid(pid,&procstatus,0)) != pid );
		DEBUG1("Ppg_send: pid %d, exit status '%s'", pid,
			Decode_status(&procstatus) );
		if( WIFEXITED(procstatus) && (n = WEXITSTATUS(procstatus)) ){
			logmsg(LOG_INFO,"Pgp_send: pgp receiving process exit status %d", n);
			Errorcode = JFAIL;
			goto error;
		} else if( WIFSIGNALED(procstatus) ){
			logmsg(LOG_INFO,"Pgp_send: pgp receving process died with signal %d, '%s'",
				n, Sigstr(n));
			Errorcode = JFAIL;
			goto error;
		}
		if( !Is_server && Verbose ){
			for( i = 0; i < info.count; ++i ){
				if( Write_fd_str(1,info.list[i]) < 0
					|| Write_fd_str(1,"\n") < 0 ) cleanup(0);
			}
		}
		Free_line_list(&info);
	}
	if( (tempfd = Checkread(tempfile,&statb)) < 0 ){
		logerr(LOG_INFO,"Pgp_send: cannot open '%s'", tempfile );
		goto error;
	}
	jobsize = statb.st_size;
	DEBUG2("Pgp_send: %0.0f bytes status, duping %d to %d", (double)jobsize, tempfd, *sock );
	if( dup2( tempfd, *sock ) == -1 ){
		logerr(LOG_INFO,"Pgp_send: dup2(%d,%d)", tempfd, *sock );
		goto error;
	}
	close(tempfd); tempfd = -1;

	status = 0;

 error:
	if( status && info.count > 0 ){
		for( i = 0; i < info.count; ++i ){
			logmsg(LOG_INFO,"error '%s'", info.list[i] );
		}
	}
	if( pipe_fd[0] >= 0 ) close(pipe_fd[0]); pipe_fd[0] = -1;
	if( tempfd >= 0 ) close(tempfd); tempfd = -1;
	if( error_fd[0] >= 0 ) close(error_fd[0]); error_fd[0] = -1;
	if( error_fd[1] >= 0 ) close(error_fd[1]); error_fd[1] = -1;
	if( pipe_fd[0] >= 0 ) close(pipe_fd[0]); pipe_fd[0] = -1;
	if( pipe_fd[1] >= 0 ) close(pipe_fd[1]); pipe_fd[1] = -1;
	Free_line_list(&passfd);
	Free_line_list(&env);
	Free_line_list(&info);
	return(status);
}

#if defined(HAVE_KRB_H) && defined(MIT_KERBEROS4)
# include <krb.h>
# include <des.h>

#if !defined(HAVE_KRB_AUTH_DEF)
 extern int  krb_sendauth(
  long options,            /* bit-pattern of options */
  int fd,              /* file descriptor to write onto */
  KTEXT ticket,   /* where to put ticket (return) or supplied in case of KOPT_DONT_MK_REQ */
  char *service, char *inst, char *realm,    /* service name, instance, realm */
  u_long checksum,         /* checksum to include in request */
  MSG_DAT *msg_data,       /* mutual auth MSG_DAT (return) */
  CREDENTIALS *cred,       /* credentials (return) */
  Key_schedule schedule,       /* key schedule (return) */
  struct sockaddr_in *laddr,   /* local address */
  struct sockaddr_in *faddr,   /* address of foreign host on fd */
  char *version);           /* version string */
 extern char* krb_realmofhost( char *host );
#endif


 struct krberr{
	int err; char *code;
 } krb4_errs[] = {
	{1, "KDC_NAME_EXP 'Principal expired'"},
	{2, "KDC_SERVICE_EXP 'Service expired'"},
	{2, "K_LOCK_EX 'Exclusive lock'"},
	{3, "KDC_AUTH_EXP 'Auth expired'"},
	{4, "CLIENT_KRB_TIMEOUT 'time between retries'"},
	{4, "KDC_PKT_VER 'Protocol version unknown'"},
	{5, "KDC_P_MKEY_VER 'Wrong master key version'"},
	{6, "KDC_S_MKEY_VER 'Wrong master key version'"},
	{7, "KDC_BYTE_ORDER 'Byte order unknown'"},
	{8, "KDC_PR_UNKNOWN 'Principal unknown'"},
	{9, "KDC_PR_N_UNIQUE 'Principal not unique'"},
	{10, "KDC_NULL_KEY 'Principal has null key'"},
	{20, "KDC_GEN_ERR 'Generic error from KDC'"},
	{21, "GC_TKFIL 'Can't read ticket file'"},
	{21, "RET_TKFIL 'Can't read ticket file'"},
	{22, "GC_NOTKT 'Can't find ticket or TGT'"},
	{22, "RET_NOTKT 'Can't find ticket or TGT'"},
	{26, "DATE_SZ 'RTI date output'"},
	{26, "MK_AP_TGTEXP 'TGT Expired'"},
	{31, "RD_AP_UNDEC 'Can't decode authenticator'"},
	{32, "RD_AP_EXP 'Ticket expired'"},
	{33, "RD_AP_NYV 'Ticket not yet valid'"},
	{34, "RD_AP_REPEAT 'Repeated request'"},
	{35, "RD_AP_NOT_US 'The ticket isn't for us'"},
	{36, "RD_AP_INCON 'Request is inconsistent'"},
	{37, "RD_AP_TIME 'delta_t too big'"},
	{38, "RD_AP_BADD 'Incorrect net address'"},
	{39, "RD_AP_VERSION 'protocol version mismatch'"},
	{40, "RD_AP_MSG_TYPE 'invalid msg type'"},
	{41, "RD_AP_MODIFIED 'message stream modified'"},
	{42, "RD_AP_ORDER 'message out of order'"},
	{43, "RD_AP_UNAUTHOR 'unauthorized request'"},
	{51, "GT_PW_NULL 'Current PW is null'"},
	{52, "GT_PW_BADPW 'Incorrect current password'"},
	{53, "GT_PW_PROT 'Protocol Error'"},
	{54, "GT_PW_KDCERR 'Error returned by KDC'"},
	{55, "GT_PW_NULLTKT 'Null tkt returned by KDC'"},
	{56, "SKDC_RETRY 'Retry count exceeded'"},
	{57, "SKDC_CANT 'Can't send request'"},
	{61, "INTK_W_NOTALL 'Not ALL tickets returned'"},
	{62, "INTK_BADPW 'Incorrect password'"},
	{63, "INTK_PROT 'Protocol Error'"},
	{70, "INTK_ERR 'Other error'"},
	{71, "AD_NOTGT 'Don't have tgt'"},
	{72, "AD_INTR_RLM_NOTGT 'Can't get inter-realm tgt'"},
	{76, "NO_TKT_FIL 'No ticket file found'"},
	{77, "TKT_FIL_ACC 'Couldn't access tkt file'"},
	{78, "TKT_FIL_LCK 'Couldn't lock ticket file'"},
	{79, "TKT_FIL_FMT 'Bad ticket file format'"},
	{80, "TKT_FIL_INI 'tf_init not called first'"},
	{81, "KNAME_FMT 'Bad Kerberos name format'"},
	{100, "MAX_HSTNM 'for compatibility'"},
	{512, "CLIENT_KRB_BUFLEN 'max unfragmented packet'"},
	{0,0}
	};

char *krb4_err_str( int err )
{
	int i;
	char *s = 0;
	for( i = 0; (s = krb4_errs[i].code) && err != krb4_errs[i].err; ++i );
	if( s == 0 ){
		static char msg[24];
		plp_snprintf(msg,sizeof(msg),"UNKNOWN %d", err );
		s = msg;
	}
	return(s);
}

#endif


int Send_krb4_auth(int *sock, struct job *job, int transfer_timeout )
{
	int status = JFAIL;
#if defined(HAVE_KRB_H) && defined(MIT_KERBEROS4)
	int ack, i;
	KTEXT_ST ticket;
	char buffer[1], *host;
	char line[LINEBUFFER];

	status = 0;
	host = RemoteHost_DYN;
	if( !safestrcasecmp( host, LOCALHOST ) ){
		host = FQDNHost_FQDN;
	}
	if( !safestrchr( host, '.' ) ){
		if( !(host = Find_fqdn(&LookupHost_IP, host)) ){
			setstatus(job, "cannot find FQDN for '%s'", host );
			return JFAIL;
		}
	}
	DEBUG1("Send_krb4_auth: FQND host '%s'", host );
	setstatus(job, "sending krb4 auth to %s@%s",
		RemotePrinter_DYN, host);
	plp_snprintf(line, sizeof(line), "%c%s\n", REQ_K4AUTH, RemotePrinter_DYN);
	status = Link_send(host, sock, transfer_timeout, line,
		strlen(line), &ack);
	DEBUG1("Send_krb4_auth: krb4 auth request ACK status %d, ack %d", status, ack );
	if( status ){
		setstatus(job, "Printer %s@%s does not support krb4 authentication",
			RemotePrinter_DYN, host);
		return JFAIL;
	}
	status = krb_sendauth(0, *sock, &ticket, KLPR_SERVICE, host,
		krb_realmofhost(host), 0, NULL, NULL,
		NULL, NULL, NULL, "KLPRV0.1");
	DEBUG1("Send_krb4_auth: krb_sendauth status %d, '%s'",
		status, krb4_err_str(status) );
	if( status != KSUCCESS ){
		setstatus(job, "krb4 authentication failed to %s@%s - %s",
			RemotePrinter_DYN, host, krb4_err_str(status));
		return JFAIL;
	}
	buffer[0] = 0;
	i = Read_fd_len_timeout(transfer_timeout, *sock, buffer, 1);
	if (i <= 0 || Alarm_timed_out){
		status = LINK_TRANSFER_FAIL;
	} else if(buffer[0]){
		status = LINK_ACK_FAIL;
	}
	if(status){
		setstatus(job,
			"krb4 authentication failed to %s@%s",
			RemotePrinter_DYN, host);
	} else {
		setstatus(job,
			"krb4 authentication succeeded to %s@%s",
			RemotePrinter_DYN, host);
	}
#endif
	return(status);
}
