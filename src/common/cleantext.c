/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: cleantext.c
 * PURPOSE: clean up text in control file
 * char *Clean_name( char *s )
 *  scan input line for non-alphanumeric, _ , - , @ characters
 *  return pointer to bad character found
 *
 * char *Clean_meta( char *s )
 *  scan input line for meta character or non-printable character
 *  return pointer to bad character found
 *
 **************************************************************************/


static char *const _id =
"$Id: cleantext.c,v 3.2 1996/08/25 22:20:05 papowell Exp papowell $";

#include "lp.h"

#define UPPER "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define LOWER "abcdefghijklmnopqrstuvwxyz"
#define DIGIT "01234567890"
#define SAFE "-_."
#define SPACE " \t"
#define LESS_SAFE SAFE SPACE "@/:()=,+-%"

char *Clean_name( char *s )
{
	int c;
	if( s ){
		while( (c = *s) ){
			if( !isalnum(c) && !strchr( SAFE, c ) ) return( s );
			++s;
		}
	}
	return( 0 );
}

/*
 * Find a possible bad character in a line
 */

char *Find_meta( char *s )
{
	int c = 0;
	if( s ){
		for( ; (c = *s); ++s ){
			if( !isprint( c )
				|| ( !isalnum( c ) && strchr( LESS_SAFE, c ) == 0) ){
				return( s );
			}
		}
		s = 0;
	}
	return( s );
}

void Clean_meta( char *s )
{
	while( (s = Find_meta( s )) ){
		*s = '_';
	}
}


/***************************************************************************
 * char * Check_format( int type, char *name, struct control_file *cfp )
 * Check to see that the file name has the correct format
 * name[0] == 'c' or 'd' (type)
 * name[1] = 'f'
 * name[2] = A-Za-z
 * name[3-5] = NNN
 * name[6-end] = only alphanumeric and ., _, or - chars
 * RETURNS: 0 if OK, error message (string) if not
 ***************************************************************************/
int Check_format( int type, char *name, struct control_file *cfp )
{
	int n, c;
	char *s, *t;
	int err = 1;

	DEBUG8("Check_format: type %d, name '%s'", type, name ); 
	switch( type ){
		case DATA_FILE: if( name[0] != 'd' ) goto error; break;
		case CONTROL_FILE: if( name[0] != 'c' ) goto error; break;
		default: goto error;
	}
	/* check for second letter */
	if( name[1] != 'f' ){
		goto error; 
	}
	if( !isalpha( name[2] ) ) goto error;
	t = s = &name[3];
	n = strtol(s,&t,10);
	c = t - s;
	/* check on length of number */
	if( c < 3 || c > 6 ) goto error;
	if( cfp->number_len == 0 ){
		cfp->number = n;
		cfp->number_len = c;
		cfp->filehostname = t;
		if( Clean_name(t) ) goto error;
	} else if( cfp->number != n || strcmp( t, cfp->filehostname ) ){
		goto error;
	}
	err = 0;

error:
	DEBUG4("Check_format: '%s' job %d, filehostname '%s', result %d",
		name, cfp->number, cfp->filehostname, err ); 
	return(err);
}
