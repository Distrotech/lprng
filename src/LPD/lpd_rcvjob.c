/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpd_receivejob.c
 * PURPOSE: receive a job from a remote connections
 **************************************************************************/

static char *const _id =
"$Id: lpd_rcvjob.c,v 3.8 1997/01/30 21:15:20 papowell Exp $";

#include "lp.h"
#include "cleantext.h"
#include "dump.h"
#include "errorcodes.h"
#include "fileopen.h"
#include "freespace.h"
#include "gethostinfo.h"
#include "getqueue.h"
#include "jobcontrol.h"
#include "killchild.h"
#include "linksupport.h"
#include "lockfile.h"
#include "malloclist.h"
#include "merge.h"
#include "pathname.h"
#include "permission.h"
#include "setstatus.h"
#include "setup_filter.h"
#include "setupprinter.h"
#include "serverpid.h"
/**** ENDINCLUDE ****/

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

int Receive_job( int *socket, char *input, int maxlen )
{
	char line[LINEBUFFER];		/* line buffer for input */
	int i;						/* ACME! variables with a wide range! */
	int file_len;				/* length of file */
	int jobsize = 0;			/* size of job */
	int ack;					/* ack to send */
	int status = 0;					/* status of the last command */
	int len;					/* length of last read */
	char *filename;				/* name of control or data file */
	int fd = -1, lock, create;	/* used for file opening and locking */
	struct stat statb;			/* status buffer */
	int temp_fd = -1;			/* temp file descriptor */
	int read_length;			/* amount to read from socket */
	int err;					/* saved errno */
	char orig_name[LINEBUFFER];	/* original printer name */
	int filetype;				/* type of file - control or data */
	int filecount = 0;			/* number of files */
	int cf_sent = 0;			/* control file already sent */
	char *fn = 0;				/* file name being written */
	struct printcap_entry *pc_entry = 0; /* printcap entry */
	struct data_file *datafile;	/* data file information */

	/* clear error message */
	if( Cfp_static == 0 ){
		malloc_or_die( Cfp_static, sizeof(Cfp_static[0]) );
		memset(Cfp_static, 0, sizeof( Cfp_static[0] ) );
	}
	Clear_control_file( Cfp_static );
	Cfp_static->error[0] = 0;
	Cfp_static->remove_on_exit = 1;

	/* clean up the printer name a bit */
	++input;
	trunc_str(input);
	Name = "Receive_job";
	DEBUGF(DRECV2)("Receive_job: doing '%s'", input );

	strncpy( orig_name,input, sizeof( orig_name ) );

	/* set up cleanup and do initialization */

	register_exit( Remove_files, 0 );

	if( Clean_name( orig_name ) ){
		plp_snprintf( Cfp_static->error, sizeof(Cfp_static->error),
			"%s: bad printer name", input );
		ack = ACK_STOP_Q;	/* no retry, don't send again */
		goto error;
	}
	setproctitle( "lpd %s '%s'", Name, orig_name );
	if( Setup_printer( orig_name, Cfp_static->error, sizeof(Cfp_static->error),
		debug_vars, 0, (void *)0, &pc_entry ) ){
		DEBUGF(DRECV2)("Receive_job: Setup_printer failed '%s'", Cfp_static->error );
		ack = ACK_STOP_Q;	/* no retry, don't send again */
		goto error;
	}

	DEBUGF(DRECV2)("Receive_job: spooling_disabled %d",
		Spooling_disabled );
	if( Spooling_disabled ){
		plp_snprintf( Cfp_static->error, sizeof(Cfp_static->error),
			"%s: spooling disabled", Printer );
		ack = ACK_RETRY;	/* retry */
		goto error;
	}

	/* send an ACK */
	DEBUGF(DRECV2)("Receive_job: sending ACK (0)" );

	status = Link_ack( ShortRemote, socket, Send_timeout, 0x100, 0 );
	if( status ){
		plp_snprintf( Cfp_static->error, sizeof(Cfp_static->error),
			"%s: Receive_job: sending ACK 0 failed", Printer );
		goto error;
	}

	do{
		DEBUGF(DRECV2)("Receive_job: from %s- getting file transfer line", FQDNRemote );
		len = sizeof(line)-1;
		line[0] = 0; line[1] = 0;
		status = Link_line_read( ShortRemote, socket, Send_timeout,
			line, &len );

		DEBUGF(DRECV2)( "Receive_job: read from %s- status %d read %d bytes '\\%d'%s'",
				FQDNRemote, status, len, line[0], line+1 );
		if( status && len == 0 ){
			DEBUGF(DRECV2)( "Receive_job: from %s- read 0", FQDNRemote );
			status = 0;
			break;
		}
		trunc_str( line );
		len = strlen( line );
		if( len < 3 ){
			plp_snprintf( Cfp_static->error, sizeof(Cfp_static->error),
				"%s: bad file name format '%s'", Printer, input );
			ack = ACK_FAIL;
			goto error;
		}

		/* input line has the form Xlen filename */
		/* get the file type */
		filetype = line[0];
		if( filetype != DATA_FILE && filetype != CONTROL_FILE ){
			plp_snprintf( Cfp_static->error, sizeof(Cfp_static->error),
				"%s: bad file type 0x%x, not data or control",
			Printer, filetype );
			ack = ACK_FAIL;
			goto error;
		}

		/* get the file length */
		{
			char *s;
			s = filename = &line[1];
			file_len = strtol( s, &filename, 10 );
			if( s == filename || !isspace( *filename ) || file_len < 0 ){
				plp_snprintf( Cfp_static->error, sizeof(Cfp_static->error),
					"%s: bad file length format '%s'", Printer, input );
				ack = ACK_FAIL;
				goto error;
			}
		}

		/* skip spaces and remove trailing ones */
		while( isspace(*filename) ) ++filename;

		if( *filename == 0 ){
			plp_snprintf( Cfp_static->error, sizeof(Cfp_static->error),
				"%s: missing job file name",
					Printer, filename );
			ack = ACK_FAIL;
			goto error;
		}

		/* check the filename format for consistency,
		 *	and make sure that the datafiles are the same as well
		 * We try at this point to make sure that there are no duplicate
		 *  job numbers in the queue
		 */
		status = Check_format( filetype, filename, Cfp_static );
		if( status ){
			plp_snprintf( Cfp_static->error, sizeof(Cfp_static->error),
				"%s: file '%s' name format not [cd]f[A-Za-z]NNNhost",
					Printer, filename );
			ack = ACK_FAIL;
			goto error;
		}

		if( filecount == 0 ){
			DEBUGF(DRECV3)("Receive_job: %s first file - checking format",
				filename );
			/*
			 * if we have a data file first, then we
			 *	will force a 'A' level priority - this
			 *  is usually pretty safe
			 */
			if( (ack = Fixup_job_number( Cfp_static )) ){
				goto error;
			}
		}
		++filecount;

		/*
		 * now we process the control file -
		 * check to see if there was one already transferred
		 */
		if( filetype == CONTROL_FILE ){
			int priority;
			if( cf_sent ){
				/* another control file is being sent!  */
				plp_snprintf( Cfp_static->error, sizeof(Cfp_static->error),
					"%s: additional control file being received! '%s'",
					Printer, filename );
				ack = ACK_FAIL;
				goto error;
			}
			cf_sent = 1;
			/* get the priority - cfX <- X is priority
             *                    012                   */
			priority = filename[2];
			/* OK, now we have to see if we have had a data file
			 *	sent first.  If we have, then we MAY have opened
			 *  the wrong hold file - we need to remove it and reopen
			 *  it.  In this case, we simply abandon all hope and accept
			 *  the job.  Note that we will have clobbered the data file
			 *  of the job with the same job number - but this is too bad.
			 */
			if( priority != Cfp_static->priority ){
				/* Ooops! we opened the wrong hold file.
				 * Well, we can 'fix' this by simply renaming the
				 * hold file- this is ugly, brutal, and nasty.
				 * But I cannot think of any other solution...
				 */
				if( unlink( Cfp_static->hold_file ) != 0 ){
					plp_snprintf( Cfp_static->error, sizeof(Cfp_static->error),
					"%s: cannot unlink '%s' ", Printer, Cfp_static->hold_file );
					ack = ACK_RETRY;
					goto error;
				}
				Cfp_static->priority = priority;
				Cfp_static->hold_file[0] = 0;
				Set_job_control( Cfp_static, (void *)0, 0 );
			}
			/* we reformat the transfername - of the job */
			strncpy( Cfp_static->original, filename,
				sizeof( Cfp_static->original ) );
			plp_snprintf( Cfp_static->transfername,
				sizeof(Cfp_static->transfername),
				"cf%c%0*d%s",
				Cfp_static->priority,
				Cfp_static->number_len,
				Cfp_static->number, Cfp_static->filehostname );
			strncpy( Cfp_static->openname,
				Add_path( SDpathname, Cfp_static->transfername ),
				sizeof( Cfp_static->openname ) );
			
			fn = Cfp_static->openname;
			temp_fd = fd = Lockf( Cfp_static->openname, &lock, &create, &statb );
			err = errno;
			if( fd < 0 || lock <= 0 ){
				plp_snprintf( Cfp_static->error, sizeof(Cfp_static->error),
				"%s: cannot lock '%s' ", Printer, fn );
				ack = ACK_RETRY;
				goto error;
			}
		} else if( filetype == DATA_FILE ){
			/*
			 * if we have a data file,  add it to removal list
			 */
			if( Data_files.count+2 > Data_files.max ){
				extend_malloc_list( &Data_files,
					sizeof( struct data_file ), 10 );
			}
			datafile = (void *)Data_files.list;
			datafile = &datafile[Data_files.count++];
			strncpy( datafile->original,filename,sizeof(datafile->original));

			/* now we get the first 3 letters - dfX and append desired
			 * job number */
			strncpy( datafile->transfername, filename,
				sizeof( datafile->transfername) );
			plp_snprintf( datafile->transfername+3,
				sizeof( datafile->transfername ) - 3, 
				"%0*d%s", Cfp_static->number_len,
				Cfp_static->number, Cfp_static->filehostname );
			strncpy( datafile->openname,
				Add_path( SDpathname, datafile->transfername ),
				sizeof( datafile->openname ) );
			DEBUGF(DRECV3)("Receive_job: datafile original '%s', transfername '%s'",
				datafile->original, datafile->transfername );

			fn = datafile->openname;
			fd = Lockf( fn, &lock, &create, &statb );
			err = errno;
			if( fd < 0 || lock <= 0 ){
				plp_snprintf( Cfp_static->error, sizeof(Cfp_static->error),
				"%s: cannot lock '%s' ", Printer, fn );
				ack = ACK_RETRY;
				goto error;
			}
		}

		/*
		 * truncate the file where we are putting data
		 */
		if( ftruncate( fd, 0 ) ){
			err = errno;
			plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
				"truncate of '%s' failed - %s", fn, Errormsg(err));
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
			plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
				"%s: job size too large '%s'", Printer, filename );
			ack = ACK_RETRY;
			goto error;
		} else if( Check_space( file_len, Minfree, SDpathname ) ){
			plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
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

		DEBUGF(DRECV3)("Receive_job: sending 0 ACK" );
		status = Link_ack( ShortRemote, socket, Send_timeout, 0x100, 0 );

		if( status ){
			plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
				"%s: sending ACK 0 for '%s' failed", Printer, filename );
			ack = ACK_RETRY;
			goto error;
		}

		/*
		 * If the file length is 0, then we transfer only as much as we have
		 * space available. Note that this will be the last file in a job
		 */

		DEBUGF(DRECV4)("Receive_job: receiving '%s' %d bytes ", filename, read_length );
		status = Link_file_read( ShortRemote, socket, Send_timeout,
			0, fd, &read_length, &ack );
		DEBUGF(DRECV4)("Receive_job: received %d bytes ", read_length );
		if( file_len == 0 ){
			/*
			 * we do not send ACK if last file is specified 0 length
			 */
			DEBUGF(DRECV4)("Receive_job: received %d bytes long file", read_length );
			if( read_length > 0 ){
				status = 0;
			} else {
				/* forget a 0 length file */
				plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
					"%s: zero length job file '%s'", Printer, filename );
				status = 1;
				ack = 0;
				goto error;
			}
			break;
		} else if( status ){
			plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
				"%s: transfer of '%s' from '%s' failed", Printer,
				filename, ShortRemote );
			ack = ACK_RETRY;
			goto error;
		}

		/*
		 * now we check the control file
		 */

		if( filetype == CONTROL_FILE ){
			if( fstat( fd, &Cfp_static->statb ) < 0 ){
				plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
					"%s: fstat '%s' failed '%s'", Printer,
					fn, Errormsg(errno) );
				ack = ACK_RETRY;
				goto error;
			}
			DEBUGF(DRECV4)("Receive_job: control file len %d, sent %d",
				Cfp_static->statb.st_size, file_len );

			/* copy the control file */
			if( lseek( fd, 0, SEEK_SET ) < 0 ){
				plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
					"%s: lseek '%s' failed '%s'", Printer,
					fn, Errormsg(errno) );
				ack = ACK_RETRY;
				goto error;
			}
			Cfp_static->cf_info = add_buffer( &Cfp_static->control_file_image,
				Cfp_static->statb.st_size+1 );
			{
				char *s;
				len = Cfp_static->statb.st_size;
				s = Cfp_static->cf_info;
				for( i = 0; len > 0 && (i = read( fd, s, len )) > 0;
					len -= i, s += i );
				*s++ = 0;
			}
			/*
			 * Parse the control file
			 */
			if( Parse_cf( SDpathname, Cfp_static, 0 ) ){
				ack = ACK_FAIL;
				goto error;
			}
			if( Cfp_static->auth_id[0] && Auth_from == 0 ){
				plp_snprintf(Cfp_static->error, sizeof(Cfp_static->error),
		"Receive_job: authorization in non-authorized transfer" );
				return( i );
			}
			if( Cfp_static->auth_id[0] == 0 && Auth_from ){
				plp_snprintf(Cfp_static->error, sizeof(Cfp_static->error),
		"Receive_job: missing authorization information" );
				return( i );
			}

			/* now we check the permissions for LPR job */

			if( Do_perm_check( Cfp_static ) ){
				ack = ACK_FAIL;
				goto error;
			}
		}

		DEBUGF(DRECV3)("Receive_job: sending 0 ACK" );
		status = Link_ack( ShortRemote, socket, Send_timeout, 0x100, 0 );


		/* close the file */
		if( temp_fd != fd && fd >= 0 ){
			close( fd );
		}
		fd = -1;
	} while( status == 0 );

	Link_close( socket );

	if( (status = Check_for_missing_files( Cfp_static, &Data_files,
		temp_fd, orig_name, 0 ) ) ){
		goto error;
	}

	DEBUGF(DRECV3)( "Receive_job: return status %d", status );
	if( temp_fd > 0 ){
		close( temp_fd );
		temp_fd = -1;
	}
	Link_close( socket );
	Cfp_static->remove_on_exit = 0;
	Start_new_server();
	return(status);

error:
	DEBUGF(DRECV3)( "Receive_job: error status %d, ack %d",
		status, ack );
	if( status == 0 ) status = JREMOVE;
	if( temp_fd > 0 ){
		close( temp_fd );
		temp_fd = -1;
	}
	Remove_files(0);
	if( Cfp_static->error[0] ){
		/* log( LOG_INFO, "Receive_job: error '%s'", Cfp_static->error ); */
		DEBUGF(DRECV1)("Receive_job: sending ACK %d, msg '%s'", ack, Cfp_static->error );
		/* shut down reception from the remote file */
		if( ack ){
			(void)Link_ack( ShortRemote, socket, Send_timeout, ack, 0);
		}
		len = strlen( Cfp_static->error );
		if( len ){
			Cfp_static->error[len] = '\n';
		(void)Link_send( ShortRemote, socket, Send_timeout, Cfp_static->error,
			len+1, 0 );
		}
	}
	Link_close( socket );
	return( status );
}

/***************************************************************************
 * Block Job Transfer
 * \RCV_BLOCKprinter user@host jobid size
 *   The actual file transferred has the format:
 *   \CONTROL_FILElen name
 *   [control file contents]
 *   \DATA_FILElen name
 *   [data file contents]
 *
 * We receive the entire file, placing it into the control file.
 * We then split the job up as usual
 ***************************************************************************/

#define MAX_INPUT_TOKENS 10

int Receive_block_job( int *socket, char *input, int maxlen )
{
	char *tokens[MAX_INPUT_TOKENS+1];
	int tokencount;
	char *orig_name;	/* original printer name */
	char line_copy[LINEBUFFER];
	char *filename;		/* transfer filename */
	int   file_len;		/* length of the file */
	char *s, *end;		/* ACME */
	int temp_fd = -1;	/* fd for received file */
	int ack;			/* ACK value */
	struct printcap_entry *pc_entry = 0;	/* printcap entry */
	int status;			/* status of transfer */
	int lock, create;	/* for locking */
	struct stat statb;	/* stat buffer */
	char *fn;			/* file name being written */
	int err;			/* error code */
	int read_length;	/* file read length */

	/* clear error message */
	if( Cfp_static == 0 ){
		malloc_or_die( Cfp_static, sizeof(Cfp_static[0]) );
		memset(Cfp_static, 0, sizeof( Cfp_static[0] ) );
	}
	Clear_control_file( Cfp_static );
	Cfp_static->error[0] = 0;

	++input;
	safestrncpy(line_copy, input);
	tokencount = 0;
	for( s = line_copy; tokencount < MAX_INPUT_TOKENS && s; s = end ){
		while( isspace(*s) ) ++s;
		if( *s ){
			tokens[tokencount++] = s;
		}
		if( (end = strpbrk( s+1, " \t" )) ){
			*end++ = 0;
		}
	}
	tokens[tokencount] = 0;

	Name = "Receive_block_job";
	DEBUGF(DRECV2)("Receive_block_job: doing '%s'", input );
	if( tokencount != 3 ){
		plp_snprintf( Cfp_static->error, sizeof(Cfp_static->error),
			"line '%s' %d tokens", input, tokencount );
		ack = ACK_STOP_Q;
		goto error;
	}

	orig_name = tokens[0];
	filename  = tokens[1];
	file_len  = strtol( tokens[2], (char **)0, 10);

	/* set up cleanup and do initialization */

	register_exit( Remove_files, 0 );

	if( Clean_name( orig_name ) ){
		plp_snprintf( Cfp_static->error, sizeof(Cfp_static->error),
			"%s: bad printer name", input );
		ack = ACK_STOP_Q;	/* no retry, don't send again */
		goto error;
	}

	setproctitle( "lpd %s '%s'", Name, orig_name );
	if( Setup_printer( orig_name, Cfp_static->error, sizeof(Cfp_static->error),
		debug_vars, 0, (void *)0, &pc_entry ) ){
		DEBUGF(DRECV2)("Receive_block_job: Setup_printer failed '%s'",
			Cfp_static->error );
		ack = ACK_STOP_Q;	/* no retry, don't send again */
		goto error;
	}

	DEBUGF(DRECV2)("Receive_block_job: spooling_disabled %d",
		Spooling_disabled );
	if( Spooling_disabled ){
		plp_snprintf( Cfp_static->error, sizeof(Cfp_static->error),
			"%s: spooling disabled", Printer );
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
		plp_snprintf( Cfp_static->error, sizeof(Cfp_static->error),
			"%s: file '%s' name format not [cd]f[A-Za-z]NNNhost",
				Printer, filename );
		ack = ACK_FAIL;
		goto error;
	}

	/*
	 * get the non colliding job number
	 */

	if( (ack = Fixup_job_number( Cfp_static )) ){
		goto error;
	}

	/*
	 * now we process the control file -
	 * we reformat the transfername - of the job
	 */
	plp_snprintf( Cfp_static->transfername,
		sizeof(Cfp_static->transfername),
		"cf%c%0*d%s",
		Cfp_static->priority,
		Cfp_static->number_len,
		Cfp_static->number, Cfp_static->filehostname );
	strncpy( Cfp_static->openname,
		Add_path( SDpathname, Cfp_static->transfername ),
		sizeof( Cfp_static->openname ) );
	
	fn = Cfp_static->openname;
	temp_fd = Lockf( fn, &lock, &create, &statb );
	err = errno;
	if( temp_fd < 0 || lock <= 0 ){
		plp_snprintf( Cfp_static->error, sizeof(Cfp_static->error),
		"%s: cannot lock '%s' ", Printer, fn );
		ack = ACK_RETRY;
		goto error;
	}

	/*
	 * truncate the file where we are putting data
	 */
	if( ftruncate( temp_fd, 0 ) ){
		err = errno;
		plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
			"truncate of '%s' failed - %s", fn, Errormsg(err));
		ack = ACK_RETRY;
		goto error;
	}

	/************************************************
	 * check for job size and available space
	 * This is done here so that we can neatly clean up
	 * if we need to. Note we do this after we truncate...
	 ************************************************/
	if( Max_job_size > 0 && (file_len+1023)/1024 > Max_job_size ){
		plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
			"%s: job size too large '%s'", Printer, filename );
		ack = ACK_RETRY;
		goto error;
	} else if( Check_space( 2*file_len, Minfree, SDpathname ) ){
		plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
			"%s: insuffcient file space '%s'", Printer, filename );
		ack = ACK_RETRY;
		goto error;
	}

	read_length = file_len;
	/*
	 * we are ready to read the file; send 0 ack saying so
	 */

	DEBUGF(DRECV3)("Receive_block_job: sending 0 ACK" );
	status = Link_ack( ShortRemote, socket, Send_timeout, 0x100, 0);

	if( status ){
		plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
			"%s: sending ACK 0 for '%s' failed", Printer, filename );
		ack = ACK_RETRY;
		goto error;
	}

	DEBUGF(DRECV4)("Receive_block_job: receiving '%s' %d bytes ", filename, file_len );
	status = Link_file_read( ShortRemote, socket, Send_timeout,
		0, temp_fd, &read_length, &ack );
	DEBUGF(DRECV4)("Receive_block_job: received %d bytes ", read_length );
	if( status ){
		plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
			"%s: transfer of '%s' from '%s' failed", Printer,
			filename, ShortRemote );
		ack = ACK_FAIL;
		goto error;
	}

	/* extract jobs */

	if( Scan_block_file( temp_fd, Cfp_static ) ){
		ack = ACK_FAIL;
		goto error;
	}

	/* now we check the permissions for LPR job */

	if( Do_perm_check( Cfp_static ) ){
		ack = ACK_FAIL;
		goto error;
	}

	DEBUGF(DRECV3)("Receive_block_job: sending 0 ACK" );
	status = Link_ack( ShortRemote, socket, Send_timeout, 0x100, 0 );
	if( status ){
		plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
			"%s: sending ACK 0 for '%s' failed", Printer, filename );
		ack = ACK_RETRY;
		goto error;
	}

	if( (status = Check_for_missing_files( Cfp_static, &Data_files,
		temp_fd, orig_name, 0 ) ) ){
		goto error;
	}
	if( temp_fd >0 ){
		close( temp_fd );
		temp_fd = -1;
	}
	Link_close( socket );
	Start_new_server();
	return(status);

error:
	status = JREMOVE;
	if( temp_fd >0 ){
		close( temp_fd );
		temp_fd = -1;
	}
	Remove_files(0);
	if( ack ){
		(void)Link_ack( ShortRemote, socket, Send_timeout, ack, 0);
	}
	if( Cfp_static->error[0] ){
		int len = strlen( Cfp_static->error );
		/* log( LOG_INFO, "Receive_job: error '%s'", Cfp_static->error ); */
		DEBUGF(DRECV1)("Receive_job: sending ACK %d, msg '%s'", ack, Cfp_static->error );
		/* shut down reception from the remote file */
		Cfp_static->error[len] = '\n';
		(void)Link_send( ShortRemote, socket, Send_timeout, Cfp_static->error,
			len+1, 0 );
	}
	return( status );
}


/***************************************************************************
 * Scan_block_file( int fd, struct control_file *cfp )
 *  we scan the block file, getting the various portions
 *  The file has the format
 *  \KEYlen name
 *  We extract the various sections and find the offsets.
 *  Note that the various name fields will be the original
 *  values;  the ones we actually use will be the transfer values
 * RETURNS: nonzero on error, cfp->error set
 *          0 on success
 ***************************************************************************/
static int read_one_line(int fd, char *buffer, int maxlen,
		struct control_file *cfp );

int Scan_block_file( int fd, struct control_file *cfp )
{
	char line[LINEBUFFER];
	char buffer[LARGEBUFFER];
	int startpos;
	int length, type;		/* type and length fields */
	char *name;				/* name field */
	int status;
	char *s;				/* Acme */
	int len, i, cnt;
	struct data_file *datafile;	/* data file information */
	char *fn;
	int datafd;
	int lock, create;		/* for the data file creation and locking */
	int err;
	struct stat statb;

	DEBUGF(DRECV3)("Scan_block_file: starting" );
	/* first we reset the file position */
	if( lseek( fd, 0, SEEK_SET ) < 0 ){
		plp_snprintf( cfp->error, sizeof(cfp->error),	
			"Scan_block_file: lseek failed '%s'", Errormsg(errno) );
		return(1);
	}
	startpos = 0;
	while( (status = read_one_line( fd, line, sizeof(line), cfp)) > 0 ){
		/* the next position is the start of data */
		startpos = lseek( fd, 0, SEEK_CUR );
		if( startpos == -1 ){
			plp_snprintf( cfp->error, sizeof(cfp->error),	
				"Scan_block_file: lseek failed '%s'", Errormsg(errno) );
			return(1);
		}
		DEBUGF(DRECV3)("Scan_block_file: '%s', end position %d",
			line, startpos );
		/* now we parse the input line */
		trunc_str(line);
		type = line[0];
		/* check for data, control file */
		if( type != CONTROL_FILE && type != DATA_FILE ){
			/* get the next line */
			continue;
		}
		DEBUGF(DRECV3)("Scan_block_file: line '%s'", line );
		name = 0;
		length = strtol( line+1, &name, 0 );
		if( name == 0 ){
			plp_snprintf( cfp->error, sizeof( cfp->error),
			"bad length information '%s'", line+1 );
			return(-1);
		}
		while( isspace(*name) ) ++name;
		if( *name == 0 ){
			plp_snprintf( cfp->error, sizeof( cfp->error),
			"bad name information '%s'", line+1 );
			return(-1);
		}
		if( type == CONTROL_FILE ){
			/* we allocate a buffer to hold the control
			 * information.
			 */
			/* get the original name from the file */
			cfp->cf_info = add_buffer( &cfp->control_file_image,
				length+1 );
			s = cfp->cf_info;
			for(i = len = length;
				len > 0 && (i = read( fd, s, len )) > 0;
				len -= i, s += i );
			*s++ = 0;
			if( i < 0 || len > 0 ){
				plp_snprintf(cfp->error, sizeof(cfp->error),
					"Scan_block_file: error reading file '%s'",
					Errormsg(errno));
				return( -1 );
			}
			/*
			 * Parse the control file
			 */
			if( Parse_cf( SDpathname, cfp, 0 ) ){
				return( -1 );
			}
			if( cfp->auth_id[0] && Auth_from == 0 ){
				plp_snprintf(cfp->error, sizeof(cfp->error),
		"Scan_block_file: authorization in non-authorized transfer" );
				return( -1 );
			}
		} else if( type == DATA_FILE ){
			/* we can put the name and location of the data file
			 * into the database
			 */
			if( Data_files.count+2 > Data_files.max ){
				extend_malloc_list( &Data_files,
					sizeof( struct data_file ), 10 );
			}
			datafile = (void *)Data_files.list;
			datafile = &datafile[Data_files.count++];
			safestrncpy( datafile->original,name);

			/* now we get the first 3 letters - dfX and append desired
			 * job number */
			safestrncpy( datafile->transfername, name );
			plp_snprintf( datafile->transfername+3,
				sizeof( datafile->transfername ) - 3, 
				"%0*d%s", cfp->number_len,
				cfp->number, cfp->filehostname );
			strncpy( datafile->openname,
				Add_path( SDpathname, datafile->transfername ),
				sizeof( datafile->openname ) );
			DEBUGF(DRECV3)("Scan_block_file: datafile original '%s', transfername '%s'",
				datafile->original, datafile->transfername );

			fn = datafile->openname;
			DEBUGF(DRECV3)("Scan_block_file: data file '%s', len %d", fn, length );
			datafd = Lockf( fn, &lock, &create, &statb );
			if( datafd < 0 || lock <= 0 ){
				plp_snprintf( cfp->error, sizeof(cfp->error),
				"cannot lock '%s' ", fn );
				return(-1);
			}
			/*
			 * truncate the file where we are putting data
			 */
			if( ftruncate( datafd, 0 ) ){
				err = errno;
				plp_snprintf( cfp->error, sizeof( cfp->error),
					"truncate of '%s' failed - %s", fn, Errormsg(err));
				return(-1);
			}
			/* now copy the file */
			DEBUGF(DRECV3)("Scan_block_file: need to read %d", length );
			for(i = len = length; len > 0 && i > 0; len -= i ){
				cnt = sizeof(buffer);
				if( i < cnt ) cnt = i;
				i = read( fd, buffer, cnt );
				DEBUGF(DRECV3)("Scan_block_file: writing %d", i );
				if( i > 0 && Write_fd_len( datafd, buffer, i ) < 0 ){
					err = errno;
					plp_snprintf( cfp->error, sizeof( cfp->error),
					"write of '%s' failed - %s", fn, Errormsg(err));
					return(-1);
				}
			}
			if( i < 0 ){
				err = errno;
				plp_snprintf( cfp->error, sizeof( cfp->error),
				"Scan_block_file: read of '%s' failed - %s",
					cfp->openname, Errormsg(err));
				return(-1);
			}
			close(datafd);
		}
		/* now we seek to the next position */
		startpos = lseek( fd, startpos+length, SEEK_SET );
		if( startpos == -1 ){
			plp_snprintf( cfp->error, sizeof(cfp->error),	
				"Scan_block_file: lseek failed '%s'", Errormsg(errno) );
			return(1);
		}
		DEBUGF(DRECV3)("Scan_block_file: new position %d", startpos );
	}
	return( status );
}

/***************************************************************************
 * static int read_one_line(int fd, char *buffer, int maxlen );
 *  reads one line (terminated by \n) into the buffer
 *RETURNS:  0 if EOF characters read
 *          n = # chars read
 *          Note: buffer terminated by 0
 ***************************************************************************/
static int read_one_line( int fd, char *buffer, int maxlen,
	struct control_file *cfp )
{
	int len = 0;
	int status = 0;
	while( len < maxlen-1 && (status = read( fd, &buffer[len], 1)) > 0 ){
		if( buffer[len] == '\n' ){
			break;
		}
		++len;
	}
	if( status < 0 ){
		plp_snprintf(cfp->error, sizeof(cfp->error),
			"read_one_line: error reading '%s' - '%s'",
			cfp->openname, Errormsg(errno));
		return( status );
	}
	buffer[len] = 0;
	return(len);
}

/***************************************************************************
 * int Fixup_job_number( struct control_file *cfp )
 *  Find a non-colliding job number for the new job
 * RETURNS: 0 if successful
 *          ack value if unsuccessful
 * Side effects: sets up control file fields;
 ***************************************************************************/
int Fixup_job_number( struct control_file *cfp )
{
	int pid;					/* process id */
	int encountered = 0;		/* wrap around guard for job numbers */
	int hold_fd = -1;			/* job hold file fd */
	int ack = 0;				/* ack value */

	/* we set the job number to a reasonable range */
	Fix_job_number( cfp );

	hold_fd = -1;

again:
	DEBUGF(DRECV3)("Receive_job: job_number %d, encountered %d, max %d",
		cfp->number, encountered, cfp->max_number  );
	/* we now try each of these in order */
	/* now check to see if there is a job number collision */

	while( hold_fd == -1
		&& cfp->number < cfp->max_number ){
		/* try to open the hold file
		 * we generate a new job name
		 * if we have a data file first, then we
		 *	will force a 'A' level priority
		 */
		plp_snprintf( cfp->transfername,
			sizeof(cfp->transfername),
			"cf%c%0*d%s",
			cfp->priority, cfp->number_len,
			cfp->number, cfp->filehostname );
		cfp->hold_file[0] = 0;
		/* now we lock the hold file for the job */
		DEBUGF(DRECV3)("Receive_job: trying %s", cfp->transfername );
		Get_job_control( cfp, &hold_fd );
		pid = cfp->hold_info.receiver;
		DEBUGF(DRECV3)("Receive_job: receiver %d", pid );
		/* if the server is still active or has received
			all of the files then we skip to a new one */
		if( pid < 0 || (pid > 0 && kill( pid, 0 ) == 0) ){
			close( hold_fd );
			cfp->hold_file[0] = 0;
			hold_fd = -1;
			++cfp->number;
		}
	}
	/* if you did not find a spot, wrap around */
	if( hold_fd == -1 ){
		cfp->number = 0;
		if( encountered++ == 0 ){
			goto again;
		}
		plp_snprintf( cfp->error, sizeof(cfp->error),
			"%s: queue full - no space", Printer );
		ack = ACK_FAIL;
		goto error;
	}
	/* now we set the receiver field to the pid */
	cfp->hold_info.receiver = getpid();
	Set_job_control( cfp, &hold_fd, 0 );

error:
	DEBUGFC(DRECV3) dump_control_file( "Fixup_job_number: done", cfp );
	return( ack );
}

/***************************************************************************
 * int Check_for_missing_files( struct control_file *cfp,
 *	struct malloc_list *data_files );
 * Check to see that the files listed in the control file and the
 *  files that arrived are present.
 ***************************************************************************/

int dfcmp(const void *l, const void *r)
{
	struct data_file *left = (void *)l;
	struct data_file *right = (void *)r;

	DEBUGF(DRECV4)( "dfcmp: '%s' to '%s'", left->original, right->original );
	return( strcmp( left->original, right->original ) );
}

int Check_for_missing_files( struct control_file *cfp,
	struct malloc_list *data_files_list, int temp_fd, char *orig_name,
	char *authentication )
{
	int status = 0;
	int i, j;
	struct data_file *jobfile;	/* job data file information */
	struct data_file *datafile;	/* data file information */
	int jobfilecount, datafilecount;
	char *dfile, *jfile;
	char **lines;

	DEBUGF(DRECV3)("Check_for_missing_files: Auth_from %d, authentication '%s'",
		Auth_from, authentication );
	DEBUGF(DRECV3)("Check_for_missing_files: %d data files arrived, %d in control file",
		cfp->data_file_list.count, data_files_list->count );

	if( cfp->original[0] == 0 ){
		plp_snprintf( cfp->error, sizeof(cfp->error),
			"missing control file" );
		status = JFAIL;
		goto error;
	}
	if( data_files_list->count == 0 ){
		plp_snprintf( cfp->error, sizeof(cfp->error),
			"no data files transferred" );
		status = JFAIL;
		goto error;
	}
	if( cfp->data_file_list.count == 0 ){
		plp_snprintf( cfp->error, sizeof(cfp->error),
			"no data files in job" );
		status = JFAIL;
		goto error;
	}

	/* check to see if the received data file
	 * is in the job list.
	 * 1. sort the received data files
	 * 2. sort the job files
	 * 3. run up the lists, checking to see if there are missing or
	 *    duplicates
	 */

	jobfile = (void *)cfp->data_file_list.list;
	datafile = (void *)data_files_list->list;
	jobfilecount = cfp->data_file_list.count;
	datafilecount = data_files_list->count;
	Mergesort( jobfile, jobfilecount, sizeof( jobfile[0] ), dfcmp );
	Mergesort( datafile, datafilecount, sizeof( datafile[0] ), dfcmp );
	i = 0;
	j = 0;
	while( i < jobfilecount && j < datafilecount ){
		/* find the first non-duplicated entry */
		jfile = jobfile[i].original;
		dfile = datafile[j].original;
		DEBUGF(DRECV3)("Check_for_missing_files: jfile '%s', dfile '%s'", jfile, dfile );
		DEBUGF(DRECV3)("Check_for_missing_files: dfile '%s' transfername '%s'", dfile,
			datafile[j].transfername );
		if( strcmp( jfile, dfile ) ){
			DEBUGF(DRECV3)("Check_for_missing_files: bad name comparision");
			plp_snprintf( cfp->error, sizeof(cfp->error),
				"missing data files in job" );
			status = JFAIL;
			goto error;
		} else {
			/* we have two files with the same name.  We need
			 * to copy the transfername and openname information
			 * to the job file
			 */
			DEBUGF(DRECV3)("Check_for_missing_files: same files");
			while( j+1 < datafilecount
				&& strcmp( dfile, datafile[j+1].original ) == 0 ){
				++j;
			}
			while( i < jobfilecount ){
				jfile = jobfile[i].original;
				if( strcmp( dfile, jfile ) ) break;
				strncpy( jobfile[i].transfername+1, datafile[j].transfername,
					sizeof(jobfile[i].transfername)-1 );
				strncpy( jobfile[i].Uinfo+1, datafile[j].transfername,
					sizeof(jobfile[i].Uinfo)-1 );
				safestrncpy( jobfile[i].openname, datafile[j].openname );
				DEBUGF(DRECV3)("Check_for_missing_files: '%s' now '%s'",
					jobfile[i].original, jobfile[i].transfername );
				++i;
			}
			++j;
		}
	}
	if( i < jobfilecount || j < datafilecount ){
		DEBUGF(DRECV3)("Check_for_missing_files: missing files");
		plp_snprintf( cfp->error, sizeof(cfp->error),
			"missing data files in job" );
		status = JFAIL;
		goto error;
	}
	DEBUGFC(DRECV3)dump_control_file("Check_for_missing_files: before writing control", cfp);

	/*
	 * truncate the control file and rewrite it
	 */
	if( ftruncate( temp_fd, 0 ) ){
		plp_snprintf( cfp->error, sizeof( cfp->error),
			"truncate of '%s' failed - %s", cfp->openname,
			Errormsg(errno));
		status = JFAIL;
		goto error;
	}

	/*******************
     * make sure that the user authentication information
	 * is valid for the transfer.
	 * Auth_from = 0 - none, should be none
	 * Auth_from = 1 - client, put in by higher level
	 * Auth_from = 2 - forwarded, should be some
	 *******************/

	if( Auth_from == 0 ){
		if( cfp->auth_id[0] ){
			plp_snprintf( cfp->error, sizeof(cfp->error),
				"authentication information in non-authenticated job" );
			status = JFAIL;
			goto error;
		}
	} else if( Auth_from == 2 ){
		char templine[LINEBUFFER];
		if( cfp->auth_id[0] == 0 ){
			plp_snprintf( cfp->error, sizeof(cfp->error),
				"missing authentication information in forwarded job" );
			status = JFAIL;
			goto error;
		}
		/* we fix up the authentication information - should be
	     * '_'auth_id@auth_host line in control file
		 */
		safestrncpy( templine, cfp->auth_id );
		(void)Insert_job_line( cfp, templine, 0, 0);
	} else if( Auth_from == 1 ){
		char templine[LINEBUFFER];
		plp_snprintf( templine, sizeof( templine ), "_%s", authentication );
		(void)Insert_job_line( cfp, templine, 0, 0);
	}

	if( cfp->FROMHOST == 0 ){
		char qn[M_FROMHOST];
		plp_snprintf( qn, sizeof(qn), "H%s", FQDNRemote );
		DEBUGF(DRECV3)("Check_for_missing_files: printer '%s' adding '%s'", orig_name, qn );
		cfp->FROMHOST = Insert_job_line( cfp, qn, 0, 0);
	}
	if( Use_date && cfp->DATE == 0 ){
		char qn[M_DATE];
		plp_snprintf( qn, sizeof(qn), "D%s",
			Time_str( 0, cfp->statb.st_ctime ) );
		DEBUGF(DRECV3)("Check_for_missing_files: printer '%s' adding '%s'", orig_name, qn );
		cfp->DATE = Insert_job_line( cfp, qn, 0, 0);
	}
	if( (Routing_filter || Use_identifier) && cfp->IDENTIFIER == 0 ){
		if( Make_identifier( cfp ) ){
			status = JFAIL;
			goto error;
		}
		DEBUGF(DRECV3)("Check_for_missing_files: printer '%s' adding '%s'",
			orig_name, cfp->identifier+1 );
		cfp->IDENTIFIER = Insert_job_line( cfp, cfp->identifier, 0, 0);
	}
	/*
	 * Now we do the routing
	 */
	DEBUGF(DRECV3)("Check_for_missing_files: Routing_filter '%s'", Routing_filter );
	if( Routing_filter && *Routing_filter && Get_route( cfp ) ){
		plp_snprintf( cfp->error, sizeof( cfp->error),
			"cannot do routing for %s", orig_name );
		status = JFAIL;
		goto error;
	}


	/*
	 * now we fix up the control file, and write it out again
	 * if necessary
	 */
	lines = (char **)cfp->control_file_lines.list;
	DEBUGF(DRECV3)("Check_for_missing_files: control_file_lines.count '%d'", cfp->control_file_lines.count );
	for(i = 0; i < cfp->control_file_lines.count; ++i ){
		/* we get the pointer to the location in file */
		char *s = lines[i];
		if( s == 0 || *s == 0 ) continue;
		Clean_meta(s);
		DEBUGF(DRECV3)("Check_for_missing_files: line[%d]='%s'", i, s );
		if( Write_fd_str( temp_fd, s ) < 0
			|| Write_fd_str( temp_fd, "\n" ) < 0  ){
			plp_snprintf( cfp->error, sizeof( cfp->error),
				"write of '%s' failed - %s", cfp->openname, Errormsg(errno));
			status = JFAIL;
			goto error;
		}
	}
	close(temp_fd);
	temp_fd = -1;
	cfp->hold_info.receiver = -1;
	if( Set_job_control( cfp, (void *)0, 0 ) ){
		plp_snprintf( cfp->error, sizeof( cfp->error),
			"could not write hold file" );
		status = JFAIL;
		goto error;
	}
	Get_job_control( cfp, (void *)0 );

error:
	DEBUGF(DRECV3)( "Check_for_missing_files: done status '%d' error '%s'",
		status, cfp->error );
	if( status ){
		DEBUGF(DRECV1)( "%s: job receive failed '%s'", Printer, cfp->error );
		Remove_files( 0 );
	} else {
		setmessage( Cfp_static, "TRACE", "%s@%s: job arrived\n%s%s",
			Printer, FQDNHost,
			Copy_hf( &Cfp_static->control_file_lines,
				&Cfp_static->control_file_print, "CONTROL=", " - " ),
			Copy_hf( &Cfp_static->hold_file_lines,
				&Cfp_static->hold_file_print, "HOLDFILE=", " - " ) );
	}
	return(status);
}

int Do_perm_check( struct control_file *cfp )
{
	int permission;				/* permission */
	int ack = 0;
	char *host;

	Perm_check.service = 'R';
	Perm_check.printer = Printer;
	if( cfp->LOGNAME && cfp->LOGNAME[1] ){
		Perm_check.user = cfp->LOGNAME+1;
		Perm_check.remoteuser = Perm_check.user;
	}
	host = 0;
	if( cfp->FROMHOST && cfp->FROMHOST[1] ){
		host = &cfp->FROMHOST[1];
	}
	if( host && Find_fqdn( &PermcheckHostIP, host, 0 ) ){
		Perm_check.host = &PermcheckHostIP;
	} else {
		Perm_check.host = 0;
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

	if( Forwarding_off && Same_host( Perm_check.host, &RemoteHostIP) ){
		plp_snprintf( cfp->error, sizeof(cfp->error),
			"%s: rejecting forwarded job originally from '%s'",
				Printer, host );
		ack = ACK_FAIL;
		goto error;
	}

	Init_perms_check();
	if( (permission = Perms_check( &Perm_file,
			&Perm_check, cfp )) == REJECT
		|| (permission == 0 &&
			(permission = Perms_check( &Local_perm_file,
				&Perm_check, cfp ))==REJECT)
		|| (permission == 0 && Last_default_perm == REJECT) ){
		plp_snprintf( cfp->error, sizeof(cfp->error),
			"%s: no permission to accept job", Printer );
		ack = ACK_FAIL;
		goto error;
	}

error:
	return(ack);
}


/***************************************************************************
 * Start_new_server()
 *  We will directly shortcut the code to start a new server by
 *  calling it directly.  This will increase the stack size,
 *  but decrease the number of processes needed.  Note that
 *  we perform a few checks first in case the process is currently
 *  running.
 ***************************************************************************/
void Start_new_server( void )
{
	DEBUGF(DRECV2)("Start_new_server: Printer '%s', Server_queue_name %s",
		Printer, Server_queue_name );
	Remove_tempfiles();
	if( Tempfile ){
		free( Tempfile );
		Tempfile = 0;
	}
	Data_files.count = 0;
	if( Cfp_static ){
		free( Cfp_static );
	}
	Cfp_static = 0;
	if( Server_queue_name && *Server_queue_name ){
		Signal_server = 1;
		Do_queue_jobs( Server_queue_name );
	} else {
		Do_queue_jobs( Printer );
	}
}
