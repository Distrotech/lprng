/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: Setup_filter.c
 * PURPOSE:
 **************************************************************************/

static char *const _id =
"$Id: setup_filter.c,v 3.20 1997/12/31 19:30:10 papowell Exp $";

/***************************************************************************
 *
 * Patrick Powell Fri May 19 14:08:18 PDT 1995
 * create the command line to invoke a filter
 *  format: the format type of the data the filter will be invoked with
 *           (a single( char) letter identifies the type).
 *  filtername: the struct filter{} data structure
 *
 * Actions:
 *
 *  If the filtername contains %-escapes( eg. %J, %h, etc.) these will
 *  be filled in with the appropriate values. Otherwise, the command
 *  will be of the form:
 *
 *  filtername arguments \   <- from filtername
 *      -PPrinter -wwidth -llength -xwidth -ylength \
 *      [-Kcontrolfilename] [-Lbannername] [-c] [-iindent] \
 *      [-Zoptions] [-Cclass] [-Jjob] [-Raccntname] \
 *      [-Nfilename] [-Sprinter descr] [-Ypermsline] \
 *      -nlogin -hHost -Fformat [affile]
 *
 *  The 'o' (of filter) only gets the -w, -l, -x, -y and -F options
 *************************************************************************/

#include "lp.h"
#include "printcap.h"
#include "setup_filter.h"
#include "printcap.h"
#include "cleantext.h"
#include "decodestatus.h"
#include "dump.h"
#include "errorcodes.h"
#include "gethostinfo.h"
#include "killchild.h"
#include "malloclist.h"
#include "pathname.h"
#include "pr_support.h"
#include "rw_pipe.h"
#include "setstatus.h"
#include "setuid.h"
#include "setup_filter.h"
#include "timeout.h"
#include "waitchild.h"
#include "jobcontrol.h"
/**** ENDINCLUDE ****/


void setup_close_on_exec( int minfd );
char * find_executable( char *execname, struct filter *filter );
static void setup_envp( struct control_file *cf,
	struct printcap_entry *printcap_entry, struct filter *filter );
static int cmd_split( struct filter *filter );

static struct printcap_entry *pc_entry;

/*
 * Make_filter(
 *   int key,      - format key, i.e.- a for 'af'
 *                   We use this to search for the filter if not provided
 *   struct control_file *cf - control file for filter
 *   struct filter *filter   - filter data structure
 *   char *line              - filter command line if available
 *   int noextra             - do not add any extra options
 *   int read_write          - create a read/write pipe
 *   int print_fd            - dup this fd to fd 1
 *   struct data_file *data_file   - data file being printed
 *   int acct_port           - dup this fd to fd 3
 *   int stderr_to_logger    - send the stderr status to a logger as well
 *                           - as to stderr
 * If the command line is empty,  the printcap is searched for
 * a filter;  the command line is expanded
 *
 * 1. if 'line' is null, search the printcap file for a filter
 *    for format 'key'
 * 2. set up the command line for the filter, appending options
 *    and expanding them as required.
 * 3. set up the environment vector for the filter
 * 4. check to see if there is an executable for the filter
 * 5. create the pipe for IO to the filter
 * 6. fork and exec filter program
 * 7. close read side of pipe (mother), write (daughter)
 */

static int Make_errlog( int stderr_pipe[], struct control_file *cf,
	char *header);

int Make_filter( int key,
	struct control_file *cf,
	struct filter *filter, char *line, int noextra,
	int read_write, int print_fd,
	struct printcap_entry *printcap_entry, struct data_file *data_file,
	int acct_port, int stderr_to_logger, int direct_read )
{
	int p[2], stderr_pipe[2];
	char *executable;
	char header[SMALLBUFFER];
	int root = 0;

	header[0] = 0;
	DEBUG1 ("Make_filter: stderr_to_logger %d, starting filter '%s'",
		stderr_to_logger, filter->cmd );
	pc_entry = printcap_entry;

	p[0] = p[1] = stderr_pipe[0] = stderr_pipe[1] = -1;
	filter->input = -1;
	if( line == 0 || *line == 0 ){
		fatal( LOG_ERR, "Make_filter: no filter for format '%c'", key );
	}
	/*
	 * set line command line
	 */
	if( Setup_filter( key, cf, line, filter, noextra, data_file ) ){
		plp_snprintf( cf->error, sizeof( cf->error ),
		"Make_filter: format '%c' bad filter '%s'", key, line );
		goto error;
	}
	if(DEBUGL2 ) dump_filter( "Make_filter", filter );

	/*
	 * find the executable
	 */
	/* check for ROOT key */
	if( (executable = filter->args.list[0]) && !strcmp( executable, "ROOT") ){
		root = 1;
	}
	executable = find_executable( filter->args.list[root], filter ); 
	DEBUG1("Make_filter: executable '%s'", executable );
	if( executable == 0 ){
		plp_snprintf( cf->error, sizeof( cf->error ),
		"Make_filter: cannot find executable file '%s' - '%s'",
			filter->args.list[0], Errormsg(errno) );
		goto error;
	}

	/*
	 * get the pipes for the filter input: we use bidirectional ones.
	 */
	if( direct_read <= 0 ){
		if( rw_pipe( p ) < 0 ){
			plp_snprintf( cf->error, sizeof( cf->error ),
			"Make_filter: rw_pipe failed - '%s'", Errormsg(errno) );
			goto error;
		}
		if( (p[0] < 3) || p[1] < 3){
			plp_snprintf( cf->error, sizeof( cf->error ),
			 "Make_filter: pipe FDs out of range");
			goto error;
		}
	} else {
		p[0] = direct_read;
		p[1] = -1;
	}

	/*
	 * create pipe and fork
	 */

	if( stderr_to_logger&& pipe( stderr_pipe ) < 0 ){
		plp_snprintf( cf->error, sizeof( cf->error ),
		 "Make_errlog: stderr_to_logger pipe failed - %s",
			Errormsg(errno) );
		goto error;
	}
	if( (filter->pid = dofork(0)) == 0 ){              /* child */
		/* pipe is stdin */ /* printer is stdout */
		if( dup2(p[0], 0) < 0 
			|| (print_fd != 1 && dup2( print_fd, 1) < 0) ){
			Errorcode = errno;
			logerr_die( LOG_NOTICE, "Make_filter: dup2 failed");
		}
#if defined(ROOT_PERMS_TO_FILTER_SECURITY_LOOPHOLE)
		if( root ) {
			DEBUG1( "Make_filter: running %s as root", executable );
			Full_root_perms();   /* run as "root" */
		} else
#endif
		if( Is_server ){
			Full_daemon_perms();   /* run as "daemon" */
		} else {
			Full_user_perms();   /* run as "user" */
		}
		/*
		 * set up the environment variables
		 */
		setup_envp(cf, printcap_entry, filter);

		if( stderr_to_logger ){
			/* close input and dup output side to stderr */
			if( dup2(stderr_pipe[1], 2) < 0  ){
				Errorcode = errno;
				logerr_die(LOG_NOTICE,"Make_filter: stderr_to_logger dup2 failed");
			}
		}
		if( acct_port > 0 ){
			dup2( acct_port, 3 );
			setup_close_on_exec(4); /* this'll close the unused pipe fds */
		} else {
			setup_close_on_exec(3); /* this'll close the unused pipe fds */
		}
		send_to_logger(0,0);
		execve( executable, &filter->args.list[root], filter->envp.list );
		Errorcode = JABORT;
		logerr_die( LOG_ERR, "Make_filter: execve '%s' failed", executable );
	} else if( filter->pid < 0 ){
		plp_snprintf( cf->error, sizeof( cf->error ),
		 "Make_filter: fork failed - '%s'", Errormsg(errno) );
		goto error;
	}

	/* fix up the stderr logger */
	if( stderr_to_logger ){
		plp_snprintf( header, sizeof(header),
			"FILTER_STATUS %d", filter->pid );
		if( Make_errlog( stderr_pipe, cf, header ) ) {
			goto error;
		}
		close( stderr_pipe[0] );
		close( stderr_pipe[1] );
	}

	/* set the status */
	setmessage( cf, header, "COMMANDLINE %s", filter->cmd );
	/* close the various pipes */
	filter->input = p[1];
	if( direct_read <= 0 ){
		(void) close( p[0] );        /* close input side */
	}

	/* we save the output device  for flushing if it is a terminal
	 * this might prevent processes from locking up when device goes
	 *	offline and we time out
	 */
	if( isatty( print_fd ) ){
		filter->output = print_fd;
	} else {
		filter->output = -1;
	}
	DEBUG2("Make_filter: pid %d,input fd %d, output fd %d, '%s'",
		filter->pid, filter->output, p[1], 
		filter->cmd );
	return( 0 );

error:
	Errorcode = JABORT;
	if( p[0] > 0 && Direct_read <= 0 ) close( p[0] );
	if( p[1] > 0 ) close( p[1] );
	if( stderr_pipe[0] > 0 ) close( stderr_pipe[0] );
	if( stderr_pipe[1] > 0 ) close( stderr_pipe[1] );
	DEBUG2( "Make_filter: ERROR '%s'", cf->error );
	return( -1 );
}

/*
 * Make_passthrough(
 *   struct filter *filter - filter information
 *   char *line          - filter command line if available
 *   int *fd             - dup fd[0] to 0, etc.
 *   in  fd_count        - number of them
 *   int stderr_to_logger    - send the stderr status to a logger as well
 *                           - as to stderr
 *
 * 1. set up the command line for the filter, appending options
 *    and expanding them as required.
 * 2. set up the environment vector for the filter
 * 3. check to see if there is an executable for the filter
 * 4. create the pipe for IO to the filter
 * 5. fork and exec filter program
 * RETURNS: process pid
 */

int Make_passthrough( struct filter *filter, char *line,
	int *fd, int fd_count, int stderr_to_logger, struct control_file *cf,
	struct printcap_entry *printcap_entry )
{
	int stderr_pipe[2];
	char *executable;
	int root = 0;
	int i;

	stderr_pipe[0] = stderr_pipe[1] = -1;
	/* search the printcap file for the current printer */
	filter->input = -1;
	if( line == 0 || *line == 0 ){
		plp_snprintf( cf->error, sizeof( cf->error ),
		 "Make_passthrough: no filter" );
		goto error;
	}
	/*
	 * setup filter command line
	 * int Setup_filter( int fmt, struct control_file *cf,
	 * 	char *filtername, struct filter *filter, int noextra,
	 * 	struct data_file *data_file )
	 */
	if( Setup_filter( 0, 0, line, filter, 1, 0 ) ){
		plp_snprintf( cf->error, sizeof( cf->error ),
		 "Make_passthrough: bad filter '%s'", line );
		goto error;
	}
	if(DEBUGL2 ) dump_filter( "Make_passthrough", filter );

	/*
	 * find the executable
	 */
	/* check for ROOT key */
	if( (executable = filter->args.list[0]) && !strcmp( executable, "ROOT") ){
		root = 1;
	}
	executable = find_executable( filter->args.list[root], filter ); 
	DEBUG1("Make_passthrough: executable '%s'", executable );
	if( executable == 0 ){
		plp_snprintf( cf->error, sizeof( cf->error ),
		 "Make_passthrough: cannot find executable file '%s'",
			filter->args.list[0] );
		goto error;
	}

	/*
	 * create pipe and fork
	 */

	DEBUG1 ("Make_passthrough: stderr_to_logger %d, starting filter '%s'",
		stderr_to_logger, filter->cmd );
	if( (filter->pid = dofork(0)) == 0 ){              /* child */
		if( stderr_to_logger ){
			int len, count, i, filter_pid;
			int pid;	/* for the stderr */
			char *s, *end;
			char head[SMALLBUFFER];
			char stderr_info[SMALLBUFFER];

			head[0] = 0;
			filter_pid = getpid();
			if( pipe( stderr_pipe ) < 0 ){
				plp_snprintf( cf->error, sizeof( cf->error ),
				 "Make_passthrough: stderr_to_logger pipe failed - %s",
					Errormsg(errno) );
				goto error;
			}
			if( (pid = dofork(0)) == 0 ){
				/* we are the daughter, waiting for stderr output */
				DEBUG3( "Make_passthrough: stderr pid %d, filter pid %d, pipe[%d,%d]",
					getpid(), filter_pid, stderr_pipe[0], stderr_pipe[1] );
				stderr_info[0] = -1;	/* clear out buffer */
				stderr_info[1] = -1;	/* clear out buffer */
				if( dup2( stderr_pipe[0], 0 ) < 0 ){
					Errorcode = JABORT;
					logerr_die( LOG_ERR, "Make_filter: stderr_to_logger - dup2 failed" );
				}
				Dup_logger_fd( 1 );
				setup_close_on_exec(3); /* this'll close the unused pipe fds */
				plp_snprintf( head, sizeof(head),
					"FILTER_STATUS PID %d", filter_pid );
				setmessage( cf, head, "COMMANDLINE %s", filter->cmd );
				do{
					/* we get the stderr information */
					len = strlen( stderr_info+1 );
					/* read more into buffer */
					count = read( 0, stderr_info+1+len, sizeof(stderr_info)-len-2 );
					if( count > 0 ){
						stderr_info[count+len+1] = 0;
						/* we put out lines */
						DEBUG4( "Make_passthrough: stderr pid read %d '%s'",
							count, stderr_info+1 );
						if( Write_fd_str( 2, stderr_info+1+len ) < 0 ) cleanup( 0 );
						for( s = stderr_info+1;
							s && (end = strchr( s, '\n' ));
							s = end+1 ){
							*end = 0;
							if( *s == '.' ){
								--s;
								*s = '.';
							}
							setmessage( cf, head, "%s", s );
						}
						/* now keep the rest of the line */
						if( s != stderr_info+1 ){
							for( i = 0; (stderr_info[i+1] = s[i]); ++i );
						}
					}
				}while( count > 0 );
				s = stderr_info+1;
				if( strlen( s ) ){
					if( *s == '.' ){
						--s;
						*s = '.';
					}
					setmessage( cf, head, "%s", s );
				}
				setmessage( cf, head, "FINISHED" );
				Errorcode = 0;
				cleanup(0);
			} else if( pid < 0 ){
				plp_snprintf( cf->error, sizeof( cf->error ),
				 "Make_passthrough: stderr_to_logger fork failed - '%s'",
					Errormsg(errno) );
				goto error;
			}
		}
#if defined(ROOT_PERMS_TO_FILTER_SECURITY_LOOPHOLE)
		if( root ) {
			DEBUG1( "Make_passthrough: running %s as root", executable );
			Full_root_perms();   /* run as "root" */
		} else
#endif
		if( Is_server ){
			Full_daemon_perms();   /* run as "daemon" */
		} else {
			Full_user_perms();   /* run as "user" */
		}
		/*
		 * set up the environment variables
		 */
		setup_envp(cf, printcap_entry, filter);
		/* pipe is stdin */ /* printer is stdout */
		for( i = 0; i < fd_count; ++i ){
			DEBUG3 ("Make_passthrough: dup fd[%d] = %d", i, fd[i] );
			if( fd[i] && dup2( fd[i], i ) == -1 ){
				plp_snprintf( cf->error, sizeof( cf->error ),
				 "Make_passthrough: dup of fd = %d failed - '%s'",
					fd[i], Errormsg(errno) );
				goto error;
			}
		}
		setup_close_on_exec(fd_count); /* this'll close the unused pipe fds */
		send_to_logger(0,0);
		execve( executable, &filter->args.list[root], filter->envp.list );
		Errorcode = JABORT;
		logerr_die( LOG_ERR, "Make_passthrough: execve '%s' failed", executable );
	} else if( filter->pid < 0 ){
		plp_snprintf( cf->error, sizeof( cf->error ),
		 "Make_passthrough: fork failed - '%s'", Errormsg(errno) );
		goto error;
	}
	/* we save the output device  for flushing if it is a terminal
	 * this might prevent processes from locking up when device goes
	 *	offline and we time out
	 */
	DEBUG2 (
		"Make_passthrough: pid %d, cmd '%s'",
		filter->pid, filter->cmd );
	return( filter->pid );

error:
	Errorcode = JABORT;
	if( stderr_pipe[0] > 0 ) close( stderr_pipe[0] );
	if( stderr_pipe[1] > 0 ) close( stderr_pipe[1] );
	DEBUG2( "Make_passthrough: ERROR '%s'", cf->error );
	return( -1 );
}



/***************************************************************************
 * int Setup_filter( int fmt, struct control_file *cf,
 *	char *filtername, struct filter *filter, int noextra )
 * 1. select the options to append to the filter
 * 2. append them and then expand the options
 * 3. purge the option line of metacharacters
 * 4. break the option line up.
 ***************************************************************************/
int Setup_filter( int fmt, struct control_file *cf,
	char *filtername, struct filter *filter, int noextra,
	struct data_file *data_file )
{
	char *bp, *ep;              /* buffer pointer and general purpose pointer */
	char cmd[LARGEBUFFER];		/* command string */
	char fixedcmd[LARGEBUFFER];		/* fixed command string */
	char *opts = 0;
	int c, i;					/* ACME- an entirely beholden company */
	char **list;				/* argument list */


	if( filter->cmd ){
		free( filter->cmd );
		filter->cmd = 0;
	}

	if( filtername == 0 || *filtername == 0 ){
		log( LOG_ERR, "Setup_filter: missing filter for format '%c'", fmt );
		return(1);
	}

	if( fmt ){
		if( Backwards_compatible_filter ){
			if( fmt == 'o' ){
				opts = BK_of_filter_options;
			} else {
				opts = BK_filter_options;
			}
		} else {
			/*
			 * basic set of options
			 * filter -PPrinter -wwidth -llength -xwidth -ylength -Fo
			 */
			if( fmt == 'o'){
				opts = OF_filter_options;
				if( opts == 0 || *opts == 0 ) opts = Filter_options;
			} else {
				/*
				 * filter -PPrinter -wwidth -llength -xwidth -ylength -Fx
				 *     [-c] [-iindent] [-Zoptions]
				 *     [-Cclass] [-Jjob] -nlogin -hHost -Fformat [affile]
				 */
				opts = Filter_options;
			}
		}
	}

	
	DEBUG4("Setup_filter: fmt '%c', filtername '%s', noextra %d",
		fmt, filtername, noextra );
	DEBUG4("Setup_filter: OF_filter_options '%s'", OF_filter_options );
	DEBUG4("Setup_filter: Filter_options '%s'", Filter_options );
	DEBUG4("Setup_filter: BK_of_filter_options '%s'", BK_of_filter_options );
	DEBUG4("Setup_filter: BK_filter_options '%s'", BK_filter_options );
	DEBUG4("Setup_filter: options '%s'", opts );

	/*
	 * check to see if you have the filter form | -$ <filterspec>
	 */

	while( isspace(*filtername) ) ++filtername;
	if( filtername[0] == '|' ) ++filtername;
	while( isspace(*filtername) ) ++filtername;

	if( strncasecmp( filtername, "-$", 2 ) == 0 ){
		noextra = 1;
		filtername += 2;
		while( isspace(*filtername) ) ++filtername;
	}
	DEBUG4("Setup_filter: noextra %d, filter '%s'",
		noextra, filtername );

	cmd[0] = 0;

	c = strlen( filtername );
	if( noextra == 0 && opts ){
		c += strlen( opts) + 2;
	}
	if( c > sizeof( cmd ) ){
		fatal( LOG_ERR,
			"Setup_filter: filter and options too long '%s'",
			filtername  );
	}

	strcpy( cmd, filtername );
	if( noextra == 0 && opts ){
		strcat( cmd, " " );
		strcat( cmd, opts );
	}

	/* set up end of buffer pointer for error detection */

	DEBUG4("Setup_filter: expanding '%s'", cmd );

	ep = fixedcmd + sizeof( fixedcmd )-2;
	ep[1] = 0;
	bp = Expand_command( cf, fixedcmd, ep, cmd, fmt, data_file );
	if( bp >= ep ){
		logerr( LOG_ERR,"Setup_filter: filter line too long '%s'", fixedcmd );
		return(1);
	}
	*ep = 0;
	filter->cmd = safestrdup( fixedcmd );
	DEBUG4("Setup_filter: cmd '%s'", filter->cmd );

	if( cmd_split( filter ) ){
		log( LOG_INFO,"Setup_filter: bad filter expansion '%s'",
			filter->cmd );
		return( 1 );
	}

	/*
	 * now purge all strings of metacharacters, replace with '_'
	 */
	list = filter->args.list;
	for( i = 0; i < filter->args.count; ++i ){
		Clean_meta( list[i] );
	}

	return( 0 );
}

/***************************************************************************
 * This is a very good idea.  Reinforced, double checked, and
 * added some cleanup 
 * Patrick Powell Sun May 21 08:18:30 PDT 1995
 ***************************************************************************
 * enhanced for security by jmason. This now wraps the supplied argument
 * in ticks( ie. -J'(stdin)') before passing it to sh for the exec.
 * Note that all metacharacters are ruthlessly removed.
 *
 * This should stop users from being able to get lpd to exec arbitrary
 * command-lines as daemon, while still allowing a large degree of freedom
 * to filter writers. (however, filter writers still need to exercise
 * care, otherwise the "daemon" userid is vulnerable).
 ***************************************************************************/
static char * add_str( char *s, char *e, char *s2)
{
	int c;
	if( s2 && s ){
		while( s2 && (c = *s2++) && (s < e) ){
			if( Is_meta( c ) ) c = '_';
			*(s++) = c;
		}
		if( s < e) *s = 0;
		if( s >= e ) s = 0;
	}
	return( s );
}

static char * add_stropt( char *s, char *e, char *s2)
{
	int c;
	if( s2 == 0 ){
		s2 = "NULL";
	}
	if( s ){
		if( s < e ) *(s++) = '\'';
			/* wrap the argument in ticks( ie. -J'(stdin)'). */
		while( s2 && (c = *s2++) && (s < e) ){
			if( Is_meta( c ) ) c = '_';
			if( c == '\'' ){
				c = '_';
			}
			*(s++) = c;
		}
		if( s < e) *(s++) = '\'';
		if( s < e) *s = 0;
		if( s >= e ) s = 0;
	}
	return( s );
}

static char * add_num( char *s, char *e, int n)
{
	(void) plp_snprintf( s, e-s, "%d", n );
	return( s+strlen(s) );
}


static char * add_numopt( char *s, char *e, int n)
{
	(void) plp_snprintf( s, e-s, "'%d'", n );
	return( s+strlen(s) );
}

/***************************************************************************
 *char *Do_dollar( struct control_file *cf, char *s, char *e,
 *	int type, int fmt, int space, int notag, struct *datafile
 *  int noquote, char *name )
 * Expand the $<tag> information in the filter definition
 * Note: see the code for the keys!
 * replace $X with -X<value>
 * if space != 0, then replace %X with -X <value>
 *
 * cf = control file where we get values from
 * s  = start or next position in expansion string
 * e  = end position in expansion string
 * type = key value
 * fmt  = filter format (i.e. - 'o' for of)
 * space = 1, put space after -X, i.e. '-X value'
 * notag = 1, do not put tag      i.e.     'value'
 * datafile = data file where we get values
 * noquote  = 1, do not put quotes around the expansion values ie. -Xvalue
 ***************************************************************************/

static char *chop( char * s)
{
	if( s && *s ){
		++s;
	} else {
		s = 0;
	}
	return( s );
}

char * Do_dollar( struct control_file *cf, char *s, char *e, int type,
	int fmt, int space, int notag, struct data_file *data_file, int noquote,
	char *name )
{
	int kind = STRING_K;
	char *str = 0;
	int n = 0;
	int prefix = type;
	static char f[] = " "; 
	static char p[] = "-X"; 
	/*                 012 */

	f[0] = fmt;

	/* skip expansion if no control file */
	/* DEBUG1("Do_dollar: type '%c' cf 0x%x, space %d, notag %d, name '%s'",
		type, cf, space, notag, name ); / **/
	if( name ){
		/* get the printcap value for name */
		str = Get_pc_option_value( name, pc_entry );
		prefix = 0;
		if( str && (*str == '=' || *str == '#') ){
			++str;
		}
	} else switch( type ){
		case 'a': str = Accounting_file; break;
		case 'b': if( cf ) kind = INTEGER_K; n = cf->jobsize; break;
		case 'c': prefix = 0; noquote = 1; space = 0;
				if( data_file && (data_file->format == 'l') ){
					str = " -c";
				}
				break;
		case 'd': str = Control_dir; break;
		case 'e': 	if( data_file && (str = data_file->openname) == 0 ){
						str = data_file->transfername;
					}
					break;
		case 'f': if( data_file ) str = chop(data_file->Ninfo); break;
		case 'h':	if( cf && cf->FROMHOST && cf->FROMHOST[1] ){
						str = Find_fqdn( &LookupHostIP, &cf->FROMHOST[1], 0 );
						if( str == 0 ) str = &cf->FROMHOST[1];
					}
					break;
		case 'i': if( cf ) str = chop(cf->INDENT); break;
		case 'j': if( cf ){
					kind = INTEGER_K;
					n = cf->number;
					}
					break;
		case 'k': if( cf ) str = cf->openname; break;
		case 'l': kind = INTEGER_K; n = Page_length; break;
		case 'm': kind = INTEGER_K; n = Cost_factor; break;   /* cost */
		case 'n': if( cf ) str = chop(cf->LOGNAME); break;
		case 'p': str = RemotePrinter; break;
		case 'r': str = RemoteHost; break;
		case 's': str = Status_file; break;
		case 't':
			str = Time_str( 0, time( (void *)0 ) ); break;
		case 'w': kind = INTEGER_K; n = Page_width; break;
		case 'x': kind = INTEGER_K; n = Page_x; break;
		case 'y': kind = INTEGER_K; n = Page_y; break;
		case 'F': str = f; break;
		case 'P': str = Printer; break;
		case 'S': str = Comment_tag; break;

		default:
			if( cf == 0 ){
				prefix = 0;
				break;
			}
			if( isupper( type ) ){
				str = chop(cf->capoptions[type-'A']);
				break;
			}
			if( isdigit( type ) ){
				str = chop(cf->digitoptions[type-'0']);
				break;
			}
			prefix = 0;
			break;
	}
	p[1] = prefix;
	/* DEBUG4("Do_dollar: prefix '%s'", p ); */
	switch( kind ){
		case STRING_K:
			if( str && *str ){
				if( prefix && notag == 0 ){
					s = add_str( s, e, p );
				}
				if( space ){
					s = add_str( s, e, " " );
				}
				/* DEBUG4("Do_dollar: string '%s'", str ); */
				if( noquote ){
					s = add_str( s, e, str );
				} else {
					s = add_stropt( s, e, str );
				}
			}
			break;
		case INTEGER_K:
			if( prefix && notag == 0 ){
				s = add_str( s, e, p );
			}
			if( space ){
				s = add_str( s, e, " " );
			}
			if( noquote ){
				s = add_num( s, e, n );
			} else {
				s = add_numopt( s, e, n );
			}
			break;
	}
	return s;
}


/**************************************************************************
 * open /dev/null on all non-open std fds( stdin, stdout, stderr).
 *************************************************************************/
static void setup_std_fds( void )
{
    int fd;
    fd = open( "/dev/null", O_RDWR, Spool_file_perms);
    while( fd >= 0 && fd < 3  ){
		fd = dup( fd );
    }
    if( fd < 0 ){
        /* well, this is pretty serious */
        logerr_die( LOG_CRIT, "setup_std_fds: cannot open /dev/null");
    }
	(void) close( fd );
}

/*
 * close all non-std file descriptors up to getdtablesize( ), and
 * call setup_std_fds().
 */

void setup_clean_fds( int minfd )
{
    int fd, max = getdtablesize();

    for( fd = minfd; fd < max; fd++ ){
        (void) close( fd);
    }
}

/*
 * setup all non-std file descriptors up to getdtablesize( )
 * so they'll close before the next exec( ) call, and call
 * setup_std_fds( ).
 * We can't simply use setup_clean_fds( ) because that breaks stuff
 * on IRIX 5.1.1 (connection to NIS? getpwuid() fails, anyway).
 */

void setup_close_on_exec( int minfd )
{
	setup_std_fds();
	setup_clean_fds( minfd );
}

/****************************************************************************
 * setup_envp(struct control_file *cf, struct printcap_entry *printcap_entry):
 *
 * set up a safe environment for filters.
 * consists of:
 *  LD_LIBRARY_PATH (safe value), PATH (safe value), TZ (from env), 
 *  USER, LOGNAME, HOME, LOGDIR, SHELL, IFS (safe value).
 *  SPOOL_DIR, CONTROL_DIR, PRINTCAP_ENTRY, and CONTROL_FILE
 ***************************************************************************/

static void add_env( struct filter *filter, char *p )
{
	int i;
	char *s;

	DEBUG4("add_env: '%s'", p );
	if( p ){
		i = strlen(p);
		s = add_buffer( &filter->env, i+1 );
		strcpy( s, p );
		p = s;
	}
	if( filter->envp.count+1 >= filter->envp.max ){
		extend_malloc_list( &filter->envp, sizeof( char *),
			filter->envp.count+100 );
	}
	filter->envp.list[filter->envp.count++] = p;
}

static void setup_envp(struct control_file *cf,
	struct printcap_entry *printcap_entry, struct filter *filter )
{
	struct passwd *pw;
	char line[LINEBUFFER];
	char *s, *end, *value;

	clear_malloc_list( &filter->env, 1 );
	clear_malloc_list( &filter->envp, 0 );

	if( (pw = getpwuid( getuid())) == 0 ){
		logerr_die( LOG_INFO, "setup_envp: getpwuid(%d) failed", getuid());
	}
	plp_snprintf(line, sizeof(line), "USER=%s", pw->pw_name);
	add_env( filter, line );

	plp_snprintf(line, sizeof(line), "LOGNAME=%s", pw->pw_name);
	add_env( filter, line );

	plp_snprintf( line, sizeof(line), "HOME=%s", pw->pw_dir);
	add_env( filter, line );

	plp_snprintf( line, sizeof(line), "LOGDIR=%s", pw->pw_dir);
	add_env( filter, line );

	plp_snprintf( line, sizeof(line), "PATH=%s", Filter_path);
	add_env( filter, line );

	plp_snprintf( line, sizeof(line), "LD_LIBRARY_PATH=%s", Filter_ld_path);
	add_env( filter, line );

	add_env( filter, "SHELL=/bin/sh" );

	plp_snprintf( line, sizeof(line), "IFS= \t");
	add_env( filter, line );

	s = getenv( "TZ" );
	if( s ){
		plp_snprintf( line, sizeof(line), "TZ=%s", s );
		add_env( filter, line );
	}
	if( SDpathname && (s = Clear_path( SDpathname )) ){
		plp_snprintf( line, sizeof(line), "SPOOL_DIR=%s", s );
		add_env( filter, line );
	}

	if( CDpathname && (s = Clear_path( CDpathname )) ){
		plp_snprintf( line, sizeof(line), "CONTROL_DIR=%s", s );
		add_env( filter, line );
	}

	if( printcap_entry ){
		s = Linearize_pc_list( printcap_entry, "PRINTCAP_ENTRY=" );
		if( s ){
			add_env( filter, s );
		}
	}
	if( cf ){
		s = Copy_hf( &cf->control_file_lines, &cf->control_file_print,
				"CONTROL=", "" );
		if( s ){
			add_env( filter, s );
		}
	}
	if( Is_server == 0 && Pass_env ){
		char envcpy[LINEBUFFER];
		safestrncpy( envcpy, Pass_env );
		for( s = envcpy; s && *s; s = end ){
			while( isspace( *s ) ) ++s;
			end = strpbrk( s, " \t,;" );
			if( end ) *end++ = 0;
			if( strlen(s) ){
				if( (value = getenv( s )) && strlen(value) != 0 ){
					plp_snprintf( line, sizeof(line),
						"%s=%s", s, value );
					add_env( filter, line );
				}
			}
		}
	}

	/* for null entry */
	add_env( filter, 0 );
	if(DEBUGL4 ){
		int i;
		logDebug( "setup_envp: environment" );
		for( i = 0; filter->envp.list[i]; i++ ){
			logDebug( " [%d] '%s'", i, filter->envp.list[i]);
		}
	}
}

/***************************************************************************
 * char *find_executable( char *execname, struct filter *filter )
 *  locate the directory containing the executable file and return full path
 *  Note: a copy is returned;  this is NOT malloced
 *  If none found, return 0
 ***************************************************************************/


static int is_executable( char *step, uid_t uid_v, gid_t gid_v )
{
	struct stat statb;

	DEBUG3("is_executable: trying '%s'", step);
	if( stat( step, &statb) >= 0
	 
		/* check to see if we can execute it; note that this is not
		 * a security-related check, so don't worry about race
		 * conditions.
		 */
		 && (((statb.st_uid == uid_v) && (statb.st_mode & S_IXUSR)) ||
			 ((statb.st_gid == gid_v) && (statb.st_mode & S_IXGRP)) ||
			 (statb.st_mode & S_IXOTH) ) ){
			DEBUG1("is_executable: yes '%s'", step );
			return(1);
	}
	DEBUG1("is_executable: no '%s'", step );
	return(0);
}

char * find_executable( char *execname, struct filter *filter )
{
	char tmppath[SMALLBUFFER];
	char *step, *end;
	uid_t uid_v;
	gid_t gid_v;

	DEBUG4("find_executable: path '%s', exec '%s'", Filter_path, execname );

	if( execname == 0 || *execname == 0 ){
		return( 0 );
	}
	uid_v = geteuid();
	gid_v = getegid();
	if( execname[0] == '/' ){
		if( is_executable( execname, uid_v, gid_v ) ){
			DEBUG1("find_executable: found executable '%s'", execname );
			return(execname);
		} else {
			DEBUG1("find_executable: not executable '%s'", execname );
			return(0);
		}
	}

	tmppath[0] = 0;
	if( Filter_path ){
		safestrncpy(tmppath, Filter_path );
	}

	/* we can now rip tmppath to shreds with impunity */

	for( step = tmppath; step && *step; step = end ){
		while( isspace(*step) ) ++step;
		end = strpbrk( step, ":; \t" );
		if( end ){
			*end++ = 0;
		}
		if( *step == 0 ) continue;

		if( *step != '/' ){
			fatal( LOG_ERR, "find_executable: Filter_path has bad entry '%s'",
				Filter_path );
		}

		Init_path( &filter->exec_path, step );
		step = Add_path( &filter->exec_path, execname );
		if( is_executable( step, uid_v, gid_v ) ){
			DEBUG1("find_executable: found executable '%s'", step );
			return(step);
		}
	}
	DEBUG1("find_executable: did not find executable '%s'", execname );
	return(0);
}


/***************************************************************************
 * word_in_quotes( char **string, - starting point of word in quotes
 *  - we remote the quotes
 *
 ***************************************************************************/
char * word_in_quotes( char *start )
{
	char *end, *begin, *next;
	int ch = *start;
	int len;


	/* find the end of the string */
	if( start[-1] == '\\' ){
		/* we have an escaped starting quote */
		strcpy( start-1, start );
		return( start );
	}
	/* find the end of the string */
	*start = 0;
	for( end = start+1, begin = start;
		(next = strchr( end, ch )); end = next+1 ){
		len = (next - end);
		/* DEBUG4("word_in_quotes: start 0x%x, begin 0x%x, next 0x%x, end 0x%x, len %d", 
			start, begin, next, end, len ); */
		strncpy( begin, end, len );
		begin += len;
		*begin = 0;
		if( next[-1] == '\\' ){
			begin[-1] = ch;
		} else {
			break;
		}
	}
	if( next == 0 ){
		return( 0 );
	}
	return(next+1);
}

static char quotes[] = "'\"";
static char metachars[] = "<>`;";       /* whinge if these crop up */

/*
 * cmd_split: splits up a command line into arguments
 * The following conventions are used:
 *   x -> x
 *   'N' -> N, "N" ->N  no interpretation of N
 *   \' -> '  \" -> ' 
 *   Note: x'N' ->xN  x"N" -> xN
 */

static int cmd_split( struct filter *filter )
{
	int c;
	char *cmd, *start;

	/* free old stuff */
	if( filter->copy ){
		free(filter->copy);
		filter->copy = 0;
	}
	/* clear the split up arguments */
	clear_malloc_list( &filter->args, 0 );

	if( filter->args.count+1 >= filter->args.max ){
		extend_malloc_list( &filter->args, sizeof( char * ),
		filter->args.count+25 );
	}
	if( filter->cmd && *filter->cmd ){
		/* duplicate string */
		c = strlen(filter->cmd)+2;
		malloc_or_die( filter->copy, c );
		filter->copy[0] = 0;
		strcpy( filter->copy+1, filter->cmd );
		
		/* work your way down the list */
		for( cmd = filter->copy+1; cmd && *cmd; ){
			/* Skip leading whitespace. */
			while( (c = *cmd) && isspace( c ) ) ++cmd;

			/* Process next token. */
			if( *cmd == 0 ){
				break;
			}
			/* add an entry */
			if( filter->args.count+1 >= filter->args.max ){
				extend_malloc_list( &filter->args,
					sizeof( char * ), 25 );
			}
			filter->args.list[filter->args.count++] = start = cmd;

			/* find end of word */
			/* DEBUG4("cmd_split: starting '%s'", start ); */
			while( (c = *cmd) && !isspace( c ) ){
				if( strchr( quotes, c ) ){
					/* DEBUG4("cmd_split: starting quote '%s'", cmd ); */
					cmd = word_in_quotes( cmd );
					if( cmd == 0 ){
						log( LOG_ERR, "cmd_split: unbalanced quote in '%s`",
							filter->cmd );
						return(-1);
					}
				} else if( strchr(metachars, c ) ){
					log( LOG_ERR, "cmd_split: metachar in '%s'",
						filter->cmd );
					return(-1);
				} else {
					++cmd;
				}
			}
			if( cmd && *cmd ){
				*cmd++ = 0;
			}
			/* DEBUG4("cmd_split: word '%s'", start ); */
		}
	}
	if( filter->args.count+1 >= filter->args.max ){
		extend_malloc_list( &filter->args, sizeof( char * ), 10 );
	}
	filter->args.list[filter->args.count] = 0;
	return( 0 );
}


/***************************************************************************
 * Expand_command(
 *   struct control_file *cfp - control file
 *   char *bp - beginning of target buffer
 *   char *ep - end of target buffer
 *   char *s  - expand this string
 *   int fmt  - format information of file 
 *   struct data_file *data_file - data file
 *  expand the '$x' entries in the command
 * Expansion form -
 *  $[0| ][-][']X - note that 0,-,' must be before key
 *  $X -> -X'field'
 *  $0 or '$ ' (space) adds a space after the X, i.e. $X -> -X 'field'
 *  $-                 suppresses -X tag, i.e. $-X -> 'field'
 *  $'                 suppresses quotes, i.e. $'X -> -Xfield
 ***************************************************************************/

char *Expand_command( struct control_file *cfp,
	char *bp, char *ep, char *s, int fmt, struct data_file *data_file )
{
	int c, space, notag, noquote;
	char name[LINEBUFFER];
	char *t;

	/*
	 * replace $X with the corresponding value
	 * note that '$$ -> $
	 */
	DEBUG4("Expand_command: fmt %c, expanding '%s'", fmt, s );
	if( s && bp ) while( (bp < ep ) && (c = (*bp++ = *s++)) ){
		if( c == '$' && (c = *s) ){
			if( c != '$' ){
				notag = 0;
				space = 0;
				noquote = 0;
				t = 0;
				while( strchr( " 0-'", c) ){
					switch( c ){
					case '0': case ' ': space = 1; break;
					case '-':           notag = 1; break;
					case '\'':          noquote = 1; break;
					}
					c = *++s;
				}
				if( c == '{' ){
					t = name;
					++s;
					while( *s && *s != '}' ){
						*t++ = *s++;
					}
					*t = 0;
					t = name;
				}
				/* DEBUG2("Expand_command: expanding '%c', space %d, notag %d, name '%s'",
					c, space, notag, t ); / **/
				--bp;
				bp = Do_dollar( cfp, bp, ep, c, fmt, space, notag,
					data_file, noquote, t );
			}
			++s;
		}
	}
	*bp = 0;
	return( bp );
}

#if defined(HAVE_TERMIOS_H)
# include <termios.h>
#endif

void Flush_filter( struct filter *filter )
{
#if defined(HAVE_TCFLUSH)
	if( filter->output > 0 && isatty( filter->output ) ){
		tcflush( filter->output, TCIOFLUSH );
	}
	if( filter->input > 0 && isatty( filter->input ) ){
		tcflush( filter->input, TCIOFLUSH );
	}
#endif
}

/***************************************************************************
 * int Close_filter( struct filter *filter, int timeout, const char *key )
 * - filter is the data structure with the filte interformation
 * - timeout > 0  we block for timeout seconds, waiting for exit
 * - timeout = 0  we block for infinite seconds
 * - timeout < 0  we do not block
 * key is identifier who called
 ***************************************************************************/
int Close_filter( struct control_file *cfp,
	struct filter *filter, int timeout, const char *key )
{
	pid_t result = 0;
	plp_status_t status = 0;
	int err;
	char error[LINEBUFFER];

	if( key == 0 ) key = "ANONYMOUS";
	DEBUG3("Close_filter: starting input %d, pid %d, timeout %d, key %s",
		filter->input, filter->pid, timeout, key );
	if( filter->input > 0 && !isatty( filter->input ) ){
		close( filter->input );
		filter->input = -1;
	}
	if( filter->pid > 0 ){
		kill( filter->pid, SIGCONT );
		Alarm_timed_out = 0;
		if( timeout > 0 ){
			result = plp_waitpid_timeout( timeout, filter->pid, &status, 0 );
		} else if( timeout == 0 ){
			result = plp_waitpid( filter->pid, &status, 0 );
		} else {
			result = plp_waitpid( filter->pid, &status, WNOHANG );
		}
		err = errno;

		DEBUG3( "Close_filter: '%s' pid %d, errno '%s', status '%s'",
			key,result, Errormsg(err), Decode_status(&status) );
		if( Alarm_timed_out ){
			plp_snprintf( error, sizeof(error),
				"timeout %d for %s filter close", timeout, key );
			if( cfp ) safestrncpy( cfp->error, error );
			setstatus( cfp, error );
			status = JABORT;
		} else if( result == -1 && err == ECHILD ){
			/* no child, already dead */
			filter->pid = 0;
		} else if( result == -1 && err != ECHILD ){
			/* something odd happened here */
			status = JFAIL;
		} else if( result == 0 ){
			/* we did not get a process */
			status = JFAIL;
		} else {
			filter->pid = -1;
			if( WIFSIGNALED( status ) ){
				plp_snprintf( error, sizeof(error),
					"%s filter died with signal '%s'", key,
					Decode_status( &status )  );
				if( cfp ) safestrncpy( cfp->error, error );
				setstatus( cfp, error );
				status = JABORT;
			} else if( status ) {
				int n = WEXITSTATUS( status );

				/* adjust status so 1 -> JFAIL */
				switch( n ){
#define XTLATE(J) case J-(JFAIL-1): n = J; break
					XTLATE(JFAIL); XTLATE(JABORT); XTLATE(JREMOVE);
					XTLATE(JACTIVE); XTLATE(JIGNORE); XTLATE(JHOLD);
					XTLATE(JNOSPOOL); XTLATE(JNOPRINT);
					case JFAIL: case JABORT: case JREMOVE:
					case JIGNORE: case JHOLD: case JNOSPOOL:
					case JNOPRINT:
						break;
					default: n = JABORT;
						break;
				}
				plp_snprintf( error, sizeof(error),
					"%s filter died with status %d '%s'", key,
					WEXITSTATUS( status ), Server_status( n ) );
				if( cfp && n == JABORT ) safestrncpy( cfp->error, error );
				setstatus( cfp, error );
				status = n;
			}
		}
	}
	DEBUG3("Close_filter: filter %s status 0x%x (%s)",key,status, Server_status(status) );
	return( status );
}


/***************************************************************************
 * char *Filter_read( char *name - name written to filter STDIN
 *      struct malloc_list - received information stored in this list
 *      char *filter - filter name
 * 1. The filter is created and the name string written to the filter
 *     STDIN.
 * 2. The output from the filter is read and stored in the malloc_list;
 *     this is also returned from the filter.
 * Note: the options passed to the filter are the defaults for
 *   the filters.
 ***************************************************************************/

char *Filter_read( char *name, struct malloc_list *list, char *filter )
{
	int buffer_total = 0;		/* total length of saved buffers */
	int buffer_index = 0;			/* buffer index in the printcapfile */
	int pipes[2];				/* pipe to read from filter */
	char *s;					/* ACME pointers */
	int i, len;					/* ACME integers */
	char *buffer, **buffers;	/* list of buffers and a buffer */
	char line[LINEBUFFER];		/* buffer for a line */
	int list_index = 0;
	struct control_file cf;

	buffer = 0;
	/* make a pipe */

	if( name == 0 || *name == 0 ){
		fatal( LOG_ERR, "Filter_read: no name" );
	}
	plp_snprintf( line, sizeof( line ), "%s\n", name );
	if( pipe( pipes ) < 0 ){
		logerr_die( LOG_ERR, "Filter_read: cannot make pipes for '%s'",
			filter );
	}
	DEBUG4("Filter_read: pipe [%d,%d]", pipes[0],pipes[1] );
	/* create the printcap info process */
	/* Make_filter( 'f',(void *)0,&Pr_fd_info,filter,1, 0, pipes[1], */
	memset( &cf, 0, sizeof(cf) );
	Make_filter( 'f',&cf,&Pr_fd_info,filter,0, 0, pipes[1],
		(void *)0, (void *)0, 0, 0, 0 );
	DEBUG4("Filter_read: filter pid %d, input fd %d, sending '%s'",
		Pr_fd_info.pid, Pr_fd_info.input, name?name:"<NULL>" );
	close( pipes[1] );

	/* at this point, you must write to the filter pipe */
	if( Write_fd_str( Pr_fd_info.input, line ) < 0 ){
		logerr( LOG_INFO, "Filter_read: filter '%s' failed", filter );
	}
	close( Pr_fd_info.input );

	/*
	 * read from the filter into a buffer
	 */
	do{
		if( buffer_total - buffer_index < LARGEBUFFER + 1 ){
			if( buffer == 0 ){
				buffer_total = LARGEBUFFER + 1024;
				list_index = list->count;
				buffer = add_buffer( list, buffer_total );
				buffers = (void *)list->list;
				if( buffers[list_index] != buffer ){
					fatal( LOG_ERR, "Filter_read: wrong buffer!" );
				}
			} else {
				buffer_total += LARGEBUFFER + 1024;
				buffer = realloc( buffer, buffer_total );
				if( buffer == 0 ){
					logerr_die( LOG_ERR, "Filter_read: realloc failed" );
				}
				buffers = (void *)list->list;
				buffers[list_index] = buffer;
			}
		}
		len = buffer_total - buffer_index - 1;
		s = buffer+buffer_index;
		i = len;
		for( ;
			len > 0 && (i = read( pipes[0], s, len ) ) > 0;
			len -= i, s += i, buffer_index += i );
		DEBUG4("Filter_read: len %d", buffer_index );
		/* check to see if we have a buffer read */
	} while( i > 0 );
	/* close the output side of the pipe */
	close( pipes[0] );

	if( buffer_index == 0 ){
		/* nothing read */
		free( buffer );
		buffer = 0;
		--list->count;
	} else {
		buffer[buffer_index] = 0;
	}

	Close_filter( 0, &Pr_fd_info, 0, "printcap" );
	DEBUG4( "Filter_read: buffer_index %d, '%s'",
		buffer_index, buffer );
	return( buffer );
}


static int Make_errlog( int stderr_pipe[], struct control_file *cf,
	char *header )
{
	int len, count, i;
	int pid;	/* for the stderr */
	char *endstr, *start;
	char stderr_info[LARGEBUFFER];

	if( (pid = dofork(0)) == 0 ){
		/* we are the daughter, waiting for stderr output */
		DEBUG4( "Make_errlog: pid %d, ppid %d, pipe[%d,%d], header '%s'",
			getpid(), getppid(), stderr_pipe[0], stderr_pipe[1], header );
		if( dup2( stderr_pipe[0], 0 ) < 0 ){
			Errorcode = JABORT;
			logerr_die(LOG_ERR, "Make_errlog: stderr_to_logger - dup2 failed");
		}
		/* save the logger fd so you do not open another */
		Dup_logger_fd( 1 );
		setup_close_on_exec(3); /* this'll close the unused pipe fds */
		stderr_info[0] = 0;	/* clear out buffer */
		start = stderr_info+1;
		start[-1] = '.';
		*start = 0;
		Errorcode = 0;
		do{
			/* we get the stderr information */
			len = strlen( start );
			/* read more into buffer */
			count = read( 0, start+len, sizeof(stderr_info)-len-3 );
			if( count > 0 ){
				start[count+len] = 0;
			} else {
				break;
			}
			/* we put out lines */
			DEBUG3( "Make_errlog: stderr pid read %d '%s'", count, start );
			endstr = strrchr( start, '\n' );
			if( endstr ){
				*endstr++ = 0;
			} else if( strlen( stderr_info+1 ) + LINEBUFFER
				> sizeof( stderr_info ) ){
				endstr = start+len;
			} else {
				continue;
			}
			if( Write_fd_str( 2, start ) < 0 
				|| Write_fd_str( 2, "\n" ) < 0 ) cleanup( 0 );
			if( start[0] == '.' ){
				setmessage( cf, header, "%s", &start[-1] );
			} else {
				setmessage( cf, header, "%s", start );
			}
			for( i = 0; (stderr_info[i+1] = endstr[i]); ++i );
		}while( count > 0 );
		if( start[0] == '.' ){
			setmessage( cf, header, "%s", &start[-1] );
		} else {
			setmessage( cf, header, "%s", &start[0] );
		}
		setmessage( cf, header, "FINISHED" );
		cleanup(0);
	} else if( pid < 0 ){
		Errorcode = JABORT;
		logerr_die( LOG_NOTICE, "Make_errlog: stderr_to_logger fork failed" );
	}
	return(0);
}
