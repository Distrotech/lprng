/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2000, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 * $Id: lockfile.h,v 5.3 2000/04/14 20:40:18 papowell Exp papowell $
 ***************************************************************************/



#ifndef _LOCKFILE_H_
#define _LOCKFILE_H_ 1

/* PROTOTYPES */

int Do_lock( int fd, int block );
int LockDevice(int fd, int block);

#endif
