/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: lpr.c,v 5.2 1999/09/29 01:58:24 papowell Exp papowell $";


#include "lp.h"
#include "child.h"
#include "errorcodes.h"
#include "fileopen.h"
#include "getopt.h"
#include "getprinter.h"
#include "getqueue.h"
#include "initialize.h"
#include "linksupport.h"
#include "patchlevel.h"
#include "printjob.h"
#include "sendauth.h"
#include "sendjob.h"

/**** ENDINCLUDE ****/

#undef EXTERN
#undef DEFINE
#define EXTERN
#define DEFINE(X) X
#include "lpr.h"

/**** ENDINCLUDE ****/

/***************************************************************************
 * main()
 * - top level of LPR Lite.  This is a cannonical method of handling
 *   input.  Note that we assume that the LPD daemon will handle all
 *   of the dirty work associated with formatting, printing, etc.
 * 
 * 1. get the debug level from command line arguments
 * 2. set signal handlers for cleanup
 * 3. get the Host computer Name and user Name
 * 4. scan command line arguments
 * 5. check command line arguments for consistency
 * 6. if we are spooling from stdin, copy stdin to a file.
 * 7. if we have a list of files,  check each for access
 * 8. create a control file
 * 9. send control file to server
 *
 ****************************************************************************/

int main(int argc, char *argv[], char *envp[])
{
	off_t job_size;
	int status;
	char *s, *t, buffer[SMALLBUFFER];
	struct job prjob;

#ifndef NODEBUG
	Debug = 0;
#endif

	/* set signal handlers */
	memset(&prjob, 0, sizeof(prjob) );
	(void) plp_signal (SIGHUP, cleanup_HUP);
	(void) plp_signal (SIGINT, cleanup_INT);
	(void) plp_signal (SIGQUIT, cleanup_QUIT);
	(void) plp_signal (SIGTERM, cleanup_TERM);

	/*
	 * set up the defaults
	 */
	Errorcode = 1;
	Initialize(argc, argv, envp);
	Setup_configuration();


	/* scan the input arguments, setting up values */
	Get_parms(argc, argv);      /* scan input args */


	/* Note: we may need the open connection to the remote printer
		to get our IP address if it is not available */

    if(DEBUGL3){
		struct stat statb;
		int i;
        logDebug("lpr: after init open fd's");
        for( i = 0; i < 20; ++i ){
            if( fstat(i,&statb) == 0 ){
                logDebug("  fd %d (0%o)", i, statb.st_mode&S_IFMT);
            }
        }
    }

	job_size = Make_job(&prjob);

    if(DEBUGL3){
		struct stat statb;
		int i;
        logDebug("lpr: after Make_job open fd's");
        for( i = 0; i < 20; ++i ){
            if( fstat(i,&statb) == 0 ){
                logDebug("  fd %d (0%o)", i, statb.st_mode&S_IFMT);
            }
        }
    }

	/*
	 * Fix the rest of the control file
	 */
	if( job_size == 0 ){
		Free_job(&prjob);
		Errorcode = 1;
		fatal(LOG_INFO,_("nothing to print"));
	}

	if( Check_for_rg_group( Logname_DYN ) ){
		Errorcode = 1;
		fatal( LOG_INFO,_("cannot use printer - not in privileged group\n") );
	}
	if( Remote_support_DYN
		&& safestrpbrk( "rR", Remote_support_DYN ) == 0 ){
		Errorcode = 1;
		fatal(LOG_INFO, _("no remote support for %s@%s"),
			RemotePrinter_DYN,RemoteHost_DYN );
	}

	/* we check to see if we need to do control file filtering */
	s = 0;
	if( Lpr_bounce_DYN ) s = Control_filter_DYN;
	if( (status = Fix_control( &prjob, s )) ){
		fatal(LOG_INFO,_("cannot print job due to control filter error") );
	}

	/* Send job to the LPD server for the printer */

	Errorcode = Send_job( &prjob, Connect_timeout_DYN,
		Connect_interval_DYN,
		Max_connect_interval_DYN,
		Send_job_rw_timeout_DYN );



	if( Errorcode ){
		Errorcode = 1;
		if(DEBUGL1)Dump_job("lpr - after error",&prjob);
		if( !Verbose ){
			Write_fd_str(2,"Status Information:\n ");
			s = Join_line_list(&Status_lines,"\n ");
			if( (t = safestrrchr(s,' ')) ) *t = 0;
			Write_fd_str(2,s);
			if(s) free(s); s = 0;
		} else {
			buffer[0] = 0;
			if( (s = Find_str_value(&prjob.info,ERROR,Value_sep)) ){
				plp_snprintf(buffer,sizeof(buffer), "job failed - %s\n",s );
			}
			Write_fd_str(2,buffer);
		}
		cleanup(0);
	}

	if( LP_mode ){
		char *id;
		int n;
		char msg[SMALLBUFFER];
		id = Find_str_value(&prjob.info,IDENTIFIER,Value_sep);
		if( id ){
			plp_snprintf( msg,sizeof(msg)-1,"request id is %s\n", id );
		} else {
			n = Find_decimal_value(&prjob.info,NUMBER,Value_sep);
			plp_snprintf( msg,sizeof(msg)-1,"request id is %d\n", n );
		}
		Write_fd_str(1, msg );
	}

	/* the dreaded -r (remove files) option */
	if( Removefiles && !Errorcode ){
		int i;
		/* eliminate any possible game playing */
		To_user();
		for( i = 0; i < Files.count; ++i ){
			if( unlink( Files.list[i] ) == -1 ){
				Warnmsg(_("Error unlinking '%s' - %s"),
					Files.list[i], Errormsg( errno ) );

			}
		}
	}

	Free_job(&prjob);
	Free_line_list(&Files);
	cleanup(0);
	return(0);
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
		if( Write_fd_str( 2, msg ) < 0 ) cleanup(0);
	} else {
		Add_line_list(&Status_lines,msg,0,0,0);
	}
	VA_END;
	return;
}

 void send_to_logger (int sfd, int mfd, struct job *job,const char *header, char *fmt){;}

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
		if( Write_fd_str( 2, msg ) < 0 ) cleanup(0);
	}
	VA_END;
	return;
}


/***************************************************************************
 * void Get_parms(int argc, char *argv[])
 * 1. Scan the argument list and get the flags
 * 2. Check for duplicate information
 ***************************************************************************/

 void usage(void);


 char LPR_optstr[]    /* LPR options */
 = "1:2:3:4:#:AC:D:F:J:K:NP:QR:T:U:VZ:bcdfghi:lkm:nprstvw:" ;
 char LPR_bsd_optstr[]    /* LPR options */
 = "1:2:3:4:#:AC:D:F:J:K:NP:QR:T:U:VZ:bcdfghi:lkmnprstvw:" ;
 char LP_optstr[]    /* LP options */
 = 	"cmprswd:D:f:H:n:o:P:q:S:t:T:y:";

void Get_parms(int argc, char *argv[] )
{
	int option, i;
	char *name, *s;

	if( argv[0] && (name = safestrrchr( argv[0], '/' )) ) {
		++name;
	} else {
		name = argv[0];
	}
	/* check to see if we simulate (poorly) the LP options */
	if( name && safestrcmp( name, "lp" ) == 0 ){
		LP_mode = 1;
	}
	DEBUG1("Get_parms: LP_mode %d", LP_mode );
	if( LP_mode ){
		while( (option = Getopt( argc, argv,
			"cmprswd:D:f:H:n:o:P:q:S:t:T:y:" )) != EOF )
		switch( option ){
		case 'c':	break;	/* use symbolic link */
		case 'm':	/* send mail */
					Setup_mailaddress = 1;
					Mailname_JOB = getenv( "USER" );
					if( Mailname_JOB == 0 ){
						Diemsg( _("USER environment variable undefined") );
					}
					break;
		case 'p':	break;	/* ignore notification */
		case 'r':	break;	/* ignore this option */
		case 's':	Verbose = 0; Silent = 1; break;	/* suppress messages flag */
		case 'w':	break;	/* no writing of message */
		case 'd':	Set_DYN(&Printer_DYN, Optarg); /* destination */
					break;
		case 'D': 	Parse_debug(Optarg,1);
					break;
		case 'f':	Format = *Optarg;
					break;
		case 'H':	/* special handling - ignore */
					break;
		case 'n':	Copies = atoi( Optarg );	/* copies */
					if( Copies <= 0 ){
						Diemsg( _("-ncopies -number of copies must be greater than 0\n"));
					}
					break;
		case 'o':	if( safestrcasecmp( Optarg, "nobanner" ) == 0 ){
						No_header = 1;
					} else if( safestrncasecmp( Optarg, "width", 5 ) == 0 ){
						s = safestrchr( Optarg, '=' );
						if( s ){
							Pwidth = atoi( s+1 );
						}
					} else {
						/* pass as Zopts */
						if( Zopts_JOB ){
							s = Zopts_JOB;
							Zopts_JOB = safestrdup3(s,",",Optarg,
								__FILE__,__LINE__);
							free(s);
						} else {
							Zopts_JOB = safestrdup(Optarg,
								__FILE__,__LINE__);
						}
					}
					break;
		case 'P':	break;	/* ignore page lis */
		case 'q':	Priority = 'Z' - atoi(Optarg);	/* get priority */
					if(Priority < 'A' ) Priority = 'A';
					if(Priority > 'Z' ) Priority = 'Z';
					break;
		/* pass these as Zopts */
		case 'S':
		case 'T':
		case 'y':
					/* pass as Zopts */
					if( Zopts_JOB ){
						s = Zopts_JOB;
						Zopts_JOB = safestrdup3(s,",",Optarg,
							__FILE__,__LINE__);
						free(s);
					} else {
						Zopts_JOB = safestrdup(Optarg,
							__FILE__,__LINE__);
					}
					break;
		case 't':
				Check_str_dup( option, &Jobname_JOB, Optarg, M_JOBNAME);
				break;
		default:
			usage();
		    break;
		}
	} else {
		while( (option = Getopt (argc, argv, LPR_bsd_DYN?LPR_bsd_optstr:LPR_optstr )) != EOF )
		switch( option ){
		case '1':
		    Check_str_dup( option, &Font1_JOB, Optarg, M_FONT);
			break;
		case '2':
		    Check_str_dup( option, &Font2_JOB, Optarg, M_FONT);
			break;
		case '3':
		    Check_str_dup( option, &Font3_JOB, Optarg, M_FONT);
			break;
		case '4':
		    Check_str_dup( option, &Font4_JOB, Optarg, M_FONT);
			break;
		case 'C':
		    Check_str_dup( option, &Classname_JOB, Optarg,
			   M_CLASSNAME);
		    break;
		case 'D': 	Parse_debug(Optarg,1);
			break;
		case 'F':
		    if( strlen (Optarg) != 1 ){
		        Diemsg( _("bad -F format string '%s'\n"), Optarg);
		    }
		    if( Format ){
		        Diemsg( _("duplicate format specification -F%s\n"), Optarg);
		    } else {
		        Format = *Optarg;
		    }
		    break;
		case 'J':
		    Check_str_dup( option, &Jobname_JOB, Optarg, M_JOBNAME);
		    break;
		case 'K':
		case '#':
		    Check_int_dup( option, &Copies, Optarg, 0);
			if( Copies <= 0 ){
		        Diemsg( _("-Kcopies -number of copies must be greater than 0\n"));
			}
		    break;
		case 'N':
			Check_for_nonprintable_DYN = 0;
			break;
		case 'P':
		    if( Printer_DYN ){
		        Check_str_dup( option, &Printer_DYN, Optarg, 0);
		    }
		    if( !Optarg || !*Optarg ){
		        Diemsg( _("missing printer name in -P option\n"));
		    }
		    Set_DYN(&Printer_DYN,Optarg);
		    break;
		case 'Q':
			Use_queuename_flag_DYN = 1;
			break;
		case 'R':
		    Check_str_dup( option, &Accntname_JOB, Optarg, M_ACCNTNAME );
		    break;
		case 'T':
		    Check_str_dup( option, &Prtitle_JOB, Optarg, M_PRTITLE);
		    break;
		case 'U': Check_str_dup( option, &Username_JOB, Optarg, M_BNRNAME );
		    break;
		case 'V':
			++Verbose;
		    break;
		case 'Z':
			if( Zopts_JOB ){
				s = Zopts_JOB;
				Zopts_JOB = safestrdup3(s,",",Optarg,
					__FILE__,__LINE__);
				free(s);
			} else {
				Zopts_JOB = safestrdup(Optarg,
					__FILE__,__LINE__);
			}
		    break;
		case 'l':
		case 'b':
		    Binary = 1;
		    break;
		case 'h':
		    Check_dup( option, &No_header);
		    break;
		case 'i':
		    Check_int_dup( option, &Indent, Optarg, 0);
		    break;
		case 'k':
		    Secure = 1;
		    break;
		case 'm':
		    /*
		     * -m Mailname
		     */
			if( LPR_bsd_DYN ){
					Mailname_JOB = getenv( "USER" );
					if( Mailname_JOB == 0 ){
						Diemsg( _("USER environment variable undefined") );
					}
					break;
			}
			if( Setup_mailaddress ){
				Diemsg( _("duplicate option %c"), option);
			}
			Setup_mailaddress = 1;
			if( Optarg[0] == '-' ){
				Diemsg( _("Missing mail name") );
			} else {
				Mailname_JOB = Optarg;
			}
		    break;
		case 'c':
		case 'd':
		case 'f':
		case 'g':
		case 'n':
		case 'p':
		case 't':
		case 'v':
		    if( Format ){
		        Diemsg( _("duplicate format specification -%c\n"), option);
		    } else {
		        Format = option;
		    }
		    break;
		case 'w':
		    Check_int_dup( option, &Pwidth, Optarg, 0);
		    break;

		/* Throw a sop to the whiners - let them wipe themselves out... */
		/* remove files */
		case 'r':
			Removefiles = 1;
			break;
		case 's':
			/* symbolic link - quietly ignored */
			break;
		default:
			usage();
		    break;
		}
	}

	/*
	 * set up the Parms[] array
	 */
	for( i = Optind; i < argc; ++i ){
		Add_line_list(&Files,argv[i],0,0,0);
	}
	if( Verbose > 0 ){
		fprintf( stdout, "Version %s\n", PATCHLEVEL );
	}
	if( Verbose > 1 ) Printlist( Copyright, stdout );
}


 char *LPR_msg = N_("\
 usage summary: %s [ -Pprinter[@host] ] [-(K|#)copies] [-Cclass][-Jinfo]\n\
   [-Raccountname] [-m mailaddr] [-Ttitle] [-i indent]\n\
   [-wnum ][ -Zoptions ] [ -Uuser ] [ -Fformat ] [ -bhkr ]\n\
   [-Ddebugopt ] [ filenames ...  ]\n\
 -b,l        - binary or literal format\n\
 -Cclass  - job class\n\
 -F format   - job format filter\n\
   -c,d,f,g,l,m,p,t,v are also format specifications\n\
 -h          - no header or banner page\n\
 -i indent   - indentation\n\
 -Jinfo   - banner and job information\n\
 -k          - non seKure filter operation, create temp file for input\n\
 -K copies, -# copies   - number of copies\n\
 -m mailaddr - mail error status to mailaddr\n\
 -Pprinter[@host] - printer on host (default environment variable PRINTER)\n\
 -r          - remove named files after spooling\n\
 -w width    - width to use\n\
 -Q          - put 'queuename' in control file\n\
 -Raccntname - accounting information\n\
 -T title    - title for 'pr' (-p) formatting\n\
 -U username - override user name (restricted)\n\
 -V          - Verbose information during spooling\n\
 -Z filteroptions - options to pass to filter\n\
   default job format -Ff\n\
 PRINTER environment variable is default printer.\n");

 char *LP_msg = N_("\
 usage summary: %s [ -c ] [ -m ] [ -p ] [ -s ] [ -w ] [ -d dest ]\n\
  [ -f form-name [ -d any ] ] [ -H special-handling ]\n\
  [ -n number ] [ -o option ] [ -P page-list ]\n\
  [ -q priority-level ] [ -S character-set [ -d any ] ]\n\
  [ -S print-wheel [ -d any ] ] [ -t title ]\n\
  [ -T content-type [ -r ] ] [ -y mode-list ]\n\
  [ file...  ]\n\
 lp simulator using LPRng,  functionality may differ slightly\n\
 -A          - use authenticated transfer\n\
 -c          - (make copy before printing - ignored)\n\
 -d printer  - destination printer\n\
 -D debugflags  - debugging flags\n\
 -f formname - first letter used as job format\n\
 -H handling - (passed as -Z handling)\n\
 -m          - mail sent to $USER on completion\n\
 -n copies   - number of copies\n\
 -o option     nobanner, width recognized\n\
               (others passed as -Z option)\n\
 -P pagelist - (print page list - ignored)\n\
 -p          - (notification on completion - ignored)\n\
 -q          - priority - 0 -> Z (highest), 25 -> A (lowest)\n\
 -s          - (suppress messages - ignored)\n\
 -S charset  - (passed as -Z charset)\n\
 -t title    - job title\n\
 -T content  - (passed as -Z content)\n\
 -w          - (write message on completion - ignored)\n\
 -y mode     - (passed as -Z mode)\n\
 LPDEST, then PRINTER environment variables are default printer.\n");

void usage(void)
{
	if(LP_mode ){
		fprintf( stderr, _(LP_msg), Name );
	} else {
		fprintf( stderr, _(LPR_msg), Name );
	}
	exit(1);
}


/***************************************************************************
 * Make_job Parms()
 * 1. we determine the name of the printer - Printer_DYN variable
 * 2. we determine the host name to be used - RemoteHost_DYN variable
 * 3. check the user name for consistency:
 * 	We have the user name from the environment
 * 	We have the user name from the -U option
 *     Allow override if we are root or some silly system (like DOS)
 * 		that does not support multiple users
 ***************************************************************************/


 void get_job_number( struct job *job );
 double Copy_stdin( struct job *job );
 double Check_files( struct job *job );

/***************************************************************************
 * Commentary:
 * The struct control_file{}  data structure contains fields that point to
 * complete lines in the control file, i.e.- 'Jjobname', 'Hhostname'
 * We set up this information in a data structure.
 * Note that this is specific to the LPR program
 *
 * Make_job()
 * 1. Get the control file number and name information
 * 2. scan the set of variables,  and determine how much space is needed.
 * 3. scan the data files,  and determine how much space is needed
 * 4. allocate the space.
 * 5. Copy variables to the allocated space,  setting up pointers in the
 *    control_file data structure.
 **************************************************************************/

int Make_job( struct job *job )
{
	char nstr[SMALLBUFFER];	/* information */
	struct jobwords *keys;	/* keyword entry in the parameter list */
	struct stat statb;
	char *s, *name, *t;		/* buffer where we allocate stuff */
	void *p;
	int i, n, tempfd;
	double job_size = 0;
	struct line_list *lp;
	char *tempfile;

	Get_printer();
	Fix_Rm_Rp_info();

	if(DEBUGL4)Dump_line_list("Make_job - PC_entry",&PC_entry_line_list );
	if(DEBUGL4)Dump_parms("Make_job",Pc_var_list);

	/* check for priority in range */
	if( Priority == 0 && Classname_JOB
		&& !Break_classname_priority_link_DYN ) Priority = cval(Classname_JOB);
	if( Priority == 0 && Default_priority_DYN ) Priority = cval(Default_priority_DYN);
	if( Priority == 0 ) Priority = 'A';
	if( islower(Priority) ) Priority = toupper( Priority );
	if( !isupper( Priority ) ){
		Diemsg(
		_("Priority (first letter of Class) not 'A' (lowest) to 'Z' (highest)") );
	}

	plp_snprintf(nstr,sizeof(nstr),"%c",Priority);
	Set_str_value(&job->info,PRIORITY,nstr);

	/* fix up the Classname_JOB 'C' option */

	if( Classname_JOB == 0 ){
		if( Backwards_compatible_DYN ){
			Classname_JOB = ShortHost_FQDN;
		} else {
			plp_snprintf(nstr,sizeof(nstr),"%c",Priority);
			Classname_JOB = nstr;
		}
	}
	Set_str_value(&job->info,CLASS,Classname_JOB);

	/* fix up the jobname */
	if( Jobname_JOB == 0 ){
		if( Files.count == 0 ){
			Set_str_value(&job->info,JOBNAME, "(stdin)" );
		} else {
			/* watch out for security loop holes */
			name = 0;
			for( i = 0; i < Files.count; ++i ){
				s = Files.list[i];
				if( safestrcmp(s, "-" ) == 0 ) s = "(stdin)";
				if( name ){
					t = name;
					name = safestrdup3(name,",",s,__FILE__,__LINE__);
					free(t);
				} else {
					name = safestrdup(s,__FILE__,__LINE__);
				}
			}
			Set_str_value(&job->info,JOBNAME, name );
			if( name ) free(name); name = 0;
		}
	} else {
		Set_str_value(&job->info,JOBNAME,Jobname_JOB );
	}
	if(DEBUGL4)Dump_line_list("Make_job - after jobname",&job->info);

	/* fix up the banner name.
	 * if you used the -U option,
     *   check to see if you have root permissions
	 *   set to -U value
	 * else set to log name of user
     * if No_header suppress banner
	 */
	if( Username_JOB ){
		/* check to see if you were root */
		if( 0 != OriginalRUID ){
			struct line_list user_list;
			char *str, *t;
			struct passwd *pw;
			int found, uid;

			DEBUG2("Make_job: checking '%s' for -U perms",
				Allow_user_setting_DYN );
			Init_line_list(&user_list);
			Split( &user_list, Allow_user_setting_DYN,File_sep,0,0,0,0,0);
			
			found = 0;
			for( i = 0; !found && i < user_list.count; ++i ){
				str = user_list.list[i];
				DEBUG2("Make_job: checking '%s'", str );
				uid = strtol( str, &t, 10 );
				if( str == t || *t ){
					/* try getpasswd */
					pw = getpwnam( str );
					if( pw ){
						uid = pw->pw_uid;
					}
				}
				DEBUG2( "Make_job: uid '%d'", uid );
				found = ( uid == OriginalRUID );
				DEBUG2( "Make_job: found '%d'", found );
			}
			if( !found ){
				Diemsg( _("-U (username) can only be used by ROOT") );
			}
		}
		Bnrname_JOB = Username_JOB;
	} else {
		Bnrname_JOB = Logname_DYN;
	}
	if( No_header ){
		Bnrname_JOB = 0;
	}
	Set_str_value(&job->info,BNRNAME, Bnrname_JOB );

	/* check the format */

	DEBUG1("Make_job: before checking format '%c'", Format );
	if( Binary ){
		Format = 'l';
	}
	if( Format == 0 && Default_format_DYN ) Format = *Default_format_DYN;
	if( Format == 0 ) Format = 'f';
	if( isupper(Format) ) Format = tolower(Format);

	DEBUG1("Make_job: after checking format '%c'", Format );
	if( safestrchr( "aios", Format )
		|| (Formats_allowed_DYN && !safestrchr( Formats_allowed_DYN, Format ) )){
		Diemsg( _("Bad format specification '%c'"), Format );
	}

	plp_snprintf(nstr,sizeof(nstr),"%c",Format);
	Set_str_value(&job->info,FORMAT,nstr);
	/* check to see how many files you want to print- limit of 52 */
	if( Files.count > 52 ){
		Diemsg( _("Sorry, can only print 52 files at a time, split job up"));
	}
	if( Copies == 0 ){
		Copies = 1;
	}
	if( Max_copies_DYN && Copies > Max_copies_DYN ){
		Diemsg( _("Maximum of %d copies allowed"), Max_copies_DYN );
	}
	Set_flag_value(&job->info,COPIES,Copies);

	/* check the for the -Q flag */
	DEBUG1("Make_job: 'qq' flag %d, queue '%s', force_queuename '%s'",
		Use_queuename_flag_DYN, Queue_name_DYN, Force_queuename_DYN );
	if( Use_queuename_flag_DYN ){
		Set_str_value(&job->info,QUEUENAME,Queue_name_DYN);
	}
	if( Force_queuename_DYN ){
		Set_str_value(&job->info,QUEUENAME,Force_queuename_DYN);
	}

	get_job_number(job);

	Set_str_value(&job->info,FILE_HOSTNAME,FQDNHost_FQDN);
	Set_str_value(&job->info,FROMHOST,FQDNHost_FQDN);


	/* we put the option strings in the buffer */
	for( keys = Lpr_parms; keys->key; ++keys ){
		DEBUG2("Make_job: key '%s', maxlen %d, key %c",
			keys->keyword?*keys->keyword:0,keys->maxlen,keys->key);
		s = 0;
		if( keys->keyword ) s = Find_str_value(&job->info,*keys->keyword,Value_sep);
		p = keys->variable;
		nstr[0] = 0;
		n = 0;
		switch( keys->type ){
		case INTEGER_K:
			if( s ){
				n = strtol(s,0,0);
			} else if( p ){
				n = *(int *)p;
			}
			if( n ) Set_letter_int(&job->controlfile,keys->key,n);
			break;
		case STRING_K:
			if( s == 0 && p ) s = *(char **)p;
			if( s ) Set_letter_str(&job->controlfile,keys->key,s);
			break;
		default: break;
		}
	}
	if(DEBUGL2)Dump_line_list("Make_job - controlfile after",
		&job->controlfile );

	/*
	 * copy from standard in?
	 */
	if (Files.count == 0) {
		job_size = Copy_stdin( job );
	} else {
		/*
		 * check to see that the input files are printable
		 */
		job_size = Check_files( job );
	}

	job_size *= Copies;
	/* now we check to see if we have LPR_filter */

	if( job_size && Lpr_bounce_DYN ){
		if(DEBUGL2) Dump_job( "Make_job - before filtering", job );
		tempfd = Make_temp_fd(&tempfile);
		name = Find_str_value(&job->info,TRANSFERNAME,Value_sep);
		if( !name ) Set_str_value(&job->info,TRANSFERNAME,"(bounce queue)");
		Print_job( tempfd, job, 0 );
		if( fstat( tempfd, &statb ) ){
			logerr_die(LOG_INFO,"Make_job: fstatb failed" );
		}
		job_size = statb.st_size;
		Free_listof_line_list(&job->datafiles);
		lp = malloc_or_die(sizeof(lp[0]),__FILE__,__LINE__);
		memset(lp,0,sizeof(lp[0]));
		Check_max(&job->datafiles,1);
		job->datafiles.list[job->datafiles.count++] = (void *)lp;
		Set_str_value(lp,OPENNAME,tempfile);
		Set_str_value(lp,TRANSFERNAME,tempfile);
		s = Bounce_queue_format_DYN;
		if(!s){
			fatal(LOG_INFO,"Make_job: no bq_format value");
		}
		Set_str_value(lp,FORMAT,Bounce_queue_format_DYN);
		Set_str_value(lp,"N","(lpr_filter)");
		Set_flag_value(lp,COPIES,1);
		Set_double_value(lp,SIZE,job_size);
		if( !name ) Set_str_value(&job->info,TRANSFERNAME,0);
	}
	if(DEBUGL2) Dump_job( "Make_job - final value", job );
	return( job_size );
}

/**************************************************************************
 * int get_job_number();
 * - get an integer value for the job number
 **************************************************************************/

void get_job_number( struct job *job )
{
	int number = getpid();
	if( DbgJob ){
		number = DbgJob;
	}
	if( Spread_jobs_DYN ) number *= Spread_jobs_DYN;
	Fix_job_number( job, number );
}

 struct jobwords Lpr_parms[]
 = {
{ 0,  STRING_K , &Accntname_JOB, M_ACCNTNAME, 'R' },
{ &BNRNAME,  STRING_K , &Bnrname_JOB, M_BNRNAME, 'L' },
{ &CLASS,  STRING_K , &Classname_JOB, M_CLASSNAME, 'C' },
{ 0,  STRING_K , &Font1_JOB, M_FONT, '1' },
{ 0,  STRING_K , &Font2_JOB, M_FONT, '2' },
{ 0,  STRING_K , &Font3_JOB, M_FONT, '3' },
{ 0,  STRING_K , &Font4_JOB, M_FONT, '4' },
{ &FROMHOST,  STRING_K , &FQDNHost_FQDN, M_FROMHOST, 'H' },
{ 0,  INTEGER_K , &Indent, M_INDENT, 'I' },
{ &JOBNAME,  STRING_K , &Jobname_JOB, M_JOBNAME, 'J' },
{ &LOGNAME,  STRING_K , &Logname_DYN, M_BNRNAME, 'P' },
{ 0,  STRING_K , &Mailname_JOB, M_MAILNAME, 'M' },
/* { &QUEUENAME,  STRING_K , &Queue_name_DYN, M_MAILNAME, 'Q' }, */
{ 0,  STRING_K , &Prtitle_JOB, M_PRTITLE, 'T' },
{ 0,  INTEGER_K , &Pwidth, M_PWIDTH, 'W' },
{ 0,  STRING_K , &Zopts_JOB, M_ZOPTS, 'Z' },
{ 0 }
} ;


/***************************************************************************
 * off_t Copy_stdin()
 * 1. we get the name of a temporary file
 * 2. we open the temporary file and read from STDIN until we get
 *    no more.
 * 3. stat the  temporary file to prevent games
 ***************************************************************************/

double Copy_stdin( struct job *job )
{
	int fd, count;
	double size = 0;
	char *tempfile;
	struct line_list *lp;
	char buffer[LARGEBUFFER];

	/* get a tempfile */
	if( Copies == 0 ) Copies = 1;
	fd = Make_temp_fd( &tempfile );

	if( fd < 0 ){
		logerr_die( LOG_INFO, _("Make_temp_fd failed") );
	} else if( fd == 0 ){
		Diemsg( _("You have closed STDIN! cannot pipe from a closed connection"));
	}
	DEBUG1("Temporary file '%s', fd %d", tempfile, fd );
	size = 0;
	while( (count = read( 0, buffer, sizeof(buffer))) > 0 ){
		if( write( fd, buffer, count ) < 0 ){
			Errorcode = JABORT;
			logerr_die( LOG_INFO, _("Copy_stdin: write to temp file failed"));
		}
		size += count;
	}
	close(fd);
	lp = malloc_or_die(sizeof(lp[0]),__FILE__,__LINE__);
	memset(lp,0,sizeof(lp[0]));
	Check_max(&job->datafiles,1);
	job->datafiles.list[job->datafiles.count++] = (void *) lp;
	Set_str_value(lp,"N","(stdin)");
	Set_str_value(lp,OPENNAME,tempfile);
	Set_str_value(lp,TRANSFERNAME,tempfile);
	Set_flag_value(lp,COPIES,1);
	plp_snprintf(buffer,sizeof(buffer),"%c",Format);
	Set_str_value(lp,FORMAT,buffer);
	Set_double_value(lp,SIZE,size);

	return( size );
}

/***************************************************************************
 * off_t Check_files( char **files, int filecount )
 * 2. check each of the input files for access
 * 3. stat the files and get the size
 * 4. Check for printability
 * 5. Put information in the data_file{} entry
 ***************************************************************************/

double Check_files( struct job *job )
{
	double size = 0;
	int i, fd, printable;
	struct stat statb;
	char *s;
	char buffer[SMALLBUFFER];
	struct line_list *lp;

	/* preallocate enough space so that things are not moved around by realloc */
	if( Copies == 0 ) Copies = 1;

	for( i = 0; i < Files.count; ++i){
		s = Files.list[i];
		DEBUG2( "Check_files: doing '%s'", s );
		if( safestrcmp( s, "-" ) == 0 ){
			size += Copy_stdin( job );
			continue;
		}
		fd = Checkread( s, &statb );
		if( fd < 0 ){
			Warnmsg( _("Cannot open file '%s', %s"), s, Errormsg( errno ) );
			continue;
		}

		printable = Check_lpr_printable( s, fd, &statb, Format );
		close( fd );
		if( printable > 0 ){
			lp = malloc_or_die(sizeof(lp[0]),__FILE__,__LINE__);
			memset(lp,0,sizeof(lp[0]));
			Check_max(&job->datafiles,1);
			job->datafiles.list[job->datafiles.count++] = (void *) lp;
			Set_str_value(lp,OPENNAME,s);
			Set_str_value(lp,TRANSFERNAME,s);
			Clean_meta(s);
			Set_str_value(lp,"N",s);
			Set_flag_value(lp,COPIES,1);
			plp_snprintf(buffer,sizeof(buffer),"%c",Format);
			Set_str_value(lp,FORMAT,buffer);
			size = size + statb.st_size;
			Set_double_value(lp,SIZE,(double)(statb.st_size) );
			DEBUG2( "Check_files: printing '%s'", s );
		} else {
			DEBUG2( "Check_files: not printing '%s'", s );
		}
	}
	if( Copies ) size *= Copies;
	DEBUG2( "Check_files: %d files, size %0.0f", job->datafiles.count, size );
	return( size );
}

/***************************************************************************
 * int Check_lpr_printable(char *file, int fd, struct stat *statb, int format )
 * 1. Check to make sure it is a regular file.
 * 2. Check to make sure that it is not 'binary data' file
 * 3. If a text file,  check to see if it has some control characters
 *
 ***************************************************************************/

int Check_lpr_printable(char *file, int fd, struct stat *statb, int format )
{
    char buf[LINEBUFFER];
    int n, i, c;                /* Acme Integers, Inc. */
    int printable = 0;
	char *err = _("cannot print '%s': %s");

	if( Check_for_nonprintable_DYN == 0 ) return(1);
	/*
	 * Do an LSEEK on the file, i.e.- see to the start
	 * Ignore any error return
	 */
	lseek( fd, 0, 0 );
    if(!S_ISREG( statb->st_mode )) {
		Diemsg(err, file, _("not a regular file"));
    } else if(statb->st_size == 0) {
		/* empty file */
		printable = -1;
    } else if ((n = read (fd, buf, sizeof(buf))) <= 0) {
        Diemsg (err, file, _("cannot read it"));
    } else if (format != 'p' && format != 'f' ){
        printable = 1;
    } else if (is_exec ( buf, n)) {
        Diemsg (err, file, _("executable program"));
    } else if (is_arch ( buf, n)) {
        Diemsg (err, file, _("archive file"));
    } else {
        printable = 1;
		if( Min_printable_count_DYN && n > Min_printable_count_DYN ){
			n = Min_printable_count_DYN;
		}
		for (i = 0; printable && i < n; ++i) {
			c = cval(buf+i);
			/* we allow backspace, escape, ^D */
			if( !isprint( c ) && !isspace( c )
				&& c != 0x08 && c != 0x1B && c!= 0x04 ) printable = 0;
		}
		if( !printable ) Diemsg (err, file, _("unprintable file"));
    }
    return(printable);
}

/***************************************************************************
 * The is_exec and is_arch are system dependent functions which
 * check if a file is an executable or archive file, based on the
 * information in the header.  Note that most of the time we will end
 * up with a non-printable character in the first 100 characters,  so
 * this test is moot.
 *
 * I swear I must have been out of my mind when I put these tests in.
 * In fact,  why bother with them?  
 *
 * Patrick Powell Wed Apr 12 19:58:58 PDT 1995
 *   On review, I agree with myself. Sun Jan 31 06:36:28 PST 1999
 ***************************************************************************/

#if defined(HAVE_A_OUT_H) && !defined(_AIX41)
#include <a.out.h>
#endif

#ifdef HAVE_EXECHDR_H
#include <sys/exechdr.h>
#endif

/* this causes trouble, eg. on SunOS. */
#ifdef IS_NEXT
#  ifdef HAVE_SYS_LOADER_H
#    include <sys/loader.h>
#  endif
#  ifdef HAVE_NLIST_H
#    include <nlist.h>
#  endif
#  ifdef HAVE_STAB_H
#    include <stab.h>
#  endif
#  ifdef HAVE_RELOC_H
#   include <reloc.h>
#  endif
#endif /* IS_NEXT */

#if defined(HAVE_FILEHDR_H) && !defined(HAVE_A_OUT_H)
#include <filehdr.h>
#endif

#if defined(HAVE_AOUTHDR_H) && !defined(HAVE_A_OUT_H)
#include <aouthdr.h>
#endif

#ifdef HAVE_SGS_H
#include <sgs.h>
#endif

/***************************************************************************
 * I really don't want to know.  This alone tempts me to rip the code out
 * Patrick Powell Wed Apr 12 19:58:58 PDT 1995
 ***************************************************************************/
#ifndef XYZZQ_
#define XYZZQ_ 1		/* ugh! antediluvian BSDism, I think */
#endif

#ifndef N_BADMAG
#  ifdef NMAGIC
#    define N_BADMAG(x) \
	   ((x).a_magic!=OMAGIC && (x).a_magic!=NMAGIC && (x).a_magic!=ZMAGIC)
#  else				/* no NMAGIC */
#    ifdef MAG_OVERLAY		/* AIX */
#      define N_BADMAG(x) (x.a_magic == MAG_OVERLAY)
#    endif				/* MAG_OVERLAY */
#  endif				/* NMAGIC */
#endif				/* N_BADMAG */

int is_exec( char *buf, int n)
{
    int i = 0;

#ifdef N_BADMAG		/* BSD, non-mips Ultrix */
#  ifdef HAVE_STRUCT_EXEC
    if (n >= sizeof (struct exec)){
		i |= !(N_BADMAG ((*(struct exec *) buf)));
	}
#  else
    if (n >= sizeof (struct aouthdr)){
		i |= !(N_BADMAG ((*(struct aouthdr *) buf)));
	}
#  endif
#endif

#ifdef ISCOFF		/* SVR4, mips Ultrix */
    if (n >= sizeof (struct filehdr)){
		i |= (ISCOFF (((struct filehdr *) buf)->f_magic));
	}
#endif

#ifdef MH_MAGIC		/* NeXT */
    if (n >= sizeof (struct mach_header)){
		i |= (((struct mach_header *) buf)->magic == MH_MAGIC);
	}
#endif

#ifdef IS_DATAGEN	/* Data General (forget it! ;) */
    {
		if( n > sizeof (struct header)){
			i |= ISMAGIC (((struct header *)buff->magic_number));
		}
    }
#endif

    return (i);
}

#include <ar.h>

int is_arch(char *buf, int n)
{
	int i = 0;
#ifdef ARMAG
	if( n >= SARMAG ){
		i = !memcmp( buf, ARMAG, SARMAG);
	}
#endif				/* ARMAG */
    return(i);
}

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

	if ( !Allow_duplicate_args_DYN ) {
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
	if ( !Allow_duplicate_args_DYN ) {
	  if (*value) {
	    Diemsg ("duplicate option %c", option);
	  }
	}
	if (arg == 0) {
		Dienoarg (option);
	}
	*value = arg;
}

/***************************************************************************
 * 1.  check to see if value has been set
 * 2.  if not, then set it
 ***************************************************************************/

void Check_dup(int option, int *value)
{
	if ( !Allow_duplicate_args_DYN ) {
	  if (*value) {
	    Diemsg ("duplicate option %c", option);
	  }
	}
	*value = 1;
}

int Start_worker( struct line_list *l, int fd )
{
	return(-1);
}
