/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *	 papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: krb_auth.c
 * PURPOSE: do kerberos authentication
 **************************************************************************/

static char *const _id =
"krb5_auth.c,v 1.9 1998/03/24 02:43:22 papowell Exp";

#include "lp.h"
#include "krb5_auth.h"
#include "fileopen.h"
#include "killchild.h"

#if defined(HAVE_KRB5_H)
#include <krb5.h>
#include <com_err.h>

krb5_error_code krb5_read_message 
	KRB5_PROTOTYPE((krb5_context,
		   krb5_pointer, 
		   krb5_data *));
krb5_error_code krb5_write_message 
	KRB5_PROTOTYPE((krb5_context,
		   krb5_pointer, 
		   krb5_data *));
/*
 * server_krb5_auth(
 *  char *keytabfile,	server key tab file - /etc/lpr.keytab
 *  char *service,		service is usually "lpr"
 *  int sock,		   socket for communications
 *  char *auth, int len authname buffer, max size
 *  char *err, int errlen error message buffer, max size
 * RETURNS: 0 if successful, non-zero otherwise, error message in err
 *   Note: there is a memory leak if authentication fails,  so this
 *   should not be done in the main or non-exiting process
 */
int des_read( krb5_context context, krb5_encrypt_block *eblock,
	int fd, char *buf, int len, char *err, int errlen );
int des_write( krb5_context context, krb5_encrypt_block *eblock,
	int fd, char *buf, int len, char *err, int errlen );

int server_krb5_auth( char *keytabfile, char *service, int sock,
	char *auth, int authlen, char *err, int errlen, char *file )
{
	krb5_context context = 0;
	krb5_auth_context auth_context = 0;
	krb5_keytab keytab = 0;  /* Allow specification on command line */
	krb5_principal server = 0;
	krb5_ticket * ticket = 0;
	char *cname = 0;
	int retval = 0;
	int fd = -1;
	char buffer[SMALLBUFFER];
	krb5_data   inbuf, outbuf;

	DEBUG0("server_krb5_auth: keytab '%s', service '%s', sock %d, file '%s'",
		keytabfile, service, sock, file );
	if ((retval = krb5_init_context(&context))){
		plp_snprintf( err, errlen, "%s '%s'",
			"krb5_init_context failed - '%s' ", error_message(retval) );
		goto done;
	}
	if( (retval = krb5_kt_resolve(context, keytabfile, &keytab) ) ){
		plp_snprintf( err, errlen,
			"krb5_kt_resolve failed - file %s '%s'",
			keytabfile,
			error_message(retval) );
		goto done;
	}
	if ((retval = krb5_sname_to_principal(context, NULL, service, 
					 KRB5_NT_SRV_HST, &server))){
		plp_snprintf( err, errlen,
			"krb5_sname_to_principal failed - service %s '%s'",
		   service, error_message(retval));
		goto done;
	}
	if(DEBUGL0){
		if((retval = krb5_unparse_name(context, server, &cname))){
			plp_snprintf( err, errlen,
				"krb5_unparse_name failed: %s", error_message(retval));
			goto done;
		}
		logDebug( "server_krb5_auth: server '%s'", cname );
		free(cname);
		cname = 0;
	}

	if((retval = krb5_recvauth(context, &auth_context, (krb5_pointer)&sock,
				   service , server, 
				   0,   /* no flags */
				   keytab,  /* default keytab is NULL */
				   &ticket))){
		plp_snprintf( err, errlen,
		"krb5_recvauth failed '%s'", error_message(retval));
		goto done;
	}

	/* Get client name */
	if((retval = krb5_unparse_name(context, 
		ticket->enc_part2->client, &cname))){
		plp_snprintf( err, errlen,
			"krb5_unparse_name failed: %s",
			error_message(retval));
		goto done;
	} else {
		strncpy( auth, cname, authlen );
		DEBUG0( "server_krb5_auth: client '%s'", cname );
	}
    /* initialize the initial vector */
    if((retval = krb5_auth_con_initivector(context, auth_context))){
		plp_snprintf( err, errlen,
			"krb5_auth_con_initvector failed: %s",
			error_message(retval));
		goto done;
	}

	krb5_auth_con_setflags(context, auth_context,
		KRB5_AUTH_CONTEXT_DO_SEQUENCE);
	if((retval = krb5_auth_con_genaddrs(context, auth_context, sock,
		KRB5_AUTH_CONTEXT_GENERATE_LOCAL_FULL_ADDR |
			KRB5_AUTH_CONTEXT_GENERATE_REMOTE_FULL_ADDR))){
		plp_snprintf( err, errlen,
			"krb5_auth_con_genaddr failed: %s",
			error_message(retval));
		goto done;
	}
  
	memset( &inbuf, 0, sizeof(inbuf) );
	memset( &outbuf, 0, sizeof(outbuf) );
	if( file ){
		struct stat statb;
		int status;

		fd = Checkwrite( file, &statb, O_WRONLY|O_TRUNC, 1, 0 );
		DEBUG0( "server_krb5_auth: opened '%s', fd %d", file, fd );
		if( fd < 0 ){
			plp_snprintf( err, errlen,
				"file open failed: %s", Errormsg(errno));
			retval = 1;
			goto done;
		}
		while( (retval = krb5_read_message(context,&sock,&inbuf)) == 0 ){
			if(DEBUGL3){
				char small[32];
				memcpy(small,inbuf.data,sizeof(small)-1);
				small[sizeof(small)-1] = 0;
				logDebug( "server_krb5_auth: got %d, '%s'",
					inbuf.length, small );
			}
			if((retval = krb5_rd_priv(context,auth_context,
				&inbuf,&outbuf,NULL))){
				plp_snprintf( err, errlen,
					"krb5_rd_safe failed: %s", error_message(retval));
				retval = 1;
				goto done;
			}
			DEBUG4( "server_krb5_auth: read '%d': '%*s'",
				outbuf.length, outbuf.length, outbuf.data );
			status = Write_fd_len( fd, outbuf.data, outbuf.length );
			if( status < 0 ){
				plp_snprintf( err, errlen,
					"write to file failed: %s", Errormsg(errno));
				retval = 1;
				goto done;
			}
			krb5_xfree(inbuf.data);
			krb5_xfree(outbuf.data);
			inbuf.length = 0;
			outbuf.length = 0;
		}
		retval = 0;
		close(fd);
		fd = -1;
	}
	/* now we fork the other direction */
	{
		int fds[2];
		int pid;
		if( pipe(fds) < 0 ){
			plp_snprintf( err, errlen, "pipe() failed - %s",
				Errormsg(errno) );
			retval = 1;
			goto done;
		}
		if( (pid = dofork(0)) < 0 ){
			plp_snprintf( err, errlen, "dofork() failed - %s",
				Errormsg(errno) );
			retval = 1;
			goto done;
		} else if( pid == 0 ){
			/* we are the child */
			close( fds[1] ); /* output side */
			while( (retval = read( fds[0],buffer,sizeof(buffer)-1)) > 0 ){
				inbuf.length = retval;
				inbuf.data = buffer;
				buffer[retval] = 0;
				DEBUG4("server_krb5_auth: sending '%s'", buffer );
				if((retval = krb5_mk_priv(context,auth_context,
					&inbuf,&outbuf,NULL))){
					plp_snprintf( err, errlen,
						"krb5_mk_priv failed: %s", error_message(retval));
					retval = 1;
					Write_fd_str(2,err);
					cleanup(0);
				}
				DEBUG4("server_krb5_auth: encoded length '%d'", outbuf.length );
				if((retval= krb5_write_message(context,&sock,&outbuf))){
					plp_snprintf( err, errlen,
						"krb5_write_message failed: %s", error_message(retval));
					Write_fd_str(2,err);
					cleanup(0);
				}
				krb5_xfree(outbuf.data);
			}
			DEBUG0("server_krb5_auth: done", buffer );
			cleanup(0);
		} else {
			close( fds[0] ); /* input side */
			if( dup2( fds[1], sock ) == -1 ){
				plp_snprintf( err, errlen, "dup2() failed - %s",
					Errormsg(errno) );
				retval = 1;
				goto done;
			}
			close( fds[1] );
		}
	}
done:
	if( fd >= 0 )	close(fd);
	if( cname )		free(cname);
	if( ticket )	krb5_free_ticket(context, ticket);
	if( server )	krb5_free_principal(context, server);
	if( auth_context)	krb5_auth_con_free(context, auth_context );
	if( context )	krb5_free_context(context);
	DEBUG0( "server_krb5: retval %d, error: '%s'", retval, err );
	return(retval);
}

/*
 * client_krb5_auth(
 *  char * keytabfile	- keytabfile, NULL for users, file name for server
 *  char * service		-service, usually "lpr"
 *  char * host			- server host name
 *  char * principal	- server principal
 *  int options		 - options for server to server
 *  char *life			- lifetime of ticket
 *  char *renew_time	- renewal time of ticket
 *  char *err, int errlen - buffer for error messages 
 *  char *file			- file to transfer
 */ 
#define KRB5_DEFAULT_OPTIONS 0
#define KRB5_DEFAULT_LIFE 60*60*10 /* 10 hours */
#define VALIDATE 0
#define RENEW 1

krb5_error_code krb5_tgt_gen( krb5_context context, krb5_ccache ccache,
	krb5_principal server, krb5_data *outbuf, int opt );

int client_krb5_auth( char *keytabfile, char *service, char *host,
	char *server_principal,
	int options, char *life, char *renew_time,
	int sock, char *err, int errlen, char *file )
{
	krb5_context context = 0;
	krb5_principal client = 0, server = 0;
	krb5_error *err_ret = 0;
	krb5_ap_rep_enc_part *rep_ret = 0;
	krb5_data cksum_data;
	krb5_ccache ccdef;
	krb5_auth_context auth_context = 0;
	krb5_timestamp now;
	krb5_deltat lifetime = KRB5_DEFAULT_LIFE;   /* -l option */
	krb5_creds my_creds;
	krb5_creds *out_creds = 0;
	krb5_keytab keytab = 0;
	krb5_deltat rlife = 0;
	krb5_address **addrs = (krb5_address **)0;
	krb5_encrypt_block eblock;	  /* eblock for encrypt/decrypt */
	krb5_data   inbuf, outbuf;
	int retval = 0;
	char *cname = 0;
	char *sname = 0;
	int fd = -1, len;
	char buffer[SMALLBUFFER];

	err[0] = 0;
	DEBUG0( "client_krb5_auth: keytab '%s',"
		" service '%s', host '%s', sock %d, file '%s'",
		keytabfile, service, host, sock, file );
	memset((char *)&my_creds, 0, sizeof(my_creds));
	memset((char *)&outbuf, 0, sizeof(outbuf));
	memset((char *)&eblock, 0, sizeof(eblock));
	options |= KRB5_DEFAULT_OPTIONS;

	if ((retval = krb5_init_context(&context))){
		plp_snprintf( err, errlen, "%s '%s'",
			"krb5_init_context failed - '%s' ", error_message(retval) );
		goto done;
	}
	if (!valid_cksumtype(CKSUMTYPE_CRC32)) {
		plp_snprintf( err, errlen,
			"valid_cksumtype CKSUMTYPE_CRC32 - %s",
			error_message(KRB5_PROG_SUMTYPE_NOSUPP) );
		retval = 1;
		goto done;
	}
	if(server_principal){
		if ((retval = krb5_parse_name(context,server_principal, &server))){
			plp_snprintf( err, errlen, "client_krb5_auth failed - "
			"when parsing name '%s'"
			" - %s", server_principal, error_message(retval) );
			goto done;
		}
		
	} else {
		if ((retval = krb5_sname_to_principal(context, host, service, 
			 KRB5_NT_SRV_HST, &server))){
			plp_snprintf( err, errlen, "client_krb5_auth failed - "
			"when parsing service/host '%s'/'%s'"
			" - %s", service,host,error_message(retval) );
			goto done;
		}
	}
	
	if(DEBUGL0){
		if((retval = krb5_unparse_name(context, server, &sname))){
			plp_snprintf( err, errlen, "client_krb5_auth failed - "
				"krb5_unparse_name of 'server' failed: %s",
				error_message(retval));
			goto done;
		}
		logDebug( "client_krb5_auth: server '%s'", sname );
		free(sname); sname = 0;
	}

	my_creds.server = server;

	cksum_data.data = host;
	cksum_data.length = strlen(host);

	if( keytabfile ){
		if ((retval = krb5_sname_to_principal(context, NULL, service, 
			 KRB5_NT_SRV_HST, &client))){
			plp_snprintf( err, errlen, "client_krb5_auth failed - "
			"when parsing name '%s'"
			" - %s", service, error_message(retval) );
			goto done;
		}
		if(DEBUGL0){
			if((retval = krb5_unparse_name(context, client, &cname))){
				plp_snprintf( err, errlen, "client_krb5_auth failed - "
					"krb5_unparse_name of 'me' failed: %s",
					error_message(retval));
				goto done;
			}
			logDebug( "client_krb5_auth: client '%s'", cname );
			free(cname); cname = 0;
		}
		my_creds.client = client;
		if((retval = krb5_kt_resolve(context, keytabfile, &keytab))){
			plp_snprintf( err, errlen, "client_krb5_auth failed - "
			 "resolving keytab '%s'"
			" '%s' - ", keytabfile, error_message(retval) );
			goto done;
		}
		if ((retval = krb5_timeofday(context, &now))) {
			plp_snprintf( err, errlen, "client_krb5_auth failed - "
			 "getting time of day"
			" - '%s'", error_message(retval) );
			goto done;
		}
		if( life && (retval = krb5_string_to_deltat(life, &lifetime)) ){
			plp_snprintf( err, errlen, "client_krb5_auth failed - "
			"bad lifetime value '%s'"
			" '%s' - ", life, error_message(retval) );
			goto done;
		}
		if( renew_time ){
			options |= KDC_OPT_RENEWABLE;
			if( (retval = krb5_string_to_deltat(renew_time, &rlife))){
				plp_snprintf( err, errlen, "client_krb5_auth failed - "
				"bad renew time value '%s'"
				" '%s' - ", renew_time, error_message(retval) );
				goto done;
			}
		}
		if((retval = krb5_cc_default(context, &ccdef))) {
			plp_snprintf( err, errlen, "client_krb5_auth failed - "
			"while getting default ccache"
			" - %s", error_message(retval) );
			goto done;
		}

		my_creds.times.starttime = 0;	 /* start timer when request */
		my_creds.times.endtime = now + lifetime;

		if(options & KDC_OPT_RENEWABLE) {
			my_creds.times.renew_till = now + rlife;
		} else {
			my_creds.times.renew_till = 0;
		}

		if(options & KDC_OPT_VALIDATE){
			/* stripped down version of krb5_mk_req */
			if( (retval = krb5_tgt_gen(context,
				ccdef, server, &outbuf, VALIDATE))) {
				plp_snprintf( err, errlen, "client_krb5_auth failed - "
				"validating tgt"
				" - %s", error_message(retval) );
				DEBUG0("%s", err );
			} else {
				/* should be done... */
				goto done;
			}
		}

		if (options & KDC_OPT_RENEW) {
			/* stripped down version of krb5_mk_req */
			if( (retval = krb5_tgt_gen(context,
				ccdef, server, &outbuf, RENEW))) {
				plp_snprintf( err, errlen, "client_krb5_auth failed - "
				"renewing tgt"
				" - %s", error_message(retval) );
				DEBUG0("%s", err );
			} else {
				/* should be done... */
				goto done;
			}
		}

		if((retval = krb5_get_in_tkt_with_keytab(context, options, addrs,
					0, 0, keytab, 0, &my_creds, 0))){
		plp_snprintf( err, errlen, "client_krb5_auth failed - "
			"while getting initial credentials"
			" - %s", error_message(retval) );
			goto done;
		}
		/* update the credentials */
		if( (retval = krb5_cc_initialize (context, ccdef, client)) ){
			plp_snprintf( err, errlen, "client_krb5_auth failed - "
			"when initializing cache"
			" - %s", error_message(retval) );
			goto done;
		}
		if( (retval = krb5_cc_store_cred(context, ccdef, &my_creds))){
			plp_snprintf( err, errlen, "client_krb5_auth failed - "
			"while storing credentials"
			" - %s", error_message(retval) );
			goto done;
		}
	} else {
		if((retval = krb5_cc_default(context, &ccdef))){
			plp_snprintf( err, errlen, "krb5_cc_default failed - %s",
				error_message( retval ) );
			goto done;
		}
		if((retval = krb5_cc_get_principal(context, ccdef, &client))){
			plp_snprintf( err, errlen, "krb5_cc_get_principal failed - %s",
				error_message( retval ) );
			goto done;
		}
		if(DEBUGL0){
			if((retval = krb5_unparse_name(context, client, &cname))){
				plp_snprintf( err, errlen, "client_krb5_auth failed - "
					"krb5_unparse_name of 'me' failed: %s",
					error_message(retval));
				goto done;
			}
			logDebug( "client_krb5_auth: client '%s'", cname );
			free(cname); cname = 0;
		}
		my_creds.client = client;
	}
	if((retval = krb5_auth_con_init(context, &auth_context))){
		plp_snprintf( err, errlen, "client_krb5_auth failed - "
			"krb5_auth_con_init failed: %s",
			error_message(retval));
		goto done;
	}

	if((retval = krb5_auth_con_genaddrs(context, auth_context, sock,
		KRB5_AUTH_CONTEXT_GENERATE_LOCAL_FULL_ADDR |
		KRB5_AUTH_CONTEXT_GENERATE_REMOTE_FULL_ADDR))){
		plp_snprintf( err, errlen, "client_krb5_auth failed - "
			"krb5_auth_con_genaddr failed: %s",
			error_message(retval));
		goto done;
	}
  
	retval = krb5_sendauth(context, &auth_context, (krb5_pointer) &sock,
			   service , client, server,
			   AP_OPTS_MUTUAL_REQUIRED,
			   &cksum_data,
			   &my_creds,
			   ccdef, &err_ret, &rep_ret, &out_creds);

	if (retval){
		if( err_ret == 0 ){
			plp_snprintf( err, errlen, "krb5_sendauth failed - %s",
				error_message( retval ) );
		} else {
			plp_snprintf( err, errlen,
				"krb5_sendauth - mutual authentication failed - %*s",
				err_ret->text.length, err_ret->text.data);
		}
		goto done;
	} else if (rep_ret == 0) {
		plp_snprintf( err, errlen,
			"krb5_sendauth - did not do mutual authentication" );
		retval = 1;
		goto done;
	} else {
		DEBUG0("client_krb5_auth: sequence number %d", rep_ret->seq_number );
	}
    /* initialize the initial vector */
    if((retval = krb5_auth_con_initivector(context, auth_context))){
		plp_snprintf( err, errlen,
			"krb5_auth_con_initvector failed: %s",
			error_message(retval));
		goto done;
	}

	krb5_auth_con_setflags(context, auth_context,
		KRB5_AUTH_CONTEXT_DO_SEQUENCE);

	memset( &inbuf, 0, sizeof(inbuf) );
	memset( &outbuf, 0, sizeof(outbuf) );

	if( file ){
		struct stat statb;

		fd = Checkread( file, &statb );
		if( fd < 0 ){
			plp_snprintf( err, errlen,
				"client_krb5_auth: could not open '%s' - '%s'", file,
					Errormsg(errno) );
			retval = 1;
			goto done;
		}
		DEBUG0( "client_krb5_auth: opened %s, fd %d", file, fd );
		while( (len = read( fd, buffer, sizeof(buffer)-1 )) > 0 ){
			/* status = Write_fd_len( sock, buffer, len ); */
			inbuf.data = buffer;
			inbuf.length = len;
			if((retval = krb5_mk_priv(context, auth_context, &inbuf,
				&outbuf, NULL))){
				plp_snprintf( err, errlen,
					"krb5_mk_priv failed: %s",
					error_message(retval));
				goto done;
			}
			if((retval = krb5_write_message(context, (void *)&sock, &outbuf))){
				plp_snprintf( err, errlen,
					"krb5_write_message failed: %s",
					error_message(retval));
				goto done;
			}
			krb5_xfree(outbuf.data);
		}
		if( len < 0 ){
			plp_snprintf( err, errlen,
				"client_krb5_auth: file read failed '%s' - '%s'", file,
					Errormsg(errno) );
			retval = 1;
			goto done;
		}
		close(fd);
		fd = -1;
		DEBUG0( "client_krb5_auth: file copy finished %s", file );
		if( shutdown(sock, 1) == -1 ){
			plp_snprintf( err, errlen,
				"shutdown failed '%s'", Errormsg(errno) );
			retval = 1;
			goto done;
		}
	}
	/* now we fork the other direction */
	{
		int fds[2];
		int pid;
		if( pipe(fds) < 0 ){
			plp_snprintf( err, errlen, "pipe() failed - %s",
				Errormsg(errno) );
			retval = 1;
			goto done;
		}
		if( (pid = dofork(0)) < 0 ){
			plp_snprintf( err, errlen, "dofork() failed - %s",
				Errormsg(errno) );
			retval = 1;
			goto done;
		} else if( pid == 0 ){
			/* we are the child */
			close( fds[0] ); /* input side */
			while((retval = krb5_read_message( context,&sock,&inbuf))==0){
				if((retval = krb5_rd_priv(context, auth_context, &inbuf,
					&outbuf, NULL))){
					plp_snprintf( err, errlen, "dup2() failed - %s",
						Errormsg(errno) );
					Write_fd_str( 2, err );
					cleanup(0);
				}
				if(Write_fd_len(fds[1],outbuf.data,outbuf.length) < 0) cleanup(0);
				krb5_xfree(inbuf.data);
				krb5_xfree(outbuf.data);
			}
			cleanup(0);
		} else {
			close( fds[1] ); /* output side */
			if( dup2( fds[0], sock ) == -1 ){
				plp_snprintf( err, errlen, "dup2() failed - %s",
					Errormsg(errno) );
				retval = 1;
				goto done;
			}
			close( fds[0] );
		}
	}
done:
	if( fd >= 0 ) close(fd);
	krb5_free_cred_contents( context, &my_creds );
	if( rep_ret )	krb5_free_ap_rep_enc_part( context, rep_ret );
	if( err_ret )	krb5_free_error( context, err_ret );
	if( server )	krb5_free_principal(context, server);
	if( client )	krb5_free_principal(context, client);
	if( auth_context) krb5_auth_con_free(context, auth_context );
	if( context )	krb5_free_context(context);
	DEBUG0( "retval %d, error: '%s'",retval, err );
	return(retval);
}

/*
 * remote_principal_krb5(
 *  char * service		-service, usually "lpr"
 *  char * host			- server host name
 *  char *buffer, int bufferlen - buffer for credentials
 *  get the principal name of the remote service
 */ 
int remote_principal_krb5( char *service, char *host, char *err, int errlen )
{
	krb5_context context = 0;
	krb5_principal server = 0;
	int retval = 0;
	char *cname = 0;

	DEBUG0("remote_principal_krb5: service '%s', host '%s'",
		service, host );
	if ((retval = krb5_init_context(&context))){
		plp_snprintf( err, errlen, "%s '%s'",
			"krb5_init_context failed - '%s' ", error_message(retval) );
		goto done;
	}
	if((retval = krb5_sname_to_principal(context, host, service,
			 KRB5_NT_SRV_HST, &server))){
		plp_snprintf( err, errlen, "krb5_sname_to_principal %s/%s failed - %s",
			service, host, error_message(retval) );
		goto done;
	}
	if((retval = krb5_unparse_name(context, server, &cname))){
		plp_snprintf( err, errlen,
			"krb5_unparse_name failed - %s", error_message(retval));
		goto done;
	}
	strncpy( err, cname, errlen );
done:
	if( cname )		free(cname);
	if( server )	krb5_free_principal(context, server);
	if( context )	krb5_free_context(context);
	DEBUG0( "remote_principal_krb5: retval %d, result: '%s'",retval, err );
	return(retval);
}

/*
 * Initialize a credentials cache.
 */

#define KRB5_DEFAULT_OPTIONS 0
#define KRB5_DEFAULT_LIFE 60*60*10 /* 10 hours */

#define VALIDATE 0
#define RENEW 1

krb5_error_code krb5_tgt_gen( krb5_context context, krb5_ccache ccache,
	krb5_principal server, krb5_data *outbuf, int opt );

/* stripped down version of krb5_mk_req */
krb5_error_code krb5_tgt_gen( krb5_context context, krb5_ccache ccache,
	 krb5_principal server, krb5_data *outbuf, int opt )
{
	krb5_error_code	   retval;
	krb5_creds		  * credsp;
	krb5_creds			creds;

	/* obtain ticket & session key */
	memset((char *)&creds, 0, sizeof(creds));
	if ((retval = krb5_copy_principal(context, server, &creds.server)))
		goto cleanup;

	if ((retval = krb5_cc_get_principal(context, ccache, &creds.client)))
		goto cleanup_creds;

	if(opt == VALIDATE) {
			if ((retval = krb5_get_credentials_validate(context, 0,
					ccache, &creds, &credsp)))
				goto cleanup_creds;
	} else {
			if ((retval = krb5_get_credentials_renew(context, 0,
					ccache, &creds, &credsp)))
				goto cleanup_creds;
	}

	/* we don't actually need to do the mk_req, just get the creds. */
cleanup_creds:
	krb5_free_cred_contents(context, &creds);

cleanup:

	return retval;
}


char *storage;
int nstored = 0;
char *store_ptr;
krb5_data desinbuf,desoutbuf;

#define ENCBUFFERSIZE 2*LARGEBUFFER

int des_read( krb5_context context,
	krb5_encrypt_block *eblock,
	int fd, char *buf, int len,
	char *err, int errlen )
{
	int nreturned = 0;
	long net_len,rd_len;
	int cc;
	unsigned char len_buf[4];
	
	if( len <= 0 ) return(len);

	if( desinbuf.data == 0 ){
		desinbuf.data = malloc_or_die(  ENCBUFFERSIZE );
		storage = malloc_or_die( ENCBUFFERSIZE );
	}
	if (nstored >= len) {
		memcpy(buf, store_ptr, len);
		store_ptr += len;
		nstored -= len;
		return(len);
	} else if (nstored) {
		memcpy(buf, store_ptr, nstored);
		nreturned += nstored;
		buf += nstored;
		len -= nstored;
		nstored = 0;
	}
	
	if ((cc = read(fd, len_buf, 4)) != 4) {
		/* XXX can't read enough, pipe must have closed */
		return(0);
	}
	rd_len =
		((len_buf[0]<<24) | (len_buf[1]<<16) | (len_buf[2]<<8) | len_buf[3]);
	net_len = krb5_encrypt_size(rd_len,eblock->crypto_entry);
	if ((net_len <= 0) || (net_len > ENCBUFFERSIZE )) {
		/* preposterous length; assume out-of-sync; only
		   recourse is to close connection, so return 0 */
		plp_snprintf( err, errlen, "des_read: "
			"read size problem");
		return(-1);
	}
	if ((cc = read( fd, desinbuf.data, net_len)) != net_len) {
		/* pipe must have closed, return 0 */
		plp_snprintf( err, errlen, "des_read: "
		"Read error: length received %d != expected %d.",
				cc, net_len);
		return(-1);
	}
	/* decrypt info */
	if((cc = krb5_decrypt(context, desinbuf.data, (krb5_pointer) storage,
						  net_len, eblock, 0))){
		plp_snprintf( err, errlen, "des_read: "
			"Cannot decrypt data from network - %s", error_message(cc) );
		return(-1);
	}
	store_ptr = storage;
	nstored = rd_len;
	if (nstored > len) {
		memcpy(buf, store_ptr, len);
		nreturned += len;
		store_ptr += len;
		nstored -= len;
	} else {
		memcpy(buf, store_ptr, nstored);
		nreturned += nstored;
		nstored = 0;
	}
	
	return(nreturned);
}


int des_write( krb5_context context,
	krb5_encrypt_block *eblock,
	int fd, char *buf, int len,
	char *err, int errlen )
{
	unsigned char len_buf[4];
	int cc;

	if( len <= 0 ) return( len );
	if( desoutbuf.data == 0 ){
		desoutbuf.data = malloc_or_die( ENCBUFFERSIZE );
	}
	desoutbuf.length = krb5_encrypt_size(len, eblock->crypto_entry);
	if (desoutbuf.length > ENCBUFFERSIZE ){
		plp_snprintf( err, errlen, "des_write: "
		"Write size problem - wanted %d", desoutbuf.length);
		return(-1);
	}
	if ((cc=krb5_encrypt(context, (krb5_pointer)buf,
					   desoutbuf.data,
					   len,
					   eblock,
					   0))){
		plp_snprintf( err, errlen, "des_write: "
		"Write encrypt problem. - %s", error_message(cc));
		return(-1);
	}
	
	len_buf[0] = (len & 0xff000000) >> 24;
	len_buf[1] = (len & 0xff0000) >> 16;
	len_buf[2] = (len & 0xff00) >> 8;
	len_buf[3] = (len & 0xff);
	if( Write_fd_len(fd, len_buf, 4) < 0 ){
		plp_snprintf( err, errlen, "des_write: "
		"Could not write len_buf - %s", Errormsg( errno ));
		return(-1);
	}
	if(Write_fd_len(fd, desoutbuf.data,desoutbuf.length) < 0 ){
		plp_snprintf( err, errlen, "des_write: "
		"Could not write data - %s", Errormsg(errno));
		return(-1);
	}
	else return(len); 
}
#endif
