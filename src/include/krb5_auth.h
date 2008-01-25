/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 ***************************************************************************/



#ifndef _KRB5_AUTH_H
#define _KRB5_AUTH_H 1

#include "user_auth.h"

/* PROTOTYPES */
int Send_krb4_auth( struct job *job, int *sock,
	int connect_timeout, char *errmsg, int errlen,
	struct security *security, struct line_list *info );
int Receive_k4auth( int *sock, char *input );
int Krb5_receive_nocrypt( int *sock,
	int transfer_timeout,
	char *user, char *jobsize, int from_server, char *authtype,
	struct line_list *info,
	char *errmsg, int errlen,
	struct line_list *header_info,
	struct security *security, char *tempfile, SECURE_WORKER_PROC do_secure_work);
int Krb5_receive( int *sock,
	int transfer_timeout,
	char *user, char *jobsize, int from_server, char *authtype,
	struct line_list *info,
	char *errmsg, int errlen,
	struct line_list *header_info,
	struct security *security, char *tempfile, SECURE_WORKER_PROC do_secure_work);
int Krb5_send_nocrypt( int *sock,
	int transfer_timeout,
	char *tempfile,
	char *error, int errlen,
	struct security *security, struct line_list *info );
int Krb5_send( int *sock,
	int transfer_timeout,
	char *tempfile,
	char *error, int errlen,
	struct security *security, struct line_list *info );

#endif
