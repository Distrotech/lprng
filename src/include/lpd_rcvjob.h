/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2000, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 * $Id: lpd_rcvjob.h,v 5.6 2000/12/25 01:51:20 papowell Exp papowell $
 ***************************************************************************/



#ifndef _LPD_RCVJOB_H_
#define _LPD_RCVJOB_H_ 1

/* PROTOTYPES */
int Receive_job( int *sock, char *input );
int Receive_block_job( int *sock, char *input );
int Scan_block_file( int fd, char *error, int errlen, char *auth_id );
int Read_one_line( int fd, char *buffer, int maxlen );
int Check_space( double jobsize, int min_space, char *pathname );
int Do_perm_check( struct job *job, char *error, int errlen );
int Check_for_missing_files( struct job *job, struct line_list *files,
	char *error, int errlen, char *auth_id );
int Find_non_colliding_job_number( struct job *job );
int Get_route( struct job *job, char *error, int errlen );

#endif