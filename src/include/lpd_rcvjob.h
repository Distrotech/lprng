/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 * $Id: lpd_rcvjob.h,v 1.74 2004/09/24 20:20:00 papowell Exp $
 ***************************************************************************/



#ifndef _LPD_RCVJOB_H_
#define _LPD_RCVJOB_H_ 1

/* PROTOTYPES */
int Receive_job( int *sock, char *input );
int Receive_block_job( int *sock, char *input );
int Scan_block_file( int fd, char *error, int errlen );
int Read_one_line( int timeout, int fd, char *buffer, int maxlen );
int Check_space( double jobsize, int min_space, char *pathname );
int Do_perm_check( struct job *job, char *error, int errlen );
int Check_for_missing_files( struct job *job, struct line_list *files,
	char *error, int errlen, struct line_list *header_info, int holdfile_fd );
int Setup_temporary_job_ticket_file( struct job *job, char *filename,
	int read_control_file,
	char *cf_file_image,
	char *error, int errlen  );
int Find_non_colliding_job_number( struct job *job );
int Do_incoming_control_filter( struct job *job, char *error, int errlen );
int Get_route( struct job *job, char *error, int errlen );
void Generate_control_file( struct job *job );

#endif
