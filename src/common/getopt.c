/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: getopt.c
 * PURPOSE: parsing command line options
 **************************************************************************/

/***************************************************************************
 * getopt(3) implementation;
 * modified so that the first time it is called it sets "Name" to argv[0];
 * Also allows '?' in  option string
 *
 * int Getopt ( argc, argv, optstring)
 *     int argc;
 *     char **argv, *opstring;
 * int Optind, Opterr;
 * char *Optarg; 
 * extern char *Name;
 * Returns: EOF if no more options left;
 *    '?' if option not in optstring;
 *    option character if in optstr.
 *      if option char is followed by : in opstring, an argument is required,
 *        and Optarg will point to the option argument
 *      if option char is followed by ? in opstring, an argument is optional
 *        and Optarg will point to the argument if it immediately follows
 *        the option character
 *
 * Getopt places the argv index of the next argument to be processed in
 * Optind. Because Optind is external, it is automatically set to zero
 * before the first call to Getopt.  When all the options have been
 * processed (i.e., up to the first non-option argument), Getopt returns
 * EOF.  The special option -- may be used to delimit the end of the
 * options; EOF will be returned, and -- will be skipped.
 *
 * Getopt prints an error message on stderr and returns the offending
 * character when it encounters an option letter that is not included in
 * optstring.  This error message may be disabled by setting Opterr to a
 * zero value.
 *
 * If argv is 0,  then parseing is redone.  This can be used
 * to rescan the command line.
 *
 * Side Effect:  when Getopt is called and Optind is 0, Name is set to
 * argv[0].  This allows pulling the program Name from the file.
 * Errors: if an argument is specified and none is there, then Optarg is
 * set to 0
 *
 ***************************************************************************/

static char *const _id =
"$Id: getopt.c,v 3.2 1996/09/09 14:24:41 papowell Exp papowell $";

#include "lp.h"

int Optind;                 /* next argv to process */
int Opterr = 1;                 /* Zero disables errors msgs */
char *Optarg;               /* Pointer to option argument */
char *next_opt;			    /* pointer to next option char */
char *Name;						/* Name of program */
char **Argv_p;
int Argc_p;

int
Getopt (int argc, char *argv[], char *optstring)
{
	char  option;               /* current option found */
	char *match;                /* matched option in optstring */

	if( argv == 0 ){
		/* reset parsing */
		next_opt = 0;
		Optind = 0;
		return(0);
	}

	if (Optind == 0) {
		/*
		 * set up the Name variable for error messages
		 * setproctitle will change this, so
		 * make a copy.
		 */
		char *basename, *basi;
		
		basename = argv[0];
		basi = strrchr( basename, '/' );
		if( basi ){
			basename = basi + 1;
		}
		Name = malloc( strlen(basename) + 1);
		if( Name == 0 ){
			fprintf( stderr, "Getopt: malloc failed\n");
			exit(1);
		}
		strcpy(Name, basename);
		Argv_p = argv;
		Argc_p = argc;
		Optind = 1;
	}

	while( next_opt == 0 || *next_opt == '\0' ){
		/* No more arguments left in current or initial string */
		if (Optind >= argc){
		    return (EOF);
		}
		next_opt = argv[Optind++];
	}

	/* check for start of option string AND no initial '-'  */
	if( (next_opt == argv[Optind-1]) ){
		if( next_opt[0] != '-' ){
			--Optind;
			return( EOF );
		} else {
			++next_opt;
		}
	}
	option = *next_opt++;
	/*
	 * Case of '--',  Force end of options
	 */
	if (option == '-') {
		return ( EOF );
	}
	/*
	 * See if option is in optstring
	 */
	if ((match = (char *) strchr (optstring, option)) == 0 ){
		if( Opterr ){
		    (void) fprintf (stderr, "%s: Illegal option '%c'\n", Name, option);
		}
		return( '?' );
	}
	/*
	 * Argument?
	 */
	if (match[1] == ':') {
		/*
		 * Set Optarg to proper value
		 */
		Optarg = 0;
		if (*next_opt != '\0') {
		    Optarg = next_opt;
		} else if (Optind < argc) {
		    Optarg = argv[Optind++];
		    if (Optarg != 0 && *Optarg == '-') {
				Optarg = 0;
			}
		}
		if( Optarg == 0 && Opterr == 0) {
			(void) fprintf (stderr,
				"%s: missing argument for '%c'\n", Name, option);
			option = '?';
		}
		next_opt = 0;
	} else if (match[1] == '?') {
		/*
		 * Set Optarg to proper value
		 */
		if (*next_opt != '\0') {
		    Optarg = next_opt;
		} else {
		    Optarg = 0;
		}
		next_opt = 0;
	}
	return (option);
}
