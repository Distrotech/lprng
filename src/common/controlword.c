/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: controlword.c
 * PURPOSE: decode control words
 **************************************************************************/

static char *const _id =
"$Id: controlword.c,v 3.1 1996/12/28 21:40:11 papowell Exp $";

#include "lp.h"
#include "control.h"
/**** ENDINCLUDE ****/

#define PAIR(X) { #X, INTEGER_K, (void *)0, X }
static struct keywords controlwords[] = {
PAIR(STATUS),
PAIR(START),
PAIR(STOP),
PAIR(ENABLE),
PAIR(DISABLE),
PAIR(ABORT),
PAIR(KILL),
PAIR(HOLD),
PAIR(RELEASE),
PAIR(TOPQ),
PAIR(LPQ),
PAIR(LPRM),
PAIR(REDIRECT),
PAIR(LPD),
PAIR(PRINTCAP),
PAIR(UP),
PAIR(DOWN),
PAIR(REREAD),
PAIR(MOVE),
PAIR(DEBUG),
PAIR(HOLDALL),
PAIR(NOHOLDALL),
PAIR(CLAss),
{0}
};


/***************************************************************************
 * Get_controlword()
 * - decode the control word and return a key
 ***************************************************************************/

int Get_controlword( char *s )
{
	int i;
	for( i = 0; controlwords[i].keyword; ++i ){
		if( strcasecmp( s, controlwords[i].keyword ) == 0 ){
			return( controlwords[i].maxval );
		}
	}
	return( 0 );
}
