/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: setuid.h
 * PURPOSE: setuid function declarations
 * "setuid.h,v 3.1 1996/12/28 21:40:36 papowell Exp";
 **************************************************************************/
/*****************************************************************
 * SetUID information
 * Original RUID and EUID, as well as the Daemon UID
 *****************************************************************/

EXTERN uid_t OriginalEUID, OriginalRUID;   /* original EUID, RUID values */
EXTERN uid_t DaemonUID;    /* Daemon UID */
EXTERN uid_t UID_root;     /* UID is root */
EXTERN gid_t DaemonGID;    /* Daemon GID */

int Getdaemon( void );
int To_root( void );
int To_daemon( void );
int To_user( void );
int To_uid( int uid );
int Full_daemon_perms( void );
int Full_root_perms( void );
int Full_user_perms( void );
int Getdaemon_group( void );
int Setdaemon_group( void );
void Reset_daemonuid( void );
