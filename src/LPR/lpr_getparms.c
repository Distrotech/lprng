/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
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
"$Id: lpr_getparms.c,v 3.2 1996/07/27 18:15:32 papowell Exp $";

#include "lpr.h"
#include "lp_config.h"
#include "printcap.h"
#include "patchlevel.h"

/***************************************************************************
 * void Get_parms(int argc, char *argv[])
 * 1. Scan the argument list and get the flags
 * 2. Check for duplicate information
 ***************************************************************************/

void usage();

void Get_parms(int argc, char *argv[] )
{
	int option;
	int i;

	for( i = 0; i < argc; ++i ){
		if( strchr( argv[i], '\n' ) ){
			Diemsg( "option has embedded new-line '%s'\n", argv[i]);
		}
	}

	while( (option = Getopt (argc, argv, LPR_optstr )) != EOF ){
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
	if( Verbose > 0 ) fprintf( stdout, "Version %s\n", PATCHLEVEL );
	if( Verbose > 1 ) Printlist( Copyright, stdout );
	if( Verbose || Debug > 0 ){
		logDebug( "LPRng version " PATCHLEVEL );
	}
}


char *msg[] = {
"usage summary:",
" %s [ -Pprinter[@host] ] [-(K|#)copies] [-Cclass][-Jinfo]",
"   [-Raccountname] [-m mailaddr] [-Ttitle] [-i indent]",
"   [-wnum ][ -Zoptions ] [ -Uuser ] [ -Fformat ] [ -bhkr ]",
"   [-Ddebugopt ] [ filenames ...  ]",
" -K copies, -# copies   - number of copies",
" -b          - binary format",
" -h          - no header or banner page",
" -i indent   - indentation",
" -k          - non seKure filter operation, create temp file for input",
" -m mailaddr - mail error status to mailaddr",
" -r          - remove named files after spooling",
" -w width    - width to use",
" -Cclass  - job class",
" -F format   - job format filter",
" -Jinfo   - banner and job information",
" -Pprinter[@host] - printer on host (default environment variable PRINTER)",
" -Q          - put 'queuename' in control file",
" -Raccntname - accounting information",
" -T title    - title for 'pr' (-p) formatting",
" -U username - override user name (restricted)",
" -V          - Verbose information during spooling",
" -Z filteroptions - options to pass to filter",
"   default job format -Ff",
"   -c,d,f,g,l,m,p,t,v are also allowed as format specification",
0
};
void usage()
{
	Printlist( msg, stderr );
	exit(1);
}
