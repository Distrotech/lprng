/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: lpd_rcvjob.c,v 1.71 2004/05/03 20:24:02 papowell Exp $";


#include "lp.h"

#include "child.h"
#include "errorcodes.h"
#include "fileopen.h"
#include "gethostinfo.h"
#include "getopt.h"
#include "getqueue.h"
#include "linksupport.h"
#include "lockfile.h"
#include "permission.h"
#include "proctitle.h"

#include "lpd_remove.h"
#include "lpd_rcvjob.h"
#include "lpd_jobs.h"
/**** ENDINCLUDE ****/

/***************************************************************************
 * Commentary:
 * Patrick Powell Mon Apr 17 05:43:48 PDT 1995
 * 
 * The protocol used to send a job to a remote host consists of the
 * following:
 * 
 * Client                                   Server
 * \2printername\n - receive a job
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
 * 1. Read the control file from the other end.
 * 2. Check to see if the printer exists,  and if has a printcap entry
 * 3. If it does, check the permissions for the user.
 * 4. Read the job to the queue.
 * 
 * Control file processing
 * 1. The control file at this end might exist already,  and be in use.
 * 	If this is the case,  we will try and allocate another control
 * 	file name if the option is allowed.
 * 2. After we lock the control file,  we will then try and read the
 * 	data files.  Again,  there might be a collision.  If this is
 * 	the case,  then we will again try to generate a new number.
 * 
 * The control file is first read into a file and then read into memory,
 * where it is parsed.  
 * 
 * Duplicate Control/Data files:
 * When copying jobs over,  you might get to a point where you
 * discover that a control and/or data file already exists.
 * 
 * if file already exists:
 * 1. if the existing file length is 0, then you can clobber the file.
 *    This is reasonable given file locking is working
 *    and games are not being played with NFS file systems.
 *    Most likely you have found an abandonded file.
 * 2. If you have the control file and it is locked,
 *    then you might as well clobber the data files
 *    as they are probably left over from another job.
 *    If you do not have the control file,  then you give up
 * 3. If the first file file is the control file,
 *    and you cannot lock it or it is locked and has a non-zero length,
 *    then you should rename the file and try again.
 *    rename the data files/control files
 *    This can be done if the first file is a control file
 *    and you cannot lock it,  or you lock it and it is
 *   non-zero in length.
 * 
 * Job Size:
 *    when the total received job size exceeds limits, then abort job
 *    when the available file space falls below limit, then abort job
 * 
 ***************************************************************************/

int Receive_job( int *sock, char *input )
{
	char line[SMALLBUFFER];		/* line buffer for input */
	char error[SMALLBUFFER];	/* line buffer for input */
	char buffer[SMALLBUFFER];	/* line buffer for input */
	int errlen = sizeof(error);
	char *tempfile;				/* name of temp file */
	double file_len;			/* length of file */
	double read_len;			/* amount to read from sock */
	double jobsize = 0;			/* size of job */
	int ack = 0;				/* ack to send */
	int status = 0;				/* status of the last command */
	double len;					/* length of last read */
	char *s, *filename;			/* name of control or data file */
	int temp_fd = -1;				/* used for file opening and locking */
	int filetype;				/* type of file - control or data */
	int fd;						/* for log file */
	int hold_fd = -1;				/* hold file */
	int db, dbf, rlen;
	int fifo_fd = -1;			/* fifo lock file */
	struct line_list files, info, l;
	struct job job;
	struct stat statb;
	int discarding_large_job = 0;

	Init_line_list(&l);
	Init_line_list(&files);
	Init_line_list(&info);
	Init_job(&job);

	Name = "RECV";

	if( input && *input ) ++input;
	Clean_meta(input);
	Split(&info,input,Whitespace,0,0,0,0,0,0);

	DEBUGFC(DRECV1)Dump_line_list("Receive_job: input", &info );
	if( info.count != 1 ){
		SNPRINTF( error, errlen) _("bad command line") );
		goto error;
	}
	if( Is_clean_name( info.list[0] ) ){
		SNPRINTF( error, errlen) _("bad printer name") );
		goto error;
	}

	setproctitle( "lpd RECV '%s'", info.list[0] );

	if( Setup_printer( info.list[0], error, errlen, 0 ) ){
		if( error[0] == 0 ){
			SNPRINTF( error, errlen) _("%s: cannot set up print queue"), Printer_DYN );
		}
		goto error;
	}


	db  = Debug;
	dbf = DbgFlag;
	s = Find_str_value(&Spool_control,DEBUG);
	if(!s) s = New_debug_DYN;
	Parse_debug( s, 0 );

	if( !(DRECVMASK & DbgFlag) ){
		Debug = db;
		DbgFlag = dbf;
	} else {
		int i, j;
		i = Debug;
		j = DbgFlag;
		Debug = db;
		DbgFlag = dbf;
		if( Log_file_DYN ){
			fd = Checkwrite( Log_file_DYN, &statb,0,0,0);
			if( fd > 0 && fd != 2 ){
				dup2(fd,2);
				close(fd);
			}
		}
		Debug = i;
		DbgFlag = j;
	}

	DEBUGF(DRECV1)("Receive_job: spooling_disabled %d",
		Sp_disabled(&Spool_control) );
	if( Sp_disabled(&Spool_control) ){
		SNPRINTF( error, errlen)
			_("%s: spooling disabled"), Printer_DYN );
		ack = ACK_RETRY;	/* retry */
		goto error;
	}

	/* send an ACK */
	DEBUGF(DRECV1)("Receive_job: sending 0 ACK for job transfer request" );

	status = Link_send( ShortRemote_FQDN, sock, Send_job_rw_timeout_DYN, "", 1, 0 );
	if( status ){
		SNPRINTF( error, errlen)
			_("%s: Receive_job: sending ACK 0 failed"), Printer_DYN );
		goto error;
	}

	/* fifo order enforcement */
	if( Fifo_DYN ){
		char * path = Make_pathname( Spool_dir_DYN, Fifo_lock_file_DYN );
		path = safestrdup3( path,"." , RemoteHost_IP.fqdn, __FILE__,__LINE__ );
		DEBUG1( "Receive_job: checking fifo_lock file '%s'", path );
		fifo_fd = Checkwrite( path, &statb, O_RDWR, 1, 0 );
		if( fifo_fd < 0 ){
			Errorcode = JABORT;
			LOGERR_DIE(LOG_ERR) _("Receive_job: cannot open lockfile '%s'"),
				path ); 
		}
		if( Do_lock( fifo_fd, 1 ) < 0 ){
			Errorcode = JABORT;
			LOGERR_DIE(LOG_ERR) _("Receive_job: cannot lock lockfile '%s'"),
				path ); 
		}
		if(path) free(path); path = 0;
	}

	while( status == 0 ){
		DEBUGF(DRECV1)("Receive_job: from %s- getting file transfer line", FQDNRemote_FQDN );
		rlen = sizeof(line)-1;
		line[0] = 0;
		status = Link_line_read( ShortRemote_FQDN, sock, Send_job_rw_timeout_DYN, line, &rlen );

		DEBUGF(DRECV1)( "Receive_job: read from %s- status %d read %d bytes '%s'",
				FQDNRemote_FQDN, status, rlen, line );
#if 0
		LOGMSG(LOG_INFO) "Receive_job: read from %s- status %d read %d bytes '%s'",
				FQDNRemote_FQDN, status, rlen, line );
#endif


		if( rlen == 0 || status ){
			DEBUGF(DRECV1)( "Receive_job: ending reading from remote" );
			/* treat like normal closing of connection */
			line[0] = 0;
			status = 0;
			break;
		}
		filetype = line[0];
		Clean_meta(line+1);

		/* make sure we have a data file transfer */
		if( filetype != DATA_FILE && filetype != CONTROL_FILE ){
			/* we may have another type of command */
			status = 0;
			break;
		}
		/* make sure we have length and filename */
		filename = 0;
		file_len = strtod(line+1,&filename);
		if ((line+1) == filename){
			/* Recover from Apple Desktop Printing stupidity.
			   It occasionally resends the queue selection cmd.
			   Darian Davis DD 03JUL2000 */
			status = 0;
			LOGERR(LOG_ERR)"Recovering from incorrect job submission");
			continue;
		}
		if( filename ){
			while( isspace(cval(filename)) ) ++filename;
			Clean_meta(filename);
			s = filename;
			while( (s = strpbrk(s," \t")) ) *s++ = '_';
		}
		if( file_len < 0
			|| filename == 0 || *filename == 0
			|| (file_len == 0 && filetype != DATA_FILE) ){
			ack = ACK_STOP_Q;
			SNPRINTF( error, errlen)
			_("%s: Receive_job - bad control line '%s', len %0.0f, name '%s'"),
				Printer_DYN, line, file_len, filename );
			goto error;
		}

		/************************************************
		 * check for job size and available space
		 * This is done here so that we can neatly clean up
		 * if we need to. Note we do this after we truncate...
		 ************************************************/
		jobsize += file_len;
		read_len = file_len;
		DEBUGF(DRECV4)("Receive_job: receiving '%s' jobsize %0.0f, file_len %0.0f, read_len %0.0f",
			filename, jobsize, file_len, read_len );

		if( read_len == 0 ) read_len = Max_job_size_DYN*1024;
		if( Max_job_size_DYN > 0 && (jobsize/1024) > (0.0+Max_job_size_DYN) ){
			if( Discard_large_jobs_DYN ){
				discarding_large_job = 1;
			} else {
				SNPRINTF( error, errlen)
					_("size %0.3fK exceeds %dK"),
					jobsize/1024, Max_job_size_DYN );
				ack = ACK_RETRY;
				goto error;
			}
		} else if( !Check_space( read_len, Minfree_DYN, Spool_dir_DYN ) ){
			SNPRINTF( error, errlen)
				_("%s: insufficient file space"), Printer_DYN );
			ack = ACK_RETRY;
			goto error;
		}

		/*
		 * we are ready to read the file; send 0 ack saying so
		 */
		DEBUGF(DRECV4)("Receive_job: next, receiving '%s' jobsize %0.0f, file_len %0.0f, read_len %0.0f",
			filename, jobsize, file_len, read_len );

		DEBUGF(DRECV2)("Receive_job: sending 0 ACK to transfer '%s', length %0.0f", filename, file_len );
		status = Link_send( ShortRemote_FQDN, sock, Send_job_rw_timeout_DYN, "", 1, 0 );
		if( status ){
			SNPRINTF( error, errlen)
				_("%s: sending ACK 0 for '%s' failed"), Printer_DYN, filename );
			ack = ACK_RETRY;
			goto error;
		}

		if( discarding_large_job ){
			temp_fd = Checkwrite( "/dev/null", &statb,0,0,0);
			tempfile = 0;
		} else {
			temp_fd = Make_temp_fd(&tempfile);
		}

		/*
		 * If the file length is 0, then we transfer only as much as we have
		 * space available. Note that this will be the last file in a job
		 */

		DEBUGF(DRECV4)("Receive_job: receiving '%s' read_len %0.0f bytes, file_len %0.0f",
			filename, read_len, file_len );
		len = read_len;
		status = Link_file_read( ShortRemote_FQDN, sock,
			Send_job_rw_timeout_DYN, 0, temp_fd, &read_len, &ack );

		DEBUGF(DRECV4)("Receive_job: status %d, read_len %0.0f, file_len %0.0f",
			status, read_len, file_len );

		/* close the file */
		close(temp_fd);
		temp_fd = -1;

		if( status 
			|| (file_len == 0 && read_len == 0)
			|| (file_len != 0 && file_len != read_len) ){
			SNPRINTF( error, errlen)
				_("%s: transfer of '%s' from '%s' failed"), Printer_DYN,
				filename, ShortRemote_FQDN );
			ack = ACK_RETRY;
			goto error;
		}

		/*
		 * we process the control file and make sure we can print it
		 */

		if( filetype == CONTROL_FILE ){
			DEBUGF(DRECV2)("Receive_job: receiving new control file, old job.info.count %d, old files.count %d",
				job.info.count, files.count );
			if( hold_fd > 0 ){
				/* we received another control file, finish this job up */
				if( !discarding_large_job ){
					if( Check_for_missing_files(&job, &files, error, errlen, 0, hold_fd) ){
						goto error;
					}
				} else {
					SNPRINTF( error, errlen)
						_("size %0.3fK exceeds %dK"),
						jobsize/1024, Max_job_size_DYN );
					Set_str_value(&job.info,ERROR,error);
					Set_nz_flag_value(&job.info,ERROR_TIME,time(0));
					Set_str_value(&job.info,INCOMING_TIME,0);
					error[0] = 0;
					if( (status = Set_hold_file( &job, 0, hold_fd )) ){
						SNPRINTF( error,errlen)
							"Error setting up hold file - %s",
							Errormsg( errno ) );
						goto error;
					}
					if( Lpq_status_file_DYN ){ unlink(Lpq_status_file_DYN); }
					discarding_large_job = 0;
				}
				close( hold_fd ); hold_fd = -1;
				Free_line_list(&files);
				jobsize = 0;
			}

			Free_job(&job);
			Set_str_value(&job.info,OPENNAME,tempfile);

			hold_fd = Setup_temporary_hold_file( &job, filename, 1, 0, error, errlen );
			if( hold_fd < 0 ){
				goto error;
			}
			if( files.count ){
				/* we have datafiles, FOLLOWED by a control file */
				if( !discarding_large_job ){
					if( Check_for_missing_files(&job, &files, error, errlen, 0, hold_fd) ){
						goto error;
					}
				} else {
					SNPRINTF( error, errlen)
						_("size %0.3fK exceeds %dK"),
						jobsize/1024, Max_job_size_DYN );
					Set_str_value(&job.info,ERROR,error);
					Set_nz_flag_value(&job.info,ERROR_TIME,time(0));
					Set_str_value(&job.info,INCOMING_TIME,0);
					error[0] = 0;
					if( (status = Set_hold_file( &job, 0, hold_fd )) ){
						SNPRINTF( error,errlen)
							"Error setting up hold file - %s",
							Errormsg( errno ) );
						goto error;
					}
					if( Lpq_status_file_DYN ){ unlink(Lpq_status_file_DYN); }
					discarding_large_job = 0;
				}
				close( hold_fd ); hold_fd = -1;
				Free_line_list(&files);
				jobsize = 0;
				Free_job(&job);
			}
		} else {
			Set_casekey_str_value(&files,filename,tempfile);
		}
		DEBUGF(DRECV2)("Receive_job: sending 0 ACK transfer done" );
		status = Link_send( ShortRemote_FQDN, sock, Send_job_rw_timeout_DYN, "",1, 0 );
	}

	DEBUGF(DRECV2)("Receive_job: eof on transfer, job.info.count %d, files.count %d",
		job.info.count, files.count );
	if( hold_fd > 0 ){
		if( !discarding_large_job ){
			if( Check_for_missing_files(&job, &files, error, errlen, 0, hold_fd) ){
				goto error;
			}
		} else {
			SNPRINTF( error, errlen)
				_("size %0.3fK exceeds %dK"),
				jobsize/1024, Max_job_size_DYN );
			Set_str_value(&job.info,ERROR,error);
			Set_nz_flag_value(&job.info,ERROR_TIME,time(0));
			Set_str_value(&job.info,INCOMING_TIME,0);
			error[0] = 0;
			if( (status = Set_hold_file( &job, 0, hold_fd )) ){
				SNPRINTF( error,errlen)
					"Error setting up hold file - %s",
					Errormsg( errno ) );
				goto error;
			}
			if( Lpq_status_file_DYN ){ unlink(Lpq_status_file_DYN); }
			discarding_large_job = 0;
		}
	}

 error:

	if( hold_fd > 0 ) close(hold_fd); hold_fd = -1;
	if( temp_fd > 0 ) close(temp_fd); temp_fd = -1;
	if( fifo_fd > 0 ) close(fifo_fd); fifo_fd = -1;

	Remove_tempfiles();
	if( error[0] ){
		DEBUGF(DRECV1)("Receive_job: error, removing job" );
		DEBUGFC(DRECV4)Dump_job("Receive_job - error", &job );
		s = Find_str_value(&job.info,HF_NAME);
		if( !ISNULL(s) ) unlink(s);
		if( ack == 0 ) ack = ACK_FAIL;
		buffer[0] = ack;
		SNPRINTF(buffer+1,sizeof(buffer)-1)"%s\n",error);
		/* LOG( LOG_INFO) "Receive_job: error '%s'", error ); */
		DEBUGF(DRECV1)("Receive_job: sending ACK %d, msg '%s'", ack, error );
		(void)Link_send( ShortRemote_FQDN, sock,
			Send_job_rw_timeout_DYN, buffer, safestrlen(buffer), 0 );
		Link_close( Send_query_rw_timeout_DYN, sock );
	} else {
		Link_close( Send_query_rw_timeout_DYN, sock );
		/* update the spool queue */
		Get_spool_control( Queue_control_file_DYN, &Spool_control );
		Set_flag_value(&Spool_control,CHANGE,1);
		Set_spool_control( 0, Queue_control_file_DYN, &Spool_control );
		if( Lpq_status_file_DYN ){
			unlink( Lpq_status_file_DYN );
		}
		s = Server_queue_name_DYN;
		if( !s ) s = Printer_DYN;

		SNPRINTF( line, sizeof(line)) "%s\n", s );
		DEBUGF(DRECV1)("Receive_jobs: Lpd_request fd %d, starting '%s'", Lpd_request, line );
		if( Write_fd_str( Lpd_request, line ) < 0 ){
			LOGERR_DIE(LOG_ERR) _("Receive_jobs: write to fd '%d' failed"),
				Lpd_request );
		}
	}
	Free_line_list(&info);
	Free_line_list(&files);
	Free_job(&job);
	Free_line_list(&l);

	cleanup( 0 );
	return(0);
}

/***************************************************************************
 * Block Job Transfer
 * \RCV_BLOCKprinter size
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

int Receive_block_job( int *sock, char *input )
{
	int temp_fd = -1, fd;	/* fd for received file */
	double read_len;	/* file read length */
	char error[SMALLBUFFER];
	int errlen = sizeof(error);
	char buffer[SMALLBUFFER];
	int ack = 0, status = 0;
	double file_len;
	char *tempfile, *s;
	struct stat statb;
	struct line_list l;
	int db, dbf;
	int discarding_large_job = 0;


	error[0] = 0;
	Init_line_list(&l);

	Name = "RECVB";

	if( *input ) ++input;
	Clean_meta(input);
	Split(&l,input,Whitespace,0,0,0,0,0,0);
	DEBUGFC(DRECV1)Dump_line_list("Receive_block_job: input", &l );

	if( l.count != 2 ){
		SNPRINTF( error, errlen-4) _("bad command line") );
		goto error;
	}
	if( Is_clean_name( l.list[0] ) ){
		SNPRINTF( error, errlen-4) _("bad printer name") );
		goto error;
	}
	setproctitle( "lpd RECVB '%s'", l.list[0] );

	if( Setup_printer( l.list[0], error, errlen-4, 0 ) ){
		if( error[0] == 0 ){
			SNPRINTF( error, errlen-4) _("%s: cannot set up printer"), Printer_DYN );
		}
		goto error;
	}


	db = Debug;
	dbf =DbgFlag;
	s = Find_str_value(&Spool_control,DEBUG);
	if(!s) s = New_debug_DYN;
	Parse_debug( s, 0 );

	if( !(DRECVMASK & DbgFlag) ){
		Debug = db;
		DbgFlag = dbf;
	} else {
		dbf = Debug;
		Debug = db;
		if( Log_file_DYN ){
			fd = Checkwrite( Log_file_DYN, &statb,0,0,0);
			if( fd > 0 && fd != 2 ){
				dup2(fd,2);
				close(fd);
			}
		}
		Debug = dbf;
	}

#ifndef NODEBUG 
	DEBUGF(DRECV1)("Receive_block_job: debug '%s', Debug %d, DbgFlag 0x%x", s, Debug, DbgFlag );
#endif


	DEBUGF(DRECV1)("Receive_block_job: spooling_disabled %d", Sp_disabled(&Spool_control) );
	if( Sp_disabled(&Spool_control) ){
		SNPRINTF( error, errlen-4)
			_("%s: spooling disabled"), Printer_DYN );
		ack = ACK_RETRY;	/* retry */
		goto error;
	}

	/* check for space */

	file_len  = strtod( l.list[1], 0 );
	read_len = file_len;

	if( Max_job_size_DYN > 0 && (read_len+1023)/1024 > Max_job_size_DYN ){
		if( Discard_large_jobs_DYN ){
			discarding_large_job = 1;
		} else {
			SNPRINTF( error, errlen)
				_("size %0.3fK exceeds %dK"),
				read_len/1024, Max_job_size_DYN );
			ack = ACK_RETRY;
			goto error;
		}
	} else if( !Check_space( read_len, Minfree_DYN, Spool_dir_DYN ) ){
		SNPRINTF( error, errlen-4)
			_("%s: insufficient file space"), Printer_DYN );
		ack = ACK_RETRY;
		goto error;
	}

	/*
	 * we are ready to read the file; send 0 ack saying so
	 */

	DEBUGF(DRECV1)("Receive_block_job: sending 0 ACK for job transfer request" );

	status = Link_send( ShortRemote_FQDN, sock, Send_job_rw_timeout_DYN, "",1, 0 );
	if( status ){
		SNPRINTF( error, errlen-4)
			_("%s: Receive_block_job: sending ACK 0 failed"), Printer_DYN );
		goto error;
	}

	temp_fd = Make_temp_fd( &tempfile );
	DEBUGF(DRECV4)("Receive_block_job: receiving '%s' %0.0f bytes ", tempfile, file_len );
	status = Link_file_read( ShortRemote_FQDN, sock,
		Send_job_rw_timeout_DYN, 0, temp_fd, &read_len, &ack );
	DEBUGF(DRECV4)("Receive_block_job: received %d bytes ", read_len );
	if( status ){
		SNPRINTF( error, errlen-4)
			_("%s: transfer of '%s' from '%s' failed"), Printer_DYN,
			tempfile, ShortRemote_FQDN );
		ack = ACK_FAIL;
		goto error;
	}

	/* extract jobs */

	if( lseek( temp_fd, 0, SEEK_SET ) == -1 ){
		SNPRINTF( error, errlen-4)	
			_("Receive_block_job: lseek failed '%s'"), Errormsg(errno) );
		ack = ACK_FAIL;
		goto error;
	}

	if( Scan_block_file( temp_fd, error, errlen-4, 0 ) ){
		ack = ACK_FAIL;
		goto error;
	}

	close( temp_fd );
	temp_fd = -1;

	DEBUGF(DRECV2)("Receive_block_job: sending 0 ACK" );
	status = Link_send( ShortRemote_FQDN, sock, Send_job_rw_timeout_DYN, "",1, 0 );
	if( status ){
		SNPRINTF( error, errlen-4)
			_("%s: sending ACK 0 for '%s' failed"), Printer_DYN, tempfile );
		ack = ACK_RETRY;
		goto error;
	}
	error[0] = 0;

 error:
	Free_line_list(&l);
	if( temp_fd > 0 ){
		close(temp_fd );
	}
	if( error[0] ){
		if( ack != 0 ) ack = ACK_FAIL;
		buffer[0] = ack;
		SNPRINTF(buffer+1,sizeof(buffer)-1)"%s\n",error);
		/* LOG( LOG_INFO) "Receive_block_job: error '%s'", error ); */
		DEBUGF(DRECV1)("Receive_block_job: sending ACK %d, msg '%s'", ack, error );
		(void)Link_send( ShortRemote_FQDN, sock,
			Send_job_rw_timeout_DYN, buffer, safestrlen(buffer), 0 );
		Link_close( Send_query_rw_timeout_DYN, sock );
	} else {
		Link_close( Send_query_rw_timeout_DYN, sock );
		Remove_tempfiles();

		s = Server_queue_name_DYN;
		if( !s ) s = Printer_DYN;

		SNPRINTF( buffer, sizeof(buffer)) "%s\n", s );
		DEBUGF(DRECV1)("Receive_block_jobs: Lpd_request fd %d, starting '%s'", Lpd_request, buffer );
		if( Write_fd_str( Lpd_request, buffer ) < 0 ){
			LOGERR_DIE(LOG_ERR) _("Receive_block_jobs: write to fd '%d' failed"),
				Lpd_request );
		}
	}
	return( error[0] != 0 );
}


/***************************************************************************
 * Scan_block_file( int fd, struct control_file *cfp )
 *  we scan the block file, getting the various portions
 *
 * Generate the compressed data files - this has the format
 *    \3count cfname\n
 *    [count control file bytes]
 *    \4count dfname\n
 *    [count data file bytes]
 *
 *  We extract the various sections and find the offsets.
 *  Note that the various name fields will be the original
 *  values;  the ones we actually use will be the transfer values
 * RETURNS: nonzero on error, error set
 *          0 on success
 ***************************************************************************/

int Scan_block_file( int fd, char *error, int errlen, struct line_list *header_info )
{
	char line[LINEBUFFER];
	char buffer[LARGEBUFFER];
	int startpos;
	int read_len, filetype, tempfd = -1;		/* type and length fields */
	char *filename;				/* name field */
	char *tempfile;				/* name field */
	int status;
	int len, count, n;
	int hold_fd = -1;
	struct line_list l, info, files;
	struct job job;
	struct stat statb;
	int discarding_large_job = 0;
	double jobsize = 0;

	if( fstat( fd, &statb) < 0 ){
		Errorcode = JABORT;
		LOGERR_DIE(LOG_INFO)"Scan_block_file: fstat failed");
	}
	DEBUGF(DRECV2)("Scan_block_file: starting, file size '%0.0f'",
		(double)(statb.st_size) );
	Init_line_list(&l);
	Init_line_list(&info);
	Init_line_list(&files);
	Init_job(&job);

	/* first we find the file position */
	
	startpos = lseek( fd, 0, SEEK_CUR );
	DEBUGF(DRECV2)("Scan_block_file: starting at %d", startpos );
	error[0] = 0;
	while( (status = Read_one_line( Send_job_rw_timeout_DYN, fd, line, sizeof(line) )) > 0 ){
		/* the next position is the start of data */
		Free_line_list(&l);
		Free_line_list(&info);
		startpos = lseek( fd, 0, SEEK_CUR );
		if( startpos == -1 ){
			SNPRINTF( error, errlen)	
				_("Scan_block_file: lseek failed '%s'"), Errormsg(errno) );
			status = 1;
			goto error;
		}
		DEBUGF(DRECV2)("Scan_block_file: '%s', end position %d",
			line, startpos );
		filetype = line[0];
		if( filetype != CONTROL_FILE && filetype != DATA_FILE ){
			/* get the next line */
			continue;
		}
		Clean_meta(line+1);
		Split(&info,line+1,Whitespace,0,0,0,0,0,0);
		if( info.count != 2 ){
			SNPRINTF( error, errlen)
			_("bad length information '%s'"), line+1 );
			status = 1;
			goto error;
		}
		DEBUGFC(DRECV2)Dump_line_list("Scan_block_file- input", &info );
		read_len = atoi( info.list[0] );
		filename = info.list[1];

		jobsize += read_len;
		if( Max_job_size_DYN > 0 && (jobsize/1024) > (0.0+Max_job_size_DYN) ){
			if( Discard_large_jobs_DYN ){
				SNPRINTF( error, errlen)
					_("size %0.3fK exceeds %dK"),
					jobsize/1024, Max_job_size_DYN );
				discarding_large_job = 1;
			}
		}
		if( discarding_large_job ){
			tempfd = Checkwrite( "/dev/null", &statb,0,0,0);
			tempfile = 0;
		} else {
			tempfd = Make_temp_fd( &tempfile );
		}
		DEBUGF(DRECV2)("Scan_block_file: tempfd %d, read_len %d", read_len, tempfd );
		for( len = read_len; len > 0; len -= count ){
			n = sizeof(buffer);
			if( n > len ) n = len;
			count = Read_fd_len_timeout(Send_job_rw_timeout_DYN,fd,buffer,n);
			DEBUGF(DRECV2)("Scan_block_file: len %d, reading %d, got count %d",
				len, n, count );
			if( count < 0 ){
				SNPRINTF( error, errlen)	
					_("Scan_block_file: read failed '%s'"), Errormsg(errno) );
				status = 1;
				goto error;
			} else if( count == 0 ){
				SNPRINTF( error, errlen)	
					_("Scan_block_file: read unexecpted EOF") );
				status = 1;
				goto error;
			}
			n = write(tempfd,buffer,count);
			if( n != count ){
				SNPRINTF( error, errlen)	
					_("Scan_block_file: lseek failed '%s'"), Errormsg(errno) );
				status = 1;
				goto error;
			}
		}
		close( tempfd);
		tempfd = -1;

		if( filetype == CONTROL_FILE ){
			DEBUGF(DRECV2)("Scan_block_file: receiving new control file, old job.info.count %d, old files.count %d",
				job.info.count, files.count );
			if( job.info.count ){
				/* we received another control file, finish this job up */
				if( discarding_large_job ){
					SNPRINTF( error, errlen)
						_("size %0.3fK exceeds %dK"),
						jobsize/1024, Max_job_size_DYN );
					Set_str_value(&job.info,ERROR,error);
					Set_str_value(&job.info,INCOMING_TIME,0);
					error[0] = 0;
					if( (status = Set_hold_file( &job, 0, hold_fd )) ){
						SNPRINTF( error,errlen)
							"Error setting up hold file - %s",
							Errormsg( errno ) );
						goto error;
					}
					if( Lpq_status_file_DYN ){ unlink(Lpq_status_file_DYN); }
				} else {
					if( Check_for_missing_files(&job, &files, error, errlen, 0, hold_fd) ){
						goto error;
					}
					Set_str_value(&job.info,INCOMING_TIME,0);
				}
				close( hold_fd ); hold_fd = -1;
				Free_line_list(&files);
				jobsize = 0;
			}
			Free_job(&job);
			Set_str_value(&job.info,OPENNAME,tempfile);

			hold_fd = Setup_temporary_hold_file( &job, filename, 1, 0, error, errlen );
			if( hold_fd < 0 ){
				goto error;
			}
			if( files.count ){
				/* we have datafiles, FOLLOWED by a control file,
					followed (possibly) by another control file */
				/* we receive another control file */
				if( discarding_large_job ){
					SNPRINTF( error, errlen)
						_("size %0.3fK exceeds %dK"),
						jobsize/1024, Max_job_size_DYN );
					Set_str_value(&job.info,ERROR,error);
					Set_nz_flag_value(&job.info,ERROR_TIME,time(0));
					Set_str_value(&job.info,INCOMING_TIME,0);
					error[0] = 0;
					if( (status = Set_hold_file( &job, 0, hold_fd )) ){
						SNPRINTF( error,errlen)
							"Error setting up hold file - %s",
							Errormsg( errno ) );
						goto error;
					}
					if( Lpq_status_file_DYN ){ unlink(Lpq_status_file_DYN); }
				} else {
					if( Check_for_missing_files(&job, &files, error, errlen, 0, hold_fd) ){
						goto error;
					}
					Set_str_value(&job.info,INCOMING_TIME,0);
				}
				close( hold_fd ); hold_fd = -1;
				Free_line_list(&files);
				jobsize = 0;
				Free_job(&job);
			}
		} else {
			Set_str_value(&files,filename,tempfile);
		}
	}

	if( files.count ){
		if( discarding_large_job ){
			SNPRINTF( error, errlen)
				_("size %0.3fK exceeds %dK"),
				jobsize/1024, Max_job_size_DYN );
			Set_str_value(&job.info,ERROR,error);
			Set_nz_flag_value(&job.info,ERROR_TIME,time(0));
			Set_str_value(&job.info,INCOMING_TIME,0);
			error[0] = 0;
			if( (status = Set_hold_file( &job, 0, hold_fd )) ){
				SNPRINTF( error,errlen)
					"Error setting up hold file - %s",
					Errormsg( errno ) );
				goto error;
			}
			if( Lpq_status_file_DYN ){ unlink(Lpq_status_file_DYN); }
		} else {
			if( Check_for_missing_files(&job, &files, error, errlen, 0, hold_fd) ){
				goto error;
			}
			Set_str_value(&job.info,INCOMING_TIME,0);
		}
	}

 error:
	if( error[0] ){
		Remove_tempfiles();
		Remove_job( &job );
	}
	Set_str_value(&job.info,INCOMING_TIME,0);
	if( hold_fd > 0 ) close(hold_fd); hold_fd = -1;
	if( tempfd > 0 ) close(tempfd); tempfd = -1;
	Free_line_list(&l);
	Free_line_list(&info);
	Free_line_list(&files);
	Free_job(&job);
	return( status );
}

/***************************************************************************
 * static int read_one_line(int fd, char *buffer, int maxlen );
 *  reads one line (terminated by \n) into the buffer
 *RETURNS:  0 if EOF characters read
 *          n = # chars read
 *          Note: buffer terminated by 0
 ***************************************************************************/
int Read_one_line( int timeout, int fd, char *buffer, int maxlen )
{
	int len, status;
	len = status = 0;

	while( len < maxlen-1 && (status = Read_fd_len_timeout( timeout, fd, &buffer[len], 1)) > 0 ){
		if( buffer[len] == '\n' ){
			break;
		}
		++len;
	}
	buffer[len] = 0;
	return( status );
}

int Check_space( double jobsize, int min_space, char *pathname )
{
	double space = Space_avail(pathname);
	int ok;

	jobsize = ((jobsize+1023)/1024);
	ok = ((jobsize + min_space) < space);

	DEBUGF(DRECV1)("Check_space: path '%s', space %0.0f Bytes, jobsize %0.0fK, ok %d",
		pathname, space, jobsize, ok );

	return( ok );
}

int Do_perm_check( struct job *job, char *error, int errlen )
{
	int permission = 0;			/* permission */
	char *s;

	DEBUGFC(DRECV1)Dump_job("Do_perm_check", job );
	Perm_check.service = 'R';
	Perm_check.printer = Printer_DYN;
	s = Find_str_value(&job->info,LOGNAME);
	Perm_check.user = s;
	Perm_check.remoteuser = s;
	Perm_check.host = 0;
	s = Find_str_value(&job->info,FROMHOST);
	if( s && Find_fqdn( &PermHost_IP, s ) ){
		Perm_check.host = &PermHost_IP;
	}
	Perm_check.remotehost = &RemoteHost_IP;

	/* check for permission */

	if( Perm_filters_line_list.count ){
		Free_line_list(&Perm_line_list);
		Merge_line_list(&Perm_line_list,&RawPerm_line_list,0,0,0);
		Filterprintcap( &Perm_line_list, &Perm_filters_line_list, "");
	}

	if( (permission = Perms_check( &Perm_line_list, &Perm_check, job, 1 ))
			== P_REJECT ){
		SNPRINTF( error, errlen)
			_("%s: no permission to print"), Printer_DYN );
	}
	Perm_check.user = 0;
	Perm_check.remoteuser = 0;
	DEBUGF(DRECV1)("Do_perm_check: permission '%s'", perm_str(permission) );
	return( permission );
}

/*
 * Process the list of control and data files, and make a job from them
 *  job - the job structure
 *  files - list of files that we received and need to check.
 *    if this is a copy of a job from another queue, files == 0
 *  spool_control - the spool control values for this queue
 *  spool_dir - the spool directory for this queue
 *  xlate_incoming_format - only valid for received files
 *  error, errlen - the error message information
 *  header_info - authentication ID to put in the job
 *   - if 0, do not update, this preserves copy
 *  returns: 0 - successful
 *          != 0 - error
 */

int Check_for_missing_files( struct job *job, struct line_list *files,
	char *error, int errlen, struct line_list *header_info, int holdfile_fd )
{
	int count, fd, i, status = 0, copies;
	struct line_list *lp = 0, datafiles;
	char *openname, *transfername;
	double jobsize;
	struct stat statb;
	struct timeval start_time;
	char *fromhost, *file_hostname, *number, *priority, *cf;

	Init_line_list(&datafiles);

	if( gettimeofday( &start_time, 0 ) ){
		Errorcode = JABORT;
		LOGERR_DIE(LOG_INFO) "Check_for_missing_files: gettimeofday failed");
	}

	DEBUG1("Check_for_missing_files: start time 0x%x usec 0x%x",
		(int)start_time.tv_sec, (int)start_time.tv_usec );
	if(DEBUGL1)Dump_job("Check_for_missing_files - start", job );
	if(DEBUGL1)Dump_line_list("Check_for_missing_files- files", files );
	if(DEBUGL1)Dump_line_list("Check_for_missing_files- header_info", header_info );

	Set_flag_value(&job->info,JOB_TIME,(int)start_time.tv_sec);
	Set_flag_value(&job->info,JOB_TIME_USEC,(int)start_time.tv_usec);

	/* we can get this as a new job or as a copy.
	 * if we get a copy,  we do not need to check this stuff
	 */
	if( !Find_str_value(&job->info,REMOTEHOST) ){
		Set_str_value(&job->info,REMOTEHOST,RemoteHost_IP.fqdn);
		Set_flag_value(&job->info,UNIXSOCKET,Perm_check.unix_socket);
		Set_flag_value(&job->info,REMOTEPORT,Perm_check.port);
	}

	if( header_info && User_is_authuser_DYN ){
		char *s = Find_str_value(header_info,AUTHUSER);
		if( !ISNULL(s) ){
			Set_str_value( &job->info,LOGNAME,s);
			DEBUG1("Check_for_missing_files: setting user to authuser '%s'", s );
		}
	}

	if(DEBUGL3) Dump_job( "Check_for_missing_files: before fixing", job );

	/* deal with authentication */

	Make_identifier( job );

	if( !(fromhost = Find_str_value(&job->info,FROMHOST)) || Is_clean_name(fromhost) ){
		Set_str_value(&job->info,FROMHOST,FQDNRemote_FQDN);
		fromhost = Find_str_value(&job->info,FROMHOST);
	}


	if( header_info ){
		char *authfrom, *authuser, *authtype, *authca;
		authfrom = Find_str_value(header_info,AUTHFROM);
		authuser = Find_str_value(header_info,AUTHUSER);
		authtype = Find_str_value(header_info,AUTHTYPE);
		authca = Find_str_value(header_info,AUTHCA);
		if( ISNULL(authuser) ) authuser = authfrom;
		Set_str_value(&job->info,AUTHUSER,authuser);
		Set_str_value(&job->info,AUTHFROM,authfrom);
		Set_str_value(&job->info,AUTHTYPE,authtype);
		Set_str_value(&job->info,AUTHCA,authca);
	}

	/*
	 * check permissions now that you have set up all the information
	 */

	if( Do_perm_check( job, error, errlen ) == P_REJECT ){
		status = 1;
		goto error;
	}


	/*
	 * accounting name fixup
	 * change the accounting name ('R' field in control file)
	 * based on hostname
	 *  hostname(,hostname)*=($K|value)*(;hostname(,hostname)*=($K|value)*)*
	 *  we have a semicolon separated list of entrires
	 *  the RemoteHost_IP is compared to these.  If a match is found then the
	 *    user name (if any) is used for the accounting name.
	 *  The user name list has the format:
	 *  ${K}  - value from control file or printcap entry - you must use the
	 *          ${K} format.
	 *  username  - user name value to substitute.
	 */
	if( Accounting_namefixup_DYN ){
		struct line_list l, listv;
		char *accounting_name = 0;
		Init_line_list(&l);
		Init_line_list(&listv);

		DEBUG1("Check_for_missing_files: Accounting_namefixup '%s'", Accounting_namefixup_DYN );
		Split(&listv,Accounting_namefixup_DYN,";",0,0,0,0,0,0);
		for( i = 0; i < listv.count; ++i ){
			char *s, *t;
			int j;
			s = listv.list[i];
			if( (t = safestrpbrk(s,"=")) ) *t++ = 0;
			Free_line_list(&l);
			DEBUG1("Check_for_missing_files: hostlist '%s'", s );
			Split(&l,s,",",0,0,0,0,0,0);
			if( Match_ipaddr_value(&l,&RemoteHost_IP) == 0 ){
				Free_line_list(&l);
				DEBUG1("Check_for_missing_files: match, using users '%s'", t );
				Split(&l,t,",",0,0,0,0,0,0);
				if(DEBUGL1)Dump_line_list("Check_for_missing_files: before Fix_dollars", &l );
				Fix_dollars(&l,job,0,0);
				if(DEBUGL1)Dump_line_list("Check_for_missing_files: after Fix_dollars", &l );
				for( j = 0; j < l.count; ++j ){
					if( !ISNULL(l.list[j]) ){
						accounting_name = l.list[j];
						break;
					}
				}
				break;
			}
		}
		DEBUG1("Check_for_missing_files: accounting_name '%s'", accounting_name );
		if( !ISNULL(accounting_name) ){
			Set_str_value(&job->info,ACCNTNAME,accounting_name );
		}
		Free_line_list(&l);
		Free_line_list(&listv);
	}

	if( Force_IPADDR_hostname_DYN ){
		char buffer[SMALLBUFFER];
		const char *const_s;
		int family;
		/* We will need to create a dummy record. - no host */
		family = RemoteHost_IP.h_addrtype;
		const_s = (char *)inet_ntop( family, RemoteHost_IP.h_addr_list.list[0],
			buffer, sizeof(buffer) );
		DEBUG1("Check_for_missing_files: remotehost '%s'", const_s );
		Set_str_value(&job->info,FROMHOST,const_s);
		fromhost = Find_str_value(&job->info,FROMHOST);
	}
	{
		char *s, *t;
		if( Force_FQDN_hostname_DYN && !safestrchr(fromhost,'.')
			&& (t = safestrchr(FQDNRemote_FQDN,'.')) ){
			s = safestrdup2(fromhost, t, __FILE__,__LINE__ );
			Set_str_value(&job->info,FROMHOST,s);
			if( s ) free(s); s = 0;
			fromhost = Find_str_value(&job->info,FROMHOST);
		}
	}

	if( !Find_str_value(&job->info,DATE) ){
		char *s = Time_str(0,0);
		Set_str_value(&job->info,DATE,s);
	}
	if( (Use_queuename_DYN || Force_queuename_DYN)
		&& !Find_str_value(&job->info,QUEUENAME) ){
		char *s = Force_queuename_DYN;
		if( s == 0 ) s = Queue_name_DYN;
		if( s == 0 ) s = Printer_DYN;
		Set_str_value(&job->info,QUEUENAME,s);
		Set_DYN(&Queue_name_DYN,s);
	}
	if( Hld_all(&Spool_control) || Auto_hold_DYN ){
		Set_flag_value( &job->info,HOLD_TIME,time((void *)0) );
	} else {
		Set_flag_value( &job->info,HOLD_TIME,0);
	}

	number = Find_str_value( &job->info,NUMBER);

	file_hostname = Find_str_value(&job->info,FROMHOST);
	if( ISNULL(file_hostname) ) file_hostname = FQDNRemote_FQDN;
	if( ISNULL(file_hostname) ) file_hostname = FQDNHost_FQDN;

	if( isdigit(cval(file_hostname)) ){
		char * s = safestrdup2("ADDR",file_hostname,__FILE__,__LINE__);
		Set_str_value(&job->info,FILE_HOSTNAME,s);
		if( s ) free(s); s = 0;
	} else {
		Set_str_value(&job->info,FILE_HOSTNAME,file_hostname);
	}
	file_hostname = Find_str_value(&job->info,FILE_HOSTNAME);

	/* fix Z options */
	Fix_Z_opts( job );
	/* fix control file name */

	{
		jobsize = 0;
		/* RedHat Linux 6.1 - sends a control file with NO data files */
		if( job->datafiles.count == 0 && files->count > 0 ){
			Check_max(&job->datafiles,files->count+1);
			for( count = 0; count < files->count; ++count  ){
				lp = malloc_or_die(sizeof(lp[0]),__FILE__,__LINE__);
				memset(lp,0,sizeof(lp[0]));
				job->datafiles.list[job->datafiles.count++] = (void *)lp;
				/*
				 * now we add the information needed
				 *  the files list has 'transfername=openname'
				 */
				transfername = files->list[count];
				if( (openname = strchr(transfername,'=')) ) *openname++ = 0;
				Set_str_value(lp,OTRANSFERNAME,transfername);
				Set_str_value(lp,FORMAT,"f");
				Set_flag_value(lp,COPIES,1);
				if( openname ) openname[-1] = '=';
			}
			if(DEBUGL1)Dump_job("RedHat Linux fix", job );
		}
		for( count = 0; count < job->datafiles.count; ++count ){
			double size;
			lp = (void *)job->datafiles.list[count];
			transfername = Find_str_value(lp,OTRANSFERNAME);
			/* find the open name and replace it in the information */
			if( (openname = Find_casekey_str_value(files,transfername,Hash_value_sep)) ){
				Set_str_value(lp,OPENNAME,openname);
				Set_casekey_str_value(&datafiles,transfername,openname);
			} else {
				SNPRINTF(error,errlen)"missing data file '%s'",transfername);
				status = 1;
				goto error;
			}
			if( (status = stat( openname, &statb )) ){
					SNPRINTF( error, errlen) "stat() '%s' error - %s",
					openname, Errormsg(errno) );
				goto error;
			}
			copies = Find_flag_value(lp,COPIES);
			if( copies == 0 ) copies = 1;
			size = statb.st_size;
			Set_double_value(lp,SIZE,size);
			jobsize += copies * size;
		}
		Set_double_value(&job->info,SIZE,jobsize);

		if(DEBUGL1)Dump_line_list("Check_for_missing_files- found", &datafiles );
		if( files->count != datafiles.count ){
			SNPRINTF(error,errlen)"too many data files");
			status = 1;
			goto error;
		}
		Free_line_list(&datafiles);
		Fix_datafile_infox( job, number,
			file_hostname, Xlate_incoming_format_DYN, 1 );
	}

	Set_str_value(&job->info,HPFORMAT,0);
	Set_str_value(&job->info,INCOMING_TIME,0);

	/* now rename the data files */
	status = 0;
	for( count = 0; status == 0 && count < job->datafiles.count; ++count ){
		lp = (void *)job->datafiles.list[count];
		openname = Find_str_value(lp,OPENNAME);
		if( stat(openname,&statb) ) continue;
		transfername = Find_str_value(lp,DFTRANSFERNAME);
		DEBUG1("Check_for_missing_files: renaming '%s' to '%s'",
			openname, transfername );
		if( (status = rename(openname,transfername)) ){
			SNPRINTF( error,errlen)
				"error renaming '%s' to '%s' - %s",
				openname, transfername, Errormsg( errno ) );
		}
	}
	if( status ) goto error;


	Generate_control_file( job );
	if( Routing_filter_DYN || Incoming_control_filter_DYN ){
		if( (status = Get_route( job, error, errlen )) ){
			DEBUG1("Check_for_missing_files: Routing_filter error '%s'", error );
			goto error;
		}
		Generate_control_file( job );
	}

	if( (status = Set_hold_file( job, 0, holdfile_fd )) ){
		SNPRINTF( error,errlen)
			"error setting up hold file - %s",
			Errormsg( errno ) );
		goto error;
	}
	if(DEBUGL1)Dump_job("Check_for_missing_files - ending", job );

 error:
	transfername = Find_str_value(&job->info,CFTRANSFERNAME);
	if( status ){
		LOGMSG(LOG_INFO) "Check_for_missing_files: FAILED '%s' %s",
			transfername?transfername:"???", error);
		/* we need to unlink the data files */
		openname = Find_str_value(&job->info,OPENNAME);
		if( openname ) unlink( openname );
		for( count = 0; count < job->datafiles.count; ++count ){
			lp = (void *)job->datafiles.list[count];
			transfername = Find_str_value(lp,DFTRANSFERNAME);
			openname = Find_str_value(lp,OPENNAME);
			if( transfername ) unlink(transfername);
			if( openname ) unlink(openname);
		}
		openname = Find_str_value(&job->info,HF_NAME);
		if( openname ) unlink(openname);
	} else {
		/*
		LOGMSG(LOG_INFO) "Check_for_missing_files: SUCCESS '%s'", transfername);
		*/
		setmessage( job, "STATE", "CREATE" );
	}

	Free_line_list(&datafiles);
	return( status );
}

/***************************************************************************
 * int Setup_temporary_hold_file( struct job *job,
 *	char *error, int errlen )
 *  sets up a hold file and control file
 ***************************************************************************/

int Setup_temporary_hold_file( struct job *job, char *filename,
	int read_control_file,
	char *cf_file_image,
	char *error, int errlen  )
{
	int fd = -1;
	/* now we need to assign a control file number */
	DEBUG1("Setup_temporary_hold_file: starting, filename %s, read_control_file %d, cf_file_image %s",
		filename, read_control_file, cf_file_image );

	/* job id collision resolution */
	Check_format( CONTROL_FILE, filename, job );

	if( (fd = Find_non_colliding_job_number( job )) < 0 ){
		SNPRINTF(error,errlen)
			"Maximum number of jobs in queue. Delete some and retry");
		goto error;
	}

	/* set up the control file information */
	Set_hf_from_cf_info( job, cf_file_image, read_control_file );

	Make_identifier( job );
	Check_for_hold( job, &Spool_control );

	DEBUG1("Setup_temporary_hold_file: hold file fd '%d'", fd );
	
	/* mark as incoming */
	Set_flag_value(&job->info,INCOMING_TIME,time((void *)0) );
	/* write status */
	if( Set_hold_file( job, 0, fd ) ){
		SNPRINTF( error,errlen)
			"error setting up hold file - %s",
			Errormsg( errno ) );
		close(fd); fd = -1;
		goto error;
	}
 error:
	return( fd );
}

/***************************************************************************
 * int Find_non_colliding_job_number( struct job *job )
 *  Find a non-colliding job number for the new job
 * RETURNS: 0 if successful
 *          ack value if unsuccessful
 * Side effects: sets up control file fields;
 ***************************************************************************/

int Find_non_colliding_job_number( struct job *job )
{
	int hold_fd = -1;			/* job hold file fd */
	struct stat statb;			/* for status */
	char hold_file[SMALLBUFFER], *number;
	int max, n, start;

	/* we set the job number to a reasonable range */
	hold_fd = -1;
	number = Fix_job_number(job,0);
	start = n = strtol(number,0,10);
	max = 1000;
	if( Long_number_DYN ) max = 1000000;
	while( hold_fd < 0 ){
		number = Fix_job_number(job,n);
		SNPRINTF(hold_file,sizeof(hold_file))"hfA%s",number);
		DEBUGF(DRECV1)("Find_non_colliding_job_number: trying %s", hold_file );
		hold_fd = Checkwrite(hold_file, &statb,
			O_RDWR|O_CREAT, 0, 0 );
		/* if the hold file locked or is non-zero, we skip to a new one */
		if( hold_fd < 0 || Do_lock( hold_fd, 0 ) < 0 || statb.st_size ){
			close( hold_fd );
			hold_fd = -1;
			hold_file[0] = 0;
			++n;
			if( n > max ) n = 0;
			if( n == start ){
				break;
			}
		} else {
			Set_str_value(&job->info,HF_NAME,hold_file);
		}
	}
	DEBUGF(DRECV1)("Find_non_colliding_job_number: fd %d", hold_file );
	return( hold_fd );
}

int Get_route( struct job *job, char *error, int errlen )
{
	int i, fd, tempfd, count, c;
	char *tempfile, *openname, *s, *t, *id;
	char buffer[SMALLBUFFER];
	int errorcode = 0;
	struct line_list info, dest, env, *lp, cf_line_list;

	DEBUG1("Get_route: routing filter '%s', control filter '%s'",
		Routing_filter_DYN, Incoming_control_filter_DYN );

	Init_line_list(&info);
	Init_line_list(&dest);
	Init_line_list(&env);
	Init_line_list(&cf_line_list);
	tempfd = -1;
	tempfile = 0;

	/* build up the list of files and initialize the DATAFILES
	 * environment variable
	 */

	s = 0;
	for( i = 0; i < job->datafiles.count; ++i ){
		lp = (void *)job->datafiles.list[i];
		openname = Find_str_value(lp,DFTRANSFERNAME);
		s = safestrdup3( s,openname," ",__FILE__,__LINE__);
	}
	Set_str_value(&env,DATAFILES,s);
	if( s ) free(s); s = 0;

	openname = Find_str_value(&job->info,OPENNAME);
	if( (fd = open(openname,O_RDONLY,0)) < 0 ){
		SNPRINTF(error,errlen)"Get_route: open '%s' failed '%s'",
			openname, Errormsg(errno) );
		errorcode = 1;
		goto error;
	}
	Max_open(fd);

	/* we pass the control file through the incoming control filter */
	if( Incoming_control_filter_DYN ){
		tempfd = Make_temp_fd(&tempfile);
		DEBUG1("Get_route: running '%s'",
			Incoming_control_filter_DYN );
		errorcode = Filter_file( Send_job_rw_timeout_DYN, fd, tempfd,
			"INCOMING_CONTROL_FILTER",
			Incoming_control_filter_DYN, Filter_options_DYN, job, &env, 0);
		switch(errorcode){
			case 0: break;
			case JHOLD:
				Set_flag_value(&job->info,HOLD_TIME,time((void *)0) );
				errorcode = 0;
				break;
			case JREMOVE:
				goto error;
				break;
			default:
				SNPRINTF(error,errlen)"Get_route: incoming control filter '%s' failed '%s'",
					Incoming_control_filter_DYN, Server_status(errorcode) );
				errorcode = JFAIL;
				goto error;
		}
		close(fd); close(tempfd); fd = -1; tempfd = -1;
		if( rename( tempfile, openname ) == -1 ){
			SNPRINTF(error,errlen)"Get_route: rename '%s' to '%s' failed - %s",
				tempfile, openname, Errormsg(errno) );
			errorcode = 1;
			goto error;
		}
		if( (fd = open(openname,O_RDONLY,0)) < 0 ){
			SNPRINTF(error,errlen)"Get_route: open '%s' failed '%s'",
				openname, Errormsg(errno) );
			errorcode = 1;
			goto error;
		}
		Max_open(fd);
		if( Get_file_image_and_split(openname,0,0, &cf_line_list, Line_ends,0,0,0,0,0,0) ){
            SNPRINTF(error,errlen)
                "Get_route: open failed - modified control file  %s - %s", openname, Errormsg(errno) );
			goto error;
		}
		for( i = 0; i < cf_line_list.count; ++i ){
			s = cf_line_list.list[i];
			/* check for blank line */
			c = cval(s);
			if( !c ) break;
			DEBUG3("Get_route: doing CF line '%s'", s );
			if( isupper(c) && c != 'U' && c != 'N' ){
				char *t;
				buffer[0] = c; buffer[1] = 0;
				t = Find_str_value(&job->info,buffer);
				if( safestrcmp(s+1,t) ){
					DEBUG3("Get_route: update control '%s'='%s'", buffer, s+1 );
					Set_str_value(&job->info,buffer,s+1);
				}
			}
		}
		for( ; i < cf_line_list.count; ++i ){
			char *t;
			s = cf_line_list.list[i];
			DEBUG3("Get_route: doing option line '%s'", s );
			/* check for blank line */
			t = safestrchr( s, '=');
			if( t ){
				*t = 0;
				DEBUG3("Get_route: update control '%s'='%s'", s, t+1 );
				Set_str_value(&job->info,s,t+1);
				*t = '=';
			}
		}
	}

	if( Routing_filter_DYN == 0 ) goto error;

	tempfd = Make_temp_fd(&tempfile);
	errorcode = Filter_file( Send_query_rw_timeout_DYN, fd, tempfd, "ROUTING_FILTER",
		Routing_filter_DYN, Filter_options_DYN, job, &env, 0);
	if(errorcode)switch(errorcode){
		case 0: break;
		case JHOLD:
			Set_flag_value(&job->info,HOLD_TIME,time((void *)0) );
			errorcode = 0;
			break;
		case JREMOVE:
			goto error;
			break;
		default:
			SNPRINTF(error,errlen)"Get_route: incoming control filter '%s' failed '%s'",
				Incoming_control_filter_DYN, Server_status(errorcode) );
			errorcode = JFAIL;
			goto error;
	}
	if( tempfd > 0 ) close(tempfd); tempfd = -1;

	Free_line_list( &env );
	Get_file_image_and_split(tempfile,0,1,&env,Line_ends,0,0,0,1,0,0);
	DEBUGFC(DRECV1)Dump_line_list("Get_route: information", &env );
	Free_line_list(&job->destination);

	id = Find_str_value(&job->info,IDENTIFIER);
	if(!id){
		FATAL(LOG_ERR)
			_("Get_route: no identifier for '%s'"),
			Find_str_value(&job->info,HF_NAME) );
	}
	count = 0;
	for(i = 0; i < env.count; ++i ){
		s = env.list[i];
		DEBUG1("Get_route: line '%s'", s );
		if( safestrcasecmp(END,s) ){
			if( isupper(cval(s)) ){
				buffer[0] = cval(s); buffer[1] = 0;
				Set_str_value(&job->destination,buffer,s+1);
			} else if( (t = safestrpbrk(s,Hash_value_sep))
					|| (t = safestrpbrk(s,Whitespace)) ){
				*t++ = 0;
				while( isspace(cval(t)) ) ++t;
				Set_str_value(&job->destination,s,t);
			} else {
				Set_str_value(&job->destination,s,"");
			}
		} else {
			DEBUGFC(DRECV1)Dump_line_list("Get_route: dest", &job->destination );
			if( (s = Find_str_value(&job->destination, DEST)) ){
				int n;
				DEBUG1("Get_route: destination '%s'", s );
				Set_flag_value(&job->destination,DESTINATION,count);
				n = Find_flag_value(&job->destination,COPIES);
				if( n < 0 ){
					Set_flag_value(&job->destination,COPIES,0);
				}
				SNPRINTF(buffer,sizeof(buffer))".%d",count+1);
				s = safestrdup2(id,buffer,__FILE__,__LINE__);
				Set_str_value(&job->destination,IDENTIFIER,s);
				if(s) free(s);
				Update_destination(job);
				++count;
			}
			Free_line_list(&job->destination);
		}
	}
	DEBUGFC(DRECV1)Dump_line_list("Get_route: dest", &job->destination );
	if( (s = Find_str_value(&job->destination, DEST)) ){
		int n;
		DEBUG1("Get_route: destination '%s'", s );
		Set_flag_value(&job->destination,DESTINATION,count);
		n = Find_flag_value(&job->destination,COPIES);
		if( n < 0 ){
			Set_flag_value(&job->destination,COPIES,0);
		}
		SNPRINTF(buffer,sizeof(buffer))".%d",count+1);
		s = safestrdup2(id,buffer,__FILE__,__LINE__);
		Set_str_value(&job->destination,IDENTIFIER,s);
		if(s) free(s);
		Update_destination(job);
		++count;
	}
	Free_line_list(&job->destination);
	Set_flag_value(&job->info,DESTINATIONS,count);
	if(DEBUGL1)Dump_job("Get_route: final", job );

 error:
	if( tempfile ) unlink(tempfile);
	Free_line_list(&info);
	Free_line_list(&dest);
	Free_line_list(&env);
	Free_line_list(&cf_line_list);
	return( errorcode );
}


/*
 * Generate_control_file:
 *  Generate a control file from hold file contents
 *  Use the X= lines and the DATAFILES entry
 */

void Generate_control_file( struct job *job )
{
	/* generate the control file */
	struct stat statb;
	int i, fd = -1;
	char *cf = 0;
	char *datalines, *openname, *transfername;

	char *priority = Find_str_value(&job->info,PRIORITY);
	char *number = Find_str_value( &job->info,NUMBER);
	char *file_hostname = Find_str_value(&job->info,FILE_HOSTNAME);
	char *s = safestrdup4("cf",priority,number,file_hostname,__FILE__,__LINE__);

	Set_str_value(&job->info,CFTRANSFERNAME,s);
	if(s) free(s); s = 0;
	datalines = Find_str_value(&job->info,DATAFILES);
	openname = Find_str_value(&job->info,OPENNAME); 
	transfername = Find_str_value(&job->info,CFTRANSFERNAME);

	for( i = 0; i < job->info.count; ++i ){
		char *t = job->info.list[i];
		int c;
		if( t && (c = t[0]) && isupper(c) && c != 'N' && c != 'U' 
			&& t[1] == '=' ){
			t[1] = 0;
			cf = safeextend4(cf,t,t+2,"\n",__FILE__,__LINE__);
			t[1] = '=';
		}
	}

	DEBUG4("Generate_control_file: cf start '%s'", cf );
	cf = safeextend2(cf,datalines,__FILE__,__LINE__);
	DEBUG4("Generate_control_file: cf final '%s'", cf );
	Set_str_value(&job->info,CF_OUT_IMAGE,cf);
	if( cf ) free(cf); cf = 0;
}
