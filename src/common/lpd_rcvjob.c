/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: lpd_rcvjob.c,v 5.5 1999/10/23 03:20:23 papowell Exp papowell $";


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

#include "lpd.h"
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
	int db, dbf, rlen;
	struct line_list files, info, l;
	struct job job;
	struct stat statb;

	Init_line_list(&l);
	Init_line_list(&files);
	Init_line_list(&info);
	Init_job(&job);

	Name = "RECV";

	if( input && *input ) ++input;
	Clean_meta(input);
	Split(&info,input,Whitespace,0,0,0,0,0);

	DEBUGFC(DRECV1)Dump_line_list("Receive_job: input", &info );
	if( info.count != 1 ){
		plp_snprintf( error, errlen, _("bad command line") );
		goto error;
	}
	if( Clean_name( info.list[0] ) ){
		plp_snprintf( error, errlen, _("bad printer name") );
		goto error;
	}

	setproctitle( "lpd RECV '%s'", info.list[0] );

	if( Setup_printer( info.list[0], error, errlen ) ){
		plp_snprintf( error, errlen, _("%s: no spool queue"), Printer_DYN );
		goto error;
	}


	db = Debug;
	dbf =DbgFlag;
	s = Find_str_value(&Spool_control,DEBUG,Value_sep);
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
#ifndef NODEBUG 
	DEBUGF(DRECV1)("Receive_job: debug '%s', Debug %d, DbgFlag 0x%x", s, Debug, DbgFlag );
#endif


	DEBUGF(DRECV1)("Receive_job: spooling_disabled %d",
		Sp_disabled(&Spool_control) );
	if( Sp_disabled(&Spool_control) ){
		plp_snprintf( error, errlen,
			_("%s: spooling disabled"), Printer_DYN );
		ack = ACK_RETRY;	/* retry */
		goto error;
	}

	/* send an ACK */
	DEBUGF(DRECV1)("Receive_job: sending 0 ACK for job transfer request" );

	status = Link_send( ShortRemote_FQDN, sock, Send_job_rw_timeout_DYN, "", 1, 0 );
	if( status ){
		plp_snprintf( error, errlen,
			_("%s: Receive_job: sending ACK 0 failed"), Printer_DYN );
		goto error;
	}

	while( status == 0 ){
		DEBUGF(DRECV1)("Receive_job: from %s- getting file transfer line", FQDNRemote_FQDN );
		rlen = sizeof(line)-1;
		line[0] = 0;
		status = Link_line_read( ShortRemote_FQDN, sock, Send_job_rw_timeout_DYN, line, &rlen );

		DEBUGF(DRECV1)( "Receive_job: read from %s- status %d read %d bytes '%s'",
				FQDNRemote_FQDN, status, rlen, line );
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
		if( filename ){
			while( isspace(cval(filename)) ) ++filename;
			s = filename;
			while( ( s = strpbrk(s," \t"))) *s++ = '_';
		}
		if( file_len < 0
			|| filename == 0 || *filename == 0
			|| (file_len == 0 && filetype != DATA_FILE) ){
			ack = ACK_STOP_Q;
			plp_snprintf( error, errlen,
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

		if( read_len == 0 ) read_len = Max_job_size_DYN*1024;
		if( Max_job_size_DYN > 0 && (read_len+1023)/1024 > Max_job_size_DYN ){
			plp_snprintf( error, errlen,
				_("%s: job size %0.0f is larger than %d K"),
				Printer_DYN, jobsize, Max_job_size_DYN );
			ack = ACK_RETRY;
			goto error;
		} else if( !Check_space( read_len, Minfree_DYN, Spool_dir_DYN ) ){
			plp_snprintf( error, errlen,
				_("%s: insufficient file space"), Printer_DYN );
			ack = ACK_RETRY;
			goto error;
		}

		/*
		 * we are ready to read the file; send 0 ack saying so
		 */

		DEBUGF(DRECV2)("Receive_job: sending 0 ACK to transfer '%s'", filename );
		status = Link_send( ShortRemote_FQDN, sock, Send_job_rw_timeout_DYN, "", 1, 0 );
		if( status ){
			plp_snprintf( error, errlen,
				_("%s: sending ACK 0 for '%s' failed"), Printer_DYN, filename );
			ack = ACK_RETRY;
			goto error;
		}

		temp_fd = Make_temp_fd(&tempfile);

		/*
		 * If the file length is 0, then we transfer only as much as we have
		 * space available. Note that this will be the last file in a job
		 */

		DEBUGF(DRECV4)("Receive_job: receiving '%s' %d bytes ", filename, read_len );
		len = read_len;
		status = Link_file_read( ShortRemote_FQDN, sock,
			Send_job_rw_timeout_DYN, 0, temp_fd, &read_len, &ack );

		DEBUGF(DRECV4)("Receive_job: received %d bytes ", read_len );

		/* close the file */
		close(temp_fd);
		temp_fd = -1;

		if( read_len == 0 || (file_len == 0 && read_len == len)
			|| status ){
			plp_snprintf( error, errlen,
				_("%s: transfer of '%s' from '%s' failed"), Printer_DYN,
				filename, ShortRemote_FQDN );
			ack = ACK_RETRY;
			goto error;
		}
		/*
		 * parse and check the control file for permissions
		 */

		if( filetype == CONTROL_FILE ){
			DEBUGF(DRECV2)("Receive_job: received control file, job.info.count %d, files.count %d",
				job.info.count, files.count );
			if( job.info.count ){
				/* we receive another control file */
				if( Check_for_missing_files(&job, &files, error, sizeof(error)) ){
					goto error;
				}
				Free_line_list(&files);
				jobsize = 0;
				Free_job(&job);
			}
			Free_line_list(&l);
			Get_file_image_and_split(0,tempfile,0,1, &l,Line_ends,0,0,0,0,0);
			Setup_job( &job, &Spool_control, Spool_dir_DYN, filename, 0, &l);
			Free_line_list(&l);
			Set_str_value(&job.info,OPENNAME,tempfile);
			Set_str_value(&job.info,ERROR,0);
			if( Do_perm_check( &job, error, errlen ) == P_REJECT ){
				goto error;
			}
			if( files.count ){
				/* we have datafiles, FOLLOWED by a control file,
					followed (possibly) by another control file */
				/* we receive another control file */
				if( Check_for_missing_files(&job, &files, error, sizeof(error)) ){
					goto error;
				}
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

	if( job.info.count ){
		if( Check_for_missing_files(&job, &files, error, sizeof(error)) ){
			goto error;
		}
		Free_line_list(&files);
		jobsize = 0;
		Free_job(&job);
	}

 error:
	Free_line_list(&info);
	Free_line_list(&files);
	Free_job(&job);
	Free_line_list(&l);

	if( temp_fd > 0 ) close(temp_fd); temp_fd = -1;

	if( error[0] ){
		if( ack != 0 ) ack = ACK_FAIL;
		buffer[0] = ack;
		plp_snprintf(buffer+1,sizeof(buffer)-1,"%s\n",error);
		/* log( LOG_INFO, "Receive_job: error '%s'", error ); */
		DEBUGF(DRECV1)("Receive_job: sending ACK %d, msg '%s'", ack, error );
		(void)Link_send( ShortRemote_FQDN, sock,
			Send_job_rw_timeout_DYN, buffer, strlen(buffer), 0 );
		Link_close( sock );
	} else {
		Link_close( sock );
		Remove_tempfiles();
		if( Server_queue_name_DYN ){
			Do_queue_jobs( Server_queue_name_DYN, 0, 0 );
		} else {
			Do_queue_jobs( Printer_DYN, 0, 0 );
		}
	}
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


	error[0] = 0;
	Init_line_list(&l);

	Name = "RECVB";

	if( *input ) ++input;
	Clean_meta(input);
	Split(&l,input,Whitespace,0,0,0,0,0);
	DEBUGFC(DRECV1)Dump_line_list("Receive_block_job: input", &l );

	if( l.count != 2 ){
		plp_snprintf( error, errlen-4, _("bad command line") );
		goto error;
	}
	if( Clean_name( l.list[0] ) ){
		plp_snprintf( error, errlen-4, _("bad printer name") );
		goto error;
	}
	setproctitle( "lpd RECVB '%s'", l.list[0] );

	if( Setup_printer( l.list[0], error, errlen-4 ) ){
		plp_snprintf( error, errlen-4, _("%s: no spool queue"), Printer_DYN );
		goto error;
	}


	db = Debug;
	dbf =DbgFlag;
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
		plp_snprintf( error, errlen-4,
			_("%s: spooling disabled"), Printer_DYN );
		ack = ACK_RETRY;	/* retry */
		goto error;
	}

	/* check for space */

	file_len  = strtod( l.list[1], 0 );
	read_len = file_len;

	if( Max_job_size_DYN > 0 && (read_len+1023)/1024 > Max_job_size_DYN ){
		plp_snprintf( error, errlen,
			_("%s: job size %0.0f is larger than %dK"),
			Printer_DYN, file_len, Max_job_size_DYN );
		ack = ACK_RETRY;
		goto error;
	} else if( !Check_space( read_len, Minfree_DYN, Spool_dir_DYN ) ){
		plp_snprintf( error, errlen-4,
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
		plp_snprintf( error, errlen-4,
			_("%s: Receive_block_job: sending ACK 0 failed"), Printer_DYN );
		goto error;
	}

	temp_fd = Make_temp_fd( &tempfile );
	DEBUGF(DRECV4)("Receive_block_job: receiving '%s' %0.0f bytes ", tempfile, file_len );
	status = Link_file_read( ShortRemote_FQDN, sock,
		Send_job_rw_timeout_DYN, 0, temp_fd, &read_len, &ack );
	DEBUGF(DRECV4)("Receive_block_job: received %d bytes ", read_len );
	if( status ){
		plp_snprintf( error, errlen-4,
			_("%s: transfer of '%s' from '%s' failed"), Printer_DYN,
			tempfile, ShortRemote_FQDN );
		ack = ACK_FAIL;
		goto error;
	}

	/* extract jobs */

	if( lseek( temp_fd, 0, SEEK_SET ) < 0 ){
		plp_snprintf( error, errlen-4,	
			_("Receive_block_job: lseek failed '%s'"), Errormsg(errno) );
		ack = ACK_FAIL;
		goto error;
	}

	if( Scan_block_file( temp_fd, error, errlen-4 ) ){
		ack = ACK_FAIL;
		goto error;
	}

	close( temp_fd );
	temp_fd = -1;

	DEBUGF(DRECV2)("Receive_block_job: sending 0 ACK" );
	status = Link_send( ShortRemote_FQDN, sock, Send_job_rw_timeout_DYN, "",1, 0 );
	if( status ){
		plp_snprintf( error, errlen-4,
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
		plp_snprintf(buffer+1,sizeof(buffer)-1,"%s\n",error);
		/* log( LOG_INFO, "Receive_block_job: error '%s'", error ); */
		DEBUGF(DRECV1)("Receive_block_job: sending ACK %d, msg '%s'", ack, error );
		(void)Link_send( ShortRemote_FQDN, sock,
			Send_job_rw_timeout_DYN, buffer, strlen(buffer), 0 );
		Link_close( sock );
	} else {
		Link_close( sock );
		Remove_tempfiles();
		if( Server_queue_name_DYN ){
			Do_queue_jobs( Server_queue_name_DYN, 0, 0 );
		} else {
			Do_queue_jobs( Printer_DYN, 0, 0 );
		}
	}
	return( error[0] != 0 );
}


/***************************************************************************
 * Scan_block_file( int fd, struct control_file *cfp )
 *  we scan the block file, getting the various portions
 *
 * Generate the compressed data files - this has the format
 *    \3cfname count\n
 *    [count control file bytes]
 *    \4dfname count\n
 *    [count data file bytes]
 *
 *  We extract the various sections and find the offsets.
 *  Note that the various name fields will be the original
 *  values;  the ones we actually use will be the transfer values
 * RETURNS: nonzero on error, error set
 *          0 on success
 ***************************************************************************/

int Scan_block_file( int fd, char *error, int errlen )
{
	char line[LINEBUFFER];
	char buffer[LARGEBUFFER];
	int startpos;
	int read_len, filetype, tempfd = -1;		/* type and length fields */
	char *filename;				/* name field */
	char *tempfile;				/* name field */
	int status;
	int len, count, n;
	struct line_list l, info, files;
	struct job job;
	struct stat statb;

	if( fstat( fd, &statb) < 0 ){
		Errorcode = JABORT;
		logerr_die( LOG_INFO,"Scan_block_file: fstat failed");
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
	while( (status = Read_one_line( fd, line, sizeof(line) )) > 0 ){
		/* the next position is the start of data */
		Free_line_list(&l);
		Free_line_list(&info);
		startpos = lseek( fd, 0, SEEK_CUR );
		if( startpos == -1 ){
			plp_snprintf( error, errlen,	
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
		Split(&info,line+1,Whitespace,0,0,0,0,0);
		if( info.count != 2 ){
			plp_snprintf( error, errlen,
			_("bad length information '%s'"), line+1 );
			status = 1;
			goto error;
		}
		DEBUGFC(DRECV2)Dump_line_list("Scan_block_file- input", &info );
		read_len = atoi( info.list[0] );
		filename = info.list[1];
		tempfd = Make_temp_fd( &tempfile );
		DEBUGF(DRECV2)("Scan_block_file: tempfd %d, read_len %d", read_len, tempfd );
		for( len = read_len; len > 0; len -= count ){
			n = sizeof(buffer);
			if( n > len ) n = len;
			count = read(fd,buffer,n);
			DEBUGF(DRECV2)("Scan_block_file: len %d, reading %d, got count %d",
				len, n, count );
			if( count < 0 ){
				plp_snprintf( error, errlen,	
					_("Scan_block_file: read failed '%s'"), Errormsg(errno) );
				status = 1;
				goto error;
			} else if( count == 0 ){
				plp_snprintf( error, errlen,	
					_("Scan_block_file: read unexecpted EOF") );
				status = 1;
				goto error;
			}
			n = write(tempfd,buffer,count);
			if( n != count ){
				plp_snprintf( error, errlen,	
					_("Scan_block_file: lseek failed '%s'"), Errormsg(errno) );
				status = 1;
				goto error;
			}
		}
		close( tempfd);
		tempfd = -1;

		if( filetype == CONTROL_FILE ){
			if( job.datafiles.count ){
				if( Check_for_missing_files(&job, &files, error, sizeof(error)) ){
					goto error;
				}
				Free_line_list(&files);
				Free_job(&job);
			}
			Free_job(&job);
			Free_line_list(&l);
			Get_file_image_and_split(0,tempfile,0,1, &l,Line_ends,0,0,0,1,0);
			DEBUGF(DRECV2)("Scan_block_file- control file '%s'", filename );
			DEBUGFC(DRECV2)Dump_line_list("Scan_block_file- control file", &l );
			Setup_job( &job, &Spool_control, Spool_dir_DYN, filename, 0, &l);
			Free_line_list(&l);
			Set_str_value(&job.info,OPENNAME,tempfile);
			Set_str_value(&job.info,ERROR,0);
			if( Do_perm_check( &job, error, errlen ) == P_REJECT ){
				goto error;
			}
		} else {
			Set_str_value(&files,filename,tempfile);
		}
	}

	if( job.datafiles.count ){
		if( Check_for_missing_files(&job, &files, error, sizeof(error)) ){
			goto error;
		}
		Free_line_list(&files);
		Free_job(&job);
	}

 error:
	if( tempfd >= 0 ) close(tempfd);
	tempfd = -1;
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
int Read_one_line( int fd, char *buffer, int maxlen )
{
	int len, status;
	len = status = 0;

	while( len < maxlen-1 && (status = read( fd, &buffer[len], 1)) > 0 ){
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

	DEBUGF(DRECV1)("Check_space: path '%s', space %0.0f, jobsize %0.0fK, ok %d",
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
	s = Find_str_value(&job->info,LOGNAME,Value_sep);
	Perm_check.user = s;
	Perm_check.remoteuser = s;
	Perm_check.host = 0;
	s = Find_str_value(&job->info,FROMHOST,Value_sep);
	if( s && Find_fqdn( &PermHost_IP, s ) ){
		Perm_check.host = &PermHost_IP;
	}

	/* check for permission */

	if( (permission = Perms_check( &Perm_line_list, &Perm_check, job, 0 ))
			== P_REJECT ){
		plp_snprintf( error, errlen,
			_("%s: no permission to print"), Printer_DYN );
	}
	Perm_check.user = 0;
	Perm_check.remoteuser = 0;
	DEBUGF(DRECV1)("Perm_check: permission '%s'", perm_str(permission) );
	return( permission );
}

/*
 * Process the list of control and data files, and make a job from them
 */

int Check_for_missing_files( struct job *job, struct line_list *files,
	char *error, int errlen )
{
	int count;
	struct line_list *lp, datafiles;
	int fd = -1, status = 0;
	char *openname, *transfername;
	plp_block_mask oblock;
	double jobsize;
	int copies;
	struct stat statb;
	struct timeval start_time;

	if( gettimeofday( &start_time, 0 ) ){
		Errorcode = JABORT;
		logerr_die( LOG_INFO, "Receive_job: gettimeofday failed");
	}
	DEBUGF(DRECV1)("Check_for_missing_files: time 0x%x usec 0x%x",
		(int)start_time.tv_sec, (int)start_time.tv_usec );
	Set_flag_value(&job->info,JOB_TIME,(int)start_time.tv_sec);
	Set_flag_value(&job->info,JOB_TIME_USEC,(int)start_time.tv_usec);

	Init_line_list(&datafiles);
	DEBUGFC(DRECV1)Dump_job("Check_for_missing_files - starting", job );
	DEBUGFC(DRECV1)Dump_line_list("Check_for_missing_files - files", files );
	jobsize = 0;
	error[0] = 0;
	for( count = 0; count < job->datafiles.count; ++count ){
		lp = (void *)job->datafiles.list[count];
		transfername = Find_str_value(lp,TRANSFERNAME,Value_sep);
		/* find the open name and replace it in the information */
		if( (openname = Find_casekey_str_value(files,transfername,Value_sep)) ){
			Set_str_value(lp,OPENNAME,openname);
			Set_casekey_str_value(&datafiles,transfername,openname);
		} else {
			plp_snprintf(error,errlen,"missing data file '%s'",transfername);
			goto error;
		}
		if( (status = stat( openname, &statb )) ){
			logerr(LOG_INFO,"Check_for_missing_files: error stat '%s'",
				openname );
		}
		copies = Find_flag_value(lp,COPIES,Value_sep);
		if( copies == 0 ) copies = 1;
		jobsize += copies * statb.st_size;
	}
	Set_double_value(&job->info,SIZE,jobsize);

	DEBUGFC(DRECV1)Dump_line_list("Check_for_missing_files- found", &datafiles );
	if( files->count != datafiles.count ){
		plp_snprintf(error,errlen,"too many data files");
		goto error;
	}
	Free_line_list(&datafiles);

	/* now we need to assign a control file number */
	if( (fd = Find_non_colliding_job_number( job, Spool_dir_DYN )) < 0 ){
		plp_snprintf(error,errlen,
			"Check_for_missing_files: cannot allocate hold file");
		goto error;
	}

	error[0] = 0;
	if( Create_control( job, error, errlen ) ){
		DEBUGF(DRECV1)("Check_for_missing_files: Create_control error '%s'", error );
		goto error;
	}

	error[0] = 0;
	if( Routing_filter_DYN && (status = Get_route( job, error, errlen)) ){
		DEBUGF(DRECV1)("Check_for_missing_files: Routing_filter error '%s'", error );
		goto error;
	}
	error[0] = 0;

	for( count = 0; count < job->datafiles.count; ++count ){
		lp = (void *)job->datafiles.list[count];
		transfername = Find_str_value(lp,TRANSFERNAME,Value_sep);
		openname = Find_str_value(lp,OPENNAME,Value_sep);
		Set_str_value(&datafiles,transfername,openname);
	}

	/* now we rename the files - after we carefully block signals */
	DEBUGFC(DRECV1)Dump_line_list("Check_for_missing_files: renaming files",
		&datafiles);
	status = 0;
	plp_block_all_signals( &oblock ); /**/
	for( count = 0; status == 0 && count < datafiles.count; ++count ){
		transfername = datafiles.list[count];
		if( (openname = safestrpbrk(transfername,Value_sep)) ){
			*openname++ = 0;
		}
		DEBUGF(DRECV1)("Check_for_missing_files: renaming '%s' to '%s'",
			openname, transfername );
		status = rename(openname,transfername);
		if( status ){
			logerr(LOG_INFO,"Check_for_missing_files: error renaming '%s' to '%s'",
				openname, transfername );
		}
	}
	if( status == 0 ){
		openname = safestrdup( Find_str_value(&job->info,OPENNAME,Value_sep),
			__FILE__,__LINE__);
		Set_str_value(&job->info,OPENNAME,0);
		if( (status = Set_hold_file( job, 0 )) ){
			logerr(LOG_INFO,"Check_for_missing_files: error setting up hold file" );
		}
		Set_str_value(&job->info,OPENNAME,openname);
		if( openname ) free(openname); openname = 0;
	}
	if( status == 0 ){
		openname = Find_str_value(&job->info,OPENNAME,Value_sep);
		transfername = Find_str_value(&job->info,TRANSFERNAME,Value_sep);
		DEBUGF(DRECV1)("Check_for_missing_files: renaming '%s' to '%s'",
			openname, transfername );
		status = rename(openname,transfername);
		if( status ){
			logerr(LOG_INFO,"Check_for_missing_files: error renaming '%s' to '%s'",
				openname, transfername );
		}
	}
	DEBUGFC(DRECV1)Dump_job("Check_for_missing_files - ending", job );
	if( status ){
		/* we need to unlink the data files */
		openname = Find_str_value(&datafiles,OPENNAME,Value_sep);
		transfername = Find_str_value(&datafiles,TRANSFERNAME,Value_sep);
		if( openname ) unlink( openname );
		if( transfername) unlink( transfername );
		for( count = 0; count < job->datafiles.count; ++count ){
			lp = (void *)job->datafiles.list[count];
			openname = Find_str_value(lp,OPENNAME,Value_sep);
			transfername = Find_str_value(lp,TRANSFERNAME,Value_sep);
			if( openname ) unlink( openname );
			if( transfername ) unlink( transfername );
		}
		openname = Find_str_value(&job->info,HF_NAME,Value_sep);
		if( openname ) unlink(openname);
	}
	plp_set_signal_mask( &oblock, 0 ); /**/

	if( status == 0 ){
		setmessage( job, "STATE", "CREATE" );
	}

 error:
	if( fd >= 0 ) close(fd);
	Free_line_list(&datafiles);
	return( error[0] != 0 );
}

/***************************************************************************
 * int Find_non_colliding_job_number( struct job *job )
 *  Find a non-colliding job number for the new job
 * RETURNS: 0 if successful
 *          ack value if unsuccessful
 * Side effects: sets up control file fields;
 ***************************************************************************/

int Find_non_colliding_job_number( struct job *job, char *dpath )
{
	int hold_fd = -1;			/* job hold file fd */
	struct stat statb;			/* for status */
	char hold_file[SMALLBUFFER], *pathname = 0, *number;
	int max, n, start;

	/* we set the job number to a reasonable range */
	hold_fd = -1;
	number = Fix_job_number(job,0);
	start = n = strtol(number,0,10);
	max = 1000;
	if( Long_number_DYN ) max = 1000000;
	while( hold_fd < 0 ){
		number = Fix_job_number(job,n);
		plp_snprintf(hold_file,sizeof(hold_file),"hfA%s",number);
		DEBUGF(DRECV1)("Find_non_colliding_job_number: trying %s",
			hold_file );
		pathname = Make_pathname( dpath, hold_file );
		
		hold_fd = Checkwrite(pathname, &statb,
			O_RDWR|O_CREAT, 0, 0 );
		/* if the hold file locked or is non-zero, we skip to a new one */
		if( hold_fd < 0 || Do_lock( hold_fd, 0 ) <= 0 || statb.st_size ){
			close( hold_fd );
			hold_fd = -1;
			if( pathname ) free(pathname); pathname = 0;
			hold_file[0] = 0;
			++n;
			if( n > max ) n = 0;
			if( n == start ){
				break;
			}
		} else {
			Set_str_value(&job->info,HF_NAME,pathname);
		}
	}
	DEBUGF(DRECV1)("Find_non_colliding_job_number: using %s", pathname );
	if( pathname ) free(pathname); pathname = 0;
	return( hold_fd );
}

int Get_route( struct job *job, char *error, int errlen )
{
	plp_status_t status;
	int i, fd, tempfd, out[2], pid, len, n, count;
	char *tempfile, *openname, *s, *t, *id;
	char buffer[SMALLBUFFER];
	int errorcode = 0;
	struct line_list l, info, dest, files;

	DEBUGF(DRECV1)("Get_route: using %s", Routing_filter_DYN );
	Init_line_list(&l);
	Init_line_list(&info);
	Init_line_list(&dest);
	Init_line_list(&files);
	openname = Find_str_value(&job->info,OPENNAME,Value_sep);
	if( (fd = open(openname,O_RDONLY,0)) < 0 ){
		plp_snprintf(error,errlen,"Get_route: open '%s' failed '%s'",
			openname, Errormsg(errno) );
		errorcode = 1;
		goto error;
	}
	if( pipe(out) == -1 ){
		plp_snprintf(error,errlen,"Get_route: pipe failed '%s'",
			Errormsg(errno) );
		errorcode = 1;
		goto error;
	}

	tempfd = Make_temp_fd(&tempfile);

	Free_line_list(&files);
	Check_max(&files,10);
	files.list[files.count++] = Cast_int_to_voidstar(fd);
	files.list[files.count++] = Cast_int_to_voidstar(tempfd);
	files.list[files.count++] = Cast_int_to_voidstar(out[1]);
	pid = Make_passthrough( Routing_filter_DYN, Filter_options_DYN, &files,job, 0);
	files.count = 0;
	Free_line_list(&files);

	if( pid < 0 ){
		plp_snprintf(error,errlen,"Get_route: filter creation failed");
		errorcode = 1;
		goto error;
	}
	close(out[1]);
	close(tempfd);
	len = 0;
	buffer[0] = 0;
	while( len < sizeof(buffer)
		&& (n = read(out[0], buffer+len, sizeof(buffer)-len-1)) > 0 ){
		buffer[len+n] = 0;
		while( (s = safestrchr(buffer,'\n')) ){
			*s++ = 0;
			DEBUGF(DRECV1)("Get_route: filter error '%s'", buffer );
			memmove(buffer,s,strlen(s)+1);
		}
		len = strlen(buffer);
	}
	if( len ){
		DEBUGF(DRECV1)("Get_route: filter error '%s'", buffer );
	}
	close( out[0] );
	
	while( (n = plp_waitpid(pid,&status,0)) != pid );
	if( WIFEXITED(status) && (n = WEXITSTATUS(status)) ){
		if( n == JHOLD ){
			Set_flag_value(&job->info,HOLD_TIME,time((void *)0) );
		} else {
			errorcode = 1;
			plp_snprintf(error,errlen,
			"Get_route: control filter process exit status %d",
				n);
			goto error;
		}
	} else if( WIFSIGNALED(status) ){
		errorcode = 1;
		plp_snprintf(error,errlen,
		"Get_route: control filter process died with signal %d, '%s'",
			n, Sigstr(n));
	}
	Get_file_image_and_split(0,tempfile,0,1,&l,Line_ends,0,0,0,1,0);
	Free_line_list(&job->destination);
	id = Find_str_value(&job->info,IDENTIFIER,Value_sep);
	if(!id) id = Find_str_value(&job->info,TRANSFERNAME,Value_sep);
	count = 0;
	for(i = 0; i < l.count; ++i ){
		s = l.list[i];
		if( safestrcasecmp(END,s) ){
			if( !isupper(cval(s))
				&& (t = safestrpbrk(s,Value_sep)) ){
				*t = '=';
			}
			Add_line_list(&job->destination,s,Value_sep,1,1);
		} else {
			if( (s = Find_str_value(&job->destination, DEST,Value_sep)) ){
				DEBUGF(DRECV1)("Get_route: destination '%s'", s );
				Set_flag_value(&job->destination,DESTINATION,count);
				n = Find_flag_value(&job->destination,COPIES,Value_sep);
				if( n < 0 ){
					Set_flag_value(&job->destination,COPIES,0);
				}
				plp_snprintf(buffer,sizeof(buffer),".%d",count+1);
				s = safestrdup2(id,buffer,__FILE__,__LINE__);
				Set_str_value(&job->destination,IDENTIFIER,s);
				if(s) free(s);
				Update_destination(job);
				++count;
			}
			Free_line_list(&job->destination);
		}
	}
	if( (s = Find_str_value(&job->destination, DEST,Value_sep)) ){
		DEBUGF(DRECV1)("Get_route: destination '%s'", s );
		Set_flag_value(&job->destination,DESTINATION,count);
		n = Find_flag_value(&job->destination,COPIES,Value_sep);
		if( n < 0 ){
			Set_flag_value(&job->destination,COPIES,0);
		}
		plp_snprintf(buffer,sizeof(buffer),".%d",count+1);
		s = safestrdup2(id,buffer,__FILE__,__LINE__);
		Set_str_value(&job->destination,IDENTIFIER,s);
		if(s) free(s);
		Update_destination(job);
		++count;
	}
	Free_line_list(&job->destination);
	Set_flag_value(&job->info,DESTINATIONS,count);
	DEBUGFC(DRECV1)Dump_job("Get_route: final", job );

 error:
	Free_line_list(&l);
	Free_line_list(&info);
	return( errorcode );
}
