/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: controlword.c
 * PURPOSE: decode control words
 **************************************************************************/

static char *const _id =
"controlword.c,v 3.6 1997/12/24 20:10:12 papowell Exp";

#include "lp.h"
#include "control.h"
/**** ENDINCLUDE ****/

#undef PAIR
#ifndef _UNPROTO_
# define PAIR(X) { #X, INTEGER_K, (void *)0, X }
#else
# define __string(X) "X"
# define PAIR(X) { __string(X), INTEGER_K, (void *)0, X }
#endif

static struct keywords controlwords[] = {
PAIR(STATUs),
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
PAIR(DEFAULTQ),
PAIR(ACTive),
PAIR(REDO),
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

char *Get_controlstr( int c )
{
	int i;
	for( i = 0; controlwords[i].keyword; ++i ){
		if( controlwords[i].maxval == c ){
			return( controlwords[i].keyword );
		}
	}
	return( 0 );
}
