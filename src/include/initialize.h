/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2000, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 * $Id: initialize.h,v 5.4 2000/06/18 21:07:35 papowell Exp papowell $
 ***************************************************************************/



#ifndef _INITIALIZE_H
#define _INITIALIZE_H

/* PROTOTYPES */
void Initialize(int argc,  char *argv[], char *envp[], int debugchar );
void Setup_configuration();
char *Get_user_information( void );

#endif
