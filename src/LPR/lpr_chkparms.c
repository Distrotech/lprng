/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpr_checkparms.c
 * PURPOSE: check the parameters and options for correctness and
 *  consistency.
 **************************************************************************/

static char *const _id =
"$Id: lpr_chkparms.c,v 3.5 1997/01/29 03:04:39 papowell Exp $";

#include "lp.h"
#include "getuserinfo.h"
#include "getprinter.h"
/**** ENDINCLUDE ****/

/***************************************************************************
Check Parms()
1. we determine the name of the printer - Printer variable
2. we determine the host name to be used - RemoteHost variable
3. check the user name for consistency:
	We have the user name from the environment
	We have the user name from the -U option
    Allow override if we are root or some silly system (like DOS)
		that does not support multiple users
 ***************************************************************************/

void Check_parms( struct printcap_entry **pce )
{
	int c;

	if( LP_mode && Printer == 0 ){
		Printer = getenv( "LPDEST" );
	}

	Get_printer( pce );

    if( (RemoteHost == 0 || *RemoteHost == 0) ){
		RemoteHost = 0;
        if( Default_remote_host && *Default_remote_host ){
            RemoteHost = Default_remote_host;
        } else if( FQDNHost && *FQDNHost ){
            RemoteHost = FQDNHost;
        }
    }
	if( RemoteHost == 0 ){
		Diemsg( "No server host specified" );
	}
	if( RemotePrinter == 0 || *RemotePrinter == 0 ){
		RemotePrinter = Printer;
	}

	/* check for priority in range */
	if( Priority == 0 && Classname && *Classname ){
		Priority = Classname[0];
	}
	if( Priority == 0 && Default_priority && *Default_priority ){
		Priority = *Default_priority;
	}
	if( Priority == 0 ){
		Priority = 'A';
	}
	if( !isupper( Priority ) ){
		Diemsg(
"Priority (first letter of Classname) not 'A' (lowest) to 'Z' (highest)" );
	}

	/* fix up the Classname 'C' option */

	if( Classname == 0 ){
		if( Backwards_compatible ){
			Classname = ShortHost;
		} else {
			static char c[2];
			c[0] = Priority;
			Classname = c;
		}
	}

	/* fix up the jobname */
	if( Jobname == 0 ){
		if( Filecount == 0 ){
			Jobname = "(stdin)";
		} else {
			static char c[M_JOBNAME+1];
			char *s;
			int i, j, v;
			
			/* watch out for security loop holes */
			for( i = 0, j = 0; j < M_JOBNAME && i < Filecount; ++i ){
				s = Files[i];
				if( strcmp(s, "-" ) == 0 ) s = "(stdin)";
				if( j && j < M_JOBNAME ) c[j++] = ' ';
				while( (v = *s++) && j < M_JOBNAME ){
					c[j++] = v;
				}
			}
			c[j++] = 0;	/* This is safe */
			Jobname = c;
		}
	}

	/* fix up the banner name.
	 * if you used the -U option,
     *   check to see if you have root permissions
	 *   set to -U value
	 * else set to log name of user
     * if No_header suppress banner
	 */
	if( Username ){
		if( !Root_perms() ){
			Diemsg( "-U (username) can only be used by ROOT" );
		}
		Bnrname = Username;
	} else {
		Bnrname = Logname;
	}
	if( No_header ){
		Bnrname = 0;
	}

	/* check the format */

	DEBUG0("Check_parms: before checking format '%s'", Format );
	if( Binary ){
		if(Format ){
			Diemsg( "Cannot specify binary with format '%s'", Format  );
		}
		Format = "l";
	}
	if( Format == 0 ){
		Format = Default_format;
	}
	if( Format == 0 ){
		Format = "f";
	}
	DEBUG0("Check_parms: after checking format '%s'", Format );
	if( strlen( Format ) != 1 || !islower( c = *Format )
		|| strchr( "aios", c )
		|| (Formats_allowed && !strchr( Formats_allowed, c ) )){
		Diemsg( "Bad format specification '%s'", Format );
	}
	/* check to see how many files you want to print- limit of 52 */
	if( Filecount > 52 ){
		Diemsg( "Sorry, can only print 52 files at a time, split job up");
	}
	if( Copies == 0 ){
		Copies = 1;
	}
	/* check the for the -Q flag */
	if( Use_queuename_flag ) Use_queuename = 1;
	if( Force_queuename ) Use_queuename = 1;
	DEBUG2("Check_parms: Use_shorthost %d, Use_queuename %d, Queue_name '%s'",
		Use_shorthost, Use_queuename, Queue_name );
}
