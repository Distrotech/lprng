/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: utilities.c
 * PURPOSE: basic set of utilities
 **************************************************************************/

static char *const _id = "$Id: utilities.c,v 3.1 1996/09/09 14:24:41 papowell Exp papowell $";

#include "lp.h"
#include "timeout.h"

/*
 * Time_str: return "cleaned up" ctime() string...
 *
 * Thu Aug 4 12:34:17 BST 1994 -> Aug  4 12:34:17
 * Thu Aug 4 12:34:17 BST 1994 -> 12:34:17
 */

char *Time_str(int shortform, time_t tm)
{
    time_t tvec;
    static char s[99];
	char *t;

	if( tm ){
		tvec = tm;
	} else {
		tvec = time( (void *) 0 );
	}
    (void) strcpy(s, ctime(&tvec) );
	t = s;
	s[24] = 0;
	if( shortform > 0 ){
		t = &s[11];
		s[19] = 0;
	} else if( shortform == 0 ){
		t = &s[4];
		s[19] = 0;
	}
	return(t);
}

/***************************************************************************
 * Print the usage message list or any list of strings
 *  Use for copyright printing as well
 ***************************************************************************/

void Printlist( char **m, FILE *f )
{
	char *s;
	if( (s = strrchr( Name, '/' )) ) ++s;
	if( s == 0 ) s = Name;
	if( s == 0 ) s = "?????";
	
	if( *m ){
		fprintf( f, *m, s );
		fprintf( f, "\n" );
		++m;
	}

    for( ; *m; ++m ){
        fprintf( f, "%s\n", *m );
    }
	fflush(f);
}


/***************************************************************************
 * Utility functions: write a string to a fd (bombproof)
 *   write a char array to a fd (fairly bombproof)
 * Note that there is a race condition here that is unavoidable;
 * The problem is that there is no portable way to disable signals;
 *  post an alarm;  <enable signals and do a read simultaneously>
 * There is a solution that involves forking a subprocess, but this
 *  is so painful as to be not worth it.  Most of the timeouts
 *  in the LPR stuff are in the order of minutes, so this is not a problem.
 *
 * Note: we do the write first and then check for timeout.
 ***************************************************************************/

int Write_fd_str( int fd, char *msg )
{
	return( msg? Write_fd_len( fd, msg, strlen(msg) ) : 0 );
}

int Write_fd_len( int fd, char *msg, int len )
{
	int i;

	i = len;
	while( len > 0 && (i = write( fd, msg, len ) ) >= 0 ){
		if( Timeout_pending && Alarm_timed_out){
			i = -1;
			break;
		}
		len -= i, msg += i;
	}
	return( i );
}

/***************************************************************************
 * void trunc_str(char *s)
 *  - remove trailing spaces from the end of a string
 *  - a \ before last trailing space will escape it
 ***************************************************************************/

void trunc_str( char *s )
{
	int l, c = 0;
	if( s ){
		l = strlen(s);
		while( l-- > 0 && isspace( s[l] ) ){
			c = s[l];
			s[l] = 0;
		}
		if( l >= 0 && s[l] == '\\' && c ) s[l] = c;
	}
}
