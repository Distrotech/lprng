/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lockfile.h
 * PURPOSE: lockfile.c functions
 * lockfile.h,v 3.2 1997/12/16 15:06:44 papowell Exp
 **************************************************************************/

#ifndef _LOCKFILE_H
#define _LOCKFILE_H


int Lockf( char *filename, struct stat *statb );
int Lock_fd( int fd, char *filename, struct stat *statb );
int Do_lock (int fd, const char *filename, int block );
int LockDevice(int fd, char *devname);

#endif
