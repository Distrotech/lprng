/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: checkpc.c
 * PURPOSE: check LPD system for problems
 **************************************************************************/

#include "lp.h"
#include "printcap.h"
#include "patchlevel.h"
#include "getcnfginfo.h"
#include "initialize.h"
#include "dump.h"
#include "permission.h"
#include "pathname.h"
#include "checkpc_perm.h"
#include "setupprinter.h"
#include "getqueue.h"
#include "removejob.h"
#include "timeout.h"
#include "fileopen.h"
#include "setuid.h"
/**** ENDINCLUDE ****/

static char *const _id =
"checkpc.c,v 3.15 1998/03/29 18:32:40 papowell Exp";

char checkpc_optstr[] = "ac:flp:rst:A:CD:PT:V";

void usage(void);
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

void Test_port(int ruid, int euid, char *serial_line );

void Make_files( struct dpathname *dpathname, char *printer );
void Scan_printer( char *name, char *error, int errorlen );
int Check_perms( struct dpathname *dpath, int fix, int age, int remove );
void Clean_log( int trunc, char *type, struct dpathname *dpath, char *logfile );
void Make_write_file( struct dpathname *dpathname,
	int flag, char *name, char *printer );

/* pathnames of the spool directory (sd) and control directory (cd) */

int main( int argc, char *argv[], char *envp[] )
{
	int i, c;
	char line[LINEBUFFER];
	char error[LINEBUFFER];
	char *s;			/* end of string */
	int ruid, euid, rgid, egid;
	char *serial_line = 0;

	/* set up the uid state */
	ruid = getuid();
	euid = geteuid();

	Verbose = 1;
	Interactive = 1;
	Is_server = 1;

	Initialize(argc, argv, envp);

	umask( 0 );
	/* set up the uid state */
	ruid = getuid();
	euid = geteuid();
	if( ruid != 0 ){
		Warnmsg("RUID not root - you may not be able to check permissions");
	}

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
			case 'c': Config_file = Optarg; break;
			case 'f': Fix = 1; break;
			case 'l': Nolog = 1; break;
			case 'p': Checkpc_Printcap_path = Optarg; break;
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

	To_root();

	if( Verbose ){
		logDebug( "LPRng version " PATCHLEVEL );
	}

	/* print errors and everything on stdout */
	dup2(1,2);


	/* check to see that all values are in order */
	Check_pc_table();

	/*
	 * set up the local host information and other
	 * things as well
	 * - common to all LPRng stuff
	 */
	Setup_configuration();

	euid = geteuid();
	ruid = getuid();
	egid = getegid();
	rgid = getgid();

	DEBUG0("Effective UID %d, Real UID %d, Effective GID %d, Real GID %d",
		euid, ruid, egid, rgid );
		

    if(DEBUGL1 || Config ){
		plp_snprintf( line, sizeof(line), "Configuration file %s", Config_file );
		dump_parms( line, Pc_var_list );
		dump_parms( "Variables", Lpd_parms );
	}

	if( Printer_perms_path ){
		Free_perms( &Perm_file );
		logDebug( "Checking permission file '%s'", Printer_perms_path );
		if( Get_perms( "all", &Perm_file, Printer_perms_path ) == 0 ){
			logDebug( "Warning: No permissions file" );
		}
/* Debugging for Permissions */
		logDebug( "Freeing Perms" );
		Free_perms( &Perm_file );
		DEBUGFC(DDB4){
			char msg[64];
			plp_snprintf( msg, sizeof(msg),
				"CHECKPC: empty perms database" );
			dump_perm_file( msg, &Perm_file );
		}
		if( Get_perms( "all", &Perm_file, Printer_perms_path ) == 0 ){
			logDebug( "Warning: No permissions file" );
		}
		logDebug( "Done Perms" );
/* */
	} else if( Verbose || DEBUGL0  ){
		logDebug( "Warning: No permissions file" );
	}

	Get_all_printcap_entries();

	if( Lockfile == 0 ){
		logDebug( "Warning: no LPD lockfile" );
	} else {
		char buffer[MAXPATHLEN];
		plp_snprintf( buffer, sizeof(buffer), "%s.%s", Lockfile, Lpd_port );
		logDebug( "LPD lockfile '%s'", buffer );
		s = strrchr( buffer, '/' );
		if( s ){
			struct dpathname newpath;
			*s++ = 0;
			Init_path( &newpath, buffer );
			Check_spool_dir( &newpath, Fix );
			To_root();
			Make_write_file( &newpath, 0, s, (char *)0 );
			if( Fix ){
				Check_perms( &newpath, Fix, 0, 0 );
			}
		}
	}

	if(Truncate){
		logDebug( "Truncating LPD log file '%s'", Logfile );
		Clean_log( Truncate, Logfile, 0, Logfile );
	}

	if( All_list.count ){
		char **line_list;
		DEBUG4("checkpc: using the All_list" );
		line_list = All_list.list;
		c = All_list.count;
		for( i = 0; i < c; ++i ){
			Printer = line_list[i];
			DEBUG4("checkpc: printcap entry [%d of %d] '%s'",
				i, c,  Printer );
			if( Printer == 0 || *Printer == 0 || ispunct( *Printer ) ){
				continue;
			}
			To_root();
			Scan_printer( Printer, error, sizeof(error) );
		}
	} else if( Expanded_printcap_entries.count > 0 ){
		struct printcap_entry *entries;
		DEBUG4("checkpc: using the printcap list" );
		entries = (void *)Expanded_printcap_entries.list;
		c = Expanded_printcap_entries.count;
		for( i = 0; i < c; ++i ){
			Printer = entries[i].names[0];
			DEBUG4("checkpc: printcap entry [%d of %d] '%s'",
				i, c,  Printer );
			if( Printer == 0 || *Printer == 0 || ispunct( *Printer ) ){
				continue;
			}
			To_root();
			Scan_printer( Printer, error, sizeof(error) );
		}
	} else {
		logDebug( "No printcap information!!!" );
	}

	exit(0);
	return(0);
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
	char *s;				/* ACME pointers */
	int i;
	struct control_file *cfp, **cfpp;
	struct stat statb;
	int fd = 0;				/* device file descriptor */
	struct printcap_entry *pc_entry = 0;

	/* do the full search */
	logDebug( "Checking printer '%s'", name );
	error[0] = 0;
	/* get printer information */
	Setup_printer( name, error, errorlen,
		debug_vars, 1, (void *)0, &pc_entry );
	DEBUG3(
	"Scan_printer: Printer '%s', RemoteHost '%s', RemotePrinter '%s', Lp '%s'",
		Printer, RemoteHost, RemotePrinter, Lp_device );
	/* check to see if printer defined in database */
	if( Printer == 0 ) return;
	if(DEBUGL0 ){
		logDebug( "Printcap variable values for '%s'", Printer );
		dump_parms( 0, Pc_var_list );
		s = Linearize_pc_list( pc_entry, "PRINTCAP_ENTRY=" );
		logDebug( "Linearized List %s", s );
	}

	if( Spool_dir ){
		/*
		 * check the permissions of files and directories
		 * Also remove old job or control files
		 */
		To_root();
		s = Clear_path( SDpathname );
		if( Check_spool_dir( SDpathname, 0 ) ){
			logDebug( "Printer '%s' spool dir '%s' needs fixing",
				Printer, s );
			if( Fix ){
				To_root();
				Check_spool_dir( SDpathname, Fix );
			}
		}
		To_root();
		s = Clear_path( CDpathname );
		if( SDpathname != CDpathname
				&& Check_spool_dir( CDpathname, 0 ) ){
			logDebug( "Printer '%s' control dir '%s' does not exist",
				Printer, s );
			if( Fix ){
				To_root();
				Check_spool_dir( CDpathname, Fix );
			}
		}


		/*
		 * create the various files needed for printing
		 */
		To_root();
		Make_files( CDpathname, Printer );

		if( Check_perms( CDpathname, 0, Age, Remove ) ){
			s = Clear_path( CDpathname );
			logDebug( " Need to fix '%s' files, control dir '%s'",
				Printer, s);
			if( Fix ){
				Check_perms( CDpathname, Fix, Age, Remove );
			}
		}
		if( SDpathname != CDpathname &&
			 Check_perms( SDpathname, Fix, Age, Fix ) > 1 ){
			s = Clear_path( SDpathname );
			logDebug( "Cannot check '%s' files, spool dir '%s'",
				Printer, s );
			return;
		}

		/*
		 * get the jobs in the queue
		 */

		To_root();
		Scan_queue( 1, 1 );
		cfpp = (void *)(C_files_list.list);
		for( i = 0; Fix && i < C_files_list.count; ++i ){
			cfp = cfpp[ i ];
			DEBUG2("Scan_printer: printer '%s' job '%s'",Printer,cfp->transfername );

			/*
			 * check for jobs with an error indication
			 */
			if( cfp->error[0] ){
				logDebug("printer '%s' job '%s' has error '%s'",
				Printer, cfp->transfername, cfp->error );
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
				 	fd = Checkwrite_timeout(
							2,s,&statb,Read_write?(O_RDWR):0,0,0 );
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
	int fd, len;
	struct stat statb;
	struct dpathname dp = *dpathname;
	char *s;

	Clear_path( &dp );
	if( flag || name == 0 || *name == 0 ){
		return;
	}
	if( name[0] == '/' ){
		Init_path(&dp, name );
		s = dp.pathname;
		len = strlen(s)-1;
		if( s[len] == '/' ) s[len] = 0;
	} else {
		s = Add2_path( &dp, name, printer );
	}

	DEBUG0("Make_write_file '%s'", s );
	if( Verbose || DEBUGL0 ){
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
	if( check_file( &dp, Fix, 0, 0, 0 ) ){
		logDebug("  ** ownership or permissions problem with '%s'", s );
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
	" -t size[kM]    truncate log files to this size (k=Kbyte, M=Mbytes)",
	" -A age[DHMS]   remove files of form ?f[A-Z][0-9][0-9][0-9] older than",
	"                age, D days (default), H hours, M minutes, S seconds",
	" -C             toggle verbose configuration information",
	" -D debuglevel  set debug level",
	" -P             toggle verbose printcap information",
	" -V             toggle verbose information",
	" -T line        test portability, use serial line device for stty test",
	0
};

void usage(void)
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
		} else if( dpath ){
			s = Add_path( dpath, logfile );
		} else {
			Warnmsg( "bad log file name format '%s'", logfile );
			return;
		}
		logDebug( "Checking %s file '%s'", type, s );
		fd = Checkwrite( s, &statb, O_RDWR, 0, 0 );
		if( fd < 0 ){
			Warnmsg( "cannot open '%s'", s );
			return;
		}
		len = statb.st_size;
		k = (trunc >= 0 && len > trunc);
		s = (k)?"needs":"no";
		logDebug( "   '%s' file %d bytes long: %s truncation",
			type, len, s );
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
				buffer = malloc_or_die( buflen );
			}
			/* seek to the end of the file - trunc */
			if( lseek( fd,  -len, SEEK_END ) < 0 ){
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
