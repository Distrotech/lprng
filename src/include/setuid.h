/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: setuid.h
 * PURPOSE: setuid function declarations
 * "$Id: setuid.h,v 3.0 1996/05/19 04:06:35 papowell Exp $";
 **************************************************************************/
/*****************************************************************
 * SetUID information
 * Original RUID and EUID, as well as the Daemon UID
 *****************************************************************/

EXTERN uid_t OriginalEUID, OriginalRUID;   /* original EUID, RUID values */
EXTERN uid_t DaemonUID;    /* Daemon UID */
EXTERN uid_t UID_root;     /* UID is root */
EXTERN gid_t DaemonGID;    /* Daemon GID */

int Getdaemon();
int To_root();
int To_daemon();
int To_user();
int To_uid( int uid );
int Full_daemon_perms();
int Full_root_perms();
int Getdaemon_group();
int Setdaemon_group();
void Reset_daemonuid();
