/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: setupauth.c
 * PURPOSE: setup and test authentication facilities.
 **************************************************************************/

static char *const _id =
"setupauth.c,v 3.3 1997/12/16 15:06:17 papowell Exp";
/**********************************************************************
 * Setup secure test
 *  setup_secure 'command 1' 'command 2'
 *  sets up commands with fd 0 = UPD socket (bidirectional)
 *                        fd 1 = stdout
 *                        fd 2 = stderr
 *                        fd 3 = original stdin
 **********************************************************************/

#include "portable.h"

extern const char *Errormsg( int err );
int tcp_open( int port, int *actual );
int tcp_connect( int port );
int tcp_accept( int fd );
char **setup_envp( char *user, char *pass_env );
char **setup_cmd( char *cmd );

char *pass = "PGPPASS,PGPPATH";
extern char **environ;

int main(int argc, char *argv[])
{
	char *s, *cmd1, *cmd2, *uids1, *uids2;
	char **args;
	int fd[2], i;
	int uid, uid1, uid2;
	int pid;
	struct passwd *pw;
	char **old_env;
	
	cmd1 = 0;
	if( argc > 1 ){
		cmd1 = argv[1];
	}
	if( cmd1 == 0 || cmd1[0] == '-' ){
		cmd1 = argv[0];
		if( (s = strrchr( cmd1, '/' ) ) ) cmd1 = s+1;
		fprintf( stderr, "usage: %s clientid 'clientcommand' serverid 'servercommand'\n", cmd1 );
		fprintf( stderr, " invoke command 1 as uid1, command2 as uid2\n" );
		fprintf( stderr, " fd 0 = bidirectional pipe\n" );
		fprintf( stderr, " fd 1 = stdout\n" );
		fprintf( stderr, " fd 2 = stderr\n" );
		fprintf( stderr, " fd 3 = original stdout\n" );
		fprintf( stderr, "Called with argc = %d\n", argc);
		for( i = 0; i < argc; ++i ){
			fprintf( stderr, "  argv[%d] = '%s'\n",i, argv[i]);
		}
		exit( 1 );
	}
	uids1 = argv[1];
	cmd1 = argv[2];
	uids2 = argv[3];
	cmd2 = argv[4];

	uid = geteuid();

	if( uid != 0 ){
		fprintf( stderr, "WARNING: not root, SUID will be skipped\n");
	}

	if( (pw = getpwnam( uids1 ) ) ){
		uid1 = pw->pw_uid;
	} else {
		fprintf( stderr, "bad UID '%s'\n", uids1 );
		return(1);
	}

	if( (pw = getpwnam( uids2 ) ) ){
		uid2 = pw->pw_uid;
	} else {
		fprintf( stderr, "bad UID '%s'\n", uids2 );
		return(1);
	}
	/* dup fd 0 to get it safe */
	if( dup2(0,3) == -1 ){
		perror( "dup2 failed" );
	}

	if( socketpair( AF_UNIX, SOCK_STREAM, 0, fd ) == -1 ){
		perror( "socketpair failed" );
	}
	old_env = environ;
	environ = setup_envp( uids2, 0 );
	args = setup_cmd( cmd2 );
	if( (pid = fork()) == 0 ){
		fprintf( stderr, "server %d\n", (int)getpid() );
		if( uid == 0 ) setuid( uid2 );
		if( dup2( fd[0], 0 ) == - 1 ){
			perror( "process 1 dup2 failed" );
			exit(0);
		}
		close( fd[0] );
		close( fd[1] );
		execvp( args[0], args );
		fprintf( stderr, "did not exec '%s'- %s\n", args[0], Errormsg(errno) );
		exit(1);
	}
	/* client */
	sleep(1);
	fprintf( stderr, "client %d\n", (int)getpid() );
	environ = old_env;
	environ = setup_envp( uids1, pass );
	args = setup_cmd( cmd1 );
	if( uid == 0 ) setuid( uid1 );
	if( dup2( fd[1], 0 ) == - 1 ){
		perror( "process 2 dup2 failed" );
		exit(0);
	}
	close( fd[0] );
	close( fd[1] );
	close(3);
	execvp( args[0], args );
	fprintf( stderr, "did not exec '%s'- %s\n", args[0], Errormsg(errno) );
	return(1);
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

int test_argv_cnt;
char *test_argv[20];
char buffer[1024];
int count;

void add_env( char *line )
{
	char *s = 0;
	if( test_argv_cnt == 0 ){
		count = 0;
	}
	if( line ){
		int len = strlen(line)+1;
		int left = sizeof( buffer ) - count;

		if( left <= len ){
			fprintf( stderr, "need a larger string buffer\n" );
			exit(1);
		}
		s = &buffer[count];
		strcpy( &buffer[count], line );
		count += len;
	}
	test_argv[test_argv_cnt++] = s;
	if( test_argv_cnt >= sizeof(test_argv)/sizeof(test_argv[0]) ){
		fprintf( stderr, "need a larger argv array\n" );
		exit(1);
	}
}

char **setup_envp( char *user, char *pass_env )
{
	struct passwd *pw;
	char line[256];
	char envs[256];
	char *s, *end;
	int len;

	test_argv_cnt = 0;

	if( (pw = getpwnam( user )) == 0 ){
		fprintf( stderr, "user '%s' does not exist\n", user );
		exit(1);
	}
	sprintf(line, "USER=%s", pw->pw_name);
	add_env( line );

	sprintf(line, "LOGNAME=%s", pw->pw_name);
	add_env( line );

	sprintf( line, "HOME=%s", pw->pw_dir);
	add_env( line );

	sprintf( line, "LOGDIR=%s", pw->pw_dir);
	add_env( line );

	add_env( "SHELL=/bin/sh" );

	sprintf( line, "IFS= \t");
	add_env( line );

	s = getenv( "TZ" );
	if( s ){
		sprintf( line, "TZ=%s", s );
		add_env( line );
	}

	if( pass_env ){
		strcpy( envs, pass_env );
		for( s = envs; s && *s; s = end ){
			while( isspace( *s ) ) ++s;
			end = strpbrk( s, " \t,;" );
			if( end ) *end++ = 0;
			/*fprintf(stderr,"looking for '%s'\n",s ); */
			if( strlen(s) ){
				strcpy( line, s );
				if( (s = getenv( line )) && strlen(s) != 0 ){
					/*fprintf(stderr,"found '%s'\n",s );*/
					len = strlen( line );
					sprintf( line+len, "=%s", s );
					/*fprintf(stderr,"adding '%s'\n",line );*/
					add_env( line );
				}
			}
		}
	}

	/* for null entry */
	add_env( 0 );
	/*{ int i;
		for( i = 0;(s = test_argv[i]); ++i ){
			fprintf(stderr,"[%d] '%s'\n",i, s );
		}
	}*/
	return( test_argv );
}

char *cmd[30];
int cmd_count;

char **setup_cmd( char *cmd_str )
{
	char *s, *end;
	int c;
	
	cmd_count = 0;
	for( s = cmd_str; s && *s; s = end ){
		while( isspace( *s ) ) *s++ = 0;
		c = *s;
		if( c == '\'' || c == '"' ){
			++s;
		}
		if( *s ){
			cmd[cmd_count++] = s;
			if( cmd_count >= sizeof(cmd)/sizeof(cmd[0]) ){
				fprintf( stderr, "need larger command vector\n" );
			}
		}
		if( c == '\'' ){
			end = strchr( s, '\'' );
		} else if( c == '"' ){
			end = strchr( s, '"' );
		} else {
			end = strpbrk( s, " \t" );
		}
		if( end ) *end++ = 0;
	}
	cmd[cmd_count] = 0;
	return( cmd );
}
