/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: initialize.c
 * PURPOSE: perform system dependent initialization
 **************************************************************************/

static char *const _id = "$Id: initialize.c,v 3.7 1997/03/24 00:45:58 papowell Exp papowell $";

#include "lp.h"
#include "initialize.h"
#include "dump.h"
#include "printcap.h"
#include "getcnfginfo.h"
#include "gethostinfo.h"
#include "getuserinfo.h"
#include "waitchild.h"
#include "killchild.h"
#include "fileopen.h"
/**** ENDINCLUDE ****/

#ifdef IS_AUX
# include <compat.h>
#endif
#include "setuid.h"
#if defined (HAVE_LOCALE_H)
# include <locale.h>
#endif

#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif

#ifdef HAVE_RESOLV_H
#include <resolv.h>
#endif


/***************************************************************************
 * general initialization.
 * This should NOT do any network operations
 ***************************************************************************/

void Initialize( void )
{
	if( !Init_done ){
		Init_done = 1;
		/* set the umask so that you create safe files */
		umask( 0177 );
		(void)signal( SIGPIPE, SIG_IGN );
#ifdef IS_AUX
		/********************************************
		 * Apparently this needs to be done for AUX
		 *******************************************/
		/* A/UX needs this to be more BSD-compatible. */
		setcompat (COMPAT_BSDPROT);
		set42sig();
#endif

		/* set suid information */
		To_user();

#if HAVE_LOCALE_H
		setlocale(LC_CTYPE, "");
#endif
#if ENABLE_NLS
#if HAVE_LOCALE_H
		setlocale(LC_MESSAGES, "");
#endif
		bindtextdomain (PACKAGE, LOCALEDIR);
		textdomain (PACKAGE);
#endif

		/* prepare to catch dead children */
		Setup_waitpid();

		/*
			open /dev/null on fd 0, 1, 2 if neccessary
			This must be done before using any other database access
			functions,  as they may open a socket and leave it open.
		*/
		if( (DevNullFD = open( "/dev/null", O_RDWR, Spool_file_perms )) < 0 ){
			logerr_die( LOG_CRIT, "Initialize: main cannot open '/dev/null'" );
		}
		while( DevNullFD < 3 ){
			if( (DevNullFD = dup(DevNullFD)) < 0 ){
				logerr_die( LOG_CRIT, "Initialize: main cannot dup '/dev/null'" );
			}
		}
		if( Interactive ){
			if( Cfp_static == 0 ){
				malloc_or_die( Cfp_static, sizeof( Cfp_static[0] ) );
				memset( Cfp_static, 0, sizeof( Cfp_static[0] ) );
			}
			Cfp_static->remove_on_exit = 1;
			register_exit( Remove_files, 0 );
		}
	}
}

void Setup_configuration()
{
	char *config_file;

	/* Get default configuration file information */
	Clear_config();
	config_file = Client_config_file;
	if( Is_server && Server_config_file && *Server_config_file ){
		config_file = Server_config_file;
	}

    /* get the configuration file information if there is any */
    if( Allow_getenv ){
		char *s;
		if( UID_root ){
			fprintf( stderr,
			"%s: WARNING- LPD_CONF environment variable option enabled\n"
			"  and running as root!  You have an exposed security breach!\n"
			"  Recompile without -DGETENV or do not run clients as ROOT\n",
			Name );
		}
		if( (s = getenv( "LPD_CONF" )) ){
			config_file = s;
		}
    }

    DEBUG0("Setup_configuration: Configuration file '%s'", config_file );

	/*
	 * Testing magic:
	 * if we are running SUID
	 *   We have set our RUID to root and EUID daemon
	 * However,  we may want to run as another UID for testing.
	 * The config file allows us to do this, but we set the SUID values
	 * from the hardwired defaults before we read the configuration file.
	 * After reading the configuration file,  we check the current
	 * DaemonUID and the requested Daemon UID.  If the requested
	 * Daemon UID == 0, then we run as the user which started LPD.
	 */

    Get_local_host();
	if( config_file ) Get_config( config_file );

	/* we now know if we are using IPV4 or IPV6 from configuration */
#if defined(IN6_ADDR)
	if( IPV6Protocol ){
		AF_Protocol = AF_INET6;
#if defined(HAVE_RESOLV_H) && defined(RES_USE_INET6)
		_res.options |= RES_USE_INET6;
#endif
	} else {
		AF_Protocol = AF_INET;
	}
#endif

	if( Is_server ){
		Reset_daemonuid();
		DEBUG0( "DaemonUID %d", DaemonUID );
		if( Is_server ){
			Setdaemon_group();
		}
	}

	/* get the fully qualified domain name of host and the
		short host name as well
		FQDN - fully qualified domain name
		Host - actual one to use in H fields
		ShortHost - short host name
		NOTE: on PCs this will be the IP address
	*/

	Logname = Get_user_information();
	if( Localhost ==0 || *Localhost == 0
		|| Find_fqdn( &LocalhostIP, Localhost, 0 ) == 0 ){
		fatal( LOG_ERR, "Setup_configuration: no Localhost value! '%s'",
			Localhost);
	}
	DEBUG0("Initialize: Host FQDN: '%s', ShortHost '%s'",
		FQDNHost, ShortHost );

	/* we can now re-expand configuration information, with correct values */
	Expand_value( Pc_var_list, &Config_info );
	if(DEBUGL0) dump_parms( "Setup_configuration - initial values",
		Pc_var_list );
}
