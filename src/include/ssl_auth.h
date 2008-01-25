/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 ***************************************************************************/



#ifndef _SSL_AUTH_H_
#define _SSL_AUTH_H_ 1

#include <openssl/ssl.h>
#include <openssl/err.h>


/* PROTOTYPES */
int Ssl_send( int *sock,
	int transfer_timeout,
	char *tempfile,
	char *errmsg, int errlen,
	struct security *security, struct line_list *info );
int Ssl_receive( int *sock, int transfer_timeout,
	char *user, char *jobsize, int from_server, char *authtype,
	struct line_list *info,
	char *errmsg, int errlen,
	struct line_list *header_info,
	struct security *security, char *tempfile,
	SECURE_WORKER_PROC do_secure_work);

#endif
