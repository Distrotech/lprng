/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: Setup_filter.c
 * PURPOSE:
 **************************************************************************/

static char *const _id =
"$Id: setup_filter.c,v 3.5 1996/09/09 14:24:41 papowell Exp papowell $";

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
#include "lp_config.h"
#include "pr_support.h"
#include "timeout.h"
#include "setuid.h"
#include "rw_pipe.h"
#include "decodestatus.h"
  
static char * do_dollar( struct control_file *cf, char *s, char *e,
	int type, int fmt, int space, int notag, struct data_file *df,
	int noquotes );

static struct malloc_list env;
static struct malloc_list envp;

void setup_close_on_exec( int minfd );
char * find_executable( char *execname );
static void setup_envp( struct control_file *cf, struct pc_used *pc_used );
static int cmd_split( struct filter *filter );

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


int Make_filter( int key,
	struct control_file *cf,
	struct filter *filter, char *line, int noextra,
	int read_write, int print_fd,
	struct pc_used *pc_used, struct data_file *data_file, int acct_port,
	int stderr_to_logger )
{
	int p[2], stderr_pipe[2];
	char *executable;
	int root = 0;

	/* search the printcap file for the current printer */
	filter->input = -1;
	if( line == 0 || *line == 0 ){
		fatal( LOG_ERR, "Make_filter: no filter for format '%c'", key );
	}
	/*
	 * set line command line
	 */
	if( Setup_filter( key, cf, line, filter, noextra, data_file ) ){
		fatal( LOG_ERR, "Make_filter: format '%c' bad filter '%s'", 
			key, line );
	}
	if( Debug > 2 ) dump_filter( "Make_filter", filter );

	/*
	 * find the executable
	 */
	/* check for ROOT key */
	if( (executable = filter->args.list[0]) && !strcmp( executable, "ROOT") ){
		root = 1;
	}
	executable = find_executable( filter->args.list[root] ); 
	if( executable == 0 ){
		Errorcode = JABORT;
		fatal( LOG_ERR, "Make_filter: cannot find executable file '%s'",
			filter->args.list[0] );
	}

	/*
	 * get the pipes for the filter input: we use bidirectional ones.
	 */
	if( rw_pipe( p ) < 0 ){
		Errorcode = JABORT;
		logerr_die( LOG_ERR, "Make_filter: rw_pipe failed" );
	}
	if( (p[0] < 3) || p[1] < 3){
		Errorcode = JABORT;
		fatal( LOG_NOTICE, "Make_filter: pipe FDs out of range");
	}

	/*
	 * set up the environment variables
	 */
	setup_envp(cf, pc_used);

	/*
	 * create pipe and fork
	 */

	DEBUG2 ("Make_filter: stderr_to_logger %d, starting filter '%s'",
		stderr_to_logger, filter->cmd );
	if( (filter->pid = fork()) == 0 ){              /* child */
		/* pipe is stdin */ /* printer is stdout */
		if( stderr_to_logger ){
			int len, count, i, filter_pid;
			int pid;	/* for the stderr */
			char *s, *end;
			char stderr_header[SMALLBUFFER];
			char stderr_info[SMALLBUFFER];

			filter_pid = getpid();
			if( pipe( stderr_pipe ) < 0 ){
				Errorcode = JABORT;
				logerr_die( LOG_ERR, "Make_filter: stderr_to_logger pipe failed" );
			}
			if( (pid = fork()) == 0 ){
				/* we are the daughter, waiting for stderr output */
				DEBUG8( "Make_filter: stderr pid %d, filter pid %d, pipe[%d,%d]",
					getpid(), filter_pid, stderr_pipe[0], stderr_pipe[1] );
				close( stderr_pipe[1] ); /* close stderr output side */
				close( p[0] ); /* close filter input side */
				close( p[1] ); /* close filter output side */
				stderr_info[0] = 0;	/* clear out buffer */
				stderr_info[1] = 0;	/* clear out buffer */
				plp_snprintf( stderr_header, sizeof(stderr_header),
					"FILTER_STATUS PID %d", filter_pid );
				setmessage( cf, stderr_header, "COMMANDLINE %s", filter->cmd );
				do{
					/* we get the stderr information */
					len = strlen( stderr_info+1 );
					/* read more into buffer */
					count = read( stderr_pipe[0], stderr_info+1+len, sizeof(stderr_info)-len-2 );
					if( count > 0 ){
						stderr_info[count+len+1] = 0;
						/* we put out lines */
						DEBUG9( "Make_filter: stderr pid read %d '%s'",
							count, stderr_info+1 );
						if( Write_fd_str( 2, stderr_info+1 ) < 0 ){
							exit( 0 );
						}
						for( s = stderr_info+1;
							s && (end = strchr( s, '\n' ));
							s = end+1 ){
							*end = 0;
							if( *s == '.' ){
								--s;
								*s = '.';
							}
							setmessage( cf, stderr_header, "%s", s );
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
					setmessage( cf, stderr_header, "%s", s );
				}
				setmessage( cf, stderr_header, "FINISHED" );
				exit(0);
			} else if( pid < 0 ){
				Errorcode = errno;
				logerr_die( LOG_NOTICE, "Make_filter: stderr_to_logger fork failed" );
			}
		}
		if( dup2(p[0], 0) < 0 
			|| (print_fd != 1 && dup2( print_fd, 1) < 0) ){
			Errorcode = errno;
			logerr_die( LOG_NOTICE, "Make_filter: dup2 failed");
		}
#if defined(ROOT_PERMS_TO_FILTER_SECURITY_LOOPHOLE)
		if( root ) {
			DEBUG2( "Make_filter: running %s as root", executable );
			Full_root_perms();   /* run as "root" */
		} else
#endif
		Full_daemon_perms();   /* run as "daemon" */
		if( stderr_to_logger ){
			/* close input and dup output side to stderr */
			close( stderr_pipe[0] );
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
		execve( executable, &filter->args.list[root], envp.list );
		Errorcode = JABORT;
		logerr_die( LOG_NOTICE, "Make_filter: execve '%s' failed", executable );
	} else if( filter->pid < 0 ){
		Errorcode = errno;
		logerr_die( LOG_NOTICE, "Make_filter: fork failed" );
	}
	filter->input = p[1];
	(void) close( p[0]);        /* close input side */
	/* we save the output device  for flushing if it is a terminal
	 * this might prevent processes from locking up when device goes
	 *	offline and we time out
	 */
	if( isatty( print_fd ) ){
		filter->output = print_fd;
	} else {
		filter->output = -1;
	}
	DEBUG3 ("started filter process %d,input fd %d, output fd %d, '%s'",
		filter->pid, filter->output, p[1], 
		filter->cmd );
	return( 0 );
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
		if( OF_filter_options == 0 ) OF_filter_options = Filter_options;
		if( fmt == 'o'){
			opts = OF_filter_options;
		} else {
			/*
			 * filter -PPrinter -wwidth -llength -xwidth -ylength -Fx
			 *     [-c] [-iindent] [-Zoptions]
			 *     [-Cclass] [-Jjob] -nlogin -hHost -Fformat [affile]
			 */
			opts = Filter_options;
		}
	}

	
	DEBUG8("Setup_filter: fmt '%c', filtername '%s', noextra %d",
		fmt, filtername, noextra );
	DEBUG8("Setup_filter: OF_filter_options '%s'", OF_filter_options );
	DEBUG8("Setup_filter: Filter_options '%s'", Filter_options );
	DEBUG8("Setup_filter: BK_of_filter_options '%s'", BK_of_filter_options );
	DEBUG8("Setup_filter: BK_filter_options '%s'", BK_filter_options );
	DEBUG8("Setup_filter: options '%s'", opts );

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
	DEBUG8("Setup_filter: noextra %d, filter '%s'",
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

	DEBUG8("Setup_filter: expanding '%s'", cmd );

	ep = fixedcmd + sizeof( fixedcmd )-1;
	bp = Expand_command( cf, fixedcmd, ep, cmd, fmt, data_file );
	if( bp >= ep ){
		logerr( LOG_INFO,"Setup_filter: filter line too long '%s'", fixedcmd );
		return(1);
	}
	*ep = 0;
	filter->cmd = safestrdup( fixedcmd );
	DEBUG8("Setup_filter: cmd '%s'", filter->cmd );

	if( cmd_split( filter ) ){
		logerr( LOG_INFO,"Setup_filter: bad filter expansion '%s'",
			filter->cmd );
		return( 1 );
	}

	/*
	 * now purge all strings of metacharacters, replace with '_'
	 */
	list = filter->args.list;
	for( i = 0; i < filter->args.count; ++i ){
		if( (bp = Find_meta( list[i] ) ) ){
			Clean_meta( bp );
		}
	}

	return( 0 );
}

/***************************************************************************
 * This is a very good idea.  Reinforced, double checked, and
 * added some cleanup 
 * Patrick Powell Sun May 21 08:18:30 PDT 1995
 ***************************************************************************
 * enhanced for security by jmason. This now wraps the supplied argument
 * in ticks( ie. -J'(stdin)') before passing it to sh for the exec; it
 * also checks the argument to see if it, in turn, contains ticks.
 * If it does, they are prepended with a backslash character( \).
 * PLP's safe-exec routines grok this correctly.
 *
 * This should stop users from being able to get lpd to exec arbitrary
 * command-lines as daemon, while still allowing a large degree of freedom
 * to filter writers. (however, filter writers still need to exercise
 * care, otherwise the "daemon" userid is vulnerable).
 ***************************************************************************/
static char *
add_str( char *s, char *e, char *s2)
{
	if( s2 && s ){
		while( s2 && *s2 && (s < e) ){
			*(s++) = *s2++;
		}
		if( s < e) *s = 0;
		if( s >= e ) s = 0;
	}
	return( s );
}

static char *
add_stropt( char *s, char *e, char *s2)
{
	if( s2 == 0 ){
		s2 = "NULL";
	}
	if( s ){
		if( s < e ) *(s++) = '\'';
			/* wrap the argument in ticks( ie. -J'(stdin)'). */
		while( s2 && *s2 && (s < e) ){
			if( *s2 == '\'' ){
				*(s++) = '\\';
			}
			if( s < e) *(s++) = *s2++;
		}
		if( s < e) *(s++) = '\'';
		if( s < e) *s = 0;
		if( s >= e ) s = 0;
	}
	return( s );
}

static char *
add_num( char *s, char *e, int n)
{
	(void) plp_snprintf( s, e-s, "%d", n );
	return( s+strlen(s) );
}


static char *
add_numopt( char *s, char *e, int n)
{
	(void) plp_snprintf( s, e-s, "'%d'", n );
	return( s+strlen(s) );
}

/***************************************************************************
 *char *do_dollar( struct control_file *cf, char *s, char *e,
 *	int type, int fmt, int space, int notag, struct *datafile
 *  int noquote )
 * Expand the $<tag> information in the filter definition
 * Note: see the code for the keys!
 * replace %X with -X<value>
 * if space != 0, then replace %X with -X <value>
 *
 * cf = control file where we get values from
 * s  = start or next position in expansion string
 * e  = end position in expansion string
 * type = key value
 * fmt  = filter format (i.e. - 'o' for of)
 * space = 1, put space after -X, i.e. '-X value'
 * notag = 1, do not put tag
 * datafile = data file where we get values
 * noquote  = 1, do not put quotes around the expansion values
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

static char *
do_dollar( struct control_file *cf, char *s, char *e, int type,
	int fmt, int space, int notag, struct data_file *data_file, int noquote )
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
	/* DEBUG9("do_dollar: type '%c' cf 0x%x, space %d, notag %d",
		type, cf, space, notag ); / **/
	switch( type ){
		case 'a': str = Accounting_file; break;
		case 'b': if( cf ) kind = INTEGER_K; n = cf->jobsize; break;
		case 'c': prefix = 0; str =  (fmt == 'l' ? " -c" : (char *)0); break;
		case 'd': str = Control_dir; break;
		case 'e': 	if( data_file && (str = data_file->openname) == 0 ){
						str = data_file->transfername;
					}
					break;
		case 'f': if( data_file ) str = chop(data_file->Ninfo); break;
		case 'h':	if( cf ){
						/* get the short host name format */
						char *s;
						str = Get_realhostname( cf );
						if( str && (s = strchr( str, '.' )) ) *s = 0;
					}
					break;
		case 'i': if( cf ) str = chop(cf->INDENT); break;
		case 'j': if( cf ) kind = INTEGER_K; n = cf->number; break;
		case 'k': if( cf ) str = cf->name; break;
		case 'l': kind = INTEGER_K; n = Page_length; break;
		case 'm': kind = INTEGER_K; n = Cost_factor; break;   /* cost */
		case 'n': if( cf ) str = chop(cf->LOGNAME); break;
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
	/* DEBUG8("do_dollar: prefix '%s'", p ); */
	switch( kind ){
		case STRING_K:
			if( str ){
				if( prefix && notag == 0 ){
					s = add_str( s, e, p );
				}
				if( space ){
					s = add_str( s, e, " " );
				}
				/* DEBUG8("do_dollar: string '%s'", str ); */
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
    int fd;

    for( fd = minfd; fd < getdtablesize( ); fd++ ){
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
 * setup_envp(struct control_file *cf, struct pc_used *pc_used):
 *
 * set up a safe environment for filters.
 * consists of:
 *  LD_LIBRARY_PATH (safe value), PATH (safe value), TZ (from env), 
 *  USER, LOGNAME, HOME, LOGDIR, SHELL, IFS (safe value).
 *  SPOOL_DIR, CONTROL_DIR, PRINTCAP, and CONTROL_FILE
 ***************************************************************************/

static void add_env( char *p )
{
	int i;
	char *s;

	DEBUG9("add_env: '%s'", p );
	if( p ){
		i = strlen(p);
		s = add_buffer( &env, i+1 );
		strcpy( s, p );
		p = s;
	}
	if( envp.count >= envp.max ){
		extend_malloc_list( &envp, sizeof( char *), 100 );
	}
	envp.list[envp.count++] = p;
}

static void setup_envp(struct control_file *cf,  struct pc_used *pc_used)
{
	struct passwd *pw;
	char line[LINEBUFFER];
	char *s;

	clear_malloc_list( &env, 1 );
	clear_malloc_list( &envp, 0 );

	if( (pw = getpwuid( getuid())) == 0 ){
		logerr_die( LOG_INFO, "setup_envp: getpwuid(%d) failed", getuid());
	}
	plp_snprintf(line, sizeof(line), "USER=%s", pw->pw_name);
	add_env( line );

	plp_snprintf(line, sizeof(line), "LOGNAME=%s", pw->pw_name);
	add_env( line );

	plp_snprintf( line, sizeof(line), "HOME=%s", pw->pw_dir);
	add_env( line );

	plp_snprintf( line, sizeof(line), "LOGDIR=%s", pw->pw_dir);
	add_env( line );

	plp_snprintf( line, sizeof(line), "PATH=%s", Filter_path);
	add_env( line );

	plp_snprintf( line, sizeof(line), "LD_LIBRARY_PATH=%s", Filter_ld_path);
	add_env( line );

	add_env( "SHELL=/bin/sh" );

	plp_snprintf( line, sizeof(line), "IFS= \t");
	add_env( line );

	s = getenv( "TZ" );
	if( s ){
		plp_snprintf( line, sizeof(line), "TZ=%s", s );
		add_env( line );
	}
	if( SDpathname && (s = Clear_path( SDpathname )) ){
		plp_snprintf( line, sizeof(line), "SPOOL_DIR=%s", s );
		add_env( line );
	}

	if( CDpathname && (s = Clear_path( CDpathname )) ){
		plp_snprintf( line, sizeof(line), "CONTROL_DIR=%s", s );
		add_env( line );
	}

	if( pc_used ){
		s = Linearize_pc_list( pc_used, "PRINTCAP=" );
		if( s ){
			add_env( s );
		}
	}
	if( cf ){
		s = cf->cf_copy;
		if( s ){
			add_env( s );
		}
	}
	
	/* for null entry */
	add_env( 0 );
	if( Debug > 5 ){
		int i;
		for( i = 0; envp.list[i]; i++ ){
			logDebug( "setup_envp: '%s'", envp.list[i]);
		}
	}
}

/***************************************************************************
 * char *find_executable( char *execname )
 *  locate the directory containing the executable file and return full path
 *  Note: a copy is returned;  this is NOT malloced
 *  If none found, return 0
 ***************************************************************************/

static uid_t uid_v;
static gid_t gid_v;

static int is_executable( char *step )
{
	struct stat statb;

	DEBUG4("is_executable: trying '%s'", step);
	if( stat( step, &statb) >= 0
	 
		/* check to see if we can execute it; note that this is not
		 * a security-related check, so don't worry about race
		 * conditions.
		 */
		 && (((statb.st_uid == uid_v) && (statb.st_mode & S_IXUSR)) ||
			 ((statb.st_gid == gid_v) && (statb.st_mode & S_IXGRP)) ||
			 (statb.st_mode & S_IXOTH) ) ){
			DEBUG8("is_executable: yes '%s'", step );
			return(1);
	}
	DEBUG8("is_executable: no '%s'", step );
	return(0);
}

char * find_executable( char *execname )
{
	static struct dpathname exec_path;
	char tmppath[MAXPATHLEN];
	char *step, *end;
	int c;

	DEBUG8("find_executable: path '%s', exec '%s'", Filter_path, execname );

	if( execname == 0 ){
		return( (char *)0 );
	}
	(void)strncpy(exec_path.pathname, execname,
		sizeof(exec_path.pathname) );
	uid_v = geteuid();
	gid_v = getegid();
	if( execname[0] == '/' ){
		if( is_executable( exec_path.pathname ) ){
			DEBUG8("find_executable: found executable '%s'", exec_path.pathname );
			return(exec_path.pathname);
		} else {
			DEBUG8("find_executable: not executable '%s'", exec_path.pathname );
			return(0);
		}
	}

	tmppath[0] = 0;
	if( Filter_path ){
		step = Filter_path;
		while( (c = *step) && isspace( c ) ) ++step;
		safestrncat(tmppath, step );
	}

	/* we can now rip tmppath to shreds with impunity */

	for( step = tmppath; step && *step; step = end ){
		end = strpbrk( step, ":; \t" );
		if( end ){
			*end++ = 0;
		}

		if( *step != '/' ){
			logerr( LOG_ERR, "find_executable: Filter_path has bad entry '%s'",
				Filter_path );
		}

		Init_path( &exec_path, step );
		step = Add_path( &exec_path, execname );
		if( is_executable( step ) ){
			DEBUG8("find_executable: found executable '%s'", exec_path.pathname );
			return(step);
		}
	}
	DEBUG8("find_executable: did not find executable '%s'", execname );
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
		/* DEBUG9("word_in_quotes: start 0x%x, begin 0x%x, next 0x%x, end 0x%x, len %d", 
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
			if( filter->args.count >= filter->args.max ){
				extend_malloc_list( &filter->args,
					sizeof( char * ), 100 );
			}
			filter->args.list[filter->args.count++] = start = cmd;

			/* find end of word */
			/* DEBUG9("cmd_split: starting '%s'", start ); */
			while( (c = *cmd) && !isspace( c ) ){
				if( strchr( quotes, c ) ){
					/* DEBUG9("cmd_split: starting quote '%s'", cmd ); */
					cmd = word_in_quotes( cmd );
					if( cmd == 0 ){
						logerr( LOG_ERR, "cmd_split: unbalanced quote in '%s`",
							filter->cmd );
						return(-1);
					}
				} else if( strchr(metachars, c ) ){
					logerr( LOG_ERR, "cmd_split: metachar in '%s`",
						filter->cmd );
					return(-1);
				} else {
					++cmd;
				}
			}
			if( cmd && *cmd ){
				*cmd++ = 0;
			}
			/* DEBUG9("cmd_split: word '%s'", start ); */
		}
	}
	if( filter->args.count >= filter->args.max ){
		extend_malloc_list( &filter->args, sizeof( char * ), 100 );
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

	/*
	 * replace $X with the corresponding value
	 * note that '$$ -> $
	 */
	DEBUG9("Expand_command: fmt %c, expanding '%s'", fmt, s );
	if( s && bp ) while( (bp < ep ) && (c = (*bp++ = *s++)) ){
		if( c == '$' && (c = *s) ){
			if( c != '$' ){
				notag = 0;
				space = 0;
				noquote = 0;
				while( strchr( " 0-'", c) ){
					switch( c ){
					case '0': case ' ': space = 1; break;
					case '-':           notag = 1; break;
					case '\'':          noquote = 1; break;
					}
					c = *++s;
				}
				/* DEBUG9("Expand_command: expanding '%c', space %d, notag %d",
					c, space, notag ); */
				--bp;
				bp = do_dollar( cfp, bp, ep, c, fmt, space, notag,
					data_file, noquote );
			}
			++s;
		}
	}
	*bp = 0;
	return( bp );
}

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

void Kill_filter( struct filter *filter, int signal )
{
	if( filter->pid ){
		kill( filter->pid, signal );
		kill( filter->pid, SIGCONT );
	}
}

int Close_filter( struct filter *filter, int timeout )
{
	static pid_t result = 0;
	plp_status_t status = 0;

	DEBUG5("Close_filter: input %d, pid %d", filter->input, filter->pid );
	if( filter->input > 0 && !isatty( filter->input ) ){
		close( filter->input );
		filter->input = -1;
	}
	if( filter->pid ){
		do{
			result = 0;
			status = 0;
			kill( filter->pid, SIGCONT );
			if( Set_timeout( timeout, 0 ) ){
				result = plp_waitpid( filter->pid, &status,
					timeout>0 ? 0 : WNOHANG );
			}
			Clear_timeout( timeout, 0 );
		}while( Alarm_timed_out == 0 && result == -1 && errno != ECHILD );

		DEBUG8( "Close_filter: result %d, status '%s'", result,
			Decode_status(&status) );
		if( result != -1 ){
			removepid( result );
			filter->pid = 0;
		} else if( Alarm_timed_out ){
			log( LOG_ERR, "Close_filter: timeout on filter close" );
			setstatus( NORMAL, "timeout on filter close" );
		}
		if( WIFSIGNALED( status ) ){
			setstatus( NORMAL, "filter died with signal '%s'",
				Decode_status( &status )  );
			status = WTERMSIG( status );
		} else if( status ) {
			status = WEXITSTATUS( status );
			setstatus( NORMAL, "filter died with status '%s'",
				Server_status( status ) );
		}
	}
	DEBUG5("Close_filter: status 0x%x", status );
	return( status );
}
