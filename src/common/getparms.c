/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: getparms.c
 * PURPOSE: extract the parameters from the command line for the
 * LPR program
 *
 **************************************************************************/

static char *const _id =
"getparms.c,v 3.3 1997/10/27 00:14:19 papowell Exp";

#include "lp.h"
#include "getparms.h"
/**** ENDINCLUDE ****/


void Dienoarg(int option)
{
	Diemsg ("option '%c' missing argument", option);
}

/***************************************************************************
 * Check_int_dup (int option, int *value, char *arg)
 * 1.  check to see if value has been set
 * 2.  if not, then get integer value from arg
 ***************************************************************************/

void Check_int_dup (int option, int *value, char *arg, int maxvalue)
{
	char *convert;

	if ( !Allow_duplicate_args ) {
	  if(*value) {
	    Diemsg ("duplicate option %c", option);
	  }
	}
	if (arg == 0) {
		Dienoarg (option);
	}
	convert = arg;
	*value = strtol( arg, &convert, 10 );
	if( *value < 0 || convert == arg || *convert ){
		Diemsg ("option %c parameter `%s` is not positive integer value",
		        option, arg );
	}
	if( maxvalue > 0 && *value > maxvalue ){
		Diemsg ("option %c parameter `%s` is not integer value from 0 - %d",
		        option, arg, maxvalue );
	}
}

/***************************************************************************
 * Check_str_dup(int option, char *value, char *arg)
 * 1.  check to see if value has been set
 * 2.  if not, then set it
 ***************************************************************************/

void Check_str_dup(int option, char **value, char *arg, int maxlen )
{
        if ( !Allow_duplicate_args ) {
	  if (*value) {
	    Diemsg ("duplicate option %c", option);
	  }
	}
	if (arg == 0) {
		Dienoarg (option);
	}
	if( maxlen && (int)(strlen(arg)) > maxlen ) {
		Diemsg ("option %c argument too long (%s)", option, arg);
	}
	*value = arg;
}

/***************************************************************************
 * 1.  check to see if value has been set
 * 2.  if not, then set it
 ***************************************************************************/

void Check_dup(int option, int *value)
{
	if ( !Allow_duplicate_args ) {
	  if (*value) {
	    Diemsg ("duplicate option %c", option);
	  }
	}
	*value = 1;
}
