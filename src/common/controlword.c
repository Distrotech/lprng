/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: controlword.c
 * PURPOSE: decode control words
 **************************************************************************/

static char *const _id =
"$Id: controlword.c,v 3.0 1996/05/19 04:05:55 papowell Exp $";

#include "lp.h"
#include "control.h"

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
PAIR(AUTOHOLD),
PAIR(NOAUTOHOLD),
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
