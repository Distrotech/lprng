/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: sendjob.c,v 5.2 1999/10/09 20:49:00 papowell Exp papowell $";


#include "lp.h"

#include "accounting.h"
#include "errorcodes.h"
#include "fileopen.h"
#include "getqueue.h"
#include "linksupport.h"
#include "sendauth.h"
#include "sendjob.h"

/**** ENDINCLUDE ****/


/***************************************************************************
 * Commentary:
 * The protocol used to send a job to a remote RemoteHost_DYN consists of the
 * following:
 * 
 * Client                                   Server
 * \2RemotePrinter_DYNname\n - receive a job
 *                                          \0  (ack)
 * \2count controlfilename\n
 * <count bytes>
 * \0
 *                                          \0
 * \3count datafilename\n
 * <count bytes>
 * \0
 *                                          \0
 * \3count datafilename\n
 * <count bytes>
 * \0
 *                                          \0
 * <close connection>
 * 
 * 1. In order to abort the job transfer,  client sends \1
 * 2. Anything but a 0 ACK is an error indication
 * 
 * NB: some spoolers require that the data files be sent first.
 * The same transfer protocol is followed, but the data files are
 * send first,  followed by the control file.
 * 
 * The Send_job() routine will try to transfer a control file
 * to the remote RemoteHost_DYN.  It does so using the following algorithm.
 * 
 * 1.  makes a connection (connection timeout)
 * 2.  sends the \2RemotePrinter_DYN and gets ACK (transfer timeout)
 * 3.  sends the control file (transfer timeout)
 * 4.  sends the data files (transfer timeout)
 * 
 * int Send_job(
 * 	struct jobfile *job,	- control file
 * 	int connect_timeout_len,	- timeout on making connection
 * 	int connect_interval,	- interval between retries
 * 	int max_connect_interval - maximum connection interval
 * 	int transfer_timeout )	- timeout on sending a file
 * 
 * 	RETURNS: 0 if successful, non-zero if not
 **************************************************************************/

int Send_job( struct job *job,
	int connect_timeout_len, int connect_interval, int max_connect_interval,
	int transfer_timeout )
{
	int sock = -1;		/* socket to use */
	char *id = 0, *s;
	char *real_host = 0, *save_host = 0;
	int status = 0, err, errcount = 0, n, len;
	char msg[SMALLBUFFER];
	char error[SMALLBUFFER];
	struct sockaddr src_sin, *bindto = 0;
 
	/* fix up the control file */
	if(DEBUGL1)Dump_job("Send_job- starting",job);
	Errorcode = 0;

	/* determine authentication type to use */
	bindto = Fix_auth(1,&src_sin);
	Add_line_list(&job->info,"error@",Value_sep,1,1);
	/* send job to the LPD server for the RemotePrinter_DYN */

	id = Find_str_value( &job->info,IDENTIFIER,Value_sep);
	if( id == 0 ) id = Find_str_value( &job->info,TRANSFERNAME,Value_sep);
	DEBUG3("Send_job: '%s'->%s@%s,connect(timeout %d,interval %d)",
		id, RemotePrinter_DYN, RemoteHost_DYN,
			connect_timeout_len, connect_interval );

	/* we check if we need to do authentication */

	setstatus( job,
	"sending job '%s' to %s@%s",
		id, RemotePrinter_DYN, RemoteHost_DYN );
	/* we pause on later tries */

 retry_connect:
	Set_str_value(&job->info,ERROR,0);
	setstatus( job, "connecting to '%s', attempt %d",
		RemoteHost_DYN, errcount+1 );
	if( Network_connect_grace_DYN > 0 ){
		plp_sleep( Network_connect_grace_DYN );
	}

	errno = 0;
	sock = Link_open_list( RemoteHost_DYN,
		&real_host, 0, connect_timeout_len, bindto );
	err = errno;
	DEBUG4("Send_job: socket %d", sock );
	if( sock < 0 ){
		++errcount;
		status = LINK_OPEN_FAIL;
		msg[0] = 0;
		if( !Is_server && err ){
			plp_snprintf( msg, sizeof(msg),
			"\nMake sure LPD server is running on the server");
		}
		plp_snprintf( error, sizeof(error)-2,
			"cannot open connection to %s - %s%s", RemoteHost_DYN,
				err?Errormsg(err):"bad or missing hostname?", msg );
		setstatus( job, error );
		if( Is_server && Retry_NOLINK_DYN ){
			if( connect_interval > 0 ){
				n = (connect_interval * (1 << (errcount - 1)));
				if( max_connect_interval && n > max_connect_interval ){
					n = max_connect_interval;
				}
				if( n > 0 ){
					setstatus( job,
					_("sleeping %d secs before retry, starting sleep"),n );
					plp_sleep( n );
				}
			}
			goto retry_connect;
		}
		goto error;
	}
	save_host = safestrdup(RemoteHost_DYN,__FILE__,__LINE__);
	Set_DYN(&RemoteHost_DYN, real_host );
	if( real_host ) free( real_host );
	setstatus( job, "connected to '%s'", RemoteHost_DYN );

	if( (Is_server && Auth_forward_DYN
		&& safestrcasecmp(Auth_client_id_DYN,NONEP)
		&& safestrcasecmp(Auth_forward_DYN,NONEP))
		|| (!Is_server && Auth_DYN && safestrcasecmp(Auth_DYN,NONEP)) ){
		/* we send the Kerberos 4 authentication,
		 * then use normal transfer.  This is only for client to server
		 * otherwise we are in trouble.
		 */
		if( safestrcasecmp( Auth_DYN, KERBEROS4 ) ){
			status = Send_secure_block( &sock, job, transfer_timeout );
		} else if( !Is_server ){
			if( (status = Send_krb4_auth( &sock, job,
				transfer_timeout)) ){
				close( sock ); sock = -1;
				goto error;
			} else {
				status = Send_normal( &sock, job, transfer_timeout, 0 );
			}
		} else {
			plp_snprintf(error,sizeof(error),
				"kerberos 4 job transfer not available" );
			status = JFAIL;
			close( sock ); sock = -1;
			goto error;
		}
	} else if( Send_block_format_DYN ){
		status = Send_block( &sock, job, transfer_timeout );
	} else {
		status = Send_normal( &sock, job, transfer_timeout, 0 );
	}
	DEBUG2("Send_job: after sending, status %d", status );
	if( status ) goto error;

	setstatus( job, "done job '%s' transfer to %s@%s",
		id, RemotePrinter_DYN, RemoteHost_DYN );

 error:

	if( status ){
		if( (s = Find_str_value(&job->info,ERROR,Value_sep )) ){
			setstatus( job, "%s", s );
		}
		plp_snprintf(error, sizeof(error)-2,
			"job '%s' transfer to %s@%s failed",
			id, RemotePrinter_DYN, RemoteHost_DYN);
		setstatus( job, error );
		DEBUG2("Send_job: sock is %d", sock);
		if( sock >= 0 ){
			len = 0;
			msg[0] = 0;
			while( len < sizeof(msg)-1
				&& (n = read(sock,msg+len,sizeof(msg)-len-1)) > 0 ){
				msg[len+n] = 0;
				DEBUG2("Send_job: read %d, '%s'", n, msg);
				while( (s = safestrchr(msg,'\n')) ){
					*s++ = 0;
					setstatus( job, "error msg: '%s'", msg );
					memmove(msg,s,strlen(s)+1);
				}
				len = strlen(msg);
			}
			DEBUG2("Send_job: read %d, '%s'", n, msg);
			if( len ) setstatus( job, "error msg: '%s'", msg );
		}
	}
	if( sock >= 0 ) close(sock); sock = -1;
	if( save_host ){
		Set_DYN(&RemoteHost_DYN,save_host);
		free(save_host); save_host = 0;
	}
	return( status );
}

/***************************************************************************
 * int Send_normal(
 * 	int sock,					- socket to use
 * 	struct job *job,	- control file
 * 	int transfer_timeout,		- transfer timeout
 * 	)						- acknowlegement status
 * 
 *  1. send the \2RemotePrinter_DYN\n string to the remote RemoteHost_DYN, wait for an ACK
 *  
 *  2. if control file first, send the control file: 
 *         send \3count cfname\n
 *         get back <0> ack
 *         send 'count' file bytes
 *         send <0> term
 *         get back <0> ack
 *  3. for each data file
 *         send the \4count dfname\n
 *             Note: count is 0 if file is filter
 *         get back <0> ack
 *         send 'count' file bytes
 *             Close socket and finish if filter
 *         send <0> term
 *         get back <0> ack
 *   4. If control file last, send the control file as in step 2.
 *       
 * 
 * If the block_fd parameter is non-zero, we write out the
 * control and data information to a file instead.
 * 
 ***************************************************************************/

int Send_normal( int *sock, struct job *job, int transfer_timeout, int block_fd )
{
	char status = 0, *id, *transfername;
	char line[SMALLBUFFER];
	char error[SMALLBUFFER];
	int ack;

	DEBUG3("Send_normal: send_data_first %d, sock %d, block_fd %d",
		Send_data_first_DYN, *sock, block_fd );

	id = Find_str_value(&job->info,"A",Value_sep);
	transfername = Find_str_value(&job->info,TRANSFERNAME,Value_sep);
	
	if( !block_fd ){
		setstatus( job, "requesting printer %s@%s",
			RemotePrinter_DYN, RemoteHost_DYN );
		plp_snprintf( line, sizeof(line), "%c%s\n",
			REQ_RECV, RemotePrinter_DYN );
		ack = 0;
		if( (status = Link_send( RemoteHost_DYN, sock, transfer_timeout,
			line, strlen(line), &ack ) )){
			char *v;
			if( (v = safestrchr(line,'\n')) ) *v = 0;
			if( ack ){
				plp_snprintf(error,sizeof(error),
					"error '%s' with ack '%s' sending str '%s' to %s@%s",
					Link_err_str(status), Ack_err_str(ack), line,
					RemotePrinter_DYN, RemoteHost_DYN );
			} else {
				plp_snprintf(error,sizeof(error),
					"error '%s' sending str '%s' to %s@%s",
					Link_err_str(status), line,
					RemotePrinter_DYN, RemoteHost_DYN );
			}
			Set_str_value(&job->info,ERROR,error);
			return(status);
		}
	}

	if( !block_fd && Send_data_first_DYN ){
		status = Send_data_files( sock, job, transfer_timeout, block_fd );
		if( !status ) status = Send_control(
			sock, job, transfer_timeout, block_fd );
	} else {
		status = Send_control( sock, job, transfer_timeout, block_fd );
		if( !status ) status = Send_data_files(
			sock, job, transfer_timeout, block_fd );
	}
	return(status);
}

int Send_control( int *sock, struct job *job, int transfer_timeout,
	int block_fd )
{
	char msg[SMALLBUFFER];
	char error[SMALLBUFFER];
	int status = 0, size, ack, err;
	char *cf = 0, *transfername = 0, *s;
	/*
	 * get the total length of the control file
	 */

	cf = Find_str_value(&job->info,CF_OUT_IMAGE,Value_sep);
	size = strlen(cf);
	transfername = Find_str_value(&job->info,TRANSFERNAME,Value_sep);

	DEBUG3( "Send_control: '%s' is %d bytes, sock %d, block_fd %d, cf '%s'",
		transfername, size, *sock, block_fd, cf );
	if( !block_fd ){
		setstatus( job, "sending control file '%s' to %s@%s",
		transfername, RemotePrinter_DYN, RemoteHost_DYN );
	}

	ack = 0;
	errno = 0;
	error[0] = 0;
	plp_snprintf( msg, sizeof(msg), "%c%d %s\n",
		CONTROL_FILE, size, transfername);
	if( !block_fd ){
		if( (status = Link_send( RemoteHost_DYN, sock, transfer_timeout,
			msg, strlen(msg), &ack )) ){
			if( (s = safestrchr(msg,'\n')) ) *s = 0;
			if( ack ){
				plp_snprintf(error,sizeof(error),
				"error '%s' with ack '%s' sending str '%s' to %s@%s",
				Link_err_str(status), Ack_err_str(ack), msg,
				RemotePrinter_DYN, RemoteHost_DYN );
			} else {
				plp_snprintf(error,sizeof(error),
				"error '%s' sending str '%s' to %s@%s",
				Link_err_str(status), msg, RemotePrinter_DYN, RemoteHost_DYN );
			}
			Set_str_value(&job->info,ERROR,error);
			status = JFAIL;
			goto error;
		}
	} else {
		if( Write_fd_str( block_fd, msg ) < 0 ){
			goto write_error;
		}
	}

	/*
	 * send the control file
	 */
	errno = 0;
	if( block_fd == 0 ){
		/* we include the 0 at the end */
		ack = 0;
		if( (status = Link_send( RemoteHost_DYN, sock, transfer_timeout,
			cf,size+1,&ack )) ){
			if( ack ){
				plp_snprintf(error,sizeof(error),
				"error '%s' with ack '%s' sending file '%s' to %s@%s",
				Link_err_str(status), Ack_err_str(ack), transfername,
				RemotePrinter_DYN, RemoteHost_DYN );
			} else {
				plp_snprintf(error,sizeof(error),
					"error '%s' sending file '%s' to %s@%s",
					Link_err_str(status), transfername,
					RemotePrinter_DYN, RemoteHost_DYN );
			}
			Set_str_value(&job->info,ERROR,error);
			status = JFAIL;
			goto error;
		}
		DEBUG3( "Send_control: control file '%s' sent", transfername );
		setstatus( job, "completed sending '%s' to %s@%s",
			transfername, RemotePrinter_DYN, RemoteHost_DYN );
	} else {
		if( Write_fd_str( block_fd, cf ) < 0 ){
			goto write_error;
		}
	}
	status = 0;
	goto error;

 write_error:
	err = errno;
	plp_snprintf(error,sizeof(error),
		"job '%s' write to temporary file failed '%s'",
		transfername, Errormsg( err ) );
	Set_str_value(&job->info,ERROR,error);
	status = JFAIL;
 error:
	return(status);
}


int Send_data_files( int *sock, struct job *job, int transfer_timeout,
	int block_fd )
{
	int count, fd, err, status = 0, ack;
	double size;
	struct line_list *lp;
	char *openname, *transfername, *id, *s;
	char msg[SMALLBUFFER];
	char error[SMALLBUFFER];
	struct stat statb;

	DEBUG3( "Send_files: data file count '%d'", job->datafiles.count );
	id = Find_str_value(&job->info,"identification",Value_sep);
	if( id == 0 ) id = Find_str_value(&job->info,TRANSFERNAME,Value_sep);
	size = 0;
	for( count = 0; count < job->datafiles.count; ++count ){
		lp = (void *)job->datafiles.list[count];
		if(DEBUGL3)Dump_line_list("Send_files - entries",lp);
		openname = Find_str_value(lp,OPENNAME,Value_sep);
		transfername = Find_str_value(lp,TRANSFERNAME,Value_sep);
		DEBUG3("Send_files: opening file '%s'", openname );

		/*
		 * open file as user; we should be running as user
		 */
		fd = Checkread( openname, &statb );
		err = errno;
		if( fd < 0 ){
			status = JFAIL;
			plp_snprintf(error,sizeof(error),
				"cannot open '%s' - '%s'", openname, Errormsg(err) );
			Set_str_value(&job->info,ERROR,error);
			goto error;
		}
		size = statb.st_size;

		DEBUG3( "Send_files: openname '%s', fd %d, size %0.0f",
			openname, fd, size );
		/*
		 * send the data file name line
		 */
		plp_snprintf( msg, sizeof(msg), "%c%0.0f %s\n",
				DATA_FILE, size, transfername );
		if( block_fd == 0 ){
			setstatus( job, "sending data file '%s' to %s@%s", transfername,
				RemotePrinter_DYN, RemoteHost_DYN );
			DEBUG3("Send_files: data file msg '%s'", msg );
			errno = 0;
			if( (status = Link_send( RemoteHost_DYN, sock, transfer_timeout,
				msg, strlen(msg), &ack )) ){
				if( (s = safestrchr(msg,'\n')) ) *s = 0;
				if( ack ){
					plp_snprintf(error,sizeof(error),
					"error '%s' with ack '%s' sending str '%s' to %s@%s",
					Link_err_str(status), Ack_err_str(ack), msg,
					RemotePrinter_DYN, RemoteHost_DYN );
				} else {
					plp_snprintf(error,sizeof(error),
					"error '%s' sending str '%s' to %s@%s",
					Link_err_str(status), msg, RemotePrinter_DYN, RemoteHost_DYN );
				}
				Set_str_value(&job->info,ERROR,error);
				goto error;
			}

			/*
			 * send the data files content
			 */
			DEBUG3("Send_files: doing transfer of '%s'", openname );
			ack = 0;
			if( (status = Link_copy( RemoteHost_DYN, sock, 0, transfer_timeout,
					openname, fd, size ))
				|| (status = Link_send( RemoteHost_DYN,sock,
					transfer_timeout,"",1,&ack )) ){
				if( ack ){
					plp_snprintf(error,sizeof(error),
					"error '%s' with ack '%s' sending file '%s' to %s@%s",
					Link_err_str(status), Ack_err_str(ack), transfername,
					RemotePrinter_DYN, RemoteHost_DYN );
				} else {
					plp_snprintf(error,sizeof(error),
					"error '%s' sending file '%s' to %s@%s",
					Link_err_str(status), transfername,
					RemotePrinter_DYN, RemoteHost_DYN );
				}
				Set_str_value(&job->info,ERROR,error);
				goto error;
			}
			setstatus( job, "completed sending '%s' to %s@%s",
				transfername, RemotePrinter_DYN, RemoteHost_DYN );
		} else {
			double total;
			int len;

			if( Write_fd_str( block_fd, msg ) < 0 ){
				goto write_error;
			}
			/* now we need to read the file and transfer it */
			total = 0;
			while( total < size && (len = read(fd, msg, sizeof(msg)) ) > 0 ){
				if( write( block_fd, msg, len ) < 0 ){
					goto write_error;
				}
				total += len;
			}
			if( total != size ){
				plp_snprintf(error,sizeof(error),
					"job '%s' did not copy all of '%s'",
					id, transfername );
				status = JFAIL;
				Set_str_value(&job->info,ERROR,error);
				goto error;
			}
		}
		close(fd); fd = -1;
	}
	goto error;

 write_error:
	err = errno;
	plp_snprintf(error,sizeof(error),
		"job '%s' write to temporary file failed '%s'",
		id, Errormsg( err ) );
	Set_str_value(&job->info,ERROR,error);
	status = JFAIL;

 error:
	return(status);
}

/***************************************************************************
 * int Send_block(
 * 	char *RemoteHost_DYN,				- RemoteHost_DYN name
 * 	char *RemotePrinter_DYN,			- RemotePrinter_DYN name
 * 	char *dpathname *dpath  - spool directory pathname
 * 	int *sock,					- socket to use
 * 	struct job *job,	- control file
 * 	int transfer_timeout,		- transfer timeout
 * 	)						- acknowlegement status
 * 
 *  1. Get a temporary file
 *  2. Generate the compressed data files - this has the format
 *       \3count cfname\n
 *       [count control file bytes]
 *       \4count dfname\n
 *       [count data file bytes]
 * 
 *  3. send the \6RemotePrinter_DYN size\n
 *     string to the remote RemoteHost_DYN, wait for an ACK
 *  
 *  4. send the compressed data files
 *       wait for an ACK
 * 
 ***************************************************************************/

int Send_block( int *sock, struct job *job, int transfer_timeout )
{
	int tempfd;			/* temp file for data transfer */
	char msg[SMALLBUFFER];	/* buffer */
	char error[SMALLBUFFER];	/* buffer */
	struct stat statb;
	double size;				/* ACME! The best... */
	int status = 0;				/* job status */
	int ack;
	char *id, *transfername, *tempfile;


	id = Find_str_value(&job->info,IDENTIFIER,Value_sep);
	transfername = Find_str_value(&job->info,TRANSFERNAME,Value_sep);
	if( id == 0 ) id = transfername;

	tempfd = Make_temp_fd( &tempfile );
	DEBUG1("Send_block: sending '%s' to '%s'", id, tempfile );

	status = Send_normal( &tempfd, job, transfer_timeout, tempfd );

	id = Find_str_value(&job->info,IDENTIFIER,Value_sep);
	transfername = Find_str_value(&job->info,TRANSFERNAME,Value_sep);
	if( id == 0 ) id = transfername;

	DEBUG1("Send_block: sendnormal of '%s' returned '%s'", id, Server_status(status) );
	if( status ) return( status );

	/* rewind the file */
	if( lseek( tempfd, 0, SEEK_SET ) == -1 ){
		Errorcode = JFAIL;
		logerr_die( LOG_INFO, "Send_files: lseek tempfd failed" );
	}
	/* now we have the copy, we need to send the control message */
	if( fstat( tempfd, &statb ) ){
		Errorcode = JFAIL;
		logerr_die( LOG_INFO, "Send_files: fstat tempfd failed" );
	}
	size = statb.st_size;

	/* now we know the size */
	DEBUG3("Send_block: size %0.0f", size );
	setstatus( job, "sending job '%s' to %s@%s, block transfer",
		id, RemotePrinter_DYN, RemoteHost_DYN );
	plp_snprintf( msg, sizeof(msg), "%c%s %0.0f\n",
		REQ_BLOCK, RemotePrinter_DYN, size );
	DEBUG3("Send_block: sending '%s'", msg );
	status = Link_send( RemoteHost_DYN, sock, transfer_timeout,
		msg, strlen(msg), &ack );
	DEBUG3("Send_block: status '%s'", Link_err_str(status) );
	if( status ){
		char *v;
		if( (v = safestrchr(msg,'\n')) ) *v = 0;
		if( ack ){
			plp_snprintf(error,sizeof(error),
			"error '%s' with ack '%s' sending str '%s' to %s@%s",
			Link_err_str(status), Ack_err_str(ack), msg,
			RemotePrinter_DYN, RemoteHost_DYN );
		} else {
			plp_snprintf(error,sizeof(error),
			"error '%s' sending str '%s' to %s@%s",
			Link_err_str(status), msg, RemotePrinter_DYN, RemoteHost_DYN );
		}
		Set_str_value(&job->info,ERROR,error);
		return(status);
	}

	/* now we send the data file, followed by a 0 */
	DEBUG3("Send_block: sending data" );
	ack = 0;
	status = Link_copy( RemoteHost_DYN, sock, 0, transfer_timeout,
		transfername, tempfd, size );
	DEBUG3("Send_block: status '%s'", Link_err_str(status) );
	if( status == 0 ){
		status = Link_send( RemoteHost_DYN,sock,transfer_timeout,"",1,&ack );
		DEBUG3("Send_block: ack status '%s'", Link_err_str(status) );
	}
	if( status ){
		char *v;
		if( (v = safestrchr(msg,'\n')) ) *v = 0;
		if( ack ){
			plp_snprintf(error,sizeof(error),
				"error '%s' with ack '%s' sending file '%s' to %s@%s",
				Link_err_str(status), Ack_err_str(ack), id,
				RemotePrinter_DYN, RemoteHost_DYN );
		} else {
			plp_snprintf(error,sizeof(error),
				"error '%s' sending file '%s' to %s@%s",
				Link_err_str(status), id, RemotePrinter_DYN, RemoteHost_DYN );
		}
		Set_str_value(&job->info,ERROR,error);
		return(status);
	} else {
		setstatus( job, "completed sending '%s' to %s@%s",
			id, RemotePrinter_DYN, RemoteHost_DYN );
	}
	close( tempfd ); tempfd = -1;
	return( status );
}

/***************************************************************************
 *int Send_secure_block(
 *	char *RemoteHost_DYN,				- RemoteHost_DYN name
 *	char *RemotePrinter_DYN,			- RemotePrinter_DYN name
 *	char *dpathname *dpath  - spool directory pathname
 *	int *sock,					- socket to use
 *	struct job *job,	- control file
 *	int transfer_timeout,		- transfer timeout
 *	)						- acknowlegement status
 *
 * 1. Get a temporary file
 * 2. Generate the compressed data files - this has the format
 *      \REQ_BLOCKRemotePrinter_DYN user@RemoteHost_DYN filename
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

int Send_secure_block( int *sock, struct job *job, int transfer_timeout )
{
	int tempfd;			/* temp file for data transfer */
	int status = 0;		/* job status */
	int n, count;			/* ACME temps */
	char *tempfilename, *auth_id, *transfername, *s;
	char buffer[SMALLBUFFER];

	DEBUG3("Send_secure_block: RemotePrinter_DYN '%s'", RemotePrinter_DYN );
	auth_id = Find_str_value(&job->info,AUTHINFO,Value_sep);
	transfername = Find_str_value(&job->info,TRANSFERNAME,Value_sep);
	if( Is_server && auth_id == 0 ){
		Errorcode = JABORT;
		fatal( LOG_ERR,
		"Send_secure_block: missing job authentication");
	}

	tempfd = Make_temp_fd( &tempfilename );
	Setup_auth_info( tempfd, 0 );
	if( status ) return( status );
	status = Send_normal( &tempfd, job, transfer_timeout, tempfd);
	if( status ) return( status );
	close( tempfd ); tempfd = -1;
	status = Send_auth_transfer( sock, transfer_timeout, tempfilename, 1 );
	DEBUG3("Send_secure_block: status %d", status );
	if( status == 0 ){
		buffer[0] = 0;
		while( (count = strlen(buffer)) < sizeof(buffer)-1
			&& (n = read(*sock, buffer+count, sizeof(buffer)-count-1)) > 0 ){
			buffer[count+n] = 0;
			DEBUG3("Send_secure_block: read %d, '%s'", n, buffer );
			while( (s = strchr( buffer, '\n' )) ){
				*s++ = 0;
				setstatus( job, "SECURE ERROR from %s@%s '%s'",
					RemotePrinter_DYN, RemoteHost_DYN, buffer );
				memmove( buffer, s, strlen(s)+1 );
			}
		}
	}
	return( status );
}
