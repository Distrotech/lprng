/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: utilities.c
 * PURPOSE: basic set of utilities
 **************************************************************************/

static char *const _id = "utilities.c,v 3.5 1997/09/18 19:46:07 papowell Exp";

#include "lp.h"
#include "utilities.h"
#include "timeout.h"
#include "errorcodes.h"
/**** ENDINCLUDE ****/

/*
 * Time_str: return "cleaned up" ctime() string...
 *
 * in YY/MO/DY/hr:mn:sc
 * Thu Aug 4 12:34:17 BST 1994 -> 12:34:17
 */

char *Time_str(int shortform, time_t t)
{
    static char buffer[99];
	struct tm *tmptr;
	struct timeval tv;

	tv.tv_usec = 0;
	if( t == 0 ){
		if( gettimeofday( &tv, 0 ) == -1 ){
			Errorcode = JABORT;
			logerr_die( LOG_ERR,"Time_str: gettimeofday failed");
		}
		t = tv.tv_sec;
	}
	tmptr = localtime( &t );
	if( shortform && Full_time == 0 ){
		plp_snprintf( buffer, sizeof(buffer),
			"%02d:%02d:%02d.%03d",
			tmptr->tm_hour, tmptr->tm_min, tmptr->tm_sec,
			(int)(tv.tv_usec/1000) );
	} else {
		plp_snprintf( buffer, sizeof(buffer),
			"%d-%02d-%02d-%02d:%02d:%02d.%03d",
			tmptr->tm_year+1900, tmptr->tm_mon+1, tmptr->tm_mday,
			tmptr->tm_hour, tmptr->tm_min, tmptr->tm_sec,
			(int)(tv.tv_usec/1000) );
	}
	/* now format the time */
	if( Ms_time_resolution == 0 ){
		char *s;
		if( ( s = strrchr( buffer, '.' )) ){
			*s = 0;
		}
	}
	return( buffer );
}

/***************************************************************************
 * Print the usage message list or any list of strings
 *  Use for copyright printing as well
 ***************************************************************************/

void Printlist( char **m, FILE *f )
{
	if( *m ){
		fprintf( f, *m, Name );
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

int Write_fd_len_timeout( int timeout, int fd, char *msg, int len )
{
	int i;
	if( Set_timeout() ){
		Set_timeout_alarm( timeout, 0 );
		i = Write_fd_len( fd, msg, len );
	} else {
		i = -1;
	}
	Clear_timeout();
	return( i );
}

int Write_fd_str( int fd, char *msg )
{
	if( msg && *msg ){
		return( Write_fd_len( fd, msg, strlen(msg) ));
	}
	return( 0 );
}

int Write_fd_str_timeout( int timeout, int fd, char *msg )
{
	if( msg && *msg ){
		return( Write_fd_len_timeout( timeout, fd, msg, strlen(msg) ) );
	}
	return( 0 );
}

int Read_fd_len_timeout( int timeout, int fd, char *msg, int len )
{
	int i;
	if( Set_timeout() ){
		Set_timeout_alarm( timeout, 0 );
		i = read( fd, msg, len );
	} else {
		i = -1;
	}
	Clear_timeout();
	return( i );
}

/***************************************************************************
 * void trunc_str(char *s)
 *  - remove trailing spaces from the end of a string
 *  - a \ before last trailing space will escape it
 ***************************************************************************/

void trunc_str( char *s )
{
	int l, c = 0, backslash_count;
	char *t;
	if( s ){
		l = strlen(s);
		while( l-- > 0 && isspace( s[l] ) ){
			c = s[l];
			s[l] = 0;
		}
		if( s[l] == '\\' ){
			t = &s[l];
			for( backslash_count = 0; l >= 0 && s[l] == '\\'; --l ){
				++backslash_count;
			}
			if( backslash_count & 1 ){
				*t = c;
			}
		}
	}
}

/**************************************************************************
 * static int Fix_job_number();
 * - fixes the job number range and value
 **************************************************************************/

void Fix_job_number( struct control_file *cfp )
{
	if( Long_number && !Backwards_compatible ){
		cfp->number_len = 6;
		cfp->max_number = 1000000;
	} else {
		cfp->number_len = 3;
		cfp->max_number = 1000;
	}
	cfp->number = cfp->number % cfp->max_number;
}


/************************************************************************
 * Make_identifier - add an identifier field to the job
 *  the identifier has the format Aname@host%id
 *  Since the identifier is put in the cfp->identifier field,
 *  and you may want to use it as the job identifier,  we put the
 *  leading 'A' character on the name.
 * 
 ************************************************************************/
int Make_identifier( struct control_file *cfp )
{
	char *user = "nobody";
	char *host = "unknown";
	int len, status = 0;

	if( cfp->identifier[0] ){
		return( 0 );
	}
	Fix_job_number( cfp );
	if( cfp->LOGNAME ){
		if( cfp->LOGNAME[1] == 0 ){
			plp_snprintf( cfp->error, sizeof( cfp->error),
				"bad LOGNAME line in '%s'", cfp->transfername );
			status = 1;
		} else {
			user = cfp->LOGNAME+1;
		}
	}
	if( cfp->FROMHOST ){
		if( cfp->FROMHOST[1] == 0 ){
			plp_snprintf( cfp->error, sizeof( cfp->error),
				"bad FROMHOST line in '%s'", cfp->transfername );
			status = 1;
		} else {
			host = cfp->FROMHOST+1;
		}
	}
	plp_snprintf( cfp->identifier, sizeof(cfp->identifier),
		"A%s@%s", user, host );
	if( (host = strchr( cfp->identifier, '.' )) ) *host = 0;
	len = strlen( cfp->identifier );
	if( Long_number ){
		struct tm *tmptr;
		time_t t = cfp->statb.st_ctime;
		int cnt;

		if( t == 0 ){
			t = time( (time_t *) 0 );
		}
		tmptr = localtime( &t );
		cnt = cfp->number % 1000;
		plp_snprintf( cfp->identifier+len, sizeof(cfp->identifier)-len,
			"+%02d%02d%02d%03d",
			tmptr->tm_hour, tmptr->tm_min, tmptr->tm_sec, cnt );
	} else {
		plp_snprintf( cfp->identifier+len, sizeof(cfp->identifier)-len,
			"+%0*d", cfp->number_len, cfp->number );
	}
	return( status );
}

/***************************************************************************
 * int *Crackline( char *line, struct token *token )
 *  cracks the input line into tokens,  separating them at whitespace
 *  Note: for most practical purposes,  you have either
 *  1 token:  keyword
 *  2 tokens: keyword value-to-<endofline>
 *  many tokens: keyword value1 value2 value3
 *    - single word tokens separated by whitespace
 ***************************************************************************/

int Crackline( char *line, struct token *token, int max )
{
	int i;
	char *end;
	for(i=0; *line && i < max; ){
		/* strip off whitespace */
		while( *line && isspace( *line ) ) ++line;
		if( *line ){
			token[i].start = line;
			for( end = line; *end && !isspace( *end ); ++end );
			token[i].length = end-line;
			line = end;
			++i;
		}
	}
	return( i );
}

