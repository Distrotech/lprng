/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
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
"$Id: cleantext.c,v 3.2 1997/01/19 14:34:56 papowell Exp $";

#include "lp.h"
#include "cleantext.h"
/**** ENDINCLUDE ****/

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
		for( ; (c = *(unsigned char *)s); ++s ){
			if( !(isalnum(c) || strchr( SAFE, c )) ) return( s );
		}
	}
	return( 0 );
}

/*
 * Find a possible bad character in a line
 */

int Is_meta( int c )
{
	return( !(isalnum( c ) || strchr( LESS_SAFE, c )) );
}

char *Find_meta( char *s )
{
	int c = 0;
	if( s ){
		for( ; (c = *(unsigned char *)s); ++s ){
			if( Is_meta( c ) ) return( s );
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

	DEBUG4("Check_format: type %d, name '%s'", type, name ); 
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
	if( type == CONTROL_FILE ){
		cfp->priority = name[2];
	}
	t = s = &name[3];
	n = strtol(s,&t,10);
	c = t - s;
	/* check on length of number */
	if( cfp->recvd_number_len == 0 ){
		if( c < 3 || c > 6 ) goto error;
		cfp->number = n;
		cfp->recvd_number = n;
		cfp->recvd_number_len = c;
		strncpy(cfp->filehostname, t, sizeof( cfp->filehostname ) );
		if( Clean_name(t) ) goto error;
		/* fix up job number so it is in correct range */
		Fix_job_number( cfp );
	} else {
			if( cfp->recvd_number != n
			|| cfp->recvd_number_len != c ){
				DEBUG4("Check_format: number disagreement" ); 
			goto error;
		}
		if( strcmp( t, cfp->filehostname ) ){
			int tlen, clen, len;
			/* now we have to decide if we have a problem with
			 * short and long file names.  First, see if the
			 * shortest part is the same
			 */
			tlen = strlen(t);
			len = clen = strlen(cfp->filehostname);
			if( tlen < len ) len = tlen;
			if( strncmp( t, cfp->filehostname, len ) ){
				DEBUG4("Check_format: name disagreement" ); 
				goto error;
			}
			/* now we know the prefixes are the same - see if they
			 * end at the dot point
			 */
			if( ((c = cfp->filehostname[len]) && c != '.' )
			  || ((c = t[len]) && c != '.' ) ){
				DEBUG4("Check_format: name truncation" ); 
				goto error;
			}
		}
	}
	err = 0;

error:
	DEBUG3("Check_format: '%s', number %d, recvd_number %d, filehostname '%s', result %d",
		name, cfp->number, cfp->recvd_number, cfp->filehostname, err ); 
	return(err);
}
