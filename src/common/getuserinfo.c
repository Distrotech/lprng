/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: getuserinfo.c
 * PURPOSE: user name information lookup
 **************************************************************************/

static char *const _id =
"$Id: getuserinfo.c,v 3.0 1996/05/19 04:06:01 papowell Exp $";
/********************************************************************
 * void Get_user_information();
 *  get the user name
 *
 * 
 ********************************************************************/

#include "lp.h"
#include "setuid.h"

char *Get_user_information()
{
	char *name = 0;
	static char uid_msg[32];
	uid_t uid = OriginalRUID;
#ifndef USER_ENV
	struct passwd *pw_ent;

	/* get the password file entry */
    if( (pw_ent = getpwuid( uid )) ){
		name =  pw_ent->pw_name;
	}

#else
	name = getenv( "USER" );
#endif
	if( name == 0 ){
		plp_snprintf( uid_msg, sizeof(uid_msg), "UID_%d", uid );
		name = uid_msg;
	} else {
		name = safestrdup( name );
	}
    return( name );
}

int Root_perms()
{
	return( getuid() == 0 );
}
