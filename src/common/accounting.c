/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2000, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: accounting.c,v 5.15 2000/12/25 01:51:04 papowell Exp papowell $";


#include "lp.h"
#include "accounting.h"
#include "lpd.h"
#include "getqueue.h"
#include "errorcodes.h"
#include "child.h"
#include "linksupport.h"
#include "fileopen.h"
/**** ENDINCLUDE ****/

int Do_accounting( int end, char *command, struct job *job, int timeout )
{
	int n, err, len, tempfd;
	char msg[SMALLBUFFER];
	char *s;
	struct line_list args;
	struct stat statb;

	Init_line_list(&args);
	msg[0] = 0;
	err = JSUCC;

	while( isspace(cval(command)) ) ++command;
	s = command;
	if( cval(s) == '|' ) ++s;
	Add_line_list(&args, s,0,0,0);
	Fix_dollars(&args, job, 1, Filter_options_DYN );
	s = args.list[0];
	DEBUG1( "Do_accounting: expanded command is '%s'", s );
	s = safeextend2(s,"\n",__FILE__,__LINE__);
	args.list[0] = s;

	tempfd = -1;

	DEBUG2("Do_accounting: command '%s'", command );
	if( (cval(command) == '|') || (cval(command) == '/') ){
		if( end == 0 && Accounting_check_DYN ){
			tempfd = Make_temp_fd( 0 );
		}
		err = Filter_file( -1, tempfd, "ACCOUNTING_FILTER",
			command, Filter_options_DYN, job, 0, 1 );
		if( tempfd > 0 && lseek(tempfd,0,SEEK_SET) == -1 ){
			Errorcode = JABORT;
			LOGERR_DIE(LOG_INFO)"Do_accounting: lseek tempfile failed");
		}
	} else if( Accounting_file_DYN && isalnum(cval(Accounting_file_DYN))
		&& safestrchr( Accounting_file_DYN, '%' ) ){
		char *host, *port;
		/* now try to open a connection to a server */
		host = safestrdup(Accounting_file_DYN,__FILE__,__LINE__);
		port = safestrchr( host, '%' );
		*port++ = 0;
		
		DEBUG2("Setup_accounting: connecting to '%s'%%'%s'",host,port);
		if( (tempfd = Link_open(host,port,Connect_timeout_DYN,0 )) < 0 ){
			err = errno;
			Errorcode= JFAIL;
			LOGERR_DIE(LOG_INFO)
				_("connection to accounting server '%s' failed '%s'"),
				Accounting_file_DYN, Errormsg(err) );
		}
		DEBUG2("Setup_accounting: socket %d", tempfd );
		if( host ) free(host); host = 0;
		if( Write_fd_str( tempfd, args.list[0] ) < 0 ){
			err = JABORT;
			LOGERR(LOG_INFO)"Do_accounting: write to %s failed", command);
		}
		shutdown(tempfd,1);
	} else {
		tempfd = Checkwrite( Accounting_file_DYN, &statb, 0, Create_files_DYN, 0 );
		DEBUG2("Setup_accounting: fd %d", tempfd );
		if( tempfd > 0 && Write_fd_str( tempfd, args.list[0] ) < 0 ){
			err = JABORT;
			LOGERR(LOG_INFO)"Do_accounting: write to %s failed", command);
		}
		if( tempfd > 0 ) close(tempfd); tempfd = -1;
	}
	if( tempfd > 0 && err == 0 && end == 0 && Accounting_check_DYN ){
		msg[0] = 0;
		len = 0;
		while( len < sizeof(msg)-1
			&& (n = read(tempfd,msg+len,sizeof(msg)-1-len)) > 0 ){
			DEBUG1("Do_accounting: read %d, '%s'", n, msg );
		}
		Free_line_list(&args);
		Split(&args,msg,Whitespace,0,0,0,0,0);
		err = JSUCC;
		if( args.count == 0 ){
			err = JSUCC;
		} else {
			s = args.list[0];
			if( !safestrcasecmp( s, "accept" ) ){
				err = JSUCC;
			} else if( !safestrcasecmp( s, "hold" ) ){
				err = JHOLD;
			} else if( !safestrcasecmp( s, "remove" ) ){
				err = JREMOVE;
			} else if( !safestrcasecmp( s, "fail" ) ){
				err = JFAIL;
			} else {
				SNPRINTF( msg, sizeof(msg))
					"accounting check failed - status message '%s'", s );
				err = JABORT;
			}
		}
	}
	if( tempfd > 0 ) close(tempfd); tempfd = -1;
	Free_line_list(&args);
	DEBUG2("Do_accounting: status %s", Server_status(err) );
	return( err );
}
