/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: fileopen.h
 * PURPOSE: fileopen.c functions
 * $Id: fileopen.h,v 3.2 1997/01/15 02:21:18 papowell Exp $
 **************************************************************************/

#ifndef _FILEOPEN_H
#define _FILEOPEN_H

/*****************************************************************
 * File open functions
 * These perform extensive checking for permissions and types
 *  see fileopen.c for details
 *****************************************************************/
int Checkread( char *file, struct stat *statb );
int Checkwrite( char *file, struct stat *statb, int rw, int create, int del );
int Make_temp_fd( struct control_file *cfp, char *path, int len );
char *Init_tempfile( struct control_file *cfp );
void Remove_files( void *p );
void Remove_tempfiles( void );
int Checkwrite_timeout(int timeout,
	char *file, struct stat *statb, int rw, int create, int delay );

EXTERN struct filter Passthrough_send;
EXTERN struct filter Passthrough_receive;

#endif
