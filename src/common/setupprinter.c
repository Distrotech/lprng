/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: setupprinter.c
 * PURPOSE: set up the printer printcap entry and other information
 **************************************************************************/

static char *const _id =
"$Id: setupprinter.c,v 3.3 1997/01/29 03:04:39 papowell Exp $";

#include "lp.h"
#include "printcap.h"
#include "checkremote.h"
#include "cleantext.h"
#include "dump.h"
#include "fileopen.h"
#include "getcnfginfo.h"
#include "getqueue.h"
#include "jobcontrol.h"
#include "pathname.h"
#include "permission.h"
#include "pr_support.h"
#include "setupprinter.h"

/**** ENDINCLUDE ****/

/***************************************************************************
 * int Setup_printer(
 *   char * name - printer name
 *   char *error, int errlen - use for error status information
 *                  passed back to upper levels
 *   struct keywords *debug_list - when you get the spool control file,
 *                  use this to parse the debug information in it.
 *   int info_only - we only want information,  so do not open spool dirs,
 *                  or take action to turn us into a server
 *   struct stat *control_statb - when you open the spool control file,
 *                  save its stat information here.
 *   struct printcap_entry **pc_entry - printcap entry found
 *
 * 1. check for a clean name, i.e. - no funny characters
 * 2. get the printcap information
 * 3. set the debug level to the new value (if any) from the printcap entry
 * 4. Check the spool directory
 * 5. Open the log file
 * 6. Get the control file permissions
 * 7. Scan the spool queue
 *
 * RETURNS:
 * 0 - no errors
 * 1 - no spool dir,  no lp= redirection
 * 2 - no spool dir,     lp= redirection
 ***************************************************************************/
static struct dpathname cdpath;			/* data for CDpathname */
static struct dpathname sdpath;			/* data for SDpathname */
static void Open_log(void);


int Setup_printer( char *name,
	char *error, int errlen,
	struct keywords *debug_list, int info_only,
	struct stat *control_statb, struct printcap_entry **pc_entry )
{
	char *s;
	int status = 0;
	struct printcap_entry *pc = 0;

	DEBUG3( "Setup_printer: printer '%s'", name );

	/* check printername for characters, underscore, digits */
	memset( &sdpath, 0, sizeof( sdpath ));
	memset( &cdpath, 0, sizeof( cdpath ));

	/* reset the configuration information, just in case
	 * this is a subserver or being used to get status
	 */
	Reset_config();
	Free_perms( &Local_perm_file );
	CDpathname = SDpathname = &sdpath;
	Orig_Lp_device =  Orig_RemoteHost =  Orig_RemotePrinter = 0;

	error[0] = 0;
	if( (s = Clean_name( name )) ){
		plp_snprintf( error, errlen,
			"printer '%s' has illegal char '%c' in name", name, *s );
		status = 1;
		goto error;
	}

	/* set printer name and printcap variables */
	if( (Printer = Full_printer_vars( name, &pc )) == 0 ){
		Printer = name;
		plp_snprintf( error, errlen,
			"no printcap for printer '%s'", name );
		status = 2;
		goto error;
	}
	if( pc_entry ) *pc_entry = pc;
	/* save the original information in the printcap database */
	Orig_Lp_device = Lp_device;
	Orig_RemoteHost = RemoteHost;
	Orig_RemotePrinter = RemotePrinter;

	DEBUG2("Setup_printer: RemoteHost '%s', RemotePrinter '%s', Lp '%s'",
		RemoteHost, RemotePrinter, Lp_device );

	Check_remotehost(1);


	if( Spool_dir && *Spool_dir ){
		s = Expand_path( SDpathname, Spool_dir );
		Spool_dir = s;
	}
	DEBUG2( "Setup_printer: Spool_dir '%s'->'%s'",
		Spool_dir, s );

	if( Spool_dir == 0 || *Spool_dir == 0 ){
		plp_snprintf( error, errlen,
			"no spool directory for printer '%s'", name );
		status = 2;
		goto error;
	}

	if( chdir( Spool_dir ) < 0 ){
		plp_snprintf( error, errlen,
			"printer '%s', chdir to '%s' failed '%s'",
				name, Spool_dir, Errormsg( errno ) );
		status = 1;
		goto error;
	}

	if( Control_dir && *Control_dir ){
		CDpathname = &cdpath;
		s = Expand_path( CDpathname, Control_dir );
		Control_dir = s;
	} else {
		Control_dir = Spool_dir;
	}
	DEBUG2( "Setup_printer: Control_dir '%s'->'%s'",
		Control_dir, s );

	DEBUG2( "Setup_printer: log file '%s'", Log_file );
	New_log_file = 0;
	if( !info_only && New_debug && *New_debug ){
		DEBUG0("Setup_printer: new debug info '%s'", New_debug );
		Parse_debug( New_debug, debug_list, Interactive );
		Open_log();
	}

	if(DEBUGL3)dump_printcap_entry("Setup_printer",pc);
	if(DEBUGL3)dump_parms("Setup_printer",Pc_var_list);

	/*
	 * get the override information from the control/spooling
	 * directory
	 */

	(void)Get_spool_control( control_statb, (void *)0 );

	Fix_update( debug_list, info_only );

	/*
	 * reset the status file descriptor so you reopen the
	 * status file.
	 */

	if( !info_only ){
		if( Status_fd > 0 ) close( Status_fd );
		Status_fd = -1;
	}

	/* get the enabled/disabled status */

	/*
	 * get the local permissions for this queue
	 */
	if( Local_permission_file && *Local_permission_file ){
		Get_perms( Printer, &Local_perm_file, Local_permission_file );
	}

	if( error[0] == 0 ){
		Scan_queue( 1, 1);
	}

error:
	DEBUG3("Setup_printer: status '%d', error '%s'", status, error );
	return( status );
}

/***************************************************************************
 * Fix_update()
 *  When you get information from the spool queue control file,  you may
 *  need to update various parameters.  This routine does the update.
 *  One of the tricky parts is to return to previous values; you can
 *  only do this if you have them saved,  which is done in Setup_printer()
 ***************************************************************************/

void Fix_update( struct keywords *debug_list, int info_only )
{
	Lp_device = Orig_Lp_device;
	RemoteHost = Orig_RemoteHost;
	RemotePrinter = Orig_RemotePrinter;
	if( Forwarding && *Forwarding ){
		Lp_device = Forwarding;
	}
	Check_remotehost(1);
#if !defined(NODEBUG)
	if( !info_only && Control_debug && *Control_debug && debug_list ){
		DEBUG0("Fix_update: new debug level %s", Control_debug );
		Parse_debug( Control_debug, debug_list, Interactive );
		DEBUG0("Fix_update: new debug level %d", Debug );
	}
#endif
	if( !info_only ){
		Open_log();
	}
}

static void Open_log(void)
{
	char *s;
	int fd;				/* file descriptor */
	struct stat statb;	/* status buffer for file */

	s = 0;
	if( Log_file ) s = Log_file;
	if( New_log_file ) s = New_log_file;
	if( s ){
		DEBUG0("Open_log: log file '%s'", s );
		if( s[0] != '/' ){
			s = Add_path( CDpathname, s );
		}
		fd = Checkwrite( s, &statb, O_WRONLY|O_APPEND, 0, 0 );
		if( fd >= 0 ){
			if( fd != 2 ){
				dup2( fd, 2 );
				close( fd );
			}
		} else {
			DEBUG0("Open_log: cannot open log file '%s'", s );
		}
	}
}
