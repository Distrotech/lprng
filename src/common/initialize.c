/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: initialize.c,v 5.4 1999/10/28 01:28:15 papowell Exp papowell $";

#include "lp.h"
#include "initialize.h"
#include "getopt.h"
#include "gethostinfo.h"
#include "proctitle.h"
#include "getqueue.h"
/**** ENDINCLUDE ****/

#ifdef IS_AUX
# include <compat.h>
#endif
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

void Initialize(int argc,  char *argv[], char *envp[] )
{
	char *s;
	int fd;

	DEBUG1("Initialize: starting");

	/*
		open /dev/null on fd 0, 1, 2, 3, 4, if neccessary
		This must be done before using any other database access
		functions,  as they may open a socket and leave it open.
	*/
	if( (fd = open( "/dev/null", O_RDWR, Spool_file_perms_DYN )) < 0 ){
		logerr_die( LOG_CRIT, "Initialize: cannot open '/dev/null'" );
	}
	DEBUG1("Initialize: /dev/null fd %d", fd );
	while( fd < 5 ){
		if( (fd = dup(fd)) < 0 ){
			logerr_die( LOG_CRIT, "Initialize: main cannot dup '/dev/null'" );
		}
	}
	close(fd);

	initsetproctitle( argc, argv, envp );
	Name = "UNKNOWN";
	if( argv && argv[0] ){
		Name = argv[0];
		if( (s = strrchr(Name,'/')) ) Name = s+1;
	}
	/* set the umask so that you create safe files */
	umask( 0077 );
	signal( SIGPIPE, SIG_IGN );
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

}

void Setup_configuration()
{
	char *s;
	struct line_list raw;

	/* Get default configuration file information */
#ifdef DMALLOC
	extern int _dmalloc_outfile;
	extern char *_dmalloc_logpath;
	char buffer[SMALLBUFFER];

	safestrdup("DMALLOC",__FILE__,__LINE__);
	if( _dmalloc_logpath && _dmalloc_outfile < 0 ){
		_dmalloc_outfile = open( _dmalloc_logpath,  O_WRONLY | O_CREAT | O_TRUNC, 0666);
	}
	plp_snprintf(buffer,sizeof(buffer),"*** Setup_configuration: pid %d\n", getpid() );
	Write_fd_str(_dmalloc_outfile,buffer);
	DEBUG1("Setup_configuration: _dmalloc_outfile fd %d", _dmalloc_outfile);
#endif

	Init_line_list(&raw);
	Clear_config();


	DEBUG1("Setup_configuration: starting, Allow_getenv %d",
		Allow_getenv_DYN );

    /* get the configuration file information if there is any */
    if( Allow_getenv_DYN ){
		if( UID_root ){
			fprintf( stderr,
			"%s: WARNING- LPD_CONF environment variable option enabled\n"
			"  and running as root!  You have an exposed security breach!\n"
			"  Recompile without -DGETENV or do not run clients as ROOT\n",
			Name );
		}
		if( (s = getenv( LPD_CONF )) ){
			Set_DYN(&Config_file_DYN, s);
		}
    }

    DEBUG1("Setup_configuration: Configuration file '%s'", Config_file_DYN );

	Get_config( Is_server || Require_configfiles_DYN, Config_file_DYN );

	if( Is_server ){
		Reset_daemonuid();
		Setdaemon_group();
		DEBUG1( "DaemonUID %d", DaemonUID );
	} else {
		s = Get_user_information();
		Set_DYN( &Logname_DYN, s );
		if(s) free(s); s = 0;
	}

	DEBUG1("Setup_configuration: Host '%s', ShortHost '%s', user '%s'",
		FQDNHost_FQDN, ShortHost_FQDN, Logname_DYN );

	if(DEBUGL2) Dump_parms( "Setup_configuration - final values", Pc_var_list );

	if( Is_server ){
		DEBUG2("Setup_configuration: Printcap_path '%s'", Printcap_path_DYN );
		Getprintcap_pathlist( 1, &raw, &PC_filters_line_list,
			Printcap_path_DYN );
		DEBUG2("Setup_configuration: Lpd_printcap_path '%s'", Lpd_printcap_path_DYN );
		Getprintcap_pathlist( 0, &raw, &PC_filters_line_list,
			Lpd_printcap_path_DYN );
		DEBUG2("Setup_configuration: Printer_perms_path '%s'", Printer_perms_path_DYN );
		Getprintcap_pathlist( 1, &RawPerm_line_list, &Perm_filters_line_list,
			Printer_perms_path_DYN );
	} else {
		DEBUG2("Setup_configuration: Printcap_path '%s'", Printcap_path_DYN );
		Getprintcap_pathlist( Require_configfiles_DYN,
			&raw, &PC_filters_line_list,
			Printcap_path_DYN );
	}
	Build_printcap_info( &PC_names_line_list, &PC_order_line_list,
		&PC_info_line_list, &raw, &Host_IP );
	/* now we can free up the raw list */
	Free_line_list( &raw );
	if(DEBUGL3){
		Dump_line_list("Setup_configuration: PC names", &PC_names_line_list );
		Dump_line_list("Setup_configuration: PC order", &PC_order_line_list );
		Dump_line_list("Setup_configuration: PC info", &PC_info_line_list );
		Dump_line_list("Setup_configuration: Perms", &RawPerm_line_list );
	}
}

/*
 * char *Get_user_information(void)
 * OUTPUT: dynamic alloc string
 *  - returns user name
 */
char *Get_user_information( void )
{
	char *name = 0;
	char uid_msg[32];
	uid_t uid = OriginalRUID;

	struct passwd *pw_ent;

	/* get the password file entry */
    if( (pw_ent = getpwuid( uid )) ){
		name =  pw_ent->pw_name;
	}
	if( name == 0 ) name = getenv( "LOGNAME" );
	if( name == 0 ) name = getenv( "USER" );
	if( name == 0 ){
		plp_snprintf( uid_msg, sizeof(uid_msg), "UID_%d", uid );
		name = uid_msg;
	}
	name = safestrdup(name,__FILE__,__LINE__);
    return( name );
}
