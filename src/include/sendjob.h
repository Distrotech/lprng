/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2001, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 * $Id: sendjob.h,v 1.2 2002/01/23 01:01:27 papowell Exp $
 ***************************************************************************/



#ifndef _SENDJOB_1_
#define _SENDJOB_1_ 1

/* PROTOTYPES */
int Send_job( struct job *job, struct job *logjob,
	int connect_timeout_len, int connect_interval, int max_connect_interval,
	int transfer_timeout, char *final_filter );
int Send_normal( int *sock, struct job *job, struct job *logjob,
	int transfer_timeout, int block_fd, char *final_filter );
int Send_control( int *sock, struct job *job, struct job *logjob, int transfer_timeout,
	int block_fd );
int Send_data_files( int *sock, struct job *job, struct job *logjob,
	int transfer_timeout, int block_fd, char *final_filter );
int Send_block( int *sock, struct job *job, struct job *logjob, int transfer_timeout );

#endif
