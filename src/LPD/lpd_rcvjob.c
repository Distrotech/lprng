/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpd_receivejob.c
 * PURPOSE: receive a job from a remote connections
 **************************************************************************/

static char *const _id =
"$Id: lpd_rcvjob.c,v 3.7 1996/09/09 14:24:41 papowell Exp papowell $";

#include "lpd.h"
#include "printcap.h"
#include "lp_config.h"
#include "permission.h"
#include "freespace.h"
#include "jobcontrol.h"

/***************************************************************************
Commentary:
Patrick Powell Mon Apr 17 05:43:48 PDT 1995

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

1. Read the control file from the other end.
2. Check to see if the printer exists,  and if has a printcap entry
3. If it does, check the permissions for the user.
4. Read the job to the queue.

Control file processing
1. The control file at this end might exist already,  and be in use.
	If this is the case,  we will try and allocate another control
	file name if the option is allowed.
2. After we lock the control file,  we will then try and read the
	data files.  Again,  there might be a collision.  If this is
	the case,  then we will again try to generate a new number.

The control file is first read into a file and then read into memory,
where it is parsed.  

Duplicate Control/Data files:
When copying jobs over,  you might get to a point where you
discover that a control and/or data file already exists.

if file already exists:
1. if the existing file length is 0, then you can clobber the file.
   This is reasonable given file locking is working
   and games are not being played with NFS file systems.
   Most likely you have found an abandonded file.
2. If you have the control file and it is locked,
   then you might as well clobber the data files
   as they are probably left over from another job.
   If you do not have the control file,  then you give up
3. If the first file file is the control file,
   and you cannot lock it or it is locked and has a non-zero length,
   then you should rename the file and try again.
   rename the data files/control files
   This can be done if the first file is a control file
   and you cannot lock it,  or you lock it and it is
  non-zero in length.

Job Size:
   when the total received job size exceeds limits, then abort job
   when the available file space falls below limit, then abort job

 ***************************************************************************/

static struct control_file *cf;	/* control file for job */
static struct control_file cf_static;	/* control file for job */

static char *tempname;			/* temporary control file */
static char *controlname;		/* real control file */
static char *holdfilename;		/* hold file */

static struct malloc_list data_files; /* list of data files received */

static void Remove_files( void *p )
{
	int i;
	struct data_file *data;	/* list of data files */

	DEBUG4( "Remove_files: removing job" );
	data = (void *)data_files.list;
	for( i = 0; i < data_files.count; ++i ){
		if( data[i].transfername ){
			DEBUG4("Remove_files: unlinking transfer '%s'",
				data[i].transfername );
			unlink( data[i].transfername );
		}
	}
	if( controlname ){
		DEBUG4("Remove_files: unlinking %s", controlname );
		unlink( controlname );
	}
	if( tempname ){
		DEBUG4("Remove_files: unlinking %s", tempname );
		unlink( tempname );
	}
	if( holdfilename ){
		DEBUG4("Remove_files: unlinking %s", holdfilename );
		unlink( holdfilename );
	}
}


int Receive_job( int *socket, char *input, int maxlen )
{
	char *s;					/* WunderPointers!  Better than ACME !*/
	char line[LINEBUFFER];		/* line buffer for input */
	char error[LINEBUFFER];		/* error */
	int i, j;					/* ACME! variables with a wide range! */
	int file_len;				/* length of file */
	int jobsize = 0;			/* size of job */
	int ack;					/* ack to send */
	int status;					/* status of the last command */
	int len;					/* length of last read */
	char *filename;				/* name of control or data file */
	char *openname;				/* name used to open the job or data file */
	char *transfername;			/* name used to open the job or data file */
	int fd, lock, create;		/* used for file opening and locking */
	struct stat statb;			/* status buffer */
	int temp_fd = -1;			/* temp file descriptor */
	struct data_file *datafile;	/* data file information */
	struct data_file *jobfiles;	/* data file information */
	int read_length;			/* amount to read from socket */
	int missing;				/* missing data file in job */
	int err;					/* saved errno */
	int jobnumber;				/* job number */
	char jobnum[7];				/* jobnum string if you need it */
	char *orig_name;			/* original printer name */
	int max_job_number;			/* maximum job number */
	char **lines;				/* lines in the file */
	char cfile_copy[LARGEBUFFER];	/* control file copy */

	/* clear error message */
	error[0] = 0;
	/* set the maximum job number */
	max_job_number = 1000;
	jobnum[0] = 0;

	/* clean up the printer name a bit */
	++input;
	if( (s = strchr( input, '\n' )) ) *s = 0;
	Name = "Receive_job";
	DEBUG3("Receive_job: doing '%s', Long_number %d, Backwards %d",
		input, Long_number, Backwards_compatible );

	orig_name = safestrdup(input);


	/* set up cleanup and do initialization */

	register_exit( Remove_files, 0 );

	if( Clean_name( input ) ){
		plp_snprintf( error, sizeof(error),
			"%s: bad printer name", input );
		ack = ACK_STOP_Q;	/* no retry, don't send again */
		goto error;
	}
	setproctitle( "lpd %s '%s'", Name, input );
	if( Setup_printer( input, error, sizeof(error),
		(void *)0, debug_vars, 0, (void *)0 ) ){
		DEBUG3("Receive_job: Setup_printer failed '%s'", error );
		ack = ACK_STOP_Q;	/* no retry, don't send again */
		goto error;
	}

	DEBUG3("Receive_job: spooling_disabled %d, Long_number %d",
		Spooling_disabled, Long_number );
	if( Spooling_disabled ){
		plp_snprintf( error, sizeof(error),
			"%s: spooling disabled", Printer );
		ack = ACK_RETRY;	/* retry */
		goto error;
	}

	/* send an ACK */
	DEBUG3("Receive_job: sending ACK (0)" );

	status = Link_send( ShortRemote, socket, Send_timeout,
		0x100, (char *)0, 0, 0 );
	if( status ){
		plp_snprintf( error, sizeof(error),
			"%s: Receive_job: sending ACK 0 failed", Printer );
		goto error;
	}

	do{
		DEBUG3("Receive_job: from %s- getting file transfer line", FQDNRemote );
		len = sizeof(line)-1;
		line[0] = 0; line[1] = 0;
		status = Link_line_read( ShortRemote, socket, Send_timeout,
			line, &len );

		DEBUG3( "Receive_job: read from %s- status %d read %d bytes '\\%d'%s'",
				FQDNRemote, status, len, line[0], line+1 );
		if( status && len == 0 ){
			DEBUG3( "Receive_job: from %s- read 0", FQDNRemote );
			status = 0;
			break;
		}
		if( len < 3 ){
			plp_snprintf( error, sizeof(error),
				"%s: bad file name format '%s'", Printer, input );
			ack = ACK_FAIL;
			goto error;
		}

		/* get the file length */
		s = filename = &line[1];
		while( isspace( *s ) ) ++s;
		file_len = strtol( s, &filename, 10 );
		if( s == filename || *filename != ' ' || file_len < 0 ){
			plp_snprintf( error, sizeof(error),
				"%s: bad file length format '%s'", Printer, input );
			ack = ACK_FAIL;
			goto error;
		}

		/* skip spaces */
		while( isspace(*filename) ) ++filename;
		if( *filename == 0 ){
			plp_snprintf( error, sizeof(error),
				"%s: missing job file name",
					Printer, filename );
			ack = ACK_FAIL;
			goto error;
		}

		/* check the filename format for consistency,
		 *	and make sure that the datafiles are the same as well
		 *	we recklessly duplicate the string, knowing we will
		 *	exit after doing a transfer...
		 */

		openname = safestrdup( filename );	/* name we get from remote */
		if( Check_format( line[0], openname, &cf_static ) ){
			plp_snprintf( error, sizeof(error),
				"%s: file '%s' name format not [cd]f[A-Za-z]NNNhost",
					Printer, filename );
			ack = ACK_FAIL;
			goto error;
		}
		jobnumber = cf_static.number;

		/* we pad the allocated buffer a tad to hold modified format */
		/* and we also allow a character at start for prefix */
		transfername = safexstrdup( filename, 10 );	/* name we transfer to */
		transfername = strcpy( transfername+1, filename );

		/* check for sending a long job number to a short number */
		if( jobnumber >= max_job_number
			&& (Long_number == 0 || Backwards_compatible) ){
			jobnumber = jobnumber % max_job_number;
			plp_snprintf( jobnum, sizeof(jobnum), "%03d", jobnumber );
		}

		/* deal with renumbering already found */
		if( jobnum[0] ){
			strcpy( transfername+3, jobnum );
			strcat( transfername, cf_static.filehostname );
		}
		DEBUG4("Receive_job: transfername '%s'", transfername );

		/* we need to open and lock the sent file name */
		DEBUG4("Receive_job: opening '%s'", transfername );
		fd = Lockf( transfername, &lock, &create, &statb );
		err = errno;

		/*
		 * fail if we cannot lock or open
		 */
		if( fd < 0 || lock <= 0 ){
			plp_snprintf( error, sizeof(error),
			"%s: cannot open or lock file '%s', %s",
				Printer, transfername, Errormsg(err) );
			ack = ACK_RETRY;
			goto error;
		}

		/*
		 * now we process the control file -
		 * check to see if there was one already transferred
		 */
		if( line[0] == CONTROL_FILE ){
			if( cf ){
				/* another control file is being sent!  */
				plp_snprintf( error, sizeof(error),
					"%s: additional control file being received! '%s'",
					Printer, filename );
				ack = ACK_FAIL;
				goto error;
			}

			/* This handles the case when you are transferring a
			 *	job, and have a conflict with the control file name.
			 *  We will have a non-zero data file.  However,  we
			 *  also might be transferring a job, data files first,
			 *  and have a conflict.  In this case, we will clobber
			 *	abandon the existing control file,  assuming it is
			 *  the result of a previously failed transfer.
			 *  We clobber the file by NOT trying to find a new name.
			 */
			if( statb.st_size && data_files.count == 0 ){
					close( fd );
					fd = -1;
			}

			/*
			 * if this is the first file being transferred
			 * AND it it the control file
			 * AND the control file exists
			 * AND the control file has non-zero length
			 * THEN we will create a new job number
			 */
			/* we see if we need to enable long job numbers here */
			if( fd < 0 && Long_number && !Backwards_compatible
				&& jobnum[0] == 0 ){
				max_job_number = 1000000;
			}

			for( j = 0; fd < 0 && j < max_job_number; ++j ){
				jobnumber = (jobnumber + 1) % max_job_number;
				if( jobnumber > 999 ){
					sprintf( jobnum, "%06d", jobnumber );
				} else {
					sprintf( jobnum, "%03d", jobnumber );
				}
				strcpy( transfername+3, jobnum );
				strcat( transfername, cf_static.filehostname );
				DEBUG4("Receive_job: collision retry '%s'", transfername );
				fd = Lockf( transfername, &lock, &create, &statb );
				err = errno;
				if( fd < 0 ){
					plp_snprintf( error, sizeof(error),
					"%s: cannot open '%s', %s",
						Printer, transfername, Errormsg(err) );
					ack = ACK_RETRY;
					goto error;
				} else if( lock <= 0 || statb.st_size ){
					close(fd);
					fd = -1;
				}
			}
			if( fd < 0 ){
				plp_snprintf( error, sizeof(error),
				"%s: cannot rename '%s' ", Printer, filename );
				ack = ACK_RETRY;
				goto error;
			}
			controlname = transfername;
			DEBUG4("Receive_job: control file transfername '%s'",
				transfername );
			/*
			 * now we create temporary file
			 */
			tempname = safestrdup(transfername);
			tempname[0] = '_';
			fd = temp_fd = Lockf( tempname, &lock, &create, &statb );
			err = errno;
			if( fd < 0 || lock <= 0 ){
				plp_snprintf( error, sizeof(error),
				"%s: cannot lock '%s' ", Printer, tempname );
				ack = ACK_RETRY;
				goto error;
			}
		} else if( line[0] == DATA_FILE ){
			/*
			 * if we have a data file,  add it to removal list
			 */
			if( data_files.count <= data_files.max ){
				extend_malloc_list( &data_files,
					sizeof( struct data_file ), 10 );
			}
			datafile = (void *)data_files.list;
			datafile = &datafile[data_files.count++];
			datafile->openname = openname;
			datafile->transfername = transfername;
		}

		/*
		 * truncate the file where we are putting data
		 */
		if( ftruncate( fd, 0 ) ){
			err = errno;
			plp_snprintf( error, sizeof( error),
				"truncate of '%s' failed - %s", transfername, Errormsg(err));
			ack = ACK_RETRY;
			goto error;
		}

		/************************************************
		 * check for job size and available space
		 * This is done here so that we can neatly clean up
		 * if we need to. Note we do this after we truncate...
		 ************************************************/
		jobsize += file_len;
		if( Max_job_size > 0 && (jobsize+1023)/1024 > Max_job_size ){
			plp_snprintf( error, sizeof( error),
				"%s: job size too large '%s'", Printer, filename );
			ack = ACK_RETRY;
			goto error;
		} else if( Check_space( file_len, Minfree, SDpathname ) ){
			plp_snprintf( error, sizeof( error),
				"%s: insuffcient file space '%s'", Printer, filename );
			ack = ACK_RETRY;
			goto error;
		}
		read_length = file_len;
		if( file_len == 0 ){
			read_length = ( Space_avail( SDpathname )
				- Space_needed( Minfree, SDpathname ) - 1 );
			if( Max_job_size && Max_job_size < read_length ){
				read_length = Max_job_size;
			}
			read_length = 1024 *read_length;
		}

		/*
		 * we are ready to read the file; send 0 ack saying so
		 */

		DEBUG4("Receive_job: sending 0 ACK" );
		status = Link_send( ShortRemote, socket, Send_timeout,
			0x100, (char *)0, 0, 0 );

		if( status ){
			plp_snprintf( error, sizeof( error),
				"%s: sending ACK 0 for '%s' failed", Printer, filename );
			ack = ACK_RETRY;
			goto error;
		}

		/*
		 * If the file length is 0, then we transfer only as much as we have
		 * space available. Note that this will be the last file in a job
		 */
		DEBUG6("Receive_job: receiving '%s' %d bytes ", filename, read_length );
		status = Link_file_read( ShortRemote, socket, Send_timeout,
			0, fd, &read_length, &ack );
		DEBUG6("Receive_job: received %d bytes ", read_length );
		if( file_len == 0 ){
			/*
			 * we do not send ACK if last file is specified 0 length
			 */
			DEBUG6("Receive_job: received %d bytes long file", read_length );
			if( read_length > 0 ){
				status = 0;
			} else {
				/* forget a 0 length file */
				plp_snprintf( error, sizeof( error),
					"%s: zero length job file '%s'", Printer, filename );
				status = 1;
				ack = 0;
				goto error;
			}
			break;
		} else if( status ){
			plp_snprintf( error, sizeof( error),
				"%s: transfer of '%s' from '%s' failed", Printer,
				filename, ShortRemote );
			ack = ACK_RETRY;
			goto error;
		}

		/*
		 * now we check the control file
		 */

		if( line[0] == CONTROL_FILE ){
			cf = &cf_static;
			/*allocate a string large enough */
			if( fstat( fd, &statb ) < 0 ){
				plp_snprintf( error, sizeof( error),
					"%s: fstat '%s' failed '%s'", Printer,
					openname, Errormsg(errno) );
				ack = ACK_RETRY;
				goto error;
			}
			cf->statb = statb;
			DEBUG6("Receive_job: control file len %d, sent %d",
				statb.st_size, file_len );

			/* copy the control file name */
			cf->name = add_buffer( &cf->control_file, strlen( openname ) );
			strcpy( cf->name, openname );
			/* read the control file into buffer */
			if( lseek( fd, (off_t)0, SEEK_SET ) < 0 ){
				plp_snprintf( error, sizeof( error),
					"%s: lseek '%s' failed '%s'", Printer,
					openname, Errormsg(errno) );
				ack = ACK_RETRY;
				goto error;
			}
			cf->cf_info = add_buffer( &cf->control_file, cf->statb.st_size+1 );
			for( i = 1, len = cf->statb.st_size, s = cf->cf_info;
				len > 0 && (i = read( fd, s, len )) > 0;
				len -= i, s += i );
			*s++ = 0;
			/*
			 * Parse the control file
			 */
			if( Parse_cf( SDpathname, cf, 0 ) ){
				strncpy( error, cf->error, sizeof(error) );
				ack = ACK_FAIL;
				goto error;
			}

			/* now we check the permissions for LPR job */

			Perm_check.service = 'R';
			Perm_check.printer = Printer;
			if( cf->LOGNAME && cf->LOGNAME[1] ){
				Perm_check.user = cf->LOGNAME+1;
				Perm_check.remoteuser = Perm_check.user;
			}
			if( cf->FROMHOST && cf->FROMHOST[1] ){
				Perm_check.host = Get_realhostname( cf );
				Perm_check.ip = ntohl(Find_ip(Perm_check.host));
			} else {
				Perm_check.host = 0;
				Perm_check.ip = 0;
			}

			/*
			 * now check to see if forwarding is turned off
			 * IP address of the sender and the job sender name are used
			 *
			 * NOTE: this may not work if remote system is multihomed
			 * host.  Attack that problem by having all connections made
			 * from the primary host name port (see Link_open() code)
			 */

			/* are you accepting forwarded jobs? */

			if( Forwarding_off && Perm_check.remoteip != Perm_check.ip ){
				plp_snprintf( error, sizeof(error),
					"%s: rejecting forwarded job originally from '%s'",
						Printer, Perm_check.host );
				goto error;
			}

			Init_perms_check();
			if( Perms_check( &Perm_file, &Perm_check, cf ) == REJECT
				|| Perms_check( &Local_perm_file, &Perm_check, cf )==REJECT
				|| Last_default_perm == REJECT ){
				plp_snprintf( error, sizeof(error),
					"%s: no permission to accept job", Printer );
				ack = ACK_FAIL;
				goto error;
			}
		}

		DEBUG4("Receive_job: sending 0 ACK" );
		status = Link_send( ShortRemote, socket, Send_timeout,
			0x100, (char *)0, 0, 0 );


		/* close the file */
		if( temp_fd != fd && fd >= 0 ){
			close( fd );
		}
		fd = -1;
	} while( status == 0 );

	/*
	 * check to see if all files arrived
	 */
	if( status == 0 ){
		lines = (char **)cf->info_lines.list;
		jobfiles = (void *)cf->data_file.list;
		datafile = (void *)data_files.list;
		DEBUG4("Receive_job: %d data files arrived, %d in control file",
			cf->data_file.count, data_files.count );

		if( cf == 0 ){
			plp_snprintf( error, sizeof(error), "missing control file" );
			status = LINK_TRANSFER_FAIL;
			goto error;
		}

		missing = 0;
		for( i = 0; !missing && i < cf->data_file.count; ++i ){
			/* check to see if the received data file
			 * is in the job list
			 */
			s = jobfiles[i].openname;
			DEBUG8("Receive_job: control file line %d data file '%s' = '%s'",
				jobfiles[i].line, s, lines[jobfiles[i].line]+1 /**/ );
			missing = 1;
			for( j = 0;
				j < data_files.count
				&& (missing = strcmp( datafile[j].openname, s )); ++j );
			if( missing == 0 ){
				/* back up one place on the transfername and put right
					value into control file */
				datafile[j].transfername[-1] = lines[jobfiles[i].line][0];
				lines[jobfiles[i].line] = datafile[j].transfername-1;
				if( jobfiles[i].Uinfo ){
					s = safestrdup( datafile[j].transfername-1 );
					s[0] = 'U';
					lines[jobfiles[i].uline] = s;
				}
			}
		}
		if( missing ){
			plp_snprintf( error, sizeof(error),
			 "data file '%s' not transferred", s );
			status = LINK_TRANSFER_FAIL;
			goto error;
		}
		missing = 1;
		for( i = 0; i < data_files.count; ++i ){
			/* check to see if the received data file
			 * is in the job list
			 */
			s = datafile[i].openname;
			DEBUG8("Receive_job: received '%s'", s );
			missing = 1;
			for( j = 0;
				j < cf->data_file.count
				&& (missing = strcmp( jobfiles[j].openname, s )); ++j );
			if( missing ) break;
		}
		if( missing ){
			plp_snprintf( error, sizeof(error),
			 "extra data file '%s'", s );
			status = LINK_TRANSFER_FAIL;
			goto error;
		}

		/*
		 * truncate the control file and rewrite it
		 */
		Remove_job_control( cf );
		holdfilename = cf->hold_file;
		if( ftruncate( temp_fd, 0 ) ){
			err = errno;
			plp_snprintf( error, sizeof( error),
				"truncate of '%s' failed - %s", tempname, Errormsg(err));
			ack = ACK_RETRY;
			goto error;
		}
		if( Use_queuename && cf->QUEUENAME == 0 ){
			char qn[M_QUEUENAME];
			plp_snprintf( qn, sizeof(qn), "Q%s", orig_name );
			DEBUG4("Receive_job: printer '%s' adding '%s'", orig_name, qn );
			cf->QUEUENAME = Prefix_job_line( cf, qn );
		}
		if( (Routing_filter || Use_identifier) && cf->IDENTIFIER == 0 ){
			DEBUG4("Receive_job: printer '%s' adding '%s'",
				orig_name, cf->identifier );
			cf->IDENTIFIER = Prefix_job_line( cf, cf->identifier-1 );
		}
		/*
		 * Now we do the routing
		 */
		DEBUG4("Receive_job: Routing_filter '%s'", Routing_filter );
		if( Routing_filter && *Routing_filter && Get_route( cf ) ){
			plp_snprintf( error, sizeof( error),
				"cannot do routing for %s", orig_name );
			ack = ACK_RETRY;
			goto error;
		}


		/*
		 * now we fix up the control file, and write it out again
		 * if necessary
		 */
		lines = (char **)cf->info_lines.list;
		DEBUG4("Receive_job: info_lines.count '%d'", cf->info_lines.count );
		cfile_copy[0] = 0;
		for(i = 0; i < cf->info_lines.count; ++i ){
			/* we get the pointer to the location in file */
			s = lines[i];
			DEBUG4("Receive_job: line[%d]='%s'", i, s );
			if( s == 0 || *s == 0 ) continue;
			if( Write_fd_str( temp_fd, s ) < 0
				|| Write_fd_str( temp_fd, "\n" ) < 0  ){
				err = errno;
				plp_snprintf( error, sizeof( error),
					"write of '%s' failed - %s", tempname, Errormsg(err));
				ack = ACK_RETRY;
				goto error;
			}
			if( Logger_destination ){
				safestrncat( cfile_copy, s );
				safestrncat( cfile_copy, "\n" );
			}
		}
		if( Set_job_control( cf ) ){
			plp_snprintf( error, sizeof( error), "could not write hold file" );
			ack = ACK_RETRY;
			goto error;
		}

		if( temp_fd >= 0 ){
			close(temp_fd);
			temp_fd = -1;
		}

		DEBUG4("Receive_job: renaming '%s' as '%s'", tempname, controlname );

		if( rename( tempname, controlname ) < 0 ){
			err = errno;
			plp_snprintf( error, sizeof( error),
				"rename of '%s' to '%s' failed - %s",
				tempname, controlname, Errormsg(err));
			ack = ACK_RETRY;
			goto error;
		}
	}

done:

	DEBUG4( "Receive_job: done status '%d'", status );
	if( status ){
		Remove_files( 0 );
	} else {
		if( Server_queue_name ){
			s = Server_queue_name;
		} else {
			s = Printer;
		}
		setmessage( cf, "TRACE", "%s@%s: job arrived\n%s",
			Printer, FQDNHost, cfile_copy );
		plp_snprintf( line, sizeof(line), "!%s\n", s );
		DEBUG4("Receive_job: sending '%s' to LPD", s );
		if( Write_fd_str( Lpd_pipe[1], line ) < 0 ){
			logerr_die( LOG_ERR, "Receive_job: write to pipe '%d' failed",
				Lpd_pipe[1] );
		}
	}
	DEBUG4( "Receive_job: return status %d", status );
	return(status);

error:
	status = JREMOVE;
	log( LOG_INFO, "Receive_job: error '%s'", error );
	if( error[0] ){
		DEBUG3("Receive_job: sending ACK %d, msg '%s'", ack, error );
		/* shut down reception from the remote file */
		(void)Link_send( ShortRemote, socket, Send_timeout,
			ack, error, '\n', 0 );
	}
	goto done;
}
