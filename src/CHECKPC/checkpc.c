/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: checkpc.c
 * PURPOSE: check LPD system for problems
 **************************************************************************/

#include "checkpc.h"
#include "patchlevel.h"
#include "printcap.h"
#include "lp_config.h"
#include "permission.h"
#include "setuid.h"
#include "checkpc_perm.h"
#include "decodestatus.h"
#include "getprinter.h"
#include "timeout.h"

static char *const _id =
"$Id: checkpc.c,v 3.3 1996/09/09 14:24:41 papowell Exp papowell $";

char checkpc_optstr[] = "ac:flp:rst:A:CD:PT:V";

void usage();
int getage( char *age );
int getk( char *age );

int Noaccount;
int Nolog;
int Nostatus;
int Fix;
int Printcap;
int Config;
int Age;
int Truncate = -1;
int Remove;
int Test;			/* carry out portability tests */
char *name;

void Make_files( struct dpathname *dpathname, char *printer );
void Scan_printer( char *name, char *error, int errorlen );
void Scan_worker( char *name, char *error, int errorlen );
int Check_perms( struct dpathname *dpath, int fix, int age, int remove );
void Clean_log( int trunc, char *type, struct dpathname *dpath, char *logfile );
void Make_write_file( struct dpathname *dpathname,
	int flag, char *name, char *printer );

struct pc_used pc_used;

/* pathnames of the spool directory (sd) and control directory (cd) */

int main( int argc, char *argv[] )
{
	int i, c;
	char line[LINEBUFFER];
	char error[LINEBUFFER];
	char *s, *end;			/* end of string */
	struct printcap *pc;
	int ruid, euid, rgid, egid;
	char *serial_line = 0;

	/* set up the uid state */
	ruid = getuid();
	euid = geteuid();

	Verbose = 1;
	Interactive = 1;

	Initialize();


    /* scan the argument list for a 'Debug' value */
    Get_debug_parm( argc, argv, checkpc_optstr, debug_vars );
	while( (c = Getopt( argc, argv, checkpc_optstr ) ) != EOF ){
		switch( c ){
			default: usage();
			case 'A':
				if( Optarg){
					Age = getage( Optarg );
				} else {
					usage();
				}
				break;
			case 'D': break;
			case 'V': Verbose = !Verbose; break;
			case 'a': Noaccount = 1; break;
			case 'c': Server_config_file = Optarg; break;
			case 'f': Fix = 1; break;
			case 'l': Nolog = 1; break;
			case 'p': Printcap_path = Optarg; break;
			case 'r': Remove = 1; break;
			case 's': Nostatus = 1; break;
			case 't':
				if( Optarg ){
					 Truncate = getk( Optarg );
				} else {
					usage();
				}
				break;
			case 'C': Config = 1; break;
			case 'P': Printcap = 1; break;
			case 'T': Test = 1; serial_line = Optarg; break;
		}
	}
	if( Test ){
		Test_port( ruid, euid, serial_line );
	}

	if( Verbose ){
		logDebug( "LPRng version " PATCHLEVEL );
	}

	/* print errors and everything on stdout */
	dup2(1,2);

    /* Get default configuration file information */
    Parsebuffer( "default configuration", Default_configuration,
		lpd_all_config, &Config_buffers );
    /* get the configuration file information if there is any */
    if( Allow_getenv ){
		if( UID_root ){
			fprintf( stderr,
			"%s: WARNING- LPD_CONF environment variable option enabled\n"
			"  and running as root!  You have an exposed security breach!\n"
			"  Recompile without -DGETENV or do not run clients as ROOT\n",
			Name );
		}
		if( (s = getenv( "LPD_CONF" )) ){
			Server_config_file = s;
		}
    }

    DEBUG0("main: Configuration file '%s'", Server_config_file?Server_config_file:"NULL" );

    Getconfig( Server_config_file, lpd_all_config, &Config_buffers );

    if( Debug > 5 ) dump_config_list( "LPD Configuration", lpd_all_config );

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

	Reset_daemonuid();
	Setdaemon_group();
	DEBUG4( "DaemonUID %d", DaemonUID );
	
	/* get the fully qualified domain name of host and the
		short host name as well
		FQDN - fully qualified domain name
		Host - actual one to use in H fields
		ShortHost - short host name
		NOTE: on PCs this will be the IP address
	*/

    Get_local_host();
	DEBUG0("Host's Fully Qualified Domain Name: '%s', Short Host '%s'",
		FQDNHost, ShortHost );
	euid = geteuid();
	ruid = getuid();
	egid = getegid();
	rgid = getgid();

	DEBUG0("Effective UID %d, Real UID %d, Effective GID %d, Real GID %d",
		euid, ruid, egid, rgid );
		

    /* expand the information in the configuration file */
    Expandconfig( lpd_all_config, &Config_buffers );

    if( Debug > 1 || Config ){
		plp_snprintf( line, sizeof(line), "Configuration file %s", Server_config_file );
		dump_config_list( line, lpd_all_config );
	}


	/* check to see that all values are in order */
	Check_lpd_config();
	Check_pc_table();
	if( Printer_perms_path ){
		Free_perms( &Perm_file );
		Get_perms( "all", &Perm_file, Printer_perms_path );
	} else if( Verbose || Debug > 0  ){
		logDebug( "No permissions file" );
	}
	i = 0;
	if( Printcap_path && *Printcap_path ){
		++i;
		Getprintcap( &Printcapfile, Printcap_path, 0 );
	}
	if( Lpd_printcap_path && *Lpd_printcap_path ){
		++i;
		Getprintcap( &Printcapfile, Lpd_printcap_path, 0 );
	}
	if( i == 0 && (Verbose || Debug > 0) ){
		logDebug( "No printcap file" );
	}
	Get_printer_vars( "all", error, sizeof(error),
		&Printcapfile, &Pc_var_list, Default_printcap_var,
		(void *)0 );

	if( Lockfile == 0 ){
		logDebug( "Warning: no LPD lockfile" );
	} else {
		logDebug( "LPD lockfile '%s'", Lockfile );
		s = strrchr( Lockfile, '/' );
		if( s ){
			struct dpathname newpath;
			*s++ = 0;
			Init_path( &newpath, Lockfile );
			Check_spool_dir( &newpath, Fix );
			Make_write_file( &newpath, 0, s, (char *)0 );
		}
	}

	if( All_list ){
		static char *all_dup;

		DEBUG8("checkpc: using the All_list '%s'", All_list );
		if( all_dup ){
			free( all_dup );
			all_dup = 0;
		}
		all_dup = safestrdup( All_list );
		for( s = all_dup; s && *s; s = end ){
			end = strpbrk( s, ", \t" );
			if( end ){
				*end++ = 0;
			}
			while( (c = *s) && isspace(c) ) ++s;
			if( c == 0 ) continue;
			Scan_worker( s, error, sizeof(error) );
		}
	} else if( Printcapfile.pcs.count ){
		DEBUG8("checkpc: using the printcap list" );
		for( i = 0; i < Printcapfile.pcs.count; ++i ){
			pc = &((struct printcap *)(Printcapfile.pcs.list))[i];
			Printer = s = pc->pcf->lines.list[pc->name];
			DEBUG8("checkpc: printcap entry [%d of %d] '%s'",
				i, Printcapfile.pcs.count, s );
			if( Debug > 8 ){
				dump_printcap( "PRINTCAP", pc );
			}
			Scan_worker( s, error, sizeof(error) );
		}
	} else {
		logDebug( "No printcap information!!!" );
	}

	exit(0);
}



/***************************************************************************
 * Scan_worker()
 * create a child for the scanning information
 * and call Scan_printer()
 ***************************************************************************/

void Scan_worker( char *name, char *error, int errorlen )
{
	pid_t pid, result;
	plp_status_t status;

	if( (pid = fork()) < 0 ){
		logerr_die( LOG_ERR, "Scan_worker: fork failed" );
	} else if( pid ){
		do{
			result = plp_waitpid( pid, &status, 0 );
			DEBUG8( "Scan_worker: result %d, '%s'",
				result, Decode_status( &status ) );
			removepid( result );
		} while( result != pid );
		return;
	}
	Scan_printer( name, error, errorlen );
	exit(0);
}

/***************************************************************************
 * Scan_printer()
 * process the printer spool queue
 * 1. get the spool queue entry
 * 2. check to see if there is a spool dir
 * 3. perform checks for various files existence and permissions
 ***************************************************************************/
void Scan_printer( char *name, char *error, int errorlen )
{
	static char *s;				/* ACME pointers */
	int i;					/* You know, sometimes C++ does have advantages */
	struct control_file *cfp, **cfpp;
	struct stat statb;
	static int fd = 0;				/* device file descriptor */

	if( Verbose || Debug > 0 ){
		logDebug( "Checking printcap entry '%s'", name );
	}
	/* do the full search */
	error[0] = 0;
	Spool_dir = 0;
	/* get printer information */
	Setup_printer( name, error, errorlen,
		&pc_used, debug_vars, 1, (void *)0 );
	DEBUG4(
	"Scan_printer: Printer '%s', RemoteHost '%s', RemotePrinter '%s', Lp '%s'",
		Printer, RemoteHost, RemotePrinter, Lp_device );
	/* check to see if printer defined in database */
	if( Printer == 0 ) return;
	if( Debug > 1 || Printcap ){
		logDebug( "Printcap variable values for '%s'", Printer );
		dump_parms( 0, Pc_var_list.names );
		s = Linearize_pc_list( &pc_used, (char *)0 );
		logDebug( "Linearized list:" );
		logDebug( "%s", s );
	}

	if( Spool_dir ){
		/*
		 * check the permissions of files and directories
		 * Also remove old job or control files
		 */
		s = Clear_path( SDpathname );
		if( Check_spool_dir( SDpathname, Fix ) > 1 ){
			logDebug( "Printer '%s' spool dir '%s' does not exist",
				Printer, s );
			return;
		}
		s = Clear_path( CDpathname );
		if( SDpathname != CDpathname
				&& Check_spool_dir( CDpathname, Fix ) > 1 ){
			logDebug( "Printer '%s' control dir '%s' does not exist",
				Printer, s );
			return;
		}


		/*
		 * create the various files needed for printing
		 */
		Make_files( CDpathname, Printer );

		s = Clear_path( CDpathname );
		if( Check_perms( CDpathname, Fix, Age, Remove ) > 1 ){
			logDebug( "Cannot check '%s' files, control dir '%s'",
				Printer, s);
			return;
		}
		s = Clear_path( SDpathname );
		if( SDpathname != CDpathname &&
			 Check_perms( SDpathname, Fix, Age, Fix ) > 1 ){
			logDebug( "Cannot check '%s' files, spool dir '%s'",
				Printer, s );
			return;
		}

		/*
		 * get the jobs in the queue
		 */

		Scan_queue( 1 );
		cfpp = (void *)(C_files_list.list);
		for( i = 0; Fix && i < C_files_list.count; ++i ){
			cfp = cfpp[ i ];
			DEBUG3("Scan_printer: printer '%s' job '%s'",Printer,cfp->name );

			/*
			 * check for jobs with an error indication
			 */
			if( cfp->error[0] ){
				logDebug("printer '%s' job '%s' has error '%s'",
				Printer, cfp->name, cfp->error );
				if( Fix ){
					Remove_job( cfp );
				}
			}
		}

		if(!Nolog)Clean_log( Truncate, "log", CDpathname, Log_file );
		if(!Noaccount)Clean_log( Truncate, "accounting",
			CDpathname, Accounting_file );
		if(!Nostatus)Clean_log( Truncate, "filter status",
			CDpathname, Status_file );

		/*
		 * check to see if we have a local or remote printer
		 * do not check if name has a | or % character in it
		 */
		if( RemoteHost == 0 && Server_names == 0 ){
			/* we should have a local printer */
			if( Lp_device == 0 || *Lp_device == 0 ){
				logDebug( "Missing 'lp' entry for local printer" );
			} else if( strpbrk( Lp_device, "|%@" ) == 0  ){
				s = Lp_device;
				if( s[0] != '/' ){
					s = Add_path( CDpathname, Lp_device );
				} 
				if( stat( s, &statb ) < 0 ){
					logDebug( "Cannot stat device '%s'", s );
				} else {
					if( Set_timeout( 2, 0 ) ){
						fd = Checkwrite( s,&statb,Read_write?(O_RDWR):0,0,0 );
					}
					Clear_timeout();
					if( Alarm_timed_out ||  fd < 0 ){
						logDebug( "Cannot open device '%s' error '%s'",
							s, Errormsg(errno) );
					} else {
						close(fd);
					}
				}
			}
		}
	}
}

/***************************************************************************
 * Make_files( char *dir,  - use this directory
 *    flag - do not create file
 *    name - file name
 *    printer - if non-zero, append to end of file name
 ***************************************************************************/

void Make_write_file( struct dpathname *dpathname,
	int flag, char *name, char *printer )
{
	int fd;
	struct stat statb;
	char *s;

	if( flag || name == 0 || *name == 0 ){
		return;
	}
	if( name[0] == '/' ){
		s = name;
	} else {
		s = Add2_path( dpathname, name, printer );
	}

	DEBUG1("Make_write_file '%s'", s );
	if( Verbose || Debug > 0 ){
		logDebug( "  checking file '%s'", s );
	}

	if( Fix ){
		if( (fd = Checkwrite( s, &statb, O_RDWR, 1, 0 )) < 0 ){
			Errorcode = 1;
			logDebug( "  ** cannot create '%s'", s );
		}
		close( fd );
	}
	if( (fd = Checkread( s, &statb )) < 0 ){
		logDebug( "  ** cannot open '%s'", s );
	}
	close( fd );
}

/***************************************************************************
 * Make_files( char *dir, char *printer )
 *  create the various status, log, accounting files in spool dir
 ***************************************************************************/
void Make_files( struct dpathname *dpathname,  char *printer )
{
	Make_write_file( dpathname, 0, "control.", printer );
	Make_write_file( dpathname, 0, "status.", printer );
	Make_write_file( dpathname, Nostatus, Status_file, 0 );
	Make_write_file( dpathname, Nolog, Log_file, 0 );
	Make_write_file( dpathname, Noaccount, Accounting_file, 0 );
}

char *msg[] = {
	"checkpc [-acflsCPV] [-Aage] [-c file] [-p file] [-D debuglevel]",
	"   Check printcap for readability.",
	"   Printcapfile can be colon separated file list.", 
	"   If no file is specified, checks the printcap_path values",
	"   from configuration information."
	" Option:",
	" -a             do not create accounting info (:af:) file",
	" -c configfile  use this file for configuration information",
	" -f             toggle fixing file status",
	" -l             do not create logging info (:lf:) file",
	" -p printcapfile  use this file for printcap information",
	" -r             remove job files older than -A age seconds",
	" -s             do not create filter status (:ps:) info file",
	" -t size        truncate log files to this size (in bytes)",
	" -A age[DHMS]   remove files of form ?f[A-Z][0-9][0-9][0-9] older than",
	"                age, D days (default), H hours, M minutes, S seconds",
	" -C             toggle verbose configuration information",
	" -D debuglevel  set debug level",
	" -P             toggle verbose printcap information",
	" -V             toggle verbose information",
	" -T line        test portability, use serial line device for stty test",
	0
};

void usage()
{
	char **s;
	for( s = msg; *s; ++s ){
		fprintf( stderr, "%s\n", *s );
	}
	exit(1);
}

int getage( char *age )
{
	int t;
	char *end = age;
	t = strtol( age, &end, 10 );
	if( t && end != age ){
		switch( *end ){
			default: t = 0; break;

			case 0:
			case 'd': case 'D': t *= 24;
			case 'h': case 'H': t *= 60;
			case 'm': case 'M': t *= 60;
			case 's': case 'S': break;
		}
	}
	if( t == 0 ){
		fprintf( stderr, "Bad format for age '%s'", age );
		usage();
	}
	return t;
}


int getk( char *age )
{
	int t;
	char *end = age;
	t = strtol( age, &end, 10 );
	if( end != age ){
		switch( *end ){
			default:
				fprintf( stderr, "Bad format for number '%s'", age );
				usage();
			case 0: break;
			case 'k': case 'K': t *= 1024; break;
			case 'm': case 'M': t *= (1024*1024); break;
		}
	}
	return t;
}

/***************************************************************************
 * Clean_log - truncate the logfile to the length (in Kbytes) specified.
 *
 ***************************************************************************/
void Clean_log( int trunc, char *type, struct dpathname *dpath, char *logfile )
{
	char *s;			/* ACME, pointers with a sharp point! */
	int fd;				/* log file */
	int j, k;		/* ACME, for integers that COUNT */
	struct stat statb;	/* for a file */
	int len;			/* length of file */

	if( logfile && *logfile ){
		if( logfile[0] == '/' ){
			s = logfile;
		} else {
			s = Add_path( dpath, logfile );
		}
		logDebug( "Checking %s file '%s'", type, s );
		fd = Checkwrite( s, &statb, O_RDWR, 0, 0 );
		if( fd < 0 ){
			Warnmsg( "cannot open '%s'", s );
			return;
		}
		k = trunc >= 0 && statb.st_size > trunc;
		logDebug( "   %s file %d bytes long: %s truncation",
			type, statb.st_size, k?"needs":"no");
		if( trunc == 0 && statb.st_size ){
			logDebug( "   truncating to 0 bytes long" );
			if( ftruncate( fd, 0 ) < 0 ){
				logerr_die( LOG_ERR, "Clean_log: ftruncate failed" );
			}
		} else if( trunc > 0 && statb.st_size > trunc ){
			static char *buffer;
			static int buflen;
			len = trunc+1;
			logDebug( "   truncating to %d bytes long", trunc );

			if( buflen < len+1 ){
				if( buffer ) free(buffer);
				buffer = 0;
				buflen = len+1;
				malloc_or_die( buffer, buflen );
			}
			/* seek to the end of the file - trunc */
			if( lseek( fd,  (off_t)(-len), SEEK_END ) < 0 ){
				logerr_die( LOG_ERR, "Clean_log: lseek on '%s' failed", s );
			}
			for( j = len, s = buffer;
				j > 0 && (k = read( fd, s, j )) > 0;
				s += k, j -= k );
			*s = 0;
			/* now find the first \n */
			if( (s = strchr( buffer, '\n' )) ){
				s++;
			} else {
				s = buffer;
			}
			logDebug( "   writing %d bytes", strlen(s) );
			if( ftruncate( fd, 0 ) < 0 ){
				logerr_die( LOG_ERR, "Clean_log: ftruncate failed" );
			}
			Write_fd_str( fd, s );
		}
		close(fd);
	}
}
