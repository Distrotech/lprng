/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: setupprinter.c
 * PURPOSE: set up the printer printcap entry and other information
 **************************************************************************/

static char *const _id =
"$Id: setupprinter.c,v 3.0 1996/05/19 04:06:13 papowell Exp $";

#include "lp.h"
#include "printcap.h"
#include "jobcontrol.h"
#include "permission.h"
#include "pr_support.h"

/***************************************************************************
 * int Setup_printer(
 *   char * name - printer name
 *   char *error, int errlen - use for error status information
 *                  passed back to upper levels
 *   struct pc_used *pc_used - printcap info used to get this printer
 *                  update with printcap information in local spool dir
 *   struct keywords *debug_list - when you get the spool control file,
 *                  use this to parse the debug information in it.
 *   int info_only - we only want information,  so do not open spool dirs,
 *                  or take action to turn us into a server
 *   struct stat *control_statb - when you open the spool control file,
 *                  save its stat information here.
 *
 * 1. check for a clean name, i.e. - no funny characters
 * 2. get the printcap information
 * 3. set the debug level to the new value (if any) from the printcap entry
 * 4. Check the spool directory
 * 5. Open the log file
 * 6. Get the control file permissions
 * 7. Scan the spool queue
 ***************************************************************************/
static struct dpathname cdpath;			/* data for CDpathname */
static struct dpathname sdpath;			/* data for SDpathname */
static void Open_log();


int Setup_printer( char *name,
	char *error, int errlen, struct pc_used *pc_used,
	struct keywords *debug_list, int info_only,
	struct stat *control_statb )
{
	char *s;
	int status = 0;

	/* check printername for characters, underscore, digits */
	memset( &sdpath, 0, sizeof( sdpath ));
	memset( &cdpath, 0, sizeof( cdpath ));
	Free_perms( &Local_perm_file );
	CDpathname = SDpathname = &sdpath;
	Orig_Lp_device =  Orig_RemoteHost =  Orig_RemotePrinter = 0;

	DEBUG4( "Setup_printer: printer '%s'", name );
	error[0] = 0;
	if( (s = Clean_name( name )) ){
		plp_snprintf( error, errlen,
			"printer '%s' has illegal char '%c' in name", name, *s );
		status = 1;
		goto error;
	}

	/* clear the list */
	if( pc_used ){
		Clear_pc_used( pc_used );
	}

	/* set printer name and printcap variables */
	if( (Printer = Get_printer_vars( name, error, errlen,
		&Printcapfile, &Pc_var_list, Default_printcap_var,
		pc_used )) == 0 ){
		plp_snprintf( error, errlen,
			"no printcap for printer '%s'", name );
		status = 1;
		goto error;
	}
	if( !info_only && New_debug && *New_debug ){
		DEBUG0("Setup_printer: new debug level %s", New_debug );
		Parse_debug( New_debug, debug_list, Interactive );
		DEBUG0("Setup_printer: new debug level %d", Debug );
	}

	if( Spool_dir && *Spool_dir ){
		s = Expand_path( SDpathname, Spool_dir );
		DEBUG3( "Setup_printer: Spool_dir '%s'->'%s'",
			Spool_dir, s );
		Spool_dir = s;
	}
	if( Control_dir && *Control_dir ){
		CDpathname = &cdpath;
		s = Expand_path( CDpathname, Control_dir );
		DEBUG3( "Setup_printer: Control_dir '%s'->'%s'",
			Control_dir, s );
		Control_dir = s;
	} else {
		Control_dir = Spool_dir;
	}

	/* save the original information in the printcap database */
	Orig_Lp_device = Lp_device;
	Orig_RemoteHost = RemoteHost;
	Orig_RemotePrinter = RemotePrinter;

	DEBUG3("Setup_printer: RemoteHost '%s', RemotePrinter '%s', Lp '%s'",
		RemoteHost, RemotePrinter, Lp_device );

	Check_remotehost(1);

	if( Spool_dir == 0 || *Spool_dir == 0 ){
		plp_snprintf( error, errlen,
			"Setup_printer: no spool directory for printer '%s'", name );
		status = 2;
		goto error;
	}

	if( chdir( Spool_dir ) < 0 ){
		plp_snprintf( error, errlen,
			"Setup_printer: printer '%s', chdir to '%s' failed '%s'",
				name, Spool_dir, Errormsg( errno ) );
		status = 1;
		goto error;
	}

	DEBUG3( "Setup_printer: log file '%s'", Log_file );
	New_log_file = 0;
	if( !info_only ){
		Open_log();
	}

	/*
	 * get the override information from the control/spooling
	 * directory
	 */

	(void)Get_spool_control( control_statb );

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

error:
	DEBUG4("Setup_printer: status '%d', error '%s'", status, error );
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
	if( !info_only && Control_debug && *Control_debug && debug_list ){
		DEBUG0("Fix_update: new debug level %s", Control_debug );
		Parse_debug( Control_debug, debug_list, Interactive );
		DEBUG0("Fix_update: new debug level %d", Debug );
	}
	if( !info_only ){
		Open_log();
	}
}

static void Open_log()
{
	char *s;
	int fd;				/* file descriptor */
	struct stat statb;	/* status buffer for file */

	s = 0;
	if( Log_file ) s = Log_file;
	if( New_log_file ) s = New_log_file;
	if( s ){
		DEBUG0("Open_log: log file %s", s );
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
			DEBUG1("Open_log: cannot open log file '%s'", s );
		}
	}
}
