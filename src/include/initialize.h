/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: initialize.h
 * PURPOSE: initialize.c functions
 * initialize.h,v 3.3 1998/03/24 02:43:22 papowell Exp
 **************************************************************************/

#ifndef _INITIALIZE_H
#define _INITIALIZE_H

/*****************************************************************
 * Initialize() performs all of the system and necessary
 * initialization.  It was created to handle the problems
 * of different systems needing dynamic initialization calls.
 * Setup_configuration does a second stage initialization.
 *****************************************************************/
void Initialize( int argc, char *argv[], char *envp[] ); 
void Setup_configuration( void );
 
#endif
