/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: AUTHENITCATE/removeoneline.c
 * PURPOSE: remove a line from the input file
 **************************************************************************/

static char *const _id =
"removeoneline.c,v 3.2 1997/09/18 19:45:41 papowell Exp";
/*
 * removeoneline [file]
 *   - reads file which should have the form:
 *     nnnn\n
 *     [contents of destfile]
 *   - removes the first line, and then writes the remaining file back
 *
 * Patrick Powell, papowell@astart.com
 * Thu Dec  5 08:34:40 PST 1996
 */

#include "portable.h"
const char * Errormsg ( int err );

char *name;

int main(int argc, char *argv[])
{
	char line[256];
	int count, len, status, size;
	char *file = 0;
	int fd;
	struct stat statb;

	if( argv[0] && (name = strrchr( argv[0], '/' ) ) ){
		++name;
	} else {
		name = argv[0];
	}
	if( name == 0 ) name = "???";
	if( argc != 2 || argv[1][0] == '-' ){
		fprintf( stderr, "usage: %s filename\n"
		"  - removes and prints last line of file\n",
			name );
		exit(1);
	}

	file = argv[1];

	fd = open( file, O_RDWR|O_NONBLOCK );
	if( fd < 0 ){
		fprintf( stderr, "%s: cannot open '%s' - '%s'\n",
			name, file, Errormsg(errno) );
		exit(2);
	}
	if( fstat( fd, &statb ) == -1 ){
		fprintf( stderr, "%s: cannot stat '%s' - '%s'\n",
			name, file, Errormsg(errno) );
		exit(2);
	}
	len = statb.st_size;
	if( len > sizeof(line)-1 ){
		len = sizeof(line)-1;
	}
	if( len <= 2 ){
		fprintf( stderr, "%s: file too small '%s'\n",
			name, file );
		exit(2);
	}
	/* now we seek to end */
	status = lseek( fd, -len, SEEK_END );
	if( status == -1 ){
		fprintf( stderr, "%s: cannot truncate '%s' - '%s'\n",
			name, file, Errormsg(errno) );
		exit(2);
	}

	/* now we read the line into buffer */
	count = read( fd, line, len );
	if( count != len ){
		fprintf( stderr, "%s: cannot read '%s' - '%s'\n",
			name, file, Errormsg(errno) );
		exit(2);
	}
	line[count] = 0;

	/* now we find the last line */
	--count;
	if( line[count] != '\n' ){
		fprintf( stderr, "%s: file not terminated in LF '%s'\n",
			name, file );
		exit(2);
	}
	if( count > 0 ){
		--count;
		while( line[count] != '\n' && count > 0 ) --count;
	}
	len = strlen( &line[count] );
	size = statb.st_size - len;
	if( ftruncate( fd, size ) == -1 ){
		fprintf( stderr, "%s: cannot truncate '%s' - '%s'\n",
			name, file, Errormsg(errno) );
		exit(2);
	}
	write( 1, &line[count+1], len-1 );
	exit(0);
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
