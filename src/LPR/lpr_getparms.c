/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpr_getparms.c
 * PURPOSE: extract the parameters from the command line for the
 * LPR program
 *
 **************************************************************************/

static char *const _id =
"lpr_getparms.c,v 3.10 1998/03/24 02:43:22 papowell Exp";

#include "lp.h"
#include "getparms.h"
#include "patchlevel.h"
/**** ENDINCLUDE ****/

/***************************************************************************
 * void Get_parms(int argc, char *argv[])
 * 1. Scan the argument list and get the flags
 * 2. Check for duplicate information
 ***************************************************************************/

void usage(void);

static char Zopts_val[SMALLBUFFER];
static char format_stat[2];

char LPR_optstr[]    /* LPR options */
 = "1:2:3:4:#:AC:D:F:J:K:NP:QR:T:U:VZ:bcdfghi:lkm:nprstvw:" ;
char LP_optstr[]    /* LP options */
 = 	"Acmprswd:D:f:H:n:o:P:q:S:t:T:y:";

void Get_parms(int argc, char *argv[] )
{
	int option;
	char *name, *s;

	if( argv[0] && (name = strrchr( argv[0], '/' )) ) {
		++name;
	} else {
		name = argv[0];
	}
	/* check to see if we simulate (poorly) the LP options */
	if( name && strcmp( name, "lp" ) == 0 ){
		Get_debug_parm( argc, argv, LP_optstr, debug_vars );
		LP_mode = 1;
		while( (option = Getopt( argc, argv,
			"Acmprswd:D:f:H:n:o:P:q:S:t:T:y:" )) != EOF )
		switch( option ){
		case 'A':	Use_auth_flag = 1; break;	/* use authentication */
		case 'c':	break;	/* use symbolic link */
		case 'm':	/* send mail */
					Setup_mailaddress = 1;
					Mailname = getenv( "USER" );
					if( Mailname == 0 ){
						Diemsg( _("USER environment variable undefined") );
					}
					break;
		case 'p':	break;	/* ignore notification */
		case 'r':	break;	/* ignore this option */
		case 's':	Verbose = 0; Silent = 1; break;	/* suppress messages flag */
		case 'w':	break;	/* no writing of message */
		case 'd':	Printer = Optarg; /* destination */
					break;
		case 'D': 	break; /* debug has already been done */
		case 'f':	Format = format_stat;	/* form - use for format */
					format_stat[0] = *Optarg;
					break;
		case 'H':	/* special handling - ignore */
					break;
		case 'n':	Copies = atoi( Optarg );	/* copies */
					if( Copies <= 0 ){
						Diemsg( _("-ncopies -number of copies must be greater than 0\n"));
					}
					break;
		case 'o':	if( strcasecmp( Optarg, "nobanner" ) == 0 ){
						No_header = 1;
					} else if( strncasecmp( Optarg, "width", 5 ) == 0 ){
						s = strchr( Optarg, '=' );
						if( s ){
							Pwidth = atoi( s+1 );
						}
					} else {
						/* pass as Zopts */
						if( Zopts_val[0] ){
							safestrncat( Zopts_val, "," );
						}
						safestrncat( Zopts_val, Optarg );
						Zopts = Zopts_val;
					}
					break;
		case 'P':	break;	/* ignore page lis */
		case 'q':	Priority = 'Z' - atoi(Optarg);	/* get priority */
					if(Priority < 'A' ) Priority = 'A';
					break;
		/* pass these as Zopts */
		case 'S':
		case 'T':
		case 'y':
					/* pass as Zopts */
					if( Zopts_val[0] ){
						safestrncat( Zopts_val, "," );
					}
					safestrncat( Zopts_val, Optarg );
					Zopts = Zopts_val;
					break;
		case 't':
				Check_str_dup( option, &Jobname, Optarg, M_JOBNAME);
				break;
		default:
			usage();
		    break;
		}
	} else {
		Get_debug_parm( argc, argv, LPR_optstr, debug_vars );
		while( (option = Getopt (argc, argv, LPR_optstr )) != EOF )
		switch( option ){
		case '1':
		    Check_str_dup( option, &Font1, Optarg, M_FONT);
			break;
		case '2':
		    Check_str_dup( option, &Font2, Optarg, M_FONT);
			break;
		case '3':
		    Check_str_dup( option, &Font3, Optarg, M_FONT);
			break;
		case '4':
		    Check_str_dup( option, &Font4, Optarg, M_FONT);
			break;
		case 'A':	Use_auth_flag = 1; break;	/* use authentication */
		case 'C':
  		    if ( Classname_length > M_CLASSNAME ) {
		      log ( LOG_ERR,
			    "Get_parms: Classname_length value too long, truncating to %d",
			    M_CLASSNAME );
		      Classname_length = M_CLASSNAME;
		    }
		    Check_str_dup( option, &Classname, Optarg,
				   Classname_length);
		    break;
		case 'D': /* debug has already been done */
			break;
		case 'F':
		    if( !Optarg ){
		        Dienoarg( option);
		    }
		    if( strlen (Optarg) != 1 ){
		        Diemsg( _("bad -F format string '%s'\n"), Optarg);
		    }
		    if( Format ){
		        Diemsg( _("duplicate format specification -F%s\n"), Optarg);
		    } else {
		        Format = Optarg;
		    }
		    break;
		case 'J':
		    Check_str_dup( option, &Jobname, Optarg, M_JOBNAME);
		    break;
		case 'K':
		case '#':
		    Check_int_dup( option, &Copies, Optarg, 0);
			if( Copies <= 0 ){
		        Diemsg( _("-Kcopies -number of copies must be greater than 0\n"));
			}
		    break;
		case 'N':
			Check_for_nonprintable = 0;
			break;
		case 'P':
		    if( Printer ){
		        Check_str_dup( option, &Printer, Optarg, 0);
		    }
		    if( !Optarg || !*Optarg ){
		        Diemsg( _("missing printer name in -P option\n"));
		    }
		    Printer = Optarg;
		    break;
		case 'Q':
			Use_queuename_flag = 1;
			break;
		case 'R':
		    Check_str_dup( option, &Accntname, Optarg, M_ACCNTNAME );
		    break;
		case 'T':
		    Check_str_dup( option, &Prtitle, Optarg, M_PRTITLE);
		    break;
		case 'U':
		    Check_str_dup( option, &Username, Optarg, M_BNRNAME );
		    break;
		case 'V':
			++Verbose;
		    break;
		case 'Z':
			if( Zopts_val[0] ){
				safestrncat( Zopts_val, "," );
			}
			safestrncat( Zopts_val, Optarg );
			Zopts = Zopts_val;
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
			if( Setup_mailaddress ){
				Diemsg( _("duplicate option %c"), option);
			}
			Setup_mailaddress = 1;
			if( Optarg[0] == '-' ){
				Diemsg( _("Missing mail name") );
			} else {
				Mailname = Optarg;
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
				static char dummy[2] = " ";
				dummy[0] = option;
		        Format = dummy;
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
	Filecount = argc - Optind;
	Files = &argv[Optind];
	if( Verbose > 0 ){
		fprintf( stdout, _("LPRng Version %s Copyright 1988-1997\n"),PATCHLEVEL );
		fprintf( stdout, _("  Patrick Powell, San Diego, <papowell@astart.com>\n") );
		fprintf( stdout, _("  Use -VV to see Copyright details\n"));
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
