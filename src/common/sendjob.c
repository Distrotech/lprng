/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: sendjob.c
 * PURPOSE: Send a print job to the remote host
 *
 **************************************************************************/

static char *const _id =
"$Id: sendjob.c,v 3.7 1996/09/09 14:24:41 papowell Exp papowell $";

#include "lp.h"
#include "lp_config.h"
#include "printcap.h"
#include "pr_support.h"
#include "sendjob.h"
#include "jobcontrol.h"

static int Bounce_filter( struct control_file *cfp,
	struct data_file *data_file, struct pc_used *pc_used,
	int fd, int *size, struct stat *statb );

/***************************************************************************
Commentary:
The protocol used to send a job to a remote host consists of the
following:

Client                                   Server
\2printername\n - receive a job
                                         \0  (ack)
\2count controlfilename\n
<count bytes>
\0
                                         \0
\3count datafilename\n
<count bytes>
\0
                                         \0
\3count datafilename\n
<count bytes>
\0
                                         \0
<close connection>

1. In order to abort the job transfer,  client sends \1
2. Anything but a 0 ACK is an error indication

NB: some spoolers require that the data files be sent first.
The same transfer protocol is followed, but the data files are
send first,  followed by the control file.

The Send_job() routine will try to transfer a control file
to the remote host.  It does so using the following algorithm.

1.  makes a connection (connection timeout)
2.  sends the \2printer and gets ACK (transfer timeout)
3.  sends the control file (transfer timeout)
4.  sends the data files (transfer timeout)

int Send_job(
	char *printer,	- name of printer
	char *host,		- remote host name
	struct dpathname *p - pathname to spool directory
	struct control_file *cfp,	- control file
	int connect_try,	- number of times to retry sending/opening connection
						- negative is infinite
	int connect_timeout,	- timeout on making connection
	int connect_interval,	- interval between retries
	int transfer_timeout )	- timeout on sending a file

	We will retry making the connection for connect_try times.
		if connnect_try == 0, we do it indefinately
	If the connect_interval > 0, we pause connect_interval between attempts;
		we add 10 seconds until we are waiting a minute
		(60 seconds) between attempts.
	RETURNS: 0 if successful, non-zero if not
 **************************************************************************/
int Send_files( char *host, char *printer,
	struct dpathname *dpath,
	int *sock, struct control_file *cfp, int transfer_timeout,
	struct pc_used *pc_used );

int Send_job( char *printer, char *host, struct control_file *cfp,
	struct dpathname *dpath,
	int max_try, int connect_timeout, int connect_interval,
	int transfer_timeout, struct pc_used *pc_used )
{
	int sock;		/* socket to use */
	int i, n;		/* AJAX Integers (ACME on strike) */
	int status = 0;	/* status of transfer */
	int attempt;
	char msg[LINEBUFFER];
	int size;
	char *line;
	int err;
 

	/* fix up the control file */

	if( Fix_control( cfp, &line ) ){
		log( LOG_ERR, "Send_job: control file '%s' bad line '%s'", cfp->name, line );
		return( JABORT );
	}

	/*
	 * do accounting at start
	 */
	if( pc_used && Accounting_remote ){
		Setup_accounting( cfp, pc_used );
		if( Accounting_start ){
			i = Do_accounting( 0, Accounting_start, cfp, Send_timeout, pc_used, 0 );
			if( i ){
				Errorcode = i;
				cleanup( 0 );
			}
		}
	}

	DEBUG4("Send_job: '%s'->'%s@%s',connect(retry %d,timeout %d,interval %d)",
		cfp->name, printer, host, max_try, connect_timeout,
		connect_interval );

	setstatus( cfp, "start job '%s' transfer to %s@%s", cfp->name,
		 printer, host );

	if( connect_interval > 0 ){
		n = connect_interval;
	} else {
		n = 2;
		connect_interval = 0;
	}
	attempt = 0;
	do {
		/* we pause on later tries */
		if( attempt > 0 ){
			setstatus( cfp,
			"sending job '%s' to '%s@%s', connect attempt %d failed, sleeping %d",
				cfp->name, printer, host, attempt, n );
			sleep( n );
			n += connect_interval;
			if( n > 60 ) n = 60;
		}
		++attempt;

		setstatus( cfp,
			"connection to %s@%s attempt %d", printer, host, attempt );

		errno = 0;
		sock = Link_open( host, 0, connect_timeout );
		DEBUG8("Send_job: socket %d", sock );
		err = errno;
		if( sock < 0 ){
			status = LINK_OPEN_FAIL;
			plp_snprintf(msg,sizeof(msg)-2,
				"connection to %s@%s failed - %s", printer, host,
					Errormsg(err) );
			setstatus( cfp, msg );
			if( Interactive ){
				/* send it directly to the error output */
				strcat(msg,"\n");
				if( Write_fd_str( 2, msg ) < 0 ){
					cleanup(0);
					exit(-2);
				}
			} else {
				log( LOG_INFO, "Send_job: error '%s'", msg );
			}
		} else {
			/* send job to the LPD server for the printer */
			status = Send_files( host, printer,
				dpath, &sock, cfp, transfer_timeout, pc_used );
			/* we read any other information that is sent */
			if( status != 0 ){
				do{
					size = sizeof( msg ) - 2;
					*msg = 0;
					i = Link_line_read( host,&sock,transfer_timeout,msg,&size);
					DEBUG4( "Send_job: error report status %d '%s'",
						i, msg );
					if( *msg ){
						if( Interactive ){
							/* send it directly to the error output */
							strcat(msg,"\n");
							if( Write_fd_str( 2, msg ) < 0 ){
								cleanup(0);
								exit(-2);
							}
						} else {
							log( LOG_INFO, "Send_job: error '%s'", msg );
						}
					}
				} while( *msg );
			}
		}
	} while( status && (max_try == 0 || attempt < max_try) );

	if( status ){
		plp_snprintf(msg, sizeof(msg)-2,
			"job '%s' transfer to %s@%s failed after %d attempts",
			cfp->name, printer, host, attempt );
		if( Interactive ){
			strcat(msg,"\n");
			if( Write_fd_str( 2, msg ) < 0 ){
				cleanup(0);
				exit(-2);
			}
		} else {
			setstatus( cfp, msg );
		}
	} else {
		DEBUG8("Send_job: pc_used 0x%x, end %d, file %s, remote %d",
			pc_used, Accounting_end, Accounting_file, Accounting_remote );
		if( pc_used && Accounting_remote ){
			i = Do_accounting( 1, Accounting_end, cfp, Send_timeout, pc_used, 0 );
			if( i ){
				Errorcode = i;
				cleanup( 0 );
			}
		}
		setstatus( cfp, "done job '%s' transfer to %s@%s",
			cfp->name, printer, host );
	}
	return( status );
}

/***************************************************************************
int Send_files(
	char *host,				- host name
	char *printer,			- printer name
	char *dpathname *dpath  - spool directory pathname
	int *sock,					- socket to use
	struct control_file *cfp,	- control file
	int transfer_timeout,		- transfer timeout
	)						- acknowlegement status

 1. send the \2printer\n string to the remote host, wait for an ACK

 
 2. if control file first, send the control file: 
        send \3count cfname\n
        get back <0> ack
        send 'count' file bytes
        send <0> term
        get back <0> ack
 3. for each data file
        send the \4count dfname\n
            Note: count is 0 if file is filter
        get back <0> ack
        send 'count' file bytes
            Close socket and finish if filter
        send <0> term
        get back <0> ack
  4. If control file last, send the control file as in step 2.
      

 ***************************************************************************/
static int cflen;
static char *cfbuf;

int Send_files( char *host, char *printer,
	struct dpathname *dpath,
	int *sock, struct control_file *cfp, int transfer_timeout,
	struct pc_used *pc_used )
{
	int status;			/* status of transfer command */
	int size;			/* size of file */
	char line[LINEBUFFER];	/* buffer for making up command to send */
	int i, j, c;			/* ACME, the very finest in integer variables */
	char *s, *t;			/* AJAX, local pointers on demand */
	struct data_file *df;	/* data file entry */
	struct stat statb;	/* status buffer */
	int fd;				/* file descriptor */
	char **lines;		/* lines in file */
	char *path = 0;		/* path to file */
	int ack;			/* ack byte */
	int err;			/* saved errno */
	int sent_data = 0;		/* flag to handle sending data files first */
	int fix_format;		/* fix up format */

	DEBUG4("Send_files: send_data_first %d", Send_data_first );
	setstatus( cfp, "sending '%s' to %s@%s", cfp->name, printer, host );
	status = Link_send( host, sock, transfer_timeout,
		REQ_RECV, printer, '\n', &ack );
	if( status ){
		setstatus( cfp, "error '%s' sending '\\%d%s\\n' to %s@%s",
			Link_err_str(status), REQ_RECV, printer, printer, host );
		return(status);
	}

	/* ugly ugly hack to send data files first to printers that
	 *	need it.  Also elegant way to test that you can handle data files
	 *	first. :-)
	 */
	if( Send_data_first ) goto send_data;
send_control:
	/*
	 * get the total length of the control file
	 */

	size = 0;
	DEBUG4("Send_files: line_count %d", cfp->info_lines.count );

	lines = cfp->info_lines.list;
	/* copy the lines into one big buffer */

	j = 0;
	for( i = 0; i < cfp->info_lines.count; ++i ){
		if( lines[i] && lines[i][0] ) j += strlen( lines[i] + 1 );
	}
	if( j >= cflen ){
		if( cfbuf ){
			free(cfbuf);
			cfbuf = 0;
		}
		cflen = ((j/1024)+1)*1024;
		malloc_or_die( cfbuf, cflen );
	}
	s = cfbuf;
	*s = 0;
	fix_format = Bounce_queue_dest && !Interactive && Xlate_format;
	for( i = 0; i < cfp->info_lines.count; ++i ){
		if( lines[i] && (c = lines[i][0]) ){
			s = s+strlen(s);
			strcpy( s, lines[i] );
			strcat( s, "\n" );
			/* check to see if you need to translate formats */
			if( fix_format && islower(c) && (t = strchr( Xlate_format, c )) ){
				j = t - Xlate_format;
				c = t[1];
				if( (j & 1) == 0 && islower(c) ){
					*s = c;
					DEBUG4("Send_files: translate format '%s'", s );
				}
			}
		}
	}


	size = strlen( cfbuf );
	DEBUG4( "Send_files: control file %d bytes '%s' ", size, cfbuf );

	/*
	 * send the command file name line
	 */
	plp_snprintf( line, sizeof(line), "%d %s", size, cfp->name );
	ack = 0;
	errno = 0;
	status = Link_send( host, sock, transfer_timeout,
		CONTROL_FILE, line, '\n', &ack );
	if( status ){
		logerr( LOG_INFO,
			"Send_files: error '%s' ack %d sending '\\%d%s\\n' to %s@%s",
			Link_err_str(status), ack, CONTROL_FILE, line, printer, host );
		switch( ack ){
		case ACK_STOP_Q: status = JABORT; break;
		default:         status = JFAIL; break;
		}
		return(status);
	}
	/*
	 * send the control file
	 */
	errno = 0;
	status = Link_send( host, sock, transfer_timeout, 0, cfbuf, 0x100, &ack );
	if( status != 0 ){
		logerr( LOG_INFO,
			"Send_files: error '%s' ack %d sending control file '%s' to %s@%s",
			Link_err_str(status), ack, cfp->name, printer, host );
		switch( ack ){
		case ACK_STOP_Q: status = JREMOVE; break;
		default:         status = JFAIL; break;
		}
		return( status );
	}
	DEBUG4( "Send_files: control file sent, ack '%d'", ack );

	DEBUG4( "Send_files: data file count '%d'", cfp->data_file.count );
	setstatus( cfp, "completed '%s' to %s@%s",
		cfp->name, printer, host );


	/*
	 * now send the data files
	 */

send_data:
	for( i = 0; sent_data == 0 && status == 0 && i < cfp->data_file.count; ++i ){
		df = (void *)cfp->data_file.list;
		df = &df[i];
		DEBUG4(
	"Send_files: [%d] transfer '%s', open '%s', fd %d, size %d, copies %d",
		i, df->transfername, df->openname, df->fd, df->statb.st_size,
		df->copies );
		if( (DbgTest & 0x01) && i == 0 ){
			DEBUG0("Send_files: DgbTest flag 0x01 set! skip first file" );
			continue;
		}
		DEBUG4( "Send_files: file '%s'", df->openname );
		setstatus( cfp, "sending '%s' to %s@%s",
			df->transfername?df->transfername:df->openname,
			printer, host );
		fd = df->fd;
		if( df->flags & PIPE_FLAG ){
			DEBUG4( "Send_files: file '%s' open already, fd %d", df->openname, fd );
			if( fstat( fd, &statb ) ){
				logerr_die( LOG_INFO, "Send_files: fstat fd '%s' failed", df->Ninfo );
			}
			/* ignore the status on LSEEK - we may have a pipe or fd */
			(void)lseek( fd, (off_t)0, 0 );
			size = 0;
		} else if( df->openname ){
			DEBUG4("Send_files: opening file '%s'", df->openname );
			/*
			 * open file as user; we should be running as user
			 */
			fd = Checkread( df->openname, &statb );
			err = errno;
			DEBUG4( "Send_files: file '%s', fd %d", path, fd );
			if( fd < 0 ){
				char *s = strrchr( df->openname, '/' );
				if( s == 0 ) s = df->openname;
				/* we cannot open the data file! */
				plp_snprintf( cfp->error, sizeof(cfp->error),
					"cannot open '%s' - '%s'", s, Errormsg(err) );
				logerr( LOG_ERR, "Send_files: %s", cfp->error );
				if( !Interactive ){
					Set_job_control( cfp );
				}
				return(JREMOVE);
			}
			if( statb.st_dev != df->statb.st_dev ){
				plp_snprintf( cfp->error, sizeof(cfp->error),
					"file '%s', st_dev %d, not %d, possible security problem",
					df->openname, statb.st_dev, df->statb.st_dev );
				log( LOG_ERR, "Send_files: %s", cfp->error );
				if( !Interactive ){
					Set_job_control( cfp );
				}
				return(JREMOVE);
			}
			if( statb.st_ino != df->statb.st_ino ){
				plp_snprintf( cfp->error, sizeof(cfp->error),
				"file '%s', st_ino %d, not %d, possible security problem",
					df->openname, statb.st_ino, df->statb.st_ino );
				log( LOG_ERR, "Send_files: %s", cfp->error );
				if( !Interactive ){
					Set_job_control( cfp );
				}
				return(JREMOVE);
			}
			if( statb.st_size != df->statb.st_size ){
				plp_snprintf( cfp->error, sizeof(cfp->error),
					"file '%s', st_size %d, not %d, possible security problem",
					df->openname, statb.st_size, df->statb.st_size );
				log( LOG_ERR, "Send_files: %s", cfp->error );
				if( !Interactive ){
					Set_job_control( cfp );
				}
				return(JREMOVE);
			}
			DEBUG4( "Send_files: file '%s' size %d",
			df->openname, statb.st_size );
			/* first we have to rewind the file */
			if( lseek( fd, (off_t)0, 0 ) == (off_t)(-1) ){
				err = errno;
				plp_snprintf( cfp->error, sizeof(cfp->error),
					"file '%s', fd %d, lseek failed - %s",
					df->openname, fd, Errormsg(err) );
				log( LOG_ERR, "Send_files: %s", cfp->error );
				if( !Interactive ){
					Set_job_control( cfp );
				}
				return(JREMOVE);
			}
			size = statb.st_size;
		} else {
			fatal( LOG_ERR, "Send_files: no job file name" );
		}

		/*
		 * Now for the infamous 'use the filters on a bounce queue'
		 * problem.  If we have a bounce queue,  we may want to
		 * run the data files through the filters before sending them
		 * to their destinations.  This is done if the :bq=destination:
		 * is set in the printcap (Bounce_queue_dest variable)
		 * is set.  We do filtering if:
		 *   1. Bounce_queue_dest is set
		 *   2. There is a filter for this format
		 *	 3. and we are not a client (Interactive = 0);
		 * To make filtering work,  we first create a temporary file in
		 * the spool directory.  We redirect the filter output to the
		 * temporary file,  and then use this as the real file.  When done,
		 * we remove the temporary file.  To ensure that we do not
		 * use too much space,  we will create only one temp file.
		 *
		 * Note: the 'p' format is supposed to be processed by the /bin/pr
		 * program.  However,  it may also get processed again at the destination
		 * by another filter.  We will assume that the user will provide a
		 * special 'p' format filter or not have it filtered in a bounce queue.
		 */

		if( Bounce_queue_dest && !Interactive ){
			if( (status = Bounce_filter( cfp, df, pc_used, fd,
					&size, &statb )) ){
				return( status );
			}
		}

		/*
		 * send the data file name line
		 */
		s = df->transfername;
		if( s == 0 ) s = df->openname;
		plp_snprintf( line, sizeof(line), "%d %s", size, s );
		DEBUG4("Send_files: data file line '\\%d'%s'", DATA_FILE, line );

		errno = 0;
		status = Link_send( host, sock, transfer_timeout,
			DATA_FILE, line, '\n', &ack );
		if( status ){
			logerr( LOG_INFO,
				"Send_files: error '%s' ack %d sending '\\%d%s\\n' to %s@%s",
				Link_err_str( status ),ack,DATA_FILE, line, printer, host );
			return(status);
		}

		/*
		 * send the data files themselves
		 */

		DEBUG4("Send_files: doing transfer of '%s'", df->openname );
		if( df->flags & PIPE_FLAG ){
			size = -1;
		}
		/* no timeout on reading, we may be reading from pipe */
		errno = 0;
		status = Link_copy( host, sock, 0, transfer_timeout,
				df->openname, fd, size );

		/*
		 * get the ACK only if you did not have an error
		 */
		if( size > 0 && status == 0 ){
			status = Link_ack( host,sock,transfer_timeout,0x100,&ack );
		}

		close(fd);
		if( status ){
			log( LOG_INFO,
				"Send_files: error '%s' sending data file '%s' to %s@%s",
				Link_err_str( status ), df->openname, printer, host );
			setstatus( cfp, "error sending '%s' to %s@%s",
				df->openname?df->openname:df->transfername,
				cfp->name, printer, host );
		} else {
			setstatus( cfp, "completed '%s' to %s@%s",
				df->transfername?df->transfername:df->openname,
				printer, host );
		}
	}

	/* the remaining part of the ugly hack */
	if( Send_data_first && sent_data == 0 ){
		sent_data = 1;
		goto send_control;
	}
	return(status);
}

/*
 * Bounce filter - 
 * 1. check to see if there is a filter for this format
 * 2. check to see if the temp file has already been created
 *    if not, create and open the temp file
 * 3. run the filter,  with its output directed to the temp file
 * 4. close the original input file,  and dup the temp file to it.
 * 5. return the temp file size
 */


static int Bounce_filter( struct control_file *cfp,
	struct data_file *data_file, struct pc_used *pc_used,
	int fd, int *size, struct stat *statb )
{
	int c, in, err;
	char *filter;
	char tempbuf[LARGEBUFFER];
	static int tempfd;

	/* get the temp file */
	/* now find the filter - similar to the printer support code */
	filter = 0;
	/*
	 * check for PR to be used
	 */
	c = data_file->format;
	switch( c ){
	case 'f': case 'l':
		filter = IF_Filter;
		break;
	default:
		filter = Find_filter( c, pc_used );
	}
	DEBUG4("Bounce_filter: filter '%s'", filter );

	/* hook filter up to the device file descriptor */
	if( filter ){
		if( tempfd == 0 ){
			plp_snprintf( tempbuf, sizeof(tempbuf)-1,
				"bfA%03d%s", cfp->number, cfp->filehostname );
			tempfd = Checkwrite( tempbuf, statb, O_RDWR, 1, 0 );
			err = errno;
			DEBUG4("Bounce_filter: temp file '%s', fd %d", tempbuf, tempfd );
			if( tempfd < 0 ){
				Errorcode = JFAIL;
				logerr_die( LOG_INFO,
					"Bounce_filter: error '%s' opening bounce queue temp file '%s'",
					Errormsg(err), tempbuf );
			}
			if( unlink( tempbuf ) < 0 ){
				Errorcode = JFAIL;
				logerr_die( LOG_INFO,
					"Bounce_filter: error '%s' unlinking temp file '%s'",
					Errormsg(err), tempbuf );
			}
		}
		if( ftruncate( tempfd, 0 ) < 0 ){
			err = errno;
			Errorcode = JFAIL;
			logerr_die( LOG_INFO,
				"Bounce_filter: error '%s' truncating bounce queue temp fd",
				Errormsg(err) );
		}
		Make_filter( c, cfp, &XF_fd_info, filter,
			0, /* no extra */
			0,	/* RW pipe */
			tempfd, /* dup to fd 1 */
			pc_used, /* printcap information */
			data_file, Accounting_port, 0 );
		/* at this point you have a filter, which is taking input
			from XF_fd_info.input; pass input file through it */
		while( (in = read(fd, tempbuf, sizeof(tempbuf)) ) > 0 ){
			Write_fd_len( XF_fd_info.input, tempbuf, in );
		}
		if( Close_filter( &XF_fd_info, Send_timeout ) ){
			err = errno;
			Errorcode = JFAIL;
			logerr_die( LOG_INFO,
				"Bounce_filter: error '%s' closing bounce queue filter",
				Errormsg(err) );
		}
		/* now we close the input file and dup the filter output to it */
		if( dup2( tempfd, fd ) < 0 ){
			Errorcode = JFAIL;
			logerr_die( LOG_INFO, "Bounce_filter: dup2 failed" );
		}
		if( lseek( fd, (off_t)0, SEEK_SET ) < 0 ){
			Errorcode = JFAIL;
			logerr_die( LOG_INFO, "Bounce_filter: lseek failed" );
		}
		if( fstat( fd, statb ) < 0 ){
			Errorcode = JFAIL;
			logerr_die( LOG_INFO, "Bounce_filter: fstat failed" );
		}
		*size = statb->st_size;
		DEBUG4("Bounce_filter: fd '%d', size %d", fd, *size );
	}
	return( 0 );
}
