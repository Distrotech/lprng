/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: serverpid.h
 * PURPOSE:
 * serverpid.h,v 3.1 1996/12/28 21:40:35 papowell Exp
 **************************************************************************/

#ifndef _SERVERPID_H
#define _SERVERPID_H

int Read_pid( int fd, char *str, int len );
void Write_pid( int fd, int pid, char *str );

#endif
