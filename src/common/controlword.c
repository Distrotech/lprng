/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2002, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: controlword.c,v 1.19 2002/03/06 17:02:50 papowell Exp $";


#include "lp.h"
#include "control.h"
/**** ENDINCLUDE ****/

 static struct keywords controlwords[] = {

{ "ABORT", N_("ABORT"), OP_ABORT },
{ "ACTIVE", N_("ACTIVE"), OP_ACTIVE },
{ "CLASS", N_("CLASS"), OP_CLASS },
{ "CLIENT", N_("CLIENT"), OP_CLIENT },
{ "DEBUG", N_("DEBUG"), OP_DEBUG },
{ "DEFAULTQ", N_("DEFAULTQ"), OP_DEFAULTQ },
{ "DISABLE", N_("DISABLE"), OP_DISABLE },
{ "DOWN", N_("DOWN"), OP_DOWN },
{ "ENABLE", N_("ENABLE"), OP_ENABLE },
{ "HOLD", N_("HOLD"), OP_HOLD },
{ "HOLDALL", N_("HOLDALL"), OP_HOLDALL },
{ "KILL", N_("KILL"), OP_KILL },
{ "LPD", N_("LPD"), OP_LPD },
{ "LPQ", N_("LPQ"), OP_LPQ },
{ "LPRM", N_("LPRM"), OP_LPRM },
{ "MOVE", N_("MOVE"), OP_MOVE },
{ "MSG", N_("MSG"), OP_MSG },
{ "NOHOLDALL", N_("NOHOLDALL"), OP_NOHOLDALL },
{ "PRINTCAP", N_("PRINTCAP"), OP_PRINTCAP },
{ "REDIRECT", N_("REDIRECT"), OP_REDIRECT },
{ "REDO", N_("REDO"), OP_REDO },
{ "RELEASE", N_("RELEASE"), OP_RELEASE },
{ "REREAD", N_("REREAD"), OP_REREAD },
{ "START", N_("START"), OP_START },
{ "STATUS", N_("STATUS"), OP_STATUS },
{ "STOP", N_("STOP"), OP_STOP },
{ "TOPQ", N_("TOPQ"), OP_TOPQ },
{ "UP", N_("UP"), OP_UP },
{ "SERVER", N_("SERVER"), OP_SERVER },
{ "DEFAULTS", N_("DEFAULTS"), OP_DEFAULTS },
{ "FLUSH", N_("FLUSH"), OP_FLUSH },
{ "LANG", N_("LANG"), OP_LANG },

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
