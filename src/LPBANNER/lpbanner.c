/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 * From the original PLP Software distribution
 *
 ***************************************************************************
 * MODULE: main.c for filter
 * PURPOSE:  setup a filter environment
 **************************************************************************/

/***************************************************************************
 * MODULE: main.c for filters
 ***************************************************************************
 * Revision History: Baseline Fri Jul 14 16:16:34 PDT 1995
 * 
 * Patrick Powell Fri Jul 14 16:16:13 PDT 1995 
 *
 ***************************************************************************/
#ifndef lint
static char *const _id =
"lpbanner.c,v 3.3 1997/09/18 19:45:43 papowell Exp";
#endif

/***************************************************************************
 *  Filter template and frontend.
 *
 *	A filter is invoked with the following parameters,
 *  which can be in any order, and perhaps some missing.
 *
 *  filtername arguments \   <- from PRINTCAP entry
 *      -PPrinter -wwidth -llength -xwidth -ylength [-c] \
 *      -Kcontrolfilename -Lbnrname \
 *      [-iindent] \
 *		[-Zoptions] [-Cclass] [-Jjob] [-Raccntname] -nlogin -hHost  \
 *      -Fformat -Ddebug [affile]
 * 
 *  1. Parameters can be in different order than the above.
 *  2. Optional parameters can be missing
 *  3. Values specified for the width, length, etc., are from PRINTCAP
 *     or from the overridding user specified options.
 *
 *  This program provides a common front end for most of the necessary
 *  grunt work.  This falls into the following classes:
 * 1. Parameter extraction.
 * 2. Suspension when used as the "of" filter.
 * 3. Termination and accounting
 *  The front end will extract parameters,  then call the filter()
 *  routine,  which is responsible for carrying out the required filter
 *  actions. filter() is invoked with the printer device on fd 1,
 *	and error log on fd 2.  The npages variable is used to record the
 *  number of pages that were used.
 *  The "halt string", which is a sequence of characters that
 *  should cause the filter to suspend itself, is passed to filter.
 *  When these characters are detected,  the "suspend_ofilter()" routine should be
 *  called.
 *
 *  On successful termination,  the accounting file will be updated.
 *
 *  The filter() routine should return 0 (success), 1 (retry) or 2 (abort).
 *
 * Parameter Extraction
 *	The main() routine will extract parameters
 *  whose values are placed in the appropriate variables.  This is done
 *  by using the ParmTable[], which has entries for each valid letter
 *  parmeter, such as the letter flag, the type of variable,
 *  and the address of the variable.
 *  The following variables are provided as a default set.
 *      -PPrinter -wwidth -llength -xwidth -ylength [-c] [-iindent] \
 *		[-Zoptions] [-Cclass] [-Jjob] [-Raccntname] -nlogin -hHost  \
 *      -Fformat [affile]
 * VARIABLE  FLAG          TYPE    PURPOSE / PRINTCAP ENTRTY
 * name     name of filter char*    argv[0], program identification
 * width    -wwidth	       int      PW, width in chars
 * length   -llength	   int      PL, length in lines
 * xwidth   -xwidth        int      PX, width in pixels
 * xlength  -xlength       int      PY, length in pixels
 * literal  -c	           int      if set, ignore control chars
 * controlfile -kcontrolfile char*  control file name
 * bnrname  -Lbnrname      char*    banner name
 * indent   -iindent       int      indent amount (depends on device)
 * zopts    -Zoptions      char*    extra options for printer
 * comment  -Scomment      char*    printer name in comment field
 * class    -Cclass        char*    classname
 * job      -Jjob          char*    jobname
 * accntname -Raccntname   char*    account for billing purposes
 * login    -nlogin        char*    login name
 * host     -hhost         char*    host name
 * format   -Fformat       char*    format
 * special   -snumber      int      Special Variable for passing flags
 * accntfile file          char*    AF, accounting file
 *
 * npages    - number of pages for accounting
 * debug     - sets debug level
 * verbose   - echo to a log file
 *
 *	The functions fatal(), logerr(), and logerr_die() can be used to report
 *	status. The variable errorcode can be set by the user before calling
 *	these functions, and will be the exit value of the program. Its default
 *	value will be 2 (abort status).
 *	fatal() reports a fatal message, and terminates.
 *	logerr() reports a message, appends information indicated by errno
 *	(see perror(2) for details), and then returns.
 *	logerr_die() will call logerr(), and then will exit with errorcode
 *	status.
 *	Both fatal() and logerr_die() call the cleanup() function before exit.
 *
 * DEBUGGING:  a simple minded debugging version can be enabled by
 * compiling with the -DDEBUG option.
 */


#define EXTERN
#define DEFINE

#ifdef HAVE_CONFIG_H
# include "config.h"
# include "portable.h"
#endif
#include "lpbanner.h"

void getargs( int argc, char *argv[], char *envp[] );

#ifdef HAVE_STDARGS
void log( char *msg, ... );
void logerr( char *msg, ... );
void logerr_die( char *msg, ... );
void fatal( char *msg, ... );
#else
void log( va_alist );
void logerr( va_alist );
void logerr_die( va_alist );
void fatal( va_alist );
#endif

extern void banner( void );
extern void cleanup( void );

int main( int argc, char *argv[], char *envp[] )
{
	/* set umask to safe level */
	umask( 0077 );
	getargs( argc, argv, envp );
	/*
	 * Turn off SIGPIPE
	 */
	(void)signal( SIGPIPE, SIG_IGN );
	banner();
	return(0);
}

/****************************************************************************/

#if !defined(HAVE_STRERROR)

# ifdef HAVE_SYS_NERR
#  if !defined(HAVE_SYS_NERR_DEF)
    extern int sys_nerr;
#  endif
#  define num_errors    (sys_nerr)
# else
#  define num_errors    (-1)            /* always use "errno=%d" */
# endif
# if  defined(HAVE_SYS_ERRLIST)
#  if !defined(HAVE_SYS_ERRLIST_DEF)
    extern const char * const sys_errlist[];
#  endif
# else
#  undef  num_errors
#  define num_errors   (-1)            /* always use "errno=%d" */
# endif

#endif

const char * Errormsg ( int err )
{
    const char *cp;

#if defined(HAVE_STRERROR)
	cp = strerror(err);
#else
# if defined(HAVE_SYS_ERRLIST)
    if (err >= 0 && err < num_errors) {
		cp = sys_errlist[err];
    } else
# endif
	{
		static char msgbuf[32];     /* holds "errno=%d". */
		/* SAFE use of sprintf */
		(void) sprintf (msgbuf, "errno=%d", err);
		cp = msgbuf;
    }
#endif
    return (cp);
}

#ifdef HAVE_STDARGS
void log(char *msg, ...)
#else
void log( va_alist ) va_dcl
#endif
{
#ifndef HAVE_STDARGS
	char *msg;
#endif
	VA_LOCAL_DECL
	VA_START(msg);
	VA_SHIFT(msg, char *);
	(void)vfprintf(stderr, msg, ap);
	(void)fprintf(stderr, "\n" );
	VA_END;
	(void)fflush(stderr);
}

#ifdef HAVE_STDARGS
void fatal(char *msg, ...)
#else
void fatal( va_alist ) va_dcl
#endif
{
#ifndef HAVE_STDARGS
	char *msg;
#endif
	VA_LOCAL_DECL
	VA_START(msg);
	VA_SHIFT(msg, char *);
	(void)fprintf(stderr, "%s: ", name);
	(void)vfprintf(stderr, msg, ap);
	(void)fprintf(stderr, "\n" );
	VA_END;
	(void)fflush(stderr);
	cleanup();
	exit(errorcode);
}

#ifdef HAVE_STDARGS
void logerr(char *msg, ...)
#else
void logerr( va_alist ) va_dcl
#endif
{
#ifndef HAVE_STDARGS
	char *msg;
#endif
	int err = errno;
	VA_LOCAL_DECL
	VA_START(msg);
	VA_SHIFT(msg, char *);
	(void)fprintf(stderr, "%s: ", name);
	(void)vfprintf(stderr, msg, ap);
	(void)fprintf(stderr, "- %s\n", Errormsg(err) );
	VA_END;
	(void)fflush(stderr);
	errno = err;
}

#ifdef HAVE_STDARGS
void logerr_die(char *msg, ...)
#else
void logerr_die( va_alist ) va_dcl
#endif
{
#ifndef HAVE_STDARGS
	char *msg;
#endif
	int err = errno;
	VA_LOCAL_DECL
	VA_START(msg);
	VA_SHIFT(msg, char *);
	(void)fprintf(stderr, "%s: ", name);
	(void)vfprintf(stderr, msg, ap);
	(void)fprintf(stderr, "- %s\n", Errormsg(err) );
	VA_END;
	(void)fflush(stderr);
	cleanup();
	exit(errorcode);
}

/*
 *	doaccnt()
 *	writes the accounting information to the accounting file
 *  This has the format: user host printer pages format date
 */
void doaccnt(void)
{
	time_t t;
	FILE *f;

	t = time((time_t *)0);
	if(accntfile && access(accntfile, W_OK) >= 0 &&
	    (f = fopen(accntfile, "a" )) != NULL) {
		fprintf(f,"%s\t%s\t%s\t%7d\t%s\t%s",
			login? login: "NULL", 
			host? host: "NULL", 
			printer? printer: "NULL", 
			npages,
			format? format: "NULL", 
			ctime(&t));
		(void)fclose(f);
	}
}

void getargs( int argc, char *argv[], char *envp[] )
{
	int i, c;		/* argument index */
	char *arg, *optarg;	/* argument */

	if( (name = argv[0]) == 0 ) name = "FILTER";
	for( i = 1; i < argc && (arg = argv[i])[0] == '-'; ++i ){
		if( (c = arg[1]) == 0 ){
			fprintf( stderr, "missing option flag");
			i = argc;
			break;
		}
		if( c == 'c' ){
			literal = 1;
			continue;
		}
		optarg = &arg[2];
		if( arg[2] == 0 ){
			optarg = argv[i++];
			if( optarg == 0 ){
				fprintf( stderr, "missing option '%c' value", c );
				i = argc;
				break;
			}
		}
		switch(c){
			case 'C': class = optarg; break; 
			case 'D': debug = atoi( optarg ); break; 
			case 'F': format = optarg; break; 
			case 'J': job = optarg; break; 
			case 'K': controlfile = optarg; break; 
			case 'L': bnrname = optarg; break; 
			case 'P': printer = optarg; break; 
			case 'Q': queuename = optarg; break; 
			case 'R': accntname = optarg; break; 
			case 'S': comment = optarg; break; 
			case 'Z': zopts = optarg; break; 
			case 'h': host = optarg; break; 
			case 'i': indent = atoi( optarg ); break; 
			case 'l': length = atoi( optarg ); break; 
			case 'n': login = optarg; break; 
			case 's': special = atoi( optarg ); break; 
			case 'v': verbose = atoi( optarg ); break; 
			case 'w': width = atoi( optarg ); break; 
			case 'x': xwidth = atoi( optarg ); break; 
			case 'y': ylength = atoi( optarg ); break;
			case 'E': errorfile = optarg; break;
			default: break;
		}
	}
	if( i < argc ){
		accntfile = argv[i];
	}
	if( errorfile ){
		int fd;
		fd = open( errorfile, O_APPEND | O_WRONLY, 0600 );
		if( fd < 0 ){
			fprintf( stderr, "cannot open error log file '%s'", errorfile );
		} else {
			fprintf( stderr, "using error log file '%s'", errorfile );
			if( fd != 2 ){
				dup2(fd, 2 );
				close(fd);
			}
		}
	}
	if( verbose || debug ){
		fprintf(stderr, "%s command: ", name );
		for( i = 0; i < argc; ++i ){
			fprintf(stderr, "%s ", argv[i] );
		}
		fprintf( stderr, "\n" );
		fflush(stderr);
	}
	if( debug ){
		fprintf(stderr, "FILTER decoded options: " );
		fprintf(stderr,"login '%s'\n", login? login : "null" );
		fprintf(stderr,"host '%s'\n", host? host : "null" );
		fprintf(stderr,"class '%s'\n", class? class : "null" );
		fprintf(stderr,"format '%s'\n", format? format : "null" );
		fprintf(stderr,"job '%s'\n", job? job : "null" );
		fprintf(stderr,"printer '%s'\n", printer? printer : "null" );
		fprintf(stderr,"queuename '%s'\n", queuename? queuename : "null" );
		fprintf(stderr,"accntname '%s'\n", accntname? accntname : "null" );
		fprintf(stderr,"zopts '%s'\n", zopts? zopts : "null" );
		fprintf(stderr,"literal, %d\n", literal);
		fprintf(stderr,"indent, %d\n", indent);
		fprintf(stderr,"length, %d\n", length);
		fprintf(stderr,"width, %d\n", width);
		fprintf(stderr,"xwidth, %d\n", xwidth);
		fprintf(stderr,"ylength, %d\n", ylength);
		fprintf(stderr,"accntfile '%s'\n", accntfile? accntfile : "null" );
		fprintf(stderr,"errorfile '%s'\n", errorfile? errorfile : "null" );
		fprintf(stderr, "FILTER environment: " );
		for( i = 0; (arg = envp[i]); ++i ){
			fprintf(stderr,"%s\n", arg );
		}
		fprintf(stderr, "RUID: %d, EUID: %d\n", (int)getuid(), (int)geteuid() );
		fflush(stderr);
	}
}

/*
 * suspend_ofilter():  suspends the output filter, waits for a signal
 */
void suspend_ofilter(void)
{
	if(debug)fprintf(stderr,"FILTER suspending\n");
	fflush(stderr);
	kill(getpid(), SIGSTOP);
	if(debug)fprintf(stderr,"FILTER awake\n");
	fflush(stderr);
}
/******************************************
 * prototype filter() and cleanup() functions;
 * filter will scan the input looking for the suspend string
 * if any.
 ******************************************/
void cleanup(void) {}

#ifdef DEBUG
filter(stop)
	char *stop;
{
	int c;
	int state, i;
	int lines = 0;

	/*
	 * do whatever initializations are needed
	 */
	/* fprintf(stderr, "filter ('%s')\n", stop ? stop : "NULL" ); */
	/*
	 * now scan the input string, looking for the stop string
	 */
	state = 0;
	npages = 1;

	while( (c = getchar()) != EOF ){
		if( c == '\n' ){
			++lines;
			if( lines > length ){
				lines -= length;
				++npages;
			}
		}
		if( stop || state ){
			if( c == stop[state] ){
				++state;
				if( stop[state] == 0 ){
					state = 0;
					if( fflush(stdout) ){
						logerr_die( "fflush returned error" );
					}
					suspend_ofilter();
				}
			} else if( state ){
				for( i = 0; i < state; ++i ){
					dochar( stop[i] );
				}
				state = 0;
				dochar( c );
			} else {
				dochar( c );
			}
		} else {
			dochar( c );
		}
	}
	if( ferror( stdin ) ){
		logerr_die( "read error on stdin");
	}
	for( i = 0; i < state; ++i ){
		dochar( stop[i] );
	}
	if( lines > 0 ){
		++npages;
	}
	if( fflush(stdout) ){
		logerr_die( "fflush returned error" );
	}
}

dochar(c)
	int c;
{
	putchar( c );
}

#endif
