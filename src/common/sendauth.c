/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2000, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: sendauth.c,v 5.19 2000/12/25 01:51:14 papowell Exp papowell $";

#include "lp.h"
#include "lpd.h"
#include "sendauth.h"
#include "sendjob.h"
#include "globmatch.h"
#include "permission.h"
#include "getqueue.h"
#include "errorcodes.h"
#include "linksupport.h"
#include "krb5_auth.h"
#include "fileopen.h"
#include "child.h"
#include "gethostinfo.h"
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
 * 1. Get a temporary file
 * 2. Generate the compressed data files - this has the format
 *      Authentication
 *      \n
 *      \3count cfname\n
 *      [count control file bytes]
 *      \4count dfname\n
 *      [count data file bytes]
 *
 * 3. send the \REQ_SECRemotePrinter_DYN user@RemoteHost_DYN file size\n
 *    string to the remote RemoteHost_DYN, wait for an ACK
 *
 * 4. send the compressed data files - this has the format
 *      wait for an ACK
 ***************************************************************************/

/*
 * Send_auth_transfer
 *  1. we send the command line and wait for ACK of 0
 *  \REQ_SEQUREprinter C/F sender_id authtype [jobsize]
 *  2. if authtype == kerberos we do kerberos
 *      - send a file to the remote end
 *      - get back a file
 *  3. if authtype == pgp we do pgp
 *      - same as kerberos
 *  3. if otherwise,  we start a process with command line options
 *       fd 0 -  sock
 *       fd 1 -  for reports
 *       fd 2 -  for errors
 *    /filter -C -P printer -n sender_id -A authtype -R remote_id -Ttempfile
 *    The tempfile will be sent to the remote end and status
 *     written back on fd 2
 *     - we save this information
 *     - reopen the file and put error messages in it.
 *  RETURN:
 *     0 - no error
 *     !=0 - error
 */

int Send_auth_transfer( int *sock, int transfer_timeout,
	struct job *job, struct job *logjob, char *error, int errlen, char *cmd,
	struct security *security, struct line_list *info )
{
	struct stat statb;
	int ack, len, n, fd;		/* ACME! The best... */
	int status = JFAIL;			/* job status */
	char *secure, *destination, *from, *client, *s;
	char *tempfile;
	char buffer[SMALLBUFFER];
	errno = 0;

	secure = 0;
	fd = Make_temp_fd(&tempfile);

	if( cmd && (s = safestrrchr(cmd,'\n')) ) *s = 0;
	DEBUG1("Send_auth_transfer: cmd '%s'", cmd );

	if(DEBUGL1)Dump_line_list("Send_auth_transfer: info ", info );

	destination = Find_str_value(info, DESTINATION, Value_sep );
	from = Find_str_value(info, FROM, Value_sep );
	client = Find_str_value(info, CLIENT, Value_sep );

	if( safestrcmp(security->config_tag, "kerberos") ){
		Put_in_auth(fd,DESTINATION,destination);
		if( Is_server ) Put_in_auth(fd,SERVER,from);
		Put_in_auth(fd,CLIENT,client);
		if( cmd ){
			Put_in_auth(fd,INPUT,cmd);
		}
	} else {
		if( cmd && (Write_fd_str(fd,cmd) < 0 || Write_fd_str(fd,"\n") < 0) ){
			SNPRINTF(error, errlen) "Send_auth_transfer: '%s' write failed - %s",
				tempfile, Errormsg(errno) );
			goto error;
		}
		if( Is_server && (Write_fd_str(fd,client) < 0 || Write_fd_str(fd,"\n") < 0) ){
			SNPRINTF(error, errlen) "Send_auth_transfer: '%s' write failed - %s",
				tempfile, Errormsg(errno) );
			goto error;
		}
	}

	if( Write_fd_str(fd,"\n") < 0 ){
		SNPRINTF(error, errlen) "Send_auth_transfer: '%s' write failed - %s",
			tempfile, Errormsg(errno) );
		goto error;
	}

	s = Find_str_value(info, CMD, Value_sep );
	if( job ){
        status = Send_normal( &fd, job, logjob, transfer_timeout, fd, 0);
        if( status ) return( status );
		errno = 0;
		if( stat(tempfile,&statb) ){
			Errorcode = JABORT;
			LOGERR_DIE(LOG_INFO)"Send_auth_transfer: stat '%s' failed",
				tempfile);
		}
		SNPRINTF( buffer,sizeof(buffer))" %0.0f",(double)(statb.st_size) );
		secure = safestrdup3(s,buffer,"\n",__FILE__,__LINE__);
	} else {
		secure = safestrdup2(s,"\n",__FILE__,__LINE__);
	}
	close( fd ); fd = -1;

	/* send the message */
	DEBUG3("Send_auth_transfer: sending '%s'", secure );
	status = Link_send( RemoteHost_DYN, sock, transfer_timeout,
		secure, strlen(secure), &ack );
	DEBUG3("Send_auth_transfer: status '%s'", Link_err_str(status) );
	if( status ){
		/* open output file */
		if( (fd = Checkwrite(tempfile,&statb,O_WRONLY|O_TRUNC,1,0)) < 0){
			Errorcode = JABORT;
			LOGERR_DIE(LOG_INFO) "Send_auth_transfer: open '%s' for write failed",
				tempfile);
		}
		/* we turn off IO from the socket */
		shutdown(*sock,1);
		if( (s = safestrchr(buffer,'\n')) ) *s = 0;
		SNPRINTF( error, errlen)
			"error '%s' sending '%s' to %s@%s\n",
			Link_err_str(status), buffer, RemotePrinter_DYN, RemoteHost_DYN );
		Write_fd_str( fd, error );
		error[0] = 0;
		DEBUG2("Send_auth_transfer: starting read");
		len = 0;
		while( (n = read(*sock,buffer+len,sizeof(buffer)-1-len)) > 0 ){
			buffer[n+len] = 0;
			DEBUG4("Send_auth_transfer: read '%s'", buffer);
			while( (s = strchr(buffer,'\n')) ){
				*s++ = 0;
				DEBUG2("Send_auth_transfer: doing '%s'", buffer);
				SNPRINTF(error,errlen)"%s\n", buffer );
				if( Write_fd_str(fd,error) < 0 ){
					Errorcode = JABORT;
					LOGERR(LOG_INFO) "Send_auth_transfer: write '%s' failed",
						tempfile );
					goto error;
				}
				memmove(buffer,s,strlen(s)+1);
			}
			len = strlen(buffer);
		}
		if( buffer[0] ){
			DEBUG2("Send_auth_transfer: doing '%s'", buffer);
			SNPRINTF(error,errlen)"%s\n", buffer );
			if( Write_fd_str(fd,error) < 0 ){
				Errorcode = JABORT;
				LOGERR(LOG_INFO) "Send_auth_transfer: write '%s' failed",
					tempfile );
				goto error;
			}
		}

		close( fd ); fd = -1;
		error[0] = 0;
		goto error;
	}

	/*
     * now we do the protocol dependent exchange
     */

	status = security->send( sock, transfer_timeout, tempfile,
		error, errlen, security, info );

 error:

	/* we are going to put the returned error status in the temp file
	 * as the device to read from
	 */
	if( secure ) free(secure); secure = 0;
	if( error[0] ){
		if( job ){
			SETSTATUS(logjob)"Send_auth_transfer: %s", error );
			Set_str_value(&job->info,ERROR,error);
		}
		if( (fd = Checkwrite(tempfile,&statb,O_WRONLY|O_TRUNC,1,0)) < 0){
			Errorcode = JFAIL;
			LOGERR_DIE(LOG_INFO)"Send_auth_transfer: cannot open '%s'", tempfile );
		}
		Write_fd_str(fd,error);
		close( fd ); fd = -1;
		error[0] = 0;
	}
	if( *sock >= 0 ){
		if( (fd = Checkread(tempfile,&statb)) < 0 ){
			Errorcode = JFAIL;
			LOGERR_DIE(LOG_INFO)"Send_auth_transfer: cannot open '%s'", tempfile );
		}
		if( dup2( fd, *sock ) == -1 ){
			Errorcode = JFAIL;
			LOGERR_DIE(LOG_INFO)"Send_auth_transfer: dup2(%d,%d)", fd, *sock );
		}
		if( fd != *sock ) close(fd); fd = -1;
	}
	Free_line_list(info);
	DEBUG3("Send_auth_transfer: exit status %d, error '%s'",
		status, error );
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

int Pgp_send( int *sock, int transfer_timeout, char *tempfile,
	char *error, int errlen,
	struct security *security, struct line_list *info )
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
	from = Find_str_value( info, FROM, Value_sep);
	destination = Find_str_value( info, ID, Value_sep );

	tempfd = -1;

	Init_line_list( &pgp_info );
    pgpfile = safestrdup2(tempfile,".pgp",__FILE__,__LINE__); 
    Check_max(&Tempfiles,1);
    Tempfiles.list[Tempfiles.count++] = pgpfile;

	status = Pgp_encode( info, tempfile, pgpfile, &pgp_info,
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
		SNPRINTF(error,errlen)
			"Pgp_send: cannot open '%s' - %s", pgpfile, Errormsg(errno) );
		goto error;
	}

	DEBUG1("Pgp_send: encrypted file size '%0.0f'", (double)(statb.st_size) );
	SNPRINTF(buffer,sizeof(buffer))"%0.0f\n",(double)(statb.st_size) );
	Write_fd_str(*sock,buffer);

	while( (len = read( tempfd, buffer, sizeof(buffer)-1 )) > 0 ){
		buffer[len] = 0;
		DEBUG4("Pgp_send: file information '%s'", buffer );
		if( write( *sock, buffer, len) != len ){
			SNPRINTF(error,errlen)
			"Pgp_send: write to socket failed - %s", Errormsg(errno) );
			goto error;
		}
	}

	DEBUG2("Pgp_send: sent file" );
	close(tempfd); tempfd = -1;
	/* we close the writing side */
	shutdown( *sock, 1 );
	if( (tempfd = Checkwrite(pgpfile,&statb,O_WRONLY|O_TRUNC,1,0)) < 0){
		SNPRINTF(error,errlen)
			"Pgp_send: open '%s' for write failed - %s", pgpfile, Errormsg(errno));
		goto error;
	}
	DEBUG2("Pgp_send: starting read");
	len = 0;
	while( (n = read(*sock,buffer,sizeof(buffer)-1)) > 0 ){
		buffer[n] = 0;
		DEBUG4("Pgp_send: read '%s'", buffer);
		if( write(tempfd,buffer,n) != n ){
			SNPRINTF(error,errlen)
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
		status = Pgp_decode( info, tempfile, pgpfile, &pgp_info,
			buffer, sizeof(buffer), error, errlen, from, info,
			&pgp_exit_code, &not_a_ciphertext );
		if( not_a_ciphertext ){
			DEBUG2("Pgp_send: not a ciphertext" );
			if( (tempfd = Checkwrite(tempfile,&statb,O_WRONLY|O_TRUNC,1,0)) < 0){
				SNPRINTF(error,errlen)
				"Pgp_send: open '%s' for write failed - %s",
					tempfile, Errormsg(errno));
			}
			if( (fd = Checkread(pgpfile,&statb)) < 0){
				SNPRINTF(error,errlen)
				"Pgp_send: open '%s' for write failed - %s",
					pgpfile, Errormsg(errno));
			}
			if( error[0] ){
				Write_fd_str(tempfd,error);
				Write_fd_str(tempfd,"\n Contents -\n");
			}
			error[0] = 0;
			len = 0;
			while( (n = read(fd, buffer+len, sizeof(buffer)-len-1)) > 0 ){
				DEBUG2("Pgp_send: read '%s'", buffer );
				while( (s = strchr( buffer, '\n')) ){
					*s++ = 0;
					for( t = buffer; *t; ++t ){
						if( !isprint(cval(t)) ) *t = ' ';
					}
					SNPRINTF(error,errlen)"  %s\n", buffer);
					Write_fd_str(tempfd, error );
					DEBUG2("Pgp_send: wrote '%s'", error );
					memmove(buffer,s,strlen(s)+1);
				}
				len = strlen(buffer);
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
		DEBUG2("Pgp_send: writing error to file '%s'", error );
		if( (tempfd = Checkwrite(tempfile,&statb,O_WRONLY|O_TRUNC,1,0)) < 0){
			SNPRINTF(error,errlen)
			"Pgp_send: open '%s' for write failed - %s",
				tempfile, Errormsg(errno));
		}
		strncpy( buffer, error, sizeof(buffer) -1 );
		buffer[sizeof(buffer)-1 ] = 0;
		while( (s = strchr( buffer, '\n')) ){
			*s++ = 0;
			for( t = buffer; *t; ++t ){
				if( !isprint(cval(t)) ) *t = ' ';
			}
			SNPRINTF(error,errlen)"  %s\n", buffer);
			Write_fd_str(tempfd, error );
			DEBUG2("Pgp_send: wrote '%s'", error );
			memmove(buffer,s,strlen(s)+1);
		}
		close( tempfd ); tempfd = -1;
		error[0] = 0;
	}
	Free_line_list(&pgp_info);
	return(status);
}
/***************************************************************************
 * void Fix_send_auth() - get the Use_auth_DYN value for the remote printer
 ***************************************************************************/

 extern struct security SendSecuritySupported[];

/****************************************************************************************
 * struct security *Fix_send_auth( char *name, struct line_list *info
 * 	char *error, int errlen )
 * 
 * Find the information about the encrypt type and then make up the string
 * to send to the server requesting the encryption
 ****************************************************************************************/

struct security *Fix_send_auth( char *name, struct line_list *info,
	struct job *job, char *error, int errlen )
{
	struct security *security = 0;
	char buffer[SMALLBUFFER], *tag, *key, *from, *client, *destination;

	if( name == 0 ){
		if( Is_server ){
			name = Auth_forward_DYN;
		} else {
			name = Auth_DYN;
		}
	}
	DEBUG1("Fix_send_auth: name '%s'", name );
	if( name ){
		for( security = SendSecuritySupported; security->name; ++security ){
			DEBUG1("Fix_send_auth: security '%s'", security->name );
			if( !Globmatch(security->name, name ) ) break;
		}
		DEBUG1("Fix_send_auth: name '%s' matches '%s'", name, security->name );
		if( security->name == 0 ){
			security = 0;
			SNPRINTF(error, errlen)
				"Send_auth_transfer: '%s' security not supported", name );
			goto error;
		}
	} else {
		DEBUG1("Fix_send_auth: no security" );
		return( 0 );
	}

	if( !(tag = security->config_tag) ) tag = security->name;
	SNPRINTF(buffer,sizeof(buffer))"%s_", tag );
	Find_default_tags( info, Pc_var_list, buffer );
	Find_tags( info, &Config_line_list, buffer );
	Find_tags( info, &PC_entry_line_list, buffer );
	if(DEBUGL1)Dump_line_list("Fix_send_auth: found info", info );

	if( !(tag = security->config_tag) ) tag = security->name;
	if( Is_server ){
		/* forwarding */
		key = "F";
		from = Find_str_value(info,ID,Value_sep);
		if(!from)from = Find_str_value(info,"server_principal",Value_sep);
		if( from == 0 && safestrcmp(tag,"kerberos") && safestrcmp(tag,"none") ){
			SNPRINTF(error, errlen)
			"Send_auth_transfer: '%s' security missing '%s_id' info", tag, tag );
			goto error;
		}
		Set_str_value(info,FROM,from);
		if( job ){
			client = Find_str_value(&job->info,AUTHINFO,Value_sep);
			Set_str_value(info,CLIENT,client);
		} else {
			client = (char *)Perm_check.authuser;
		}
		if( client == 0 
			&& !(client = Find_str_value(info,"default_client_name",Value_sep))
			&& safestrcmp(tag,"none") ){
			SNPRINTF(error, errlen)
			"Send_auth_transfer: security '%s' missing authenticated client", tag );
			goto error;
		}
		Set_str_value(info,CLIENT,client);
		destination = Find_str_value(info,FORWARD_ID,Value_sep);
		if(!destination)destination = Find_str_value(info,"forward_principal",Value_sep);
		if( destination == 0 && safestrcmp(tag, "kerberos")
			&& safestrcmp(tag, "none")){
			SNPRINTF(error, errlen)
			"Send_auth_transfer: '%s' security missing '%s_forward_id' info", tag, tag );
			goto error;
		}
	} else {
		/* from client */
		key = "C";
		from = Logname_DYN;
		Set_str_value(info,FROM,from);
		client = Logname_DYN;
		Set_str_value(info,CLIENT,client);
		destination = Find_str_value(info,ID,Value_sep);
		if(!destination)destination = Find_str_value(info,"server_principal",Value_sep);
		if( destination == 0 && safestrcmp(tag, "kerberos")
			&& safestrcmp(tag, "none") ){
			SNPRINTF(error, errlen)
			"Send_auth_transfer: '%s' security missing '%s_id' info", tag, tag );
			goto error;
		}
	}

	Set_str_value(info,DESTINATION,destination);

	DEBUG1("Send_auth_transfer: pr '%s', key '%s', from '%s',"
		" destination '%s'",
		RemotePrinter_DYN,key, from, tag);
	SNPRINTF( buffer, sizeof(buffer))
		"%c%s %s %s %s",
		REQ_SECURE,RemotePrinter_DYN,key, from, tag );
	Set_str_value(info,CMD,buffer);
	DEBUG1("Send_auth_transfer: sending '%s'", buffer );

 error:
	if( error[0] ) security = 0;
	DEBUG1("Fix_send_auth: error '%s'", error );
	if(DEBUGL1)Dump_line_list("Fix_send_auth: info", info );
  
	return(security);
}

void Put_in_auth( int tempfd, const char *key, char *value )
{
	char *v = Escape(value,1);
	DEBUG1("Put_in_auth: fd %d, key '%s' value '%s', v '%s'",
		tempfd, key, value, v );
	if(
		Write_fd_str(tempfd,key) < 0
		|| Write_fd_str(tempfd,"=") < 0
		|| Write_fd_str(tempfd,v) < 0
		|| Write_fd_str(tempfd,"\n") < 0
		){
		Errorcode = JFAIL;
		LOGERR_DIE(LOG_INFO)"Put_in_auth: cannot write to file" );
	}
	if( v ) free(v); v = 0;
}


/*
 * we include user specified code for authentication
 */

#define SENDING
#include "user_auth.stub"

 struct security SendSecuritySupported[] = {
	/* name,       config_tag, connect,    send,   receive */
#if defined(KERBEROS)
	{ "kerberos*", "kerberos", 0,           Krb5_send },
# if defined(MIT_KERBEROS4)
	{ "kerberos4", "kerberos", Send_krb4_auth, 0 },
# endif
#endif
	{ "pgp",       "pgp",      0,           Pgp_send },
	{ "none",      "none",     0,           0 },
#if defined(USER_SEND)
 USER_SEND
#endif
	{0}
};
