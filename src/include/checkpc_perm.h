/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: checkpc_perm.c
 * PURPOSE: definitions of functions
 * checkpc_perm.h,v 3.1 1996/12/28 21:40:23 papowell Exp
 **************************************************************************/

int check_file( struct dpathname *dpath, int fix, time_t t, int age, int rem );
int fix_create_dir( struct dpathname *dpath, struct stat *statb );
int fix_create_file( struct dpathname *dpath, struct stat *statb );
int fix_owner( char *path );
int Check_perms( struct dpathname *dpath, int fix, int age, int remove );
int check_file( struct dpathname *dpath,
	int fix, time_t t, int age, int remove );
int fix_create_dir( struct dpathname *dpath, struct stat *statb );
int fix_owner( char *path );
int fix_perms( char *path, int perms );
int Check_spool_dir( struct dpathname *dpath, int fix );
