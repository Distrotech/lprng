/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2002, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 * $Id: printjob.h,v 1.30 2002/05/06 01:06:43 papowell Exp $
 ***************************************************************************/



#ifndef _PRINTJOB_H_
#define _PRINTJOB_H_ 1

/* PROTOTYPES */
int Print_job( int output, int status_device, struct job *job,
	int timeout, int poll_for_status, char *user_filter );
int Run_OF_filter( int timeout, int *of_pid, int *of_stdin, int *of_stderr,
	int output, char **outbuf, int *outmax, int *outlen,
	struct job *job, char *id, int terminate_of,
	char *msgbuffer, int msglen );
void Print_banner( char *name, char *pgm, struct job *job );
int Write_outbuf_to_OF( struct job *job, char *title,
	int of_fd, char *buffer, int outlen,
	int of_error, char *msg, int msgmax,
	int timeout, int poll_for_status );
int Get_status_from_OF( struct job *job, char *title, int of_pid,
	int of_error, char *msg, int msgmax,
	int timeout, int suspend, int max_wait );
int Wait_for_pid( int of_pid, char *name, int suspend, int timeout );

#endif
