/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: AUTHENTICATE/readfile.c
 * PURPOSE: read file information
 **************************************************************************/

static char *const _id =
"readfilecount.c,v 3.2 1997/09/18 19:45:41 papowell Exp";

/*
 * readfilecount [-D]
 *   - reads STDIN, which should have the form:
 *     nnnn\n
 *     [contents of destfile]
 *   - writes the contents to stdout 
 *
 * Patrick Powell, papowell@astart.com
 * Thu Dec  5 08:34:40 PST 1996
 */

#include "portable.h"

int main()
{
	int count, len, outlen, c;
	char buffer[4096];
	int error;

	for( count = 0; count < sizeof(buffer); ++count ){
		len = read( 0, &buffer[count], 1 );
		if( len <= 0 ){
			perror( "readfilecount: read of count truncated\n" );
			/* write buffer contents */
			error = 1;
			goto error;
		}
		c = buffer[count];
		if( c == '\n' ) break;
		if( !isdigit(c) ){
			error = 2;
			fprintf( stderr, "readfilecount: non-digit value received\n" );
			++count;
			goto error;
		}
	}
	if( count >= sizeof(buffer) ){
		fprintf( stderr, "input format error\n" );
		exit(3);
	}
	buffer[count] = 0;
	count = atoi( buffer );

	while( count > 0 ){
		len = count;
		if( len > sizeof(buffer) ) len = sizeof(buffer);
		len = read(0, buffer, len );
		if( len <= 0 ){
			perror( "read truncated\n" );
			exit(4);
		}
		count -= len;
		outlen = write(1, buffer, len );
		if( outlen != len ){
			perror( "write truncated\n" );
			exit(5);
		}
	}
	exit(0);

error:
	/* write buffer contents */
	write(2,buffer,count);
	while( (len = read( 0, buffer, sizeof(buffer) )) > 0 ){
		count = write(2,buffer,len);
		if( count < 0 ) break;
	}
	exit(error);
}
