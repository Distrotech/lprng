/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
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
"cleantext.c,v 3.6 1998/03/29 18:32:47 papowell Exp";

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
	return( !(isalnum( c ) || strchr( LESS_SAFE, c )|| (Safe_chars && strchr(Safe_chars,c)) ) );
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
		case DATA_FILE: if( name[0] != 'd' ){
				plp_snprintf(cfp->error, sizeof(cfp->error),
					"data file does not start with 'd' - '%s'",
					name );
				cfp->name_format_error = 1;
				DEBUG1("Check_format: %s", cfp->error ); 
				if(!Fix_bad_job) goto error;
			}
			break;
		case CONTROL_FILE: if( name[0] != 'c' ){
				plp_snprintf(cfp->error, sizeof(cfp->error),
					"control file does not start with 'd' - '%s'",
					name );
				cfp->name_format_error = 1;
				DEBUG1("Check_format: %s", cfp->error ); 
				if(!Fix_bad_job) goto error;
			}
			break;
		default:
			plp_snprintf(cfp->error, sizeof(cfp->error),
				"bad file type '%c' - '%s' ", type,
				name );
			cfp->name_format_error = 1;
			DEBUG1("Check_format: %s", cfp->error ); 
			goto error;
	}
	/* check for second letter */
	if( name[1] != 'f' ){
		plp_snprintf(cfp->error, sizeof(cfp->error),
			"second letter must be f not '%c' - '%s' ", name[1],
				name );
		cfp->name_format_error = 1;
		DEBUG1("Check_format: %s", cfp->error ); 
		if(!Fix_bad_job) goto error;
	}
	if( !isalpha( name[2] ) ){
		plp_snprintf(cfp->error, sizeof(cfp->error),
			"third letter must be letter not '%c' - '%s' ", name[2],
				name );
		cfp->name_format_error = 1;
		DEBUG1("Check_format: %s", cfp->error ); 
		goto error;
	}
	if( type == CONTROL_FILE ){
		cfp->priority = name[2];
	}
	t = s = &name[3];
	n = strtol(s,&t,10);
	c = t - s;
	/* check on length of number */
	if( cfp->recvd_number_len == 0 ){
		if( c < 3 || c > 6 ){
			plp_snprintf(cfp->error, sizeof(cfp->error),
				"id number length out of bounds '%s' ",
					name );
			goto error;
		}
		cfp->number = n;
		cfp->recvd_number = n;
		cfp->recvd_number_len = c;
		strncpy(cfp->filehostname, t, sizeof( cfp->filehostname ) );
		/* fix up job number so it is in correct range */
		if( Clean_name(cfp->filehostname) ){
			plp_snprintf(cfp->error, sizeof(cfp->error),
				"bad hostname '%s' - '%s' ", cfp->filehostname,
					name );
			cfp->name_format_error = 1;
			DEBUG1("Check_format: %s", cfp->error ); 
			if(!Fix_bad_job) goto error;
			goto error;
		}
		Fix_job_number( cfp );
	}
	if( cfp->recvd_number != n
		|| cfp->recvd_number_len != c ){
		plp_snprintf(cfp->error, sizeof(cfp->error),
			"received file id number not '%d' - '%s' ", n,
				name );
		cfp->name_format_error = 1;
		DEBUG1("Check_format: %s", cfp->error ); 
		if(!Fix_bad_job) goto error;
	} else if( strcmp( t, cfp->filehostname ) ){
		int tlen, clen, len;
		/* now we have to decide if we have a problem with
		 * short and long file names.  First, see if the
		 * shortest part is the same
		 */
		tlen = strlen(t);
		len = clen = strlen(cfp->filehostname);
		if( tlen < len ) len = tlen;
		if( strncmp( t, cfp->filehostname, len ) ){
			plp_snprintf(cfp->error, sizeof(cfp->error),
				"host name '%s' not '%s' - '%s'", t, cfp->filehostname,
					name );
			cfp->name_format_error = 1;
			DEBUG1("Check_format: %s", cfp->error ); 
			if(!Fix_bad_job) goto error;
		} else if( ((c = cfp->filehostname[len]) && c != '.' )
		  || ((c = t[len]) && c != '.' ) ){
			/* now we know the prefixes are the same - see if they
			 * end at the dot point
			 */
			plp_snprintf(cfp->error, sizeof(cfp->error),
				"host name '%s' not '%s' - '%s'", t, cfp->filehostname,
					name );
			cfp->name_format_error = 1;
			DEBUG1("Check_format: %s", cfp->error ); 
			if(!Fix_bad_job) goto error;
		}
	}
	/* clear out error message */
	cfp->error[0] = 0;
	err = 0;

error:
	DEBUG3("Check_format: '%s', number %d, recvd_number %d, filehostname '%s', result %d",
		name, cfp->number, cfp->recvd_number, cfp->filehostname, err ); 
	return(err);
}
