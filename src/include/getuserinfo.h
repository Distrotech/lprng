/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: getuserinfo.h
 * PURPOSE: getuserinfo.c functions
 * getuserinfo.h,v 3.1 1996/12/28 21:40:28 papowell Exp
 **************************************************************************/

#ifndef _GETUSERINFO_H
#define _GETUSERINFO_H

/*****************************************************************
 * Get User Name
 * Does a lookup in the password information,
 *   or defaults to 'nobody'
 *****************************************************************/
char *Get_user_information( void );
int Root_perms( void );

#endif
