/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: jobcontrol.h
 * PURPOSE: read and write the spool queue control file
 * $Id: jobcontrol.h,v 3.1 1996/08/25 22:20:05 papowell Exp papowell $
 **************************************************************************/

EXTERN char *Control_debug;

int Get_job_control( struct control_file *cf );
int Set_job_control( struct control_file *cf );
int Remove_job_control( struct control_file *cf );
int Get_route( struct control_file *cf );

/* get and set the current spooling and printing permissions */

int Get_spool_control( struct stat *statb );
int Set_spool_control();
