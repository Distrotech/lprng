/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: checkpc.c,v 5.2 1999/10/24 22:16:13 papowell Exp papowell $";



#include "lp.h"
#include "getopt.h"
#include "checkpc.h"
#include "patchlevel.h"
#include "getprinter.h"
#include "getqueue.h"
#include "initialize.h"
#include "lockfile.h"
#include "fileopen.h"
#include "child.h"
#include "stty.h"
#include "proctitle.h"
#include "lpd_remove.h"

/**** ENDINCLUDE ****/

 
 int Noaccount;
 int Nolog,  Nostatus,  Fix,  Age, Printcap, Config;
 int Truncate = -1;
 int Remove;
 int Test;			/* carry out portability tests */
 char *User_specified_printer;
 time_t Current_time;
 int Check_path_list( char *plist, int allow_missing );


/* pathnames of the spool directory (sd) and control directory (cd) */

int main( int argc, char *argv[], char *envp[] )
{
	int i, c, found_pc;
	char *path;		/* end of string */
	int ruid, euid, rgid, egid;
	char *printcap;
	char *serial_line = 0;
	struct line_list values;
	char *s, *t;

	s = t = printcap = 0;
	Init_line_list(&values);
	/* set up the uid state */
	To_root();
	time(&Current_time);

	Verbose = 0;
	Warnings = 1;
	Is_server = 1;
	/* send trace on stdout */ 
	dup2(1,2);

#ifndef NODEBUG
	Debug = 0;
#endif


    /* scan the argument list for a 'Debug' value */
	while( (c = Getopt( argc, argv, "aflprst:A:CD:P:T:V" ) ) != EOF ){
		switch( c ){
			default: usage();
			case 'a': Noaccount = 1; break;
			case 'f': Fix = 1; break;
			case 'l': Nolog = 1; break;
			case 'r': Remove = 1; break;
			case 's': Nostatus = 1; break;
			case 't':
				if( Optarg ){
					 Truncate = getk( Optarg );
				} else {
					usage();
				}
				break;
			case 'A':
				if( Optarg){
					Age = getage( Optarg );
				} else {
					usage();
				}
				break;
			case 'C': ++Config; break;
			case 'D': Parse_debug(Optarg,1); break;
			case 'V': ++Verbose; break;
			case 'p': ++Printcap; break;
			case 'P': User_specified_printer = Optarg; break;
			case 'T':
				To_user();
				initsetproctitle( argc, argv, envp );
				Test_port( getuid(), geteuid(), serial_line );
				exit(0);
				break;
		}
	}

	if( Verbose ){
		if(Verbose)Msg( "LPRng version " PATCHLEVEL );
	}

	Initialize(argc, argv, envp);
	Setup_configuration();


	To_root();

	if(Verbose)Msg("Checking for configuration files '%s'", Config_file_DYN);
	found_pc = Check_path_list( Config_file_DYN, 0 );
	if( found_pc == 0 ){
		Warnmsg("No configuration file found in '%s'", Config_file_DYN );
	}

	if(Verbose)Msg("Checking for printcap files '%s'", Printcap_path_DYN);
	found_pc = Check_path_list( Printcap_path_DYN, 0 );
	if( Lpd_printcap_path_DYN ){
		if(Verbose)Msg("Checking for lpd only printcap files '%s'", Lpd_printcap_path_DYN);
		found_pc += Check_path_list( Lpd_printcap_path_DYN, 1 );
	}
	if( found_pc == 0 ){
		Warnmsg("No printcap files!!!" );
	}

	Get_all_printcap_entries();

	euid = geteuid();
	ruid = getuid();
	egid = getegid();
	rgid = getgid();


	DEBUG1("Effective UID %d, Real UID %d, Effective GID %d, Real GID %d",
		euid, ruid, egid, rgid );
	if(Verbose)Msg(" DaemonUID %d, DaemonGID %d", DaemonUID, DaemonGID );


	if(Verbose || Config)Msg("Using Config file '%s'", Config_file_DYN);
	if( Config ){ 
		Msg("CONFIG START");
		s = Join_line_list(&Config_line_list,"\n");
		Write_fd_str(1,s);
		Msg("CONFIG END");
		if( s ) free(s); s = 0;
	}
	/* print errors and everything on stdout */

	if( Lockfile_DYN == 0 ){
		Warnmsg( "Warning: no LPD lockfile" );
	} else if( Lpd_port_DYN == 0 ){
		Warnmsg( "Warning: no LPD port" );
	} else {
		int oldfile = Spool_file_perms_DYN;
		Spool_file_perms_DYN = 0644;
		path = safestrdup3( Lockfile_DYN,".", Lpd_port_DYN, __FILE__, __LINE__ ); 
		if(Verbose)Msg( "LPD lockfile '%s'", path );
		if( path[0] != '/' || !(s = safestrrchr(path,'/')) ){
			Warnmsg( "Warning: LPD lockfile '%s' not absolute path", path );
		} else {
			*s = 0;
			if( *path && Check_spool_dir( path, 0 ) ){
				Warnmsg( "  LPD Lockfile directory '%s' needs fixing",
				path);
			} else {
				*s = '/';
				To_root();
				Make_write_file( 0, path, 0 );
			}
		}
		Spool_file_perms_DYN = oldfile;
	}

	if( Verbose ) Show_info();

	if(Verbose)Msg("Checking printcap info");
	if( User_specified_printer ){
		Set_DYN(&Printer_DYN,User_specified_printer);
		Scan_printer();
	} else {
		for( i = 0; i < All_line_list.count; ++i ){
			Set_DYN(&Printer_DYN,All_line_list.list[i]);
			Scan_printer();
		}
	}

	if(Verbose){
		Msg("");
		Msg("*** Checking for client info ****");
		Msg("");
	}
	Is_server = 0;
	Initialize(argc, argv, envp);
	Setup_configuration();
	Get_all_printcap_entries();
	if(Verbose)Show_info();
	for( i = 0; i < All_line_list.count; ++i ){
		Set_DYN(&Printer_DYN,All_line_list.list[i]);
		Fix_Rm_Rp_info();
		if(Verbose){
			Msg("Printer '%s': %s@%s",
				Printer_DYN, RemotePrinter_DYN, RemoteHost_DYN);
		}
	}

	return(0);
}

void Show_info(void)
{
	int i;
	char *s, *t, *w, *printcap;
	s = t = w = printcap = 0;
	s = Join_line_list(&PC_names_line_list,"\n :");
	printcap = safestrdup3("Names","\n :",s,__FILE__,__LINE__);
	if( (w = safestrrchr(printcap,' ')) ) *w = 0;
	if( Write_fd_str( 1, printcap ) < 0 ) cleanup(0);

	if(s) free(s); s = 0;
	if( t ) free(t); t = 0;
	if(printcap) free(printcap); printcap = 0;
	s = Join_line_list(&All_line_list,"\n :");
	printcap = safestrdup3("All","\n :",s,__FILE__,__LINE__);
	if( (w = safestrrchr(printcap,' ')) ) *w = 0;
	if( Write_fd_str( 1, printcap ) < 0 ) cleanup(0);


	if( Write_fd_str( 1,"Printcap Information\n") < 0 ) cleanup(0);
	for( i = 0; i < All_line_list.count; ++i ){
		Set_DYN(&Printer_DYN,All_line_list.list[i]);
		Fix_Rm_Rp_info();
		if( s ) free(s); s = 0;
		if( t ) free(t); t = 0;
		if( printcap ) free(printcap); printcap = 0;
		t = Join_line_list(&PC_alias_line_list,"|");
		s = Join_line_list(&PC_entry_line_list,"\n :");
		if( s && t ){
			if( (w = safestrrchr(t,'|')) ) *w = 0;
			printcap = safestrdup3(t,"\n :",s,__FILE__,__LINE__);
			if( (w = safestrrchr(printcap,' ')) ) *w = 0;
			if( Write_fd_str( 1, printcap ) < 0 ) cleanup(0);
		}
	}
	if( s ) free(s); s = 0;
	if( t ) free(t); t = 0;
	if( printcap ) free(printcap); printcap = 0;
}

/***************************************************************************
 * Scan_printer()
 * process the printer spool queue
 * 1. get the spool queue entry
 * 2. check to see if there is a spool dir
 * 3. perform checks for various files existence and permissions
 ***************************************************************************/

void Scan_printer(void)
{
	DIR *dir;
	struct dirent *d;
	struct line_list files, *lp;
	char *s, *t, *u, *w, *id, *cf_name, *path;		/* ACME pointers */
	int count, jobfile, datafile;
	struct stat statb;
	int fd = 0;				/* device file descriptor */
	int i, j, n, age;
	char error[SMALLBUFFER];
	int errorlen = sizeof(error);
	struct job job;
	time_t delta;

	Init_line_list(&files);
	Init_job(&job);
	error[0] = 0;

	if(Verbose)Msg( "Checking printer '%s'", Printer_DYN );

	/* get printer information */
	Setup_printer( Printer_DYN, error, errorlen);
	if( !Is_server ) return;
	DEBUG3(
	"Scan_printer: Printer_DYN '%s', RemoteHost_DYN '%s', RemotePrinter_DYN '%s', Lp '%s'",
		Printer_DYN, RemoteHost_DYN, RemotePrinter_DYN, Lp_device_DYN );
	/* check to see if printer defined in database */
	if( Spool_dir_DYN == 0 ){
		t = Join_line_list(&PC_alias_line_list,"|");
		if( t && (w = safestrrchr(t,'|')) ) *w = 0;
		s = Join_line_list(&PC_entry_line_list,"\n :");
		u = safestrdup3(t,"\n :",s,__FILE__,__LINE__);
		if( (w = safestrrchr(u,' ')) ) *w = 0;
		Warnmsg("%s: Bad printcap entry - missing 'sd' or 'client' entry?",
			Printer_DYN);
		Msg("%s", u );
		if(s) free(s); s = 0;
		if(t) free(t); t = 0;
		if(u) free(u); u = 0;
		return;
	}

	/*
	 * check the permissions of files and directories
	 * Also remove old job or control files
	 */
	if( Printcap ){
		Msg("PRINTCAP START");
		t = Join_line_list(&PC_alias_line_list,"|");
		if( t && (w = safestrrchr(t,'|')) ) *w = 0;
		s = Join_line_list(&PC_entry_line_list,"\n :");
		u = safestrdup3(t,"\n :",s,__FILE__,__LINE__);
		if( (w = safestrrchr(u,'\n')) ) *w = 0;
		Msg("%s", u );
		if(s) free(s); s = 0;
		if(t) free(t); t = 0;
		if(u) free(u); u = 0;
		Msg("PRINTCAP END");
	}
	if( Check_spool_dir( Spool_dir_DYN, 1 ) > 1 ){
		Warnmsg( "  Printer_DYN '%s' spool dir '%s' needs fixing",
			Printer_DYN, Spool_dir_DYN );
		return;
	}
	if( !(dir = opendir( Spool_dir_DYN )) ){
		Warnmsg( "  Printer_DYN '%s' spool dir '%s' cannot be scanned '%s'",
			Printer_DYN, Spool_dir_DYN, Errormsg(errno) );
		return;
	}
	s = 0;
	while( (d = readdir(dir)) ){
		if(s) free(s); s = 0;
		cf_name = d->d_name;
		if( safestrcmp( cf_name, "." ) == 0
			|| safestrcmp( cf_name, ".." ) == 0 ) continue;
		DEBUG2("Scan_printer: file '%s'", cf_name );
		s = Make_pathname(Spool_dir_DYN,cf_name);
		if( stat(s,&statb) == -1 ){
			Warnmsg( "  stat of file '%s' failed '%s'",
				s, Errormsg(errno) );
			continue;
		}
		delta = Current_time - statb.st_mtime;
		jobfile = ( !safestrncmp( cf_name,"cf",2 )
			|| !safestrncmp( cf_name,"hf",2 )
			|| !safestrncmp( cf_name,"df",2 )
			|| !safestrncmp( cf_name,"temp",4 ) );
		if( delta > Age && statb.st_size == 0 ){
			if(Verbose)Msg( "  file '%s', zero length file unchanged in %ld hours",
				cf_name, (long)((delta+3599)/3600) );
			if( Remove ) unlink(s);
		}
		if( jobfile ){
			Add_line_list(&files,cf_name,0,1,1);
		}
		Check_file( s, Fix, 0, 0 );
	}
	closedir(dir);
	if(s) free(s); s = 0;
	Free_line_list(&files);

	Make_write_file( Spool_dir_DYN, Queue_control_file_DYN, 0 );
	if( !Find_first_key(&files,s,Value_sep,&n ) ){
		Remove_line_list(&files,n);
	}

	Make_write_file( Spool_dir_DYN, Queue_status_file_DYN, 0 );
	if( !Find_first_key(&files,s,Value_sep,&n ) ){
		Remove_line_list(&files,n);
	}
	Fix_clean(Status_file_DYN,Nostatus,&files);
	Fix_clean(Log_file_DYN,Nolog,&files);
	Fix_clean(Accounting_file_DYN,Noaccount,&files);

	/*
	 * get the jobs in the queue
	 */
	Free_line_list( &Sort_order );
	Scan_queue( Spool_dir_DYN, &Spool_control, &Sort_order,0,0,0,0 );

	for( count = 0; count < Sort_order.count; ++count ){
		Free_job(&job);
		s = Sort_order.list[count];
		if( (s = safestrchr(Sort_order.list[count],';')) ){
			Split(&job.info,s+1,";",1,Value_sep,1,1,1);
		}
		if( Setup_cf_info( Spool_dir_DYN, 0, &job ) ){
			id = Find_str_value(&job.info,TRANSFERNAME,Value_sep);
			Warnmsg( "bad job '%s'", id );
			continue;
		}
		id = Find_str_value(&job.info,TRANSFERNAME,Value_sep);
		if(Verbose)Msg("  checking job '%s'", id );
		age = 0;
		path = Make_pathname(Spool_dir_DYN,id);
		if( stat( path, &statb ) == 0 ){
			age = Current_time - statb.st_mtime;
		}
		if( path ) free(path); path = 0;
		/* now we decide if we want to remove the job */
		if( Remove && age > Age ){
			if(Verbose)Msg(" removing job '%s'", id );
			Remove_job( &job );
		}
		/* now we remove the files we have for the job in the files list */
		id = Find_str_value(&job.info,TRANSFERNAME,Value_sep);
		if( !Find_first_key(&files,id,Value_sep,&n ) ){
			Remove_line_list(&files,n);
		}
		id = Find_str_value(&job.info,HF_NAME,Value_sep);
		if( (s = safestrrchr(id,'/')) ){
			++s;
		} else {
			s = id;
		}
		if( !Find_first_key(&files,s,Value_sep,&n ) ){
			Remove_line_list(&files,n);
		}
		for( datafile = 0; datafile < job.datafiles.count; ++datafile ){
			lp = (void *)job.datafiles.list[datafile];
			id = Find_str_value(lp,OPENNAME,Value_sep);
			if( (s = safestrrchr(id,'/')) ){
				++s;
			} else {
				s = id;
			}
			if( !Find_first_key(&files,s,Value_sep,&n ) ){
				Remove_line_list(&files,n);
			}
		}
	}
	Free_job(&job);

	if( files.count ){
		Warnmsg("Unexplained job files");
		for( i = 0; i < files.count; ++i ){
			s = files.list[i];
			Msg(" %s", s );
			if( Remove || Fix ){
				path = Make_pathname( Spool_dir_DYN, s );
				if( stat( path, &statb ) == 0 ){
					age = Current_time - statb.st_mtime;
					if( age > Age || Fix ){
						if(Verbose)Msg("  removing '%s'", s );
						unlink(path);
					}
				}
			}
		}
	}
	Free_line_list(&files);

	/*
	 * check to see if we have a local or remote printer
	 * do not check if name has a | or % character in it
	 */

		/* we should have a local printer */
	if( Server_queue_name_DYN == 0
		&& RemotePrinter_DYN == 0 && Lp_device_DYN == 0 ){
		Warnmsg( "Missing 'lp' and 'rp' entry for local printer" );
	} else if( Lp_device_DYN && safestrpbrk( Lp_device_DYN, "|%@" ) == 0  ){
		s = Lp_device_DYN;
		fd = -1;
		if( s[0] != '/' ){
			Warnmsg( "lp device not absolute  pathname '%s'", s );
		} else if( (fd = Checkwrite(s,&statb,0,0,0)) < 0 ){
			Warnmsg( "cannot open lp device '%s' - %s", s, Errormsg(errno) );
		}
		if( fd >= 0 ) close(fd);
	}
	Free_line_list(&files);

	/* check the filters */
	strcpy(error,"xf");
	for( i = 'a'; i <= 'z'; ++i ){
		if( safestrchr("afls",i) ) continue;
		error[0] = i;
		Free_line_list(&files);
		s = Find_str_value(&PC_entry_line_list,error,Value_sep);
		if(!s) s = Find_str_value(&Config_line_list,error,Value_sep);
		Split(&files,s,Whitespace,0,0,0,0,0);
		if( files.count ){
			if(Verbose)Msg("  '%s' filter '%s'", error, s );
			for( j = 0; j < files.count; ++j ){
				s = files.list[j];
				if( cval(s) == '|' ) ++s;
				if( !safestrcasecmp(s,"$-")  ) continue;
				if( !safestrcasecmp(s,"root") ) continue;
				break;
			}
			if(Verbose)Msg("    executable '%s'", s );
			if( stat(s,&statb) ){
				Warnmsg("cannot stat '%s' filter '%s' - %s", error,
				s, Errormsg(errno) );
			} else if(!S_ISREG(statb.st_mode)) {
				Warnmsg("'%s' filter '%s' not a file", error);
			} else {
				n = statb.st_mode & 0111;
				if( !(n & 0001)
					&& !((n & 0010) && statb.st_gid == DaemonGID )
					&& !((n & 0100) && statb.st_uid == DaemonUID ) ){
					Warnmsg("'%s' filter '%s' does not have execute perms",
						error, s );
				}
			}
		}
	}
	Free_line_list(&files);

	strcpy(error,"filter");
	s = Find_str_value(&PC_entry_line_list,error,Value_sep);
	if(!s) s = Find_str_value(&Config_line_list,error,Value_sep);
	Split(&files,s,Whitespace,0,0,0,0,0);
	if( files.count ){
		s = files.list[0];
		if(Verbose)Msg("  '%s' filter '%s'", error, s );
		if( stat(s,&statb) ){
			Warnmsg("cannot stat '%s' filter '%s' - %s", error,
			s, Errormsg(errno) );
		} else if(!S_ISREG(statb.st_mode)) {
			Warnmsg("'%s' filter '%s' not a file", error);
		} else {
			n = statb.st_mode & 0111;
			if( !(n & 0001)
				&& !((n & 0010) && statb.st_gid == DaemonGID )
				&& !((n & 0100) && statb.st_uid == DaemonUID ) ){
				Warnmsg("'%s' filter '%s' does not have execute perms",
					error, s );
			}
		}
	}
	Free_line_list(&files);

	strcpy(error,"bp");
	s = Find_str_value(&PC_entry_line_list,error,Value_sep);
	if(!s) s = Find_str_value(&Config_line_list,error,Value_sep);
	Split(&files,s,Whitespace,0,0,0,0,0);
	if( files.count ){
		s = files.list[0];
		if(Verbose)Msg("  '%s' filter '%s'", error, s );
		if( stat(s,&statb) ){
			Warnmsg("cannot stat '%s' filter '%s' - %s", error,
			s, Errormsg(errno) );
		} else if(!S_ISREG(statb.st_mode)) {
			Warnmsg("'%s' filter '%s' not a file", error);
		} else {
			n = statb.st_mode & 0111;
			if( !(n & 0001)
				&& !((n & 0010) && statb.st_gid == DaemonGID )
				&& !((n & 0100) && statb.st_uid == DaemonUID ) ){
				Warnmsg("'%s' filter '%s' does not have execute perms",
					error, s );
			}
		}
	}
	Free_line_list(&files);
}

/***************************************************************************
 * Make_write_file(
 *    dir - directory name
 *    name - file name
 *    printer - if non-zero, append to end of file name
 ***************************************************************************/

void Make_write_file( char *dirpath, char *name, char *printer )
{
	int fd;
	struct stat statb;
	char *s, *path;

	if( name == 0 || *name == 0 ){
		return;
	}
	s = safestrdup2(name,printer,__FILE__,__LINE__);
	path = Make_pathname(dirpath,s);
	DEBUG1("Make_write_file '%s'", path );
	if( Verbose || DEBUGL1 ){
		if(Verbose)Msg( "  checking '%s' file", s );
	}

	if( (fd = Checkread( path, &statb )) < 0 ){
		if( Fix ){
			fd = Checkwrite( path, &statb, O_RDWR, 1, 0 );
		}
	}
	if( fd < 0 ){
		Warnmsg( " ** cannot open '%s' - '%s'", path, Errormsg(errno) );
	} else if( Check_file( path, Fix, 0, 0 ) ){
		Warnmsg("  ** ownership or permissions problem with '%s'", path );
	}
	if( s ) free(s); s = 0;
	if( path ) free(path); path = 0;
	if( fd >= 0 ) close(fd); fd = -1;
}

 char *usemsg[] = {
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
	for( s = usemsg; *s; ++s ){
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

void Clean_log( int trunc, char  *dpath, char *logfile )
{
	char *path, *xpath;
	int fd, dfd;				/* log file */
	int k, n;		/* ACME, for integers that COUNT */
	struct stat statb;	/* for a file */
	double len;			/* length of file */
	char buffer[LARGEBUFFER];
	char *sx;

	path = xpath = 0;
	fd = dfd = -1;
	while( logfile && *logfile ){
		if( logfile[0] == '/' ){
			path = safestrdup(logfile,__FILE__,__LINE__);
		} else {
			path = Make_pathname(dpath,logfile);
		}
		fd = Checkwrite( path, &statb, O_RDWR, 0, 0 );
		if( fd < 0 ){
			Warnmsg( "  cannot open '%s' - '%s'", path, Errormsg(errno) );
			break;
		}
		len = statb.st_size/1024;
		k = (trunc >= 0 && len > trunc);
		sx = (k)?"needs":"no";
		plp_snprintf(buffer,sizeof(buffer)," to %d K", trunc );
		if(Verbose)Msg( "  cleaning '%s' file, %0.0f bytes long: %s truncation%s",
			logfile, len, sx, (trunc>=0)?buffer:"" );
		if( k == 0 ) break;
		if( trunc == 0 ){
			Warnmsg( "   truncating to 0 bytes long" );
			if( ftruncate( fd, 0 ) < 0 ){
				logerr_die( LOG_ERR, "Clean_log: ftruncate failed" );
			}
		} else {
			Warnmsg( "   truncating to %d bytes long", trunc );
			xpath = safestrdup2(path,".x",__FILE__,__LINE__);
			dfd = Checkwrite( xpath, &statb, 0,1,0);
			if( lseek(fd,-(trunc *1024),SEEK_END) == -1 ){
				logerr_die( LOG_ERR, "Clean_log: lseek failed" );
			}
			while( (n = read(fd,buffer,sizeof(buffer)-1)) >0 ){
				buffer[n] = 0;
				if((sx = safestrchr(buffer,'\n')) ){
					*sx++ = 0;
					Write_fd_str( dfd, sx );
					break;
				}
			}
			while( (n = read(fd,buffer,sizeof(buffer)-1)) >0 ){
				buffer[n] = 0;
				Write_fd_str( dfd, buffer );
			}
			if( rename( xpath, path ) == -1 ){
				logerr_die( LOG_ERR, "Clean_log: rename '%s' to '%s' failed",
					xpath, path );
			}
		}
		break;
	}
	if( dfd >= 0 ) close( dfd ); dfd = -1;
	if( fd >= 0 ) close( fd ); fd = -1;
	if(path) free(path); path = 0;
	if(xpath) free(xpath); xpath = 0;
}

/***************************************************************************
 * Check_file( char  *dpath   - pathname of directory/files
 *    int fix  - fix or check
 *    int t    - time to compare against
 *    int age  - maximum age of file
 ***************************************************************************/

int Check_file( char  *path, int fix, int age, int rmflag )
{
	struct stat statb;
	int old;
	int err = 0;

	DEBUG4("Check_file: '%s', fix %d, time 0x%x, age %d",
		path, fix, Current_time, age );

	if( stat( path, &statb ) ){
		if(Verbose)Msg( "cannot stat file '%s', %s", path, Errormsg(errno) );
		err = 1;
		return( err );
	}
	if( S_ISDIR( statb.st_mode ) ){
		Warnmsg("'%s' is a directory, not a file", path );
		return(2);
	} else if( !S_ISREG( statb.st_mode ) ){
		Warnmsg( "'%s' not a regular file - unusual", path );
		return(2) ;
	}
	if( statb.st_uid != DaemonUID || statb.st_gid != DaemonGID ){
		Warnmsg( "owner/group of '%s' are %d/%d, not %d/%d", path,
			(int)(statb.st_uid), (int)(statb.st_gid), DaemonUID, DaemonGID );
		if( fix ){
			if( Fix_owner( path ) ) err = 2;
		}
	}
	if( 07777 & (statb.st_mode ^ Spool_file_perms_DYN) ){
		Warnmsg( "permissions of '%s' are 0%o, not 0%o", path,
			statb.st_mode & 07777, Spool_file_perms_DYN );
		if( fix ){
			if( Fix_perms( path, Spool_file_perms_DYN ) ) err = 1;
		}
	}
	if( age ){
		old = Current_time - statb.st_ctime;
		if( old >= age ){
			fprintf( stdout,
				"file %s age is %d secs, max allowed %d secs\n",
				path, old, age );
			if( rmflag ){
				fprintf( stdout, "removing '%s'\n", path );
				if( unlink( path ) == -1 ){
					Warnmsg( "cannot remove '%s', %s", path,
						Errormsg(errno) );
				}
			}
		}
	}
	return( err );
}

int Fix_create_dir( char  *path, struct stat *statb )
{
	char *s;
	int err = 0;

	s = path+strlen(path)-1;
	if( *s == '/' ) *s = 0;
	if( stat( path, statb ) == 0 ){
		if( !S_ISDIR( statb->st_mode ) ){
			if( !S_ISREG( statb->st_mode ) ){
				Warnmsg( "not regular file '%s'", path );
				err = 1;
			} else if( unlink( s ) ){
				Warnmsg( "cannot unlink file '%s', %s",
					path, Errormsg(errno) );
				err = 1;
			}
		}
	}
	/* we don't have a directory */
	if( stat( path, statb ) ){
		To_root();
		if( mkdir( path, Spool_dir_perms_DYN ) ){
			Warnmsg( "mkdir '%s' failed, %s", path, Errormsg(errno) );
			err = 1;
		} else {
			err = Fix_owner( path );
		}
		To_user();
	}
	return( err );
}

int Fix_owner( char *path )
{
	int status = 0;
	int err;

	To_root();
	if( geteuid() == 0 ){
		Warnmsg( "  changing ownership '%s' to %d/%d", path, DaemonUID, DaemonGID );
		status = chown( path, DaemonUID, DaemonGID );
		To_user();
		err = errno;
		if( status ){
			Warnmsg( "chown '%s' failed, %s", path, Errormsg(err) );
		}
		errno = err;
	}
	return( status != 0 );
}

int Fix_perms( char *path, int perms )
{
	int status;
	int err;

	To_root();
	status = chmod( path, perms );
	To_daemon();
	err = errno;

	if( status ){
		Warnmsg( "chmod '%s' to 0%o failed, %s", path, perms,
			 Errormsg(err) );
	}
	errno = err;
	return( status != 0 );
}


/***************************************************************************
 * Check to see that the spool directory exists, and create it if necessary
 ***************************************************************************/

int Check_spool_dir( char *path, int owner )
{
	struct stat statb;
	struct line_list parts;
	char *pathname = 0;
	int err = 0, i;

	/* get the required group and user ids */
	
	if(Verbose)Msg(" Checking directory: '%s'", path );
	pathname = path+strlen(path)-1;
	if( pathname[0] == '/' ) *pathname = 0;
	Init_line_list(&parts);
	if( path == 0 || path[0] != '/' || strstr(path,"/../") ){
		Warnmsg("bad spooldir path '%s'", path );
		return(2);
	}

	pathname = 0;
	Split(&parts,path,"/",0,0,0,0,0);

	for( i = 0; i < parts.count; ++i ){
		pathname = safeextend3(pathname,"/",parts.list[i],__FILE__,__LINE__);
		if(Verbose) Msg("   directory '%s'", pathname);
		if( stat( pathname, &statb ) || !S_ISDIR( statb.st_mode ) ){
			if( Fix ){
				if( Fix_create_dir( pathname, &statb ) ){
					return(2);
				}
			} else {
				Warnmsg(" bad directory - %s", pathname );
				return( 2 );
			}
		}
		To_daemon();
		if( stat( pathname, &statb ) == 0 && S_ISDIR( statb.st_mode )
			&& chdir( pathname ) == -1 ){
			if( !Fix ){
				Warnmsg( "cannot chdir to '%s' as UID %d, GRP %d - '%s'",
					pathname, geteuid(), getegid(), Errormsg(errno) );
			} else {
				Fix_perms( pathname, Spool_dir_perms_DYN );
				if( chdir( pathname ) == -1 ){
					Warnmsg( "Permission change FAILED: cannot chdir to '%s' as UID %d, GRP %d - '%s'",
					pathname, geteuid(), getegid(), Errormsg(errno) );
					Fix_owner( pathname );
					Fix_perms( pathname, Spool_dir_perms_DYN );
				}
				if( chdir( pathname ) == -1 ){
					Warnmsg( "Owner and Permission change FAILED: cannot chdir to '%s' as UID %d, GRP %d - '%s'",
					pathname, geteuid(), getegid(), Errormsg(errno) );
				}
			}
		}
	}
	if(pathname) free(pathname); pathname = 0;
	Free_line_list(&parts);
	/* now we look at the last directory */
	if( !owner ) return(err);
	if( statb.st_uid != DaemonUID || statb.st_gid != DaemonGID ){
		Warnmsg( "owner/group of '%s' are %d/%d, not %d/%d", path,
			(int)(statb.st_uid), (int)(statb.st_gid), DaemonUID, DaemonGID );
		err = 1;
		if( Fix ){
			if( Fix_owner( path ) ) err = 2;
		}
	}
	if( 07777 & (statb.st_mode ^ Spool_dir_perms_DYN) ){
		Warnmsg( "permissions of '%s' are 0%o, not 0%o", path,
			statb.st_mode & 07777, Spool_dir_perms_DYN );
		err = 1;
		if( Fix ){
			if( Fix_perms( path, Spool_dir_perms_DYN ) ) err = 1;
		}
	}
	return(err);
}

/***************************************************************************
 * We have put a slew of portatbility tests in here.
 * 1. setuid
 * 2. RW/pipes, and as a side effect, waitpid()
 * 3. get file system size (/tmp)
 * 4. try nonblocking open
 * 5. try locking test
 * 6. getpid() test
 * 7. try serial line locking
 * 8. try file locking
 ***************************************************************************/

void Test_port(int ruid, int euid, char *serial_line )
{
	FILE *tf;
	char line[LINEBUFFER];
	char cmd[LINEBUFFER];
	char t1[LINEBUFFER];
	char t2[LINEBUFFER];
	char stty[LINEBUFFER];
	char diff[LINEBUFFER];
	char *sttycmd;
	char *diffcmd;
	int ttyfd;
	static pid_t pid, result;
	plp_status_t status;
	double freespace;
	static int fd;
	static int i, err;
	struct stat statb;
	char *Stty_command;

	status = 0;
	fd = -1;

	/*
	 * SETUID
	 * - try to go to user and then back
	 */
	fflush(stderr);
	fflush(stdout);

	Spool_file_perms_DYN = 000600;
	Spool_dir_perms_DYN =  042700;
	if( ( ruid == 0 && euid == 0 ) || (ruid != 0 && euid != 0 ) ){
			fprintf( stderr,
				"*******************************************************\n" );
			fprintf( stderr, "***** not SETUID, skipping setuid checks\n" );
			fprintf( stderr,
				"*******************************************************\n" );
			goto freespace;
	} else if( ( ruid == 0 || euid == 0 ) ){
		if( UID_root == 0 ){
		fprintf( stderr,
			"checkpc: setuid code failed!! Portability problems\n" );
			exit(1);
		}
		if( To_uid(1) ){
			fprintf( stderr,
			"checkpc: To_uid() seteuid code failed!! Portability problems\n" );
			exit(1);
		}
		if( To_user() ){
			fprintf( stderr,
			"checkpc: To_usr() seteuid code failed!! Portability problems\n" );
			exit(1);
		}
		fprintf( stderr, "***** SETUID code works\n" );
	}

 freespace:

	freespace = Space_avail( "/tmp" );

	fprintf( stderr, "***** Free space '/tmp' = %0.0f Kbytes \n"
		"   (check using df command)\n", (double)freespace );

	/*
	 * check serial line
	 */
	if( serial_line == 0 ){
		fprintf( stderr,
			"*******************************************************\n" );
		fprintf( stderr, "********** Missing serial line\n" );
		fprintf( stderr,
			"*******************************************************\n" );
		goto test_lockfd;
	} else {
		fprintf( stderr, "Trying to open '%s'\n",
			serial_line );
		fd = Checkwrite_timeout( 2, serial_line, &statb, O_RDWR, 0, 1 );
		err = errno;
		if( Alarm_timed_out ){
			fprintf( stderr,
				"ERROR: open of '%s'timed out\n"
				" Check to see that the attached device is online\n",
				serial_line );
			goto test_stty;
		} else if( fd < 0 ){
			fprintf( stderr, "Error opening line '%s'\n", Errormsg(err));
			goto test_stty;
		} else if( !isatty( fd ) ){
			fprintf( stderr,
				"*******************************************************\n" );
			fprintf( stderr, "***** '%s' is not a serial line!\n",
				serial_line );
			fprintf( stderr,
				"*******************************************************\n" );
			goto test_stty;
		} else {
			fprintf( stderr, "\nTrying read with timeout\n" );
			i = Read_fd_len_timeout( 1, fd, cmd, sizeof(cmd) );
			err = errno;
			if( Alarm_timed_out ){
				fprintf( stderr, "***** Read with Timeout successful\n" );
			} else {
				 if( i < 0 ){
					fprintf( stderr,
					"***** Read with Timeout FAILED!! Error '%s'\n",
						Errormsg( err ) );
				} else {
					fprintf( stderr,
						"***** Read with Timeout FAILED!! read() returned %d\n",
							i );
		fprintf( stderr,
"***** On BSD derived systems CARRIER DETECT (CD) = OFF indicates EOF condition.\n" );
		fprintf( stderr,
"*****  Check that CD = ON and repeat test with idle input port.\n" );
		fprintf( stderr,
"*****  If the test STILL fails,  then you have problems.\n" );
				}
			}
		}
		/*
		 * now we try locking the serial line
		 */
		/* we try to lock the serial line */
		fprintf( stderr, "\nChecking for serial line locking\n" );
		fflush(stderr);
		fflush(stdout);
#if defined(LOCK_DEVS) && LOCK_DEVS == 0
		fprintf( stderr,
			"*******************************************************\n" );
		fprintf( stderr,
			"******** Device Locking Disabled by compile time options" );
		fprintf( stderr, "\n" );
		fprintf( stderr,
			"*******************************************************\n" );
		fflush(stderr);
		goto test_stty;
#endif

		i = 0;
		if( Set_timeout() ){
			Set_timeout_alarm( 1 );
			i =  LockDevice( fd, 0 );
		}
		Clear_timeout();
		err = errno;
		if( Alarm_timed_out || i < 0 ){
			if( Alarm_timed_out ){
				fprintf( stderr, "LockDevice timed out - %s", Errormsg(err) );
			}
			fprintf( stderr,
				"*******************************************************\n" );
				fprintf( stderr, "********* LockDevice failed -  %s\n",
					Errormsg(err) );
				fprintf( stderr, "********* Try an alternate lock routine\n" );
			fprintf( stderr,
				"*******************************************************\n" );
			fflush(stderr);
			goto test_stty;
		}
		
		fprintf( stderr, "***** LockDevice with no contention successful\n" );
		fflush(stderr);
		/*
		 * now we fork a child with tries to reopen the file and lock it
		 */
		if( (pid = fork()) < 0 ){
			fprintf( stderr, "fork failed - %s", Errormsg(errno) );
		} else if( pid == 0 ){
			close(fd);
			fd = -1;
			i = -1;
			fprintf( stderr, "Daughter re-opening line '%s'\n", serial_line );
			fflush(stderr);
			if( Set_timeout() ){
				Set_timeout_alarm( 1 );
				fd = Checkwrite( serial_line, &statb, O_RDWR, 0, 0 );
				if( fd >= 0 ) i = LockDevice( fd, 1 );
			}
			Clear_timeout();
			err = errno;
			fprintf( stderr, "Daughter open completed- fd '%d', lock %d\n",
				 fd, i );
			fflush(stderr);
			if( Alarm_timed_out ){
				fprintf( stderr, "Timeout opening line '%s'\n",
					serial_line );
			} else if( fd < 0 ){
				fprintf( stderr, "Error opening line '%s' - %s\n",
				serial_line, Errormsg(err));
			} else if( i > 0 ){
				fprintf( stderr, "Lock '%s' succeeded! wrong result\n",
					serial_line);
			} else {
				fprintf( stderr, "**** Lock '%s' failed, desired result\n",
					serial_line);
			}
			fflush(stderr);
			if( fd >= 0 ){
				fprintf( stderr,"Daughter closing '%d'\n", fd );
				fflush(stderr);
				close( fd );
			}
			fflush(stderr);
			fprintf( stderr,"Daughter exit with '%d'\n", (i >= 0)  );
			fflush(stderr);
			exit(i >= 0);
		} else {
			status = 0;
			fflush(stderr);
			fprintf( stderr, "Mother starting sleep\n" );
			fflush(stderr);
			plp_usleep(2000);
			fprintf( stderr, "Mother sleep done\n" );
			fflush(stderr);
			while(1){
				result = plp_waitpid( -1, &status, 0 );
				err = errno;
				fprintf( stderr, "waitpid result %d, status %d, errno '%s'\n",
					(int)result, status, Errormsg(err) );
				if( result == pid ){
					fprintf( stderr, "Daughter exit status %d\n", status );
					fflush(stderr);
					if( status != 0 ){
						fprintf( stderr, "LockDevice failed\n");
					}
					break;
				} else if( (result == -1 && errno == ECHILD) || result == 0 ){
					break;
				} else if( result == -1 && errno != EINTR ){
					fprintf( stderr,
						"plp_waitpid() failed!  This should not happen!");
					status = -1;
					break;
				}
				fflush(stderr);
			}
			fflush(stderr);
			if( status == 0 ){
				fprintf( stderr, "***** LockDevice() works\n" );
			}
			fflush(stderr);
		}
 test_stty:
		/*
		 * do an STTY operation, then print the status.
		 * we cheat and use a shell script; check the output
		 */
		if( fd <= 0 ) goto test_lockfd;
		fprintf( stderr, "\n\n" );
		fprintf( stderr, "Checking stty functions, fd %d\n\n", fd );
		fflush(stderr);
		if( (pid = fork()) < 0 ){
			fprintf( stderr, "fork failed - %s", Errormsg(errno) );
		} else if( pid == 0 ){
			/* default for status */
			plp_snprintf( t1, sizeof(t1), "/tmp/t1XXX%d", getpid() );
			plp_snprintf( t2, sizeof(t2), "/tmp/t2XXX%d", getpid() );
			diffcmd = "diff -c %s %s 1>&2";
			ttyfd = 1;	/*stdout is reported */
			sttycmd = "stty -a 2>%s";	/* on stderr */
#if defined(SUNOS4_1_4)
			ttyfd = 1;	/*stdout is reported */
			sttycmd = "/bin/stty -a 2>%s";	/* on stderr */
#elif defined(SOLARIS) || defined(SVR4) || defined(linux)
			ttyfd = 0;	/* stdin is reported */
			sttycmd = "/bin/stty -a >%s";	/* on stdout */
#elif (defined(BSD) && (BSD >= 199103))	/* HMS: Might have to be 199306 */
			ttyfd = 0;	/* stdin is reported */
			sttycmd = "stty -a >%s";	/* on stdout */
#elif defined(BSD) /* old style BSD4.[23] */
			sttycmd = "stty everything 2>%s";
#endif
			if( fd != ttyfd ){
				i = dup2(fd, ttyfd );
				if( i != ttyfd ){
					fprintf( stderr, "dup2() failed - %s\n", Errormsg(errno) );
					exit(-1);
				}
				close( fd );
			}
			plp_snprintf( stty, sizeof(stty), sttycmd, t1 );
			plp_snprintf( diff, sizeof(diff), diffcmd, t1, t2 );
			plp_snprintf( cmd, sizeof(cmd), "%s; cat %s 1>&2", stty, t1 );
			fprintf( stderr,
			"Status before stty, using '%s', on fd %d->%d\n",
				cmd, fd, ttyfd );
			fflush(stderr); fflush(stdout); i = system( cmd ); fflush(stdout);
			fprintf( stderr, "\n\n" );
			Stty_command = "9600 -even odd echo";
			fprintf( stderr, "Trying 'stty %s'\n", Stty_command );
			fflush(stderr);
			Do_stty( ttyfd );
			plp_snprintf( stty, sizeof(stty), sttycmd, t2 );
			plp_snprintf( cmd, sizeof(cmd),
				"%s; %s", stty, diff );
			fprintf( stderr, "Doing '%s'\n", cmd );
			fflush(stderr); i = system( cmd ); fflush(stdout);
			fprintf( stderr, "\n\n" );
			Stty_command = "1200 -odd even";
			fprintf( stderr, "Trying 'stty %s'\n", Stty_command );
			fflush(stderr);
			Do_stty( ttyfd );
			fprintf( stderr, "Doing '%s'\n", cmd );
			fflush(stderr); i = system( cmd ); fflush(stdout);
			fprintf( stderr, "\n\n" );
			Stty_command = "300 -even -odd -echo cbreak";
			fprintf( stderr, "Trying 'stty %s'\n", Stty_command );
			fflush(stderr);
			Do_stty( ttyfd );
			plp_snprintf( stty, sizeof(stty), sttycmd, serial_line, t2 );
			fprintf( stderr, "Doing '%s'\n", cmd );
			fflush(stderr); i = system( cmd ); fflush(stdout);
			fprintf( stderr, "\n\n" );
			fprintf( stderr, "Check the above for parity, speed and echo\n" );
			fprintf( stderr, "\n\n" );
			unlink(t1);
			unlink(t2);
			fflush(stderr);
			exit(0);
		} else {
			close(fd);
			fd = -1;
			status = 0;
			while(1){
				result = plp_waitpid( -1, &status, 0 );
				if( result == pid ){
					fprintf( stderr, "Daughter exit status %d\n", status );
					fflush(stderr);
					if( status != 0 ){
						fprintf( stderr, "STTY operation failed\n");
					}
					break;
				} else if( (result == -1 && errno == ECHILD) || result == 0 ){
					break;
				} else if( result == -1 && errno == EINTR ){
					fprintf( stderr,
						"plp_waitpid() failed!  This should not happen!");
					status = -1;
					break;
				}
				fflush(stderr);
			}
			fflush(stderr);
			if( status == 0 ){
				fprintf( stderr, "***** STTY works\n" );
			}
			fflush(stderr);
		}
	}
 test_lockfd:
	fflush(stderr);
	if( fd >= 0 ) close(fd);
	fd = -1;

	fprintf( stderr, "\n\n" );
	fflush(stderr);
	/*
	 * check out Lockf
	 */
	plp_snprintf( line, sizeof(line), "/tmp/XX%dXX", getpid );
	fprintf( stderr, "Checking Lockf '%s'\n", line );
	fflush(stderr);
	if( (fd = Checkwrite(line, &statb, O_RDWR, 1, 0 )) < 0) {
		err = errno;
		fprintf( stderr,
			"open '%s' failed: wrong result - '%s'\n",
			line, Errormsg(errno)  );
		exit(1);
	}
	if( Do_lock( fd, 0 ) <= 0 ) {
		fprintf( stderr,
			"Mother could not lock '%s', in correct result\n", line );
		exit(0);
	}
	sprintf( cmd, "ls -l %s", line );
	fflush(stderr); i = system( cmd ); fflush(stdout);
	if( (pid = fork()) < 0 ){
		fprintf( stderr, "fork failed!\n");
	} else if ( pid == 0 ){
		fprintf( stderr, "Daughter re-opening and locking '%s'\n", line );
		close( fd );
		if( (fd = Checkwrite(line, &statb, O_RDWR, 1, 0 )) < 0) {
			err = errno;
			fprintf( stderr,
				"Daughter re-open '%s' failed: wrong result - '%s'\n",
				line, Errormsg(errno)  );
			exit(1);
		}
		if( Do_lock( fd, 0 ) <= 0 ) {
			fprintf( stderr,
				"Daughter could not lock '%s', correct result\n", line );
			exit(0);
		}
		fprintf( stderr,
			"Daughter locked '%s', incorrect result\n", line );
		exit(1);
	}
	fflush(stderr);
	plp_usleep(1000);
	fflush(stderr);
	status = 0;
	while(1){
		result = plp_waitpid( -1, &status, 0 );
		if( result == pid ){
			fprintf( stderr, "Daughter exit status %d\n", status );
			break;
		} else if( (result == -1 && errno == ECHILD) || result == 0 ){
			break;
		} else if( result == -1 && errno != EINTR ){
			fprintf( stderr,
				"plp_waitpid() failed!  This should not happen!");
			status = -1;
			break;
		}
		fflush(stderr);
	}
	if( status == 0 ){
		fprintf( stderr, "***** Lockf() works\n" );
	}
	fflush(stderr);

	if( (pid = fork()) < 0 ){
		fprintf( stderr, "fork failed!\n");
		fflush(stderr);
	} else if ( pid == 0 ){
		int lock = 0;
		fprintf( stderr, "Daughter re-opening '%s'\n", line );
		fflush(stderr);
		close( fd );
		if( (fd = Checkwrite(line, &statb, O_RDWR, 1, 0 )) < 0) {
			err = errno;
			fprintf( stderr,
				"Daughter re-open '%s' failed: wrong result - '%s'\n",
				line, Errormsg(errno)  );
			exit(1);
		}
		fprintf( stderr, "Daughter blocking for lock\n" );
		fflush(stderr);
		lock = Do_lock( fd, 1 );
		if( lock <= 0 ){
			fprintf( stderr, "Daughter lock '%s' failed! wrong result\n",
				line );
			fflush(stderr);
			exit( 1 );
		}
		fprintf( stderr, "Daughter lock '%s' succeeded, correct result\n",
			line );
		fflush(stderr);
		exit(0);
	}
	fprintf( stderr, "Mother pausing before releasing lock on fd %d\n", fd );
	fflush(stderr);
	sleep(3);

	fprintf( stderr, "Mother closing '%s', releasing lock on fd %d\n",
		line, fd );
	close( fd );
	fflush(stderr);
	fd = -1;
	status = 0;
	while(1){
		result = plp_waitpid( -1, &status, 0 );
		if( result == pid ){
			fprintf( stderr, "Daughter exit status %d\n", status );
			break;
		} else if( (result == -1 && errno == ECHILD) || result == 0 ){
			break;
		} else if( result == -1 && errno != EINTR ){
			fprintf( stderr,
				"plp_waitpid() failed!  This should not happen!");
			status = -1;
			break;
		}
		fflush(stderr);
	}
	fflush(stderr);
	if( status == 0 ){
		fprintf( stdout, "***** Lockf() with unlocking works\n" );
	}
	fflush(stderr);

	if( fd >= 0 ) close(fd);
	fd = - 1;
	unlink( line );


/***************************************************************************
 * check out the process title
 ***************************************************************************/

	fprintf( stdout, "checking if setting process info to 'lpd XXYYZZ' works\n" );
	setproctitle( "lpd %s", "XXYYZZ" );
	/* try simple test first */
	i = 0;
	if( (tf = popen( "ps | grep XXYYZZ | grep -v grep", "r" )) ){
		while( fgets( line, sizeof(line), tf ) ){
			printf( line );
			++i;
		}
		fclose(tf);
	}
	
	if( i == 0 && (tf = popen( "ps | grep XXYYZZ | grep -v grep", "r" )) ){
		while( fgets( line, sizeof(line), tf ) ){
			printf( line );
			++i;
		}
		fclose(tf);
	}
	if( i ){
		fprintf( stdout, "***** setproctitle works\n" );
	} else {
		fprintf( stdout, "***** setproctitle debugging aid unavailable (not a problem)\n" );
	}
	exit(0);
}

/* VARARGS2 */
#ifdef HAVE_STDARGS
 void setstatus (struct job *job,char *fmt,...)
#else
 void setstatus (va_alist) va_dcl
#endif
{
#ifndef HAVE_STDARGS
    struct job *job;
    char *fmt;
#endif
	char msg[LARGEBUFFER];
    VA_LOCAL_DECL

    VA_START (fmt);
    VA_SHIFT (job, struct job * );
    VA_SHIFT (fmt, char *);

	msg[0] = 0;
	(void) plp_vsnprintf( msg, sizeof(msg)-2, fmt, ap);
	DEBUG4("setstatus: %s", msg );
	if(  Verbose ){
		(void) plp_vsnprintf( msg, sizeof(msg)-2, fmt, ap);
		strcat( msg,"\n" );
		Write_fd_str( 2, msg );
	} else {
		Add_line_list(&Status_lines,msg,0,0,0);
	}
	VA_END;
	return;
}

 void send_to_logger (int sdf, int mfd, struct job *job,const char *header, char *fmt){;}

/* VARARGS2 */
#ifdef HAVE_STDARGS
 void setmessage (struct job *job,const char *header, char *fmt,...)
#else
 void setmessage (va_alist) va_dcl
#endif
{
#ifndef HAVE_STDARGS
    struct job *job;
    char *fmt, *header;
#endif
	char msg[LARGEBUFFER];
    VA_LOCAL_DECL

    VA_START (fmt);
    VA_SHIFT (job, struct job * );
    VA_SHIFT (header, char * );
    VA_SHIFT (fmt, char *);

	msg[0] = 0;
	if( Verbose ){
		(void) plp_vsnprintf( msg, sizeof(msg)-2, fmt, ap);
		strcat( msg,"\n" );
		Write_fd_str( 2, msg );
	}
	VA_END;
	return;
}

void Fix_clean( char *s, int no, struct line_list *files )
{
	int n;
	struct stat statb;
	if( s ){
		if(!no){
			Make_write_file( Spool_dir_DYN, s, 0 );
			Clean_log( Truncate, Spool_dir_DYN, s );
		} else {
			if( stat(s,&statb) == 0 && Fix ){
				Msg(" removing '%s'", s );
				unlink(s);
			}
		}
		if( !Find_first_key(files,s,Value_sep,&n ) ){
			Remove_line_list(files,n);
		}
	}
}

int Start_worker( struct line_list *l, int fd )
{
	return(1);
}

int Check_path_list( char *plist, int allow_missing )
{
	struct line_list values;
	char *path, *s;
	int found_pc = 0, i, fd;
	struct stat statb;

	Init_line_list(&values);
	Split(&values,plist,File_sep,0,0,0,0,0);
	for( i = 0; i < values.count; ++i ){
		path = values.list[i];
		if( path[0] == '|' ){
			++path;
			while( isspace(cval(path)) ) ++path;
			if( (s = strpbrk(path,Whitespace)) ) *s = 0;
		}
		if( path[0] == '/' ){
			if( (fd = Checkread( path,&statb ) ) < 0 ){
				if( stat( path, &statb ) ){
					if( ! allow_missing ) Warnmsg(" '%s' not present", path );
				} else {
					Warnmsg(" '%s' cannot be opened - check path permissions", path );
				}
			} else {
				close(fd);
				if(Verbose)Msg("  found '%s', mod 0%o", path, statb.st_mode );
				++found_pc;
				if( (statb.st_mode & 0444) != 0444 ){
					Warnmsg(" '%s' is not world readable", path );
					Warnmsg(" this file should have (suggested) 644 permissions, owned by root" );
				}
			}
		} else {
			Warnmsg("not absolute pathname '%s' in '%s'", path, plist );
		}
	}
	Free_line_list(&values);
	return(found_pc);
}
