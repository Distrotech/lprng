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
 * $Id: initialize.h,v 3.2 1997/12/20 21:16:26 papowell Exp $
 **************************************************************************/

#ifndef _INITIALIZE_H
#define _INITIALIZE_H

/*****************************************************************
 * Initialize() performs all of the system and necessary
 * initialization.  It was created to handle the problems
 * of different systems needing dynamic initialization calls.
 * Setup_configuration does a second stage initialization.
 *****************************************************************/
void Initialize( char *argv[] ); 
void Setup_configuration( void );
 
#endif
