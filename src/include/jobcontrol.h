/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: jobcontrol.h
 * PURPOSE: read and write the spool queue control file
 * $Id: jobcontrol.h,v 3.4 1997/01/30 21:15:20 papowell Exp $
 **************************************************************************/


int Get_job_control( struct control_file *cf, int *fd );
int Set_job_control( struct control_file *cf, int *fd, int force_change );
int Remove_job_control( struct control_file *cf );
int Get_route( struct control_file *cf );

/* get and set the current spooling and printing permissions */

int Get_spool_control( struct stat *statb, int *fd );
int Set_spool_control( int *fd, int forcechange );
char *Copy_hf( struct malloc_list *data, struct malloc_list *copy,
	char *header, char *prefix );
