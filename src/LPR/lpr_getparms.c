/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpr_getparms.c
 * PURPOSE: extract the parameters from the command line for the
 * LPR program
 *
 **************************************************************************/

static char *const _id =
"$Id: lpr_getparms.c,v 3.2 1997/01/19 14:34:56 papowell Exp $";

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

static char Zopts_val[LINEBUFFER];
static char format_stat[2];

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
		LP_mode = 1;
		Verbose = 1;
		while( (option = Getopt( argc, argv,
			"Acmprswd:D:f:H:n:o:P:q:S:t:T:y:" )) != EOF )
		switch( option ){
		case 'A':	Use_auth_flag = 1; break;	/* use authentication */
		case 'c':	break;	/* use symbolic link */
		case 'm':	/* send mail */
					Setup_mailaddress = 1;
					Mailname = getenv( "USER" );
					if( Mailname == 0 ){
						Diemsg( "USER environment variable undefined" );
					}
					break;
		case 'p':	break;	/* ignore notification */
		case 'r':	break;	/* ignore this option */
		case 's':	Verbose = 0; break;	/* suppress messages flag */
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
						Diemsg( "-ncopies -number of copies must be greater than 0\n");
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
							safestrncat( Zopts_val, " " );
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
						safestrncat( Zopts_val, " " );
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
	} else while( (option = Getopt (argc, argv, LPR_optstr )) != EOF ){
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
		    Check_str_dup( option, &Classname, Optarg, M_CLASSNAME);
		    break;
		case 'D': /* debug has already been done */
			break;
		case 'F':
		    if( !Optarg ){
		        Dienoarg( option);
		    }
		    if( strlen (Optarg) != 1 ){
		        Diemsg( "bad -F format string '%s'\n", Optarg);
		    }
		    if( Format ){
		        Diemsg( "duplicate format specification -F%s\n", Optarg);
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
		        Diemsg( "-Kcopies -number of copies must be greater than 0\n");
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
		        Diemsg( "missing printer name in -P option\n");
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
		    Check_str_dup( option, &Zopts, Optarg, 0);
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
				Diemsg( "duplicate option %c", option);
			}
			Setup_mailaddress = 1;
			if( Optarg[0] == '-' ){
				Diemsg( "Missing mail name" );
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
		        Diemsg( "duplicate format specification -%c\n", option);
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
		fprintf( stdout, "LPRng Version %s Copyright 1988-1997\n",PATCHLEVEL );
		fprintf( stdout, "  Patrick Powell, San Diego, <papowell@sdsu.edu>\n" );
		fprintf( stdout, "  Use -VV to see Copyright details\n");
	}
	if( Verbose > 1 ) Printlist( Copyright, stdout );
}


char *LPR_msg[] = {
"usage summary: %s [ -Pprinter[@host] ] [-(K|#)copies] [-Cclass][-Jinfo]",
"   [-Raccountname] [-m mailaddr] [-Ttitle] [-i indent]",
"   [-wnum ][ -Zoptions ] [ -Uuser ] [ -Fformat ] [ -bhkr ]",
"   [-Ddebugopt ] [ filenames ...  ]",
" -b,l        - binary or literal format",
" -Cclass  - job class",
" -F format   - job format filter",
"   -c,d,f,g,l,m,p,t,v are also format specifications",
" -h          - no header or banner page",
" -i indent   - indentation",
" -Jinfo   - banner and job information",
" -k          - non seKure filter operation, create temp file for input",
" -K copies, -# copies   - number of copies",
" -m mailaddr - mail error status to mailaddr",
" -Pprinter[@host] - printer on host (default environment variable PRINTER)",
" -r          - remove named files after spooling",
" -w width    - width to use",
" -Q          - put 'queuename' in control file",
" -Raccntname - accounting information",
" -T title    - title for 'pr' (-p) formatting",
" -U username - override user name (restricted)",
" -V          - Verbose information during spooling",
" -Z filteroptions - options to pass to filter",
"   default job format -Ff",
" A filename of the form '-' will read from stdin.",
" PRINTER environment variable is default printer.",
0
};
char *LP_msg[] = {
"usage summary: %s [ -c ] [ -m ] [ -p ] [ -s ] [ -w ] [ -d dest ]",
"  [ -f form-name [ -d any ] ] [ -H special-handling ]",
"  [ -n number ] [ -o option ] [ -P page-list ]",
"  [ -q priority-level ] [ -S character-set [ -d any ] ]",
"  [ -S print-wheel [ -d any ] ] [ -t title ]",
"  [ -T content-type [ -r ] ] [ -y mode-list ]",
"  [ file...  ]",
" lp simulator using LPRng,  functionality may differ slightly",
" -A          - use authenticated transfer",
" -c          - (make copy before printing - ignored)",
" -d printer  - destination printer",
" -D debugflags  - debugging flags",
" -f formname - first letter used as job format",
" -H handling - (passed as -Z handling)",
" -m          - mail sent to $USER on completion",
" -n copies   - number of copies",
" -o option     nobanner, width recognized",
"               (others passed as -Z option)",
" -P pagelist - (print page list - ignored)",
" -p          - (notification on completion - ignored)",
" -q          - priority - 0 -> Z (highest), 25 -> A (lowest)",
" -s          - (suppress messages - ignored)",
" -S charset  - (passed as -Z charset)",
" -t title    - job title",
" -T content  - (passed as -Z content)",
" -w          - (write message on completion - ignored)",
" -y mode     - (passed as -Z mode)",
" A filename of the form '-' will read from stdin.",
" LPDEST, then PRINTER environment variables are default printer.",
0
};
void usage(void)
{
	if(LP_mode ){
		Printlist( LP_msg, stderr );
	} else {
		Printlist( LPR_msg, stderr );
	}
	exit(1);
}
