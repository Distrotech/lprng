/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: controlword.c,v 5.2 1999/10/23 02:36:44 papowell Exp papowell $";


#include "lp.h"
#include "control.h"
/**** ENDINCLUDE ****/

 static struct keywords controlwords[] = {

{ "ABORT", OP_ABORT },
{ "ACTIVE", OP_ACTIVE },
{ "CLASS", OP_CLASS },
{ "CLIENT", OP_CLIENT },
{ "DEBUG", OP_DEBUG },
{ "DEFAULTQ", OP_DEFAULTQ },
{ "DISABLE", OP_DISABLE },
{ "DOWN", OP_DOWN },
{ "ENABLE", OP_ENABLE },
{ "HOLD", OP_HOLD },
{ "HOLDALL", OP_HOLDALL },
{ "KILL", OP_KILL },
{ "LPD", OP_LPD },
{ "LPQ", OP_LPQ },
{ "LPRM", OP_LPRM },
{ "MOVE", OP_MOVE },
{ "MSG", OP_MSG },
{ "NOHOLDALL", OP_NOHOLDALL },
{ "PRINTCAP", OP_PRINTCAP },
{ "REDIRECT", OP_REDIRECT },
{ "REDO", OP_REDO },
{ "RELEASE", OP_RELEASE },
{ "REREAD", OP_REREAD },
{ "START", OP_START },
{ "STATUS", OP_STATUS },
{ "STOP", OP_STOP },
{ "TOPQ", OP_TOPQ },
{ "UP", OP_UP },
{ "SERVER", OP_SERVER },
{ "DEFAULTS", OP_DEFAULTS },

{0}
};


/***************************************************************************
 * Get_controlword()
 * - decode the control word and return a key
 ***************************************************************************/

int Get_controlword( char *s )
{
	return( Get_keyval( s, controlwords ) );
}

char *Get_controlstr( int c )
{
	return( Get_keystr( c, controlwords ) );
}
