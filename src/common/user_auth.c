/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 ***************************************************************************/

/*
 * This code is, sadly,  a whimpy excuse for the dynamically loadable
 * modules.  The idea is that you can put your user code in here and it
 * will get included in various files.
 * 
 * Supported Sections:
 *   User Authentication
 * 
 *   DEFINES      FILE WHERE INCLUDED PURPOSE
 *   USER_RECEIVE  lpd_secure.c       define the user authentication
 *                                    This is an entry in a table
 *   USER_SEND     sendauth.c         define the user authentication
 *                                    This is an entry in a table
 *   RECEIVE       lpd_secure.c       define the user authentication
 *                            This is the code referenced in USER_RECEIVE
 *   SENDING       sendauth.c       define the user authentication
 *                            This is the code referenced in USER_SEND
 * 
 */

#include "lp.h"
#include "user_auth.h"
#include "krb5_auth.h"
#include "errorcodes.h"
#include "fileopen.h"
#include "linksupport.h"
#include "child.h"
#include "getqueue.h"
#include "lpd_secure.h"
#include "lpd_dispatch.h"
#include "permission.h"
#include "globmatch.h"

#ifdef SSL_ENABLE
/* The Kerberos 5 support is MIT-specific. */
#define OPENSSL_NO_KRB5
# include "ssl_auth.h"
#endif


/**************************************************************
 * Secure Protocol
 *
 * the following is sent on *sock:  
 * \REQ_SECUREprintername C/F user authtype \n        - receive a command
 *             0           1   2   3
 * \REQ_SECUREprintername C/F user authtype jobsize\n - receive a job
 *             0           1   2   3        4
 *          Printer_DYN    |   |   |        + jobsize
 *                         |   |   authtype 
 *                         |  user
 *                        from_server=1 if F, 0 if C
 *                         
 * The authtype is used to look up the security information.  This
 * controls the dispatch and the lookup of information from the
 * configuration and printcap entry for the specified printer
 *
 * The info line_list has the information, stripped of the leading
 * xxxx_ of the authtype name.
 * For example:
 *
 * forward_id=test      <- forward_id from configuration/printcap
 * id=test              <- id from configuration/printcap
 * 
 * If there are no problems with this information, a single 0 byte
 * should be written back at this point, or a nonzero byte with an
 * error message.  The 0 will cause the corresponding transfer
 * to be started.
 * 
 * The handshake and with the remote end should be done now.
 *
 * The client will send a string with the following format:
 * destination=test\n     <- destination ID (URL encoded)
 *                       (destination ID from above)
 * server=test\n          <- if originating from server, the server key (URL encoded)
 *                       (originator ID from above)
 * client=papowell\n      <- client id
 *                       (client ID from above)
 * input=%04t1\n          <- input or command
 * This information will be extracted by the server.
 * The 'Do_secure_work' routine can now be called,  and it will do the work.
 * 
 * ERROR MESSAGES:
 *  If you generate an error,  then you should log it.  If you want
 *  return status to be returned to the remote end,  then you have
 *  to take suitable precautions.
 * 1. If the error is detected BEFORE you send the 0 ACK,  then you
 *    can send an error back directly.
 * 2. If the error is discovered as the result of a problem with
 *    the encryption method,  it is strongly recommended that you
 *    simply send a string error message back.  This should be
 *    detected by the remote end,  which will then decide that this
 *    is an error message and not status.
 *
 **************************************************************/

static const struct security *SecuritySupported[] = {
	/* name, server_name, config_name, flags,
        client  connect, send, send_done
		server  accept, receive, receive_done
	*/
#if defined(KERBEROS)
	&kerberos5_auth,
	&k5conn_auth,
#endif
	&test_auth,
	&md5_auth,
#ifdef SSL_ENABLE
	&ssl_auth,
#endif
	NULL
};

char *ShowSecuritySupported( char *str, int maxlen )
{
	int i, len;
	const char *name;
	str[0] = 0;
	for( len = i = 0; SecuritySupported[i] != NULL; ++i ){
		name = SecuritySupported[i]->name;
		plp_snprintf( str+len,maxlen-len, "%s%s",len?",":"",name );
		len += strlen(str+len);
	}
	return( str );
}

const struct security *FindSecurity( const char *name ) {
	const struct security *s, **p;

	for( p = SecuritySupported ; (s = *p) != NULL ; p++ ) {
		if( !Globmatch(s->name, name ) )
			return s;
	}
	return NULL;
}
