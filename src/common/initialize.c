/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: initialize.c
 * PURPOSE: perform system dependent initialization
 **************************************************************************/

static char *const _id = "$Id: initialize.c,v 3.1 1996/08/25 22:20:05 papowell Exp papowell $";

#include "lp.h"
#ifdef IS_AUX
# include <compat.h>
#endif
#include "setuid.h"
#if defined (HAVE_LOCALE_H)
# include <locale.h>
#endif


void Initialize()
{
	static int done;
	if( !done ){
		done = 1;
		/* set the umask so that you create safe files */
		umask( 0077 );
#ifdef IS_AUX
		/********************************************
		 * Apparently this needs to be done for AUX
		 *******************************************/
		/* A/UX needs this to be more BSD-compatible. */
		setcompat (COMPAT_BSDPROT);
		set42sig();
#endif
		
		To_user();
#if defined(HAVE_LOCALE_H)
		setlocale(LC_CTYPE, "");
#endif
		/* prepare to catch dead children */
		Setup_waitpid();
	}
}
