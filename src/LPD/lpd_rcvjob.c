/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpd_receivejob.c
 * PURPOSE: receive a job from a remote connections
 **************************************************************************/

static char *const _id =
"lpd_rcvjob.c,v 3.23 1998/03/29 18:32:42 papowell Exp";

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

int Receive_job( int *socket, char *input, int maxlen, int transfer_timeout )
{
	char line[LINEBUFFER];		/* line buffer for input */
	int i;						/* ACME! variables with a wide range! */
	int file_len;				/* length of file */
	int jobsize = 0;			/* size of job */
	int ack;					/* ack to send */
	int status = 0;					/* status of the last command */
	int len;					/* length of last read */
	char *s, *filename;			/* name of control or data file */
	int fd = -1;				/* used for file opening and locking */
	int read_length;			/* amount to read from socket */
	int err;					/* saved errno */
	char orig_name[LINEBUFFER];	/* original printer name */
	int filetype;				/* type of file - control or data */
	int filecount = 0;			/* number of files */
	int cf_sent = 0;			/* control file already sent */
	int sending_data_first = 0;	/* control or data file first */
	struct printcap_entry *pc_entry = 0; /* printcap entry */
	struct data_file *datafile;	/* data file information */
	int hold_fd = -1;				/* hold file */
	char *fn = 0;				/* file name */

	/* clear error message */
	if( Cfp_static == 0 ){
		Cfp_static = malloc_or_die( sizeof(Cfp_static[0]) );
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
	Queue_name = orig_name;

	/* set up cleanup and do initialization */

	register_exit( Remove_files, 0 );

	if( Clean_name( orig_name ) ){
		plp_snprintf( Cfp_static->error, sizeof(Cfp_static->error),
			_("%s: bad printer name"), input );
		ack = ACK_STOP_Q;	/* no retry, don't send again */
		goto error;
	}
	proctitle( "lpd %s '%s'", Name, orig_name );
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
			_("%s: spooling disabled"), Printer );
		ack = ACK_RETRY;	/* retry */
		goto error;
	}

	/* send an ACK */
	DEBUGF(DRECV2)("Receive_job: sending ACK (0)" );

	status = Link_ack( ShortRemote, socket,
		transfer_timeout, 0x100, 0 );
	if( status ){
		plp_snprintf( Cfp_static->error, sizeof(Cfp_static->error),
			_("%s: Receive_job: sending ACK 0 failed"), Printer );
		goto error;
	}

	do{
		DEBUGF(DRECV2)("Receive_job: from %s- getting file transfer line", FQDNRemote );
		len = sizeof(line)-1;
		line[0] = 0; line[1] = 0;
		status = Link_line_read( ShortRemote, socket,
			transfer_timeout,
			line, &len );

		DEBUGF(DRECV2)( "Receive_job: read from %s- status %d read %d bytes '%s'",
				FQDNRemote, status, len, line );
		if( len == 0 || status ){
			DEBUGF(DRECV2)( "Receive_job: ending reading from remote" );
			/* treat like normal closing of connection */
			status = 0;
			break;
		}
		trunc_str( line );
		len = strlen( line );
		if( len < 3 ){
			DEBUGF(DRECV2)( "Receive_job: short line, ending reading from remote" );
			/*
			plp_snprintf( Cfp_static->error, sizeof(Cfp_static->error),
				_("%s: bad file name format '%s'"), Printer, line ); */
			status = 0;
			break;
		}

		/* check to see if we have a 'start printer' command
		 * input line has the form Xlen filename
		 *                         ^ file type
		 * if a start command sent has form \001printer
		 *                                   ^ not right form
		 */
		filetype = line[0];
		if( filetype != DATA_FILE && filetype != CONTROL_FILE ){
			/* we see that this is a total whack of RFC1179 */
			/* we try to process the last job sent */
			if( filetype == ABORT_XFER ){
				/* send ACK and then process received jobs */
				/* don't bother checking to see if the remote end got the
					ACK as sometimes they simple send the 'REQ_START'
					and then close the socket */
				Link_ack( ShortRemote, socket,
					transfer_timeout,
					0x100, 0 );
				goto error;
			}
			Link_ack( ShortRemote, socket,
				transfer_timeout,
				0x100, 0 );
			if( !Sync_lpr ) Link_close( socket );
			status = 0;
			break;
		}

		/* get the file length */
		s = filename = &line[1];
		file_len = strtol( s, &filename, 10 );
		/* insanity checks for strtol AND file lengths */
		if( s == filename
			|| !isspace( *filename ) || file_len <= 0 ){
			/* we could have some command sent after the job transfer */
			/* we assume that this is a bad command - log it
				for debugging,  then process last job
			 */
			DEBUGF(DRECV1)("Receive_job: bad file transfer line '%s'",
				line );
			if( !Sync_lpr) Link_close( socket );
			status = 0;
			break;
		}

		/* skip spaces and remove trailing ones */
		while( isspace(*filename) ) ++filename;

		if( *filename == 0 ){
			plp_snprintf( Cfp_static->error, sizeof(Cfp_static->error),
				_("%s: missing job file name"),
					Printer, filename );
			ack = ACK_FAIL;
			goto error;
		}
		/*
		 * Total abuse of RFC1179 - sending more than one job
		 * at a time.  If the next job is a control file,
		 * then we need to do all the checks and then
		 * receive a new job.
		 */
		if( (filetype == CONTROL_FILE && cf_sent )
			|| (filetype == DATA_FILE && sending_data_first && cf_sent) ){
			if( (status = Check_for_missing_files( Cfp_static, &Data_files,
				orig_name, 0, &hold_fd, pc_entry ) ) ){
				goto error;
			}
			/* now we reinitialize things */
			filecount = 0;	/* first file of job */
			close( hold_fd );
			hold_fd = -1;
			Clear_control_file( Cfp_static );	/* get ready for next job */
			Data_files.count = 0;
			cf_sent = 0;
			sending_data_first = 0;
			Cfp_static->error[0] = 0;
			Cfp_static->remove_on_exit = 1;
		}

		if( filecount == 0 && filetype == DATA_FILE ){
			sending_data_first = 1;
		}

		/* check the filename format for consistency,
		 *	and make sure that the datafiles are the same as well
		 * We try at this point to make sure that there are no duplicate
		 *  job numbers in the queue
		 */
		status = Check_format( filetype, filename, Cfp_static );
		if( status ){
			char buffer[LINEBUFFER];
			safestrncpy(buffer,Cfp_static->error);
			plp_snprintf( Cfp_static->error, sizeof(Cfp_static->error),
				_("%s: file '%s' name problems- %s"),
					Printer, filename, buffer );
			ack = ACK_FAIL;
			goto error;
		}

		if( filecount == 0 ){
			DEBUGF(DRECV3)("Receive_job: %s first file - checking format",
				filename );
			/*
			 * get the hold file name for job
			 */
			if( (hold_fd = Find_non_colliding_job_number( Cfp_static,
				CDpathname )) < 0 ){
				goto error;
			}
		}
		++filecount;

		/*
		 * now we process the control file -
		 */
		if( filetype == CONTROL_FILE ){
			int priority;
			cf_sent = 1;
			/* get the priority - cfX <- X is priority
             *                    012                   */
			priority = filename[2];
			Cfp_static->priority = priority;
			/* OK, now we have to see if we have had a data file
			 *	sent first.  If we have, then we MAY have opened
			 *  the wrong hold file - we need to remove it and reopen
			 *  it.  In this case, we simply abandon all hope and accept
			 *  the job.  Note that we MAY have clobbered the data files
			 *  of a job with the same job number sending the data files first
			 *  - but this is too bad.
			 */
			/* we reformat the transfername - of the job */
			strncpy( Cfp_static->original, filename,
				sizeof( Cfp_static->original ) );
			plp_snprintf( Cfp_static->transfername,
				sizeof(Cfp_static->transfername),
				"cf%c%0*d%s",
				Cfp_static->priority,
				Cfp_static->number_len,
				Cfp_static->number, Cfp_static->filehostname );
			
			/* we open a temporary file for the file, later we rename it */
			fd = Make_temp_fd( Cfp_static->openname,
				sizeof(Cfp_static->openname) );
			err = errno;
			fn = Cfp_static->openname;
			if( fd < 0 ){
				plp_snprintf( Cfp_static->error, sizeof(Cfp_static->error),
				_("%s: cannot open temp file for control file '%s'"),
					Printer, Cfp_static->original );
				ack = ACK_RETRY;
				goto error;
			}
		} else if( filetype == DATA_FILE ){
			/*
			 * if we have a data file,  add it to removal list
			 */
			if( Data_files.count+2 > Data_files.max ){
				extend_malloc_list( &Data_files,
					sizeof( struct data_file ), 10,__FILE__,__LINE__  );
			}
			datafile = (void *)Data_files.list;
			datafile = &datafile[Data_files.count++];
			strncpy( datafile->original,filename,sizeof(datafile->original));
			fd = Make_temp_fd( datafile->openname,
				sizeof(datafile->openname) );
			fn = datafile->openname;
			err = errno;
			if( fd < 0 ){
				plp_snprintf( Cfp_static->error, sizeof(Cfp_static->error),
				_("%s: cannot open temp file for data file '%s'"),
					Printer, fn );
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
				_("truncate of '%s' failed - %s"), fn, Errormsg(err));
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
				_("%s: job size too large '%s'"), Printer, filename );
			ack = ACK_RETRY;
			goto error;
		} else if( Check_space( file_len, Minfree, SDpathname ) ){
			plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
				_("%s: insufficient file space '%s'"), Printer, filename );
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
		status = Link_ack( ShortRemote, socket,
			transfer_timeout,
			0x100, 0 );

		if( status ){
			plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
				_("%s: sending ACK 0 for '%s' failed"), Printer, filename );
			ack = ACK_RETRY;
			goto error;
		}

		/*
		 * If the file length is 0, then we transfer only as much as we have
		 * space available. Note that this will be the last file in a job
		 */

		DEBUGF(DRECV4)("Receive_job: receiving '%s' %d bytes ", filename, read_length );
		status = Link_file_read( ShortRemote, socket,
			transfer_timeout,
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
					_("%s: zero length job file '%s'"), Printer, filename );
				status = 1;
				ack = 0;
				goto error;
			}
			/* we finish processing the job now */
			break;
		} else if( status ){
			plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
				_("%s: transfer of '%s' from '%s' failed"), Printer,
				filename, ShortRemote );
			ack = ACK_RETRY;
			goto error;
		}

		/*
		 * now we parse and check the control file
		 */

		if( filetype == CONTROL_FILE ){
			if( fstat( fd, &Cfp_static->statb ) < 0 ){
				plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
					_("%s: fstat '%s' failed '%s'"), Printer,
					fn, Errormsg(errno) );
				ack = ACK_RETRY;
				goto error;
			}
			DEBUGF(DRECV4)("Receive_job: control file len %d, sent %d",
				Cfp_static->statb.st_size, file_len );

			/* copy the control file */
			if( lseek( fd, 0, SEEK_SET ) < 0 ){
				plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
					_("%s: lseek '%s' failed '%s'"), Printer,
					fn, Errormsg(errno) );
				ack = ACK_RETRY;
				goto error;
			}
			Cfp_static->cf_info = add_buffer( &Cfp_static->control_file_image,
				Cfp_static->statb.st_size+1,__FILE__,__LINE__  );
			len = Cfp_static->statb.st_size;
			s = Cfp_static->cf_info;
			for( i = 0; len > 0 && (i = read( fd, s, len )) > 0;
				len -= i, s += i );
			*s++ = 0;
			err = errno;
			if( i < 0 ){
				plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
					_("%s: read '%s' failed '%s'"), Printer,
					fn, Errormsg(err) );
				ack = ACK_RETRY;
				goto error;
			}
			len = Cfp_static->statb.st_size;
			if( strlen( Cfp_static->cf_info ) != len ){
				plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
					_("%s: temp file for file '%s' did not have '%d' bytes"),
					Printer, fn, len );
				ack = ACK_RETRY;
				goto error;
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
		_("Receive_job: authorization in non-authorized transfer") );
				ack = ACK_FAIL;
				goto error;
			}
			if( Cfp_static->auth_id[0] == 0 && Auth_from ){
				plp_snprintf(Cfp_static->error, sizeof(Cfp_static->error),
		_("Receive_job: missing authorization information") );
				ack = ACK_FAIL;
				goto error;
			}

			/* now we check the permissions for LPR job */

			if( Do_perm_check( Cfp_static ) ){
				ack = ACK_FAIL;
				goto error;
			}
		}

		DEBUGF(DRECV3)("Receive_job: sending 0 ACK" );
		status = Link_ack( ShortRemote, socket,
			transfer_timeout, 0x100, 0 );

		/* close the file */
		close(fd);
		fd = -1;
  
		/* UWCSEHACK **************************************
		 * This is essentially to fix communication problem with
		 * Berkeley lpr which sends df files before cf files.
		 * If all df files as listed in cf have be received,
		 * break off the loop so that next job can come in.
		 *
		 * This code can be removed if the code in 
		 * "Total abuse of RFC1179" section fixes the problem.
		 * It did not fix the problem in 3.2.5.
		 **************************************************/
		if (filetype == CONTROL_FILE
		   && Cfp_static->data_file_list.count == Data_files.count
		   && Data_files.count ) {
			struct data_file *cf_df;
			struct data_file *df_df;
			int cfcount = Cfp_static->data_file_list.count;
			int dfcount = Data_files.count;
			int missing = 0;

			cf_df = (void *)Cfp_static->data_file_list.list;
			df_df = (void *)Data_files.list;
			for(; !missing && cfcount ; cf_df++, cfcount--) {
					/* check to see if the received data file
					 * is in the job list
					 */
			   for( ;dfcount &&
					(missing = strcmp( cf_df->openname,
					df_df->openname )); dfcount--, df_df++ );
			}
			if (!missing) break;
		}
	} while( status == 0 );

	if( !Sync_lpr ) Link_close( socket );

	if( (status = Check_for_missing_files( Cfp_static, &Data_files,
		orig_name, 0, &hold_fd, pc_entry ) ) ){
		goto error;
	}

	/* at this point, you have handled all jobs sent to you,
	 * and now have to deal with the last non-job transfer command.
	 * Be brutal - simply close the socket and see what happens
	 */


	DEBUGF(DRECV3)( "Receive_job: finished receiving job");
	Clear_control_file( Cfp_static );
	Cfp_static->remove_on_exit = 0;
	close( hold_fd );
	Link_close( socket );
	Start_new_server();
	cleanup(0);

error:
	DEBUGF(DRECV3)( "Receive_job: error status %d, ack %d",
		status, ack );
	Remove_files(0);
	if( hold_fd > 0 ){
		close( hold_fd );
		hold_fd = -1;
	}
	if( fd > 0 ){
		close( fd );
		fd = -1;
	}
	if( Cfp_static->error[0] ){
		/* log( LOG_INFO, "Receive_job: error '%s'", Cfp_static->error ); */
		DEBUGF(DRECV1)("Receive_job: sending ACK %d, msg '%s'", ack, Cfp_static->error );
		/* shut down reception from the remote file */
		if( ack ){
			(void)Link_ack( ShortRemote, socket,
				transfer_timeout, ack, 0);
		}
		len = strlen( Cfp_static->error );
		if( len ){
			Cfp_static->error[len] = '\n';
		(void)Link_send( ShortRemote, socket,
			transfer_timeout,
			Cfp_static->error, len+1, 0 );
		}
	}
	Link_close( socket );
	cleanup( 0 );
	return(0);
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

int Receive_block_job( int *socket, char *input, int maxlen, int transfer_timeout )
{
	char *tokens[MAX_INPUT_TOKENS+1];
	int tokencount;
	char *orig_name;	/* original printer name */
	char line_copy[LINEBUFFER];
	char *filename;		/* transfer filename */
	int   file_len;		/* length of the file */
	char *s, *end;		/* ACME */
	int fd = -1;	/* fd for received file */
	int ack;			/* ACK value */
	struct printcap_entry *pc_entry = 0;	/* printcap entry */
	int status;			/* status of transfer */
	char *fn;			/* file name being written */
	int err;			/* error code */
	int read_length;	/* file read length */
	int hold_fd = -1;	/* hold file */

	/* clear error message */
	if( Cfp_static == 0 ){
		Cfp_static = malloc_or_die( sizeof(Cfp_static[0]) );
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
			_("line '%s' %d tokens"), input, tokencount );
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
			_("%s: bad printer name"), input );
		ack = ACK_STOP_Q;	/* no retry, don't send again */
		goto error;
	}

	proctitle( "lpd %s '%s'", Name, orig_name );
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
			_("%s: file '%s' name problems- %s"),
				Printer, filename, buffer );
		ack = ACK_FAIL;
		goto error;
	}

	/*
	 * get the non colliding job number
	 */

	if( (hold_fd = Find_non_colliding_job_number( Cfp_static,
			CDpathname )) < 0){
		ack = ACK_FAIL;
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
	fd = Make_temp_fd( Cfp_static->openname,
		sizeof(Cfp_static->openname) );
	err = errno;
	fn = Cfp_static->openname;
	if( fd < 0 ){
		plp_snprintf( Cfp_static->error, sizeof(Cfp_static->error),
		_("%s: cannot open temp file for control file '%s'"),
			Printer, Cfp_static->original );
		ack = ACK_RETRY;
		goto error;
	}

	/*
	 * truncate the file where we are putting data
	 */
	if( ftruncate( fd, 0 ) ){
		err = errno;
		plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
			_("truncate of '%s' failed - %s"), fn, Errormsg(err));
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
			_("%s: job size too large '%s'"), Printer, filename );
		ack = ACK_RETRY;
		goto error;
	} else if( Check_space( 2*file_len, Minfree, SDpathname ) ){
		plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
			_("%s: insufficient file space '%s'"), Printer, filename );
		ack = ACK_RETRY;
		goto error;
	}

	read_length = file_len;
	/*
	 * we are ready to read the file; send 0 ack saying so
	 */

	DEBUGF(DRECV3)("Receive_block_job: sending 0 ACK" );
	status = Link_ack( ShortRemote, socket,
		transfer_timeout, 0x100, 0);

	if( status ){
		plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
			_("%s: sending ACK 0 for '%s' failed"), Printer, filename );
		ack = ACK_RETRY;
		goto error;
	}

	DEBUGF(DRECV4)("Receive_block_job: receiving '%s' %d bytes ", filename, file_len );
	status = Link_file_read( ShortRemote, socket,
		transfer_timeout,
		0, fd, &read_length, &ack );
	DEBUGF(DRECV4)("Receive_block_job: received %d bytes ", read_length );
	if( status ){
		plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
			_("%s: transfer of '%s' from '%s' failed"), Printer,
			filename, ShortRemote );
		ack = ACK_FAIL;
		goto error;
	}

	/* extract jobs */

	if( Scan_block_file( fd, Cfp_static ) ){
		ack = ACK_FAIL;
		goto error;
	}
	close( fd );
	fd = -1;

	/* now we check the permissions for LPR job */

	if( Do_perm_check( Cfp_static ) ){
		ack = ACK_FAIL;
		goto error;
	}

	DEBUGF(DRECV3)("Receive_block_job: sending 0 ACK" );
	status = Link_ack( ShortRemote, socket,
		transfer_timeout, 0x100, 0 );
	if( status ){
		plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
			_("%s: sending ACK 0 for '%s' failed"), Printer, filename );
		ack = ACK_RETRY;
		goto error;
	}

	if( (status = Check_for_missing_files( Cfp_static, &Data_files,
		orig_name, 0, &hold_fd, pc_entry ) ) ){
		goto error;
	}
	DEBUGF(DRECV3)( "Receive_block_job: finished receiving job");
	Link_close( socket );
	close( hold_fd );
	Clear_control_file( Cfp_static );
	Cfp_static->error[0] = 0;
	Start_new_server();
	cleanup(0);

error:
	status = JREMOVE;
	Remove_files(0);
	if( fd > 0 ){
		close( fd );
		fd = -1;
	}
	if( ack ){
		(void)Link_ack( ShortRemote, socket,
			transfer_timeout, ack, 0);
	}
	if( Cfp_static->error[0] ){
		int len = strlen( Cfp_static->error );
		/* log( LOG_INFO, "Receive_job: error '%s'", Cfp_static->error ); */
		DEBUGF(DRECV1)("Receive_job: sending ACK %d, msg '%s'", ack, Cfp_static->error );
		/* shut down reception from the remote file */
		Cfp_static->error[len] = '\n';
		(void)Link_send( ShortRemote, socket,
			transfer_timeout,
			Cfp_static->error, len+1, 0 );
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
	int err;

	DEBUGF(DRECV3)("Scan_block_file: starting" );
	/* first we reset the file position */
	if( lseek( fd, 0, SEEK_SET ) < 0 ){
		plp_snprintf( cfp->error, sizeof(cfp->error),	
			_("Scan_block_file: lseek failed '%s'"), Errormsg(errno) );
		return(1);
	}
	startpos = 0;
	while( (status = read_one_line( fd, line, sizeof(line), cfp)) > 0 ){
		/* the next position is the start of data */
		startpos = lseek( fd, 0, SEEK_CUR );
		if( startpos == -1 ){
			plp_snprintf( cfp->error, sizeof(cfp->error),	
				_("Scan_block_file: lseek failed '%s'"), Errormsg(errno) );
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
			_("bad length information '%s'"), line+1 );
			return(-1);
		}
		while( isspace(*name) ) ++name;
		if( *name == 0 ){
			plp_snprintf( cfp->error, sizeof( cfp->error),
			_("bad name information '%s'"), line+1 );
			return(-1);
		}
		if( type == CONTROL_FILE ){
			/* we allocate a buffer to hold the control
			 * information.
			 */
			/* get the original name from the file */
			cfp->cf_info = add_buffer( &cfp->control_file_image,
				length+1,__FILE__,__LINE__  );
			s = cfp->cf_info;
			for(i = len = length;
				len > 0 && (i = read( fd, s, len )) > 0;
				len -= i, s += i );
			*s++ = 0;
			if( i < 0 || len > 0 ){
				plp_snprintf(cfp->error, sizeof(cfp->error),
					_("Scan_block_file: error reading file '%s'"),
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
		_("Scan_block_file: authorization in non-authorized transfer") );
				return( -1 );
			}
		} else if( type == DATA_FILE ){
			/* we can put the name and location of the data file
			 * into the database
			 */
			if( Data_files.count+2 > Data_files.max ){
				extend_malloc_list( &Data_files,
					sizeof( struct data_file ), 10,__FILE__,__LINE__  );
			}
			datafile = (void *)Data_files.list;
			datafile = &datafile[Data_files.count++];
			safestrncpy( datafile->original,name);
			datafd = Make_temp_fd( datafile->openname,
				sizeof(datafile->openname) );
			fn = datafile->openname;
			DEBUGF(DRECV3)("Scan_block_file: data file '%s', len %d", fn, length );
			if( datafd < 0 ){
				plp_snprintf( cfp->error, sizeof(cfp->error),
				_("cannot create tempfile '%s' "), fn );
				return(-1);
			}
			/*
			 * truncate the file where we are putting data
			 */
			if( ftruncate( datafd, 0 ) ){
				err = errno;
				plp_snprintf( cfp->error, sizeof( cfp->error),
					_("truncate of '%s' failed - %s"), fn, Errormsg(err));
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
					_("write of '%s' failed - %s"), fn, Errormsg(err));
					return(-1);
				}
			}
			if( i < 0 ){
				err = errno;
				plp_snprintf( cfp->error, sizeof( cfp->error),
				_("Scan_block_file: read of '%s' failed - %s"),
					cfp->openname, Errormsg(err));
				return(-1);
			}
			close(datafd);
		}
		/* now we seek to the next position */
		startpos = lseek( fd, startpos+length, SEEK_SET );
		if( startpos == -1 ){
			plp_snprintf( cfp->error, sizeof(cfp->error),	
				_("Scan_block_file: lseek failed '%s'"), Errormsg(errno) );
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
			_("read_one_line: error reading '%s' - '%s'"),
			cfp->openname, Errormsg(errno));
		return( status );
	}
	buffer[len] = 0;
	return(len);
}

int dfcmp(const void *l, const void *r)
{
	struct data_file *left = (void *)l;
	struct data_file *right = (void *)r;

	DEBUGF(DRECV4)( "dfcmp: '%s' to '%s'", left->original, right->original );
	return( strcmp( left->original, right->original ) );
}

/***************************************************************************
 * int Check_for_missing_files(
 * struct control_file *cfp  - control file
 * struct malloc_list *data_files - data files
 * char *orig_name - original printer name
 * char *authentication - authentication information
 *  1. Check to see that the files listed in the control file and the
 *     files that arrived are present.
 *  2. Update the control file with information
 *  3. If necessary rename temp file as control file
 ***************************************************************************/

int Check_for_missing_files( struct control_file *cfp,
	struct malloc_list *data_files_list,
	char *orig_name,
	char *authentication, int *hold_fd,
	struct printcap_entry *pc_entry)
{
	int status = 0;
	int i, j, found, temp_fd = -1;
	struct data_file *jobfile;	/* job data file information */
	struct data_file *datafile;	/* data file information */
	int jobfilecount, datafilecount;
	char *s, *dfile, *jfile;
	char **lines;

	DEBUGF(DRECV3)("Check_for_missing_files: Auth_from %d, authentication '%s'",
		Auth_from, authentication );
	DEBUGF(DRECV3)("Check_for_missing_files: %d data files arrived, %d in control file",
		data_files_list->count, cfp->data_file_list.count );

	if( cfp->original[0] == 0 ){
		plp_snprintf( cfp->error, sizeof(cfp->error),
			_("missing control file") );
		status = JFAIL;
		goto error;
	}
	if( data_files_list->count == 0 ){
		plp_snprintf( cfp->error, sizeof(cfp->error),
			_("no data files transferred") );
		status = JFAIL;
		goto error;
	}
	if( cfp->data_file_list.count == 0 ){
		plp_snprintf( cfp->error, sizeof(cfp->error),
			_("no data files in job") );
		status = JFAIL;
		goto error;
	}

	if( Auth_from == 0 ){
		if( cfp->auth_id[0] ){
			plp_snprintf( cfp->error, sizeof(cfp->error),
				_("authentication information in non-authenticated job") );
			status = JFAIL;
			goto error;
		}
	} else if( Auth_from == 2 ){
		char templine[LINEBUFFER];
		if( cfp->auth_id[0] == 0 ){
			plp_snprintf( cfp->error, sizeof(cfp->error),
				_("missing authentication information in forwarded job") );
			status = JFAIL;
			goto error;
		}
		/* we fix up the authentication information - should be
	     * '_'auth_id@auth_host line in control file
		 */
		safestrncpy( templine, cfp->auth_id );
		(void)Insert_job_line( cfp, templine, 0, 0,__FILE__,__LINE__ );
	} else if( Auth_from == 1 ){
		char templine[LINEBUFFER];
		plp_snprintf( templine, sizeof( templine ), "_%s", authentication );
		(void)Insert_job_line( cfp, templine, 0, 0,__FILE__,__LINE__ );
	}

	if( cfp->FROMHOST == 0 || cfp->FROMHOST[1] == 0 ){
		char qn[M_FROMHOST];
		plp_snprintf( qn, sizeof(qn), "H%s", FQDNRemote );
		DEBUGF(DRECV3)("Check_for_missing_files: printer '%s' adding '%s'", orig_name, qn );
		if( cfp->FROMHOST ) cfp->FROMHOST[0] = 0;
		cfp->FROMHOST = Insert_job_line( cfp, qn, 0, 0,__FILE__,__LINE__ );
		cfp->name_format_error = 1;
	} else if( Force_FQDN_hostname && strchr(cfp->FROMHOST,'.') == 0){
		char qn[M_FROMHOST];
		/* Kludge!  Kludge!
		 * The following code will assign the FQDN Domain
		 * to be that of the Remote Host which is originating
		 * the job.  This allows the server and the remote host to be
		 * in different domains.
		 */
		s = strchr( FQDNRemote,'.');
		plp_snprintf( qn, sizeof(qn), "H%s%s", cfp->FROMHOST+1, s );
		DEBUGF(DRECV3)("Check_for_missing_files: printer '%s' adding '%s'", orig_name, qn );
		/* clear out the existing data in the file */
		if( cfp->FROMHOST ) cfp->FROMHOST[0] = 0;
		cfp->FROMHOST = Insert_job_line( cfp, qn, 0, 0,__FILE__,__LINE__ );
		cfp->name_format_error = 1;
	}
	if( Use_date && cfp->DATE == 0 ){
		char qn[M_DATE];
		plp_snprintf( qn, sizeof(qn), "D%s",
			Time_str( 0, cfp->statb.st_ctime ) );
		DEBUGF(DRECV3)("Check_for_missing_files: printer '%s' adding '%s'", orig_name, qn );
		cfp->DATE = Insert_job_line( cfp, qn, 0, 0,__FILE__,__LINE__ );
	}
	DEBUGFC(DRECV3)dump_control_file("Check_for_missing_files: before QUEUENAME", cfp);
	if( (Use_queuename || Force_queuename) &&
		(cfp->QUEUENAME == 0 || cfp->QUEUENAME[0] == 0 || cfp->QUEUENAME[1] == 0) ){
		char qn[M_QUEUENAME];
		s = Force_queuename;
		if( s == 0 || *s == 0 ) s = Queue_name;
		if( s == 0 || *s == 0 ) s = Printer;
		plp_snprintf(qn, sizeof(qn)-1, "Q%s", s );
		DEBUGF(DRECV3)("Check_for_missing_files: printer '%s' adding '%s'", orig_name, qn );
		if( cfp->QUEUENAME ) cfp->QUEUENAME[0] = 0;
		cfp->QUEUENAME = Insert_job_line( cfp, qn, 0, 0,__FILE__,__LINE__ );
	}

	if( (status = Fix_data_file_info( cfp )) ) goto error;

 	jobfile = (void *)cfp->data_file_list.list;
 	datafile = (void *)data_files_list->list;
 	jobfilecount = cfp->data_file_list.count;
 	datafilecount = data_files_list->count;

	/* check to see if the received data file
	 * is in the job list.
	 * 1. sort the received data files
	 * 2. sort the job files
	 * 3. run up the lists, checking to see if there are missing or
	 *    duplicates
	 */

	DEBUGFC(DRECV3){
		logDebug( "Check_for_missing_files: before purging");
		logDebug( "  %d in control file", jobfilecount);
		for( i = 0; i < jobfilecount; ++i ){
			logDebug( "  [%d] original '%s', cfline '%s', open '%s'",
				i, jobfile[i].original, jobfile[i].cfline, jobfile[i].openname );
		}
		logDebug( "  %d arrived", datafilecount);
		for( i = 0; i < datafilecount; ++i ){
			logDebug( "  [%d] original '%s', cfline '%s', open '%s'",
				i, datafile[i].original, datafile[i].cfline, datafile[i].openname );
		}
	}

	/* sort the jobs that were sent and eliminate duplicates */
	Mergesort( datafile, datafilecount, sizeof( datafile[0] ), dfcmp );
	j = 0;
	for( i = 0; i < datafilecount; ++i ){
		datafile[i].d_flags = 0;
		if( i == 0 ){
			datafile[j++] = datafile[i];
		} else {
			jfile = datafile[i-1].original; 
			dfile = datafile[i].original;
			if( strcmp( jfile, dfile ) ){
				datafile[j++] = datafile[i];
			}
		}
	}
	datafilecount = j;

	DEBUGFC(DRECV3){
		logDebug( "Check_for_missing_files: after purging");
		logDebug( "  %d in control file", jobfilecount);
		for( i = 0; i < jobfilecount; ++i ){
			logDebug( "  [%d] original '%s', cfline '%s', open '%s'",
				i, jobfile[i].original, jobfile[i].cfline, jobfile[i].openname );
		}
		logDebug( "  %d arrived", datafilecount);
		for( i = 0; i < datafilecount; ++i ){
			logDebug( "  [%d] original '%s', cfline '%s', open '%s'",
				i, datafile[i].original, datafile[i].cfline, datafile[i].openname );
		}
	}
	for( i = 0; i < jobfilecount; ++i ){
		jobfile[i].d_flags = 0;
	}
	for( i = 0; i < jobfilecount; ++i ){
		/* skip over copies */
		jfile = jobfile[i].original;
		found = 0;
		for( j = 0; found == 0 && j < datafilecount; ++j ){
			dfile = datafile[j].original;
			DEBUGF(DRECV3)("Check_for_missing_files: jfile '%s', dfile '%s'",
					jfile, dfile );
			found = (strcmp( jfile, dfile ) == 0 );
			/* we have two files with the same name.  We need
			 * to copy the transfername and openname information
			 * to the job file
			 */
			if( found ) break;
		}
		if( found ){
			datafile[j].d_flags = 1;
			safestrncpy( datafile[j].cfline, jobfile[i].cfline );
			safestrncpy( jobfile[i].openname, datafile[j].openname );
		} else {
			DEBUGF(DRECV3)("Check_for_missing_files: missing data file %s",
				jfile);
			plp_snprintf( cfp->error, sizeof(cfp->error),
				_("missing data file '%s'"), jfile );
			status = JFAIL;
			goto error;
		}
	}
	for( j = 0; j < datafilecount; ++j ){
		if( datafile[j].d_flags == 0 ){
			DEBUGF(DRECV3)("Check_for_missing_files: extra data file %s",
				datafile[j].original);
			plp_snprintf( cfp->error, sizeof(cfp->error),
				_("extra data file '%s'"), datafile[j].original );
			status = JFAIL;
			goto error;
		}
		DEBUGF(DRECV3)("Check_for_missing_files: renaming '%s' to '%s'",
				datafile[j].openname, datafile[j].cfline+1 );
		if( rename( datafile[j].openname, datafile[j].cfline+1 ) == -1 ){
			plp_snprintf( cfp->error, sizeof( cfp->error ),
				_("cannot rename '%s' to '%s' - %s"),
				datafile[j].openname,
				datafile[j].cfline+1,
				Errormsg(errno) );
			status = JFAIL;
			goto error;
		}
	}

	/*******************
     * make sure that the user authentication information
	 * is valid for the transfer.
	 * Auth_from = 0 - none, should be none
	 * Auth_from = 1 - client, put in by higher level
	 * Auth_from = 2 - forwarded, should be some
	 *******************/

	if( (Routing_filter || Use_identifier) && cfp->IDENTIFIER == 0 ){
		if( Make_identifier( cfp ) ){
			status = JFAIL;
			goto error;
		}
		DEBUGF(DRECV3)("Check_for_missing_files: printer '%s' adding '%s'",
			orig_name, cfp->identifier+1 );
		cfp->IDENTIFIER = Insert_job_line( cfp, cfp->identifier, 0, 0,__FILE__,__LINE__ );
	}
	if( Auto_hold || Hold_all ){
		cfp->hold_info.hold_time = time( (void *)0 );
	}


	/*
	 * open a temporary working file
	 */

	temp_fd = Make_temp_fd( cfp->openname, sizeof(cfp->openname) );
	DEBUGF(DRECV3)("Check_for_missing_files: control file temp '%s'",
		cfp->openname );
	if( ftruncate( temp_fd, 0 ) ){
		plp_snprintf( cfp->error, sizeof( cfp->error),
			_("truncate of control file failed - %s"), Errormsg(errno));
		status = JFAIL;
		goto error;
	}
	/*
	 * now write out the control file
	 */
	
	DEBUGFC(DRECV3)dump_control_file("Check_for_missing_files: before writing control", cfp);

	lines = (char **)cfp->control_file_lines.list;
	DEBUGF(DRECV3)("Check_for_missing_files: control file linecount '%d'",
		cfp->control_file_lines.count );
	for(i = 0; i < cfp->control_file_lines.count; ++i ){
		/* we get the pointer to the location in file */
		char *s = lines[i];
		if( s == 0 || *s == 0 ) continue;
		Clean_meta(s);
		DEBUGF(DRECV3)("Check_for_missing_files: line[%d]='%s'", i, s );
		if( Write_fd_str( temp_fd, s ) < 0
			|| Write_fd_str( temp_fd, "\n" ) < 0  ){
			plp_snprintf( cfp->error, sizeof( cfp->error),
				_("write of '%s' failed - %s"), cfp->openname, Errormsg(errno));
			status = JFAIL;
			goto error;
		}
	}
	/*
	 * Now we do the routing
	 */
	DEBUGF(DRECV3)("Check_for_missing_files: Routing_filter '%s'", Routing_filter );
	if( Routing_filter && *Routing_filter ){
		if( lseek( temp_fd, 0, SEEK_SET ) < 0 ){
			plp_snprintf( Cfp_static->error, sizeof( Cfp_static->error),
				_("%s: lseek '%s' failed '%s'"), Printer,
				cfp->openname, Errormsg(errno) );
			status = JFAIL;
			goto error;
		}
		status =  Get_route( cfp, temp_fd, pc_entry );
		DEBUGF(DRECV3)("Check_for_missing_files: Routing_filter returned %d",
			status );
		switch(status){
			case JSUCC:
				break;
			case JHOLD:
				cfp->hold_info.hold_time = time( (void *)0 );
				status = JSUCC;
				break;
			default:
				plp_snprintf( cfp->error, sizeof( cfp->error),
					_("cannot do routing for %s"), orig_name );
				status = JFAIL;
				goto error;
		}
	}
	close(temp_fd);

error:
	DEBUGF(DRECV3)( "Check_for_missing_files: done status '%d' error '%s'",
		status, cfp->error );
	if( status ){
		DEBUGF(DRECV1)( "%s: job receive failed '%s'", Printer, cfp->error );
		Remove_files( 0 );
	} else {
		Set_job_control( cfp, hold_fd );
		setmessage( Cfp_static, "TRACE", "%s@%s: job arrived\n%s%s",
			Printer, FQDNHost,
			Copy_hf( &Cfp_static->control_file_lines,
				&Cfp_static->control_file_print, "CONTROL=", " - " ),
			Copy_hf( &Cfp_static->hold_file_lines,
				&Cfp_static->hold_file_print, "HOLDFILE=", " - " ) );
		if( rename( cfp->openname, cfp->transfername ) == -1 ){
			plp_snprintf( cfp->error, sizeof( cfp->error ),
				_("cannot rename '%s' to '%s' - %s"),
				cfp->openname, cfp->transfername, Errormsg(errno) );
			setmessage( Cfp_static, "TRACE", "%s@%s: %s",
				Printer, FQDNHost, cfp->error );
			status = JFAIL;
			goto error;
		}
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
			_("%s: rejecting forwarded job originally from '%s'"),
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
			_("%s: no permission to accept job from '%s@%s'"),
			Printer, Perm_check.user, host );
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
	/* act as though you started new */
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
		Do_queue_jobs( Server_queue_name );
	} else {
		Do_queue_jobs( Printer );
	}
	cleanup(0);
}
