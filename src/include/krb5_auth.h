/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: krb5_auth.h
 * PURPOSE: kerberos authentication functions
 * "krb5_auth.h,v 1.2 1997/01/24 20:27:06 papowell Exp";
 **************************************************************************/
/*****************************************************************
 * SetUID information
 * Original RUID and EUID, as well as the Daemon UID
 *****************************************************************/

#ifndef _KRB5_AUTH_H
#define _KRB5_AUTH_H 1

int server_krb5_auth( char *keytabfile, char *service, int sock,
	char *auth, int authlen, char *err, int errlen, char *file );
int local_principal_krb5(char *service,char *err,int errlen );

int client_krb5_auth( char *keytabfile, char *service, char *host,
	char *server_principal,
    int options, char *life, char *renew_time,
    int sock, char *err, int errlen, char *file );

int remote_principal_krb5( char *service, char *host, char *err, int errlen );
int server_cred_krb5( char *service, char *keytab_name,
    int options,  char *life, char *renew_time,
    char *err, int errlen );           


#endif
