/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: sendjob.c
 * PURPOSE: Send a print job to the remote host
 *
 **************************************************************************/

static char *const _id =
"sendauth.c,v 3.10 1998/03/24 02:43:22 papowell Exp";

#include "lp.h"
#include "sendauth.h"
#include "errorcodes.h"
#include "fileopen.h"
#include "killchild.h"
#include "linksupport.h"
#include "sendauth.h"
#include "setstatus.h"
#include "setup_filter.h"
#include "printcap.h"
#include "malloclist.h"
#include "cleantext.h"
#include "krb5_auth.h"
/**** ENDINCLUDE ****/

/***************************************************************************
Commentary:

int Send_auth_command(
	char *host,		- remote host name
	int *sock,      - socket to use
	int transfer_timeout 	- timeout on sending a file
	int controlkey 	- control key
	char *line 	- line to follow control key

1. We generate the authentication buffer.  This consists
   of the line,  followed by authentication information
   if we are forwarding it.

2. We write this to a temp file.

3. We invoke the authenticator to transfer file.

4. We wait for the authenticator to return any information,
   and then we read it.

5. We wait for authenticator to exit, and use the error status
   as an indication of success.


 ***************************************************************************/

int Send_auth_command( char *printer, char *host, int *sock,
	int transfer_timeout, char *line, int output )
{
	int tempfd;			/* temp file for data transfer */
	char tempbuf[LARGEBUFFER];	/* buffer */
	int err;	/* ACME! The best... */
	int status = 0;				/* job status */
	char tempfilename[MAXPATHLEN];

	DEBUG0("Send_auth_command: printer '%s' on host '%s'",
		printer, host );

	/* check to see if the Cfp_static is defined */
	if( Cfp_static == 0 ){
		Cfp_static = malloc_or_die( sizeof(Cfp_static[0]) );
		memset(Cfp_static, 0, sizeof( Cfp_static[0] ) );
	}

	DEBUG0("Send_auth_command: authentication '%s'", Cfp_static->auth_id );
	/* set up printer */
	if( printer == 0 || *printer == 0 ){
		printer = "NULL";
	}

	if( Is_server && Cfp_static->auth_id[0] == 0 ){
		fatal( LOG_ERR,
		"Send_auth_command: server has missing job authentication");
	}

	/* make temp file */
	tempfd = Make_temp_fd( tempfilename, sizeof(tempfilename) );
	if( tempfd <= 0 ){
		err = errno;
		Errorcode = JFAIL;
		logerr_die( LOG_INFO,
			"Send_auth_command: error opening temp fd" );
	}
	if( ftruncate( tempfd, 0 ) < 0 ){
		err = errno;
		Errorcode = JFAIL;
		logerr_die( LOG_INFO,
			"Send_auth_command: error truncating temp fd" );
	}

	/* now we put in the control message, followed by authentication
	 * information
	 */
	/* now we need to add the authentication information */
	safestrncpy( tempbuf, line );
	if( Cfp_static->auth_id[0] ){
		int len;
		len = strlen( tempbuf );
		plp_snprintf( tempbuf+len, sizeof(tempbuf)-len,
			"%s\n", Cfp_static->auth_id+1 );
	}
	if( Write_fd_str( tempfd, tempbuf ) < 0 ){
		err = errno;
		Errorcode = JFAIL;
		logerr_die( LOG_INFO,
			"Send_auth_command: error writing temp fd" );
	}
	status = Send_auth_transfer( 0, printer, host,
		sock, transfer_timeout, (void *)0, tempfd, tempfilename, output, 0 );
	DEBUG0("Send_auth_command: done" );
	return( status );
}

int Send_auth_transfer(int logtransfer, char *printer, char *host,
	int *sock, int transfer_timeout, struct printcap_entry *printcap_entry,
	int tempfd, char *tempfilename, int output, char *printfile )
{
	char tempbuf[LINEBUFFER];	/* buffer */
	struct stat statb;
	int size, i, ack, key;	/* ACME! The best... */
	int status = 0;				/* job status */
	char *user;
	int fd_list[10]; /* fd 0 - 5 */
	int pipe_fd[2];
	char *auth = 0;

#if defined(HAVE_KRB5_H)
	int use_kerberos = 0;

	if( Use_auth && strcasecmp( Use_auth, "kerberos" ) == 0 ){
		DEBUG0("Send_auth_transfer: using built in Kerberos authentication" );
		use_kerberos = 1;
	} else 
#endif
	{
		if( Is_server ){
			auth = Server_authentication_command;
		} else {
			auth = User_authentication_command;
		}
		if( auth == 0 || *auth == 0 ){
			fatal( LOG_ERR, "no authentication command available");
		}
	}

	/* get the user id's and flags */
	if( strpbrk( Use_auth, " \t\n") || Find_meta( Use_auth )  ){
		Errorcode = JFAIL;
		fatal( LOG_ERR, "Send_auth_transfer: use_auth has bad format '%s'",
				Use_auth );
	}
	if( Is_server ){
		user = Server_user;
		if( user == 0 || *user == 0 ){
			user = Daemon_user;
		}
		key = 'F';
	} else {
		user = Logname;
		if( Remote_user == 0 || *Remote_user == 0 ){
			Remote_user = Server_user; /* default to the server user */
		}
		key = 'C';
	}
	if( user == 0 || *user == 0 ){
		Errorcode = JFAIL;
		fatal( LOG_ERR, "Send_auth_transfer: missing user or server id" );
	}
	if( strpbrk( user, " \t\n") || Find_meta( user)  ){
		Errorcode = JFAIL;
		fatal( LOG_ERR, "Send_auth_transfer: user name has bad format '%s'",
				user );
	}
	if( Remote_user == 0 || *Remote_user == 0 ){
		Remote_user = Daemon_user; /* default to the daemon user */
	}
	if( Remote_user == 0 || *Remote_user == 0 ){
		Errorcode = JFAIL;
		fatal( LOG_ERR, "Send_auth_transfer: missing user or server id" );
	}
	if( strpbrk( Remote_user, " \t\n") || Find_meta( Remote_user )  ){
		Errorcode = JFAIL;
		fatal( LOG_ERR, "Send_auth_transfer: remote_user has bad format '%s'",
				Remote_user );
	}
	DEBUG1("Send_auth_transfer: after Server_user '%s', Remote_user '%s'", Server_user, Remote_user);

	/* now we have the copy, we need to send the control message */
	if( fstat( tempfd, &statb ) ){
		Errorcode = JFAIL;
		logerr_die( LOG_INFO, "Send_auth_transfer: fstat tempfd failed" );
	}
	size = statb.st_size;

	/* close the file */
	if( close( tempfd ) == -1 ){
		Errorcode = JFAIL;
		logerr_die( LOG_INFO, "Send_auth_transfer: close tempfd failed" );
	}

	/* now we know the size */
	DEBUG3("Send_auth_transfer: size %d", size );
	if(logtransfer){
		setstatus( Cfp_static, "sending job '%s' to %s@%s",
			Cfp_static->transfername, printer, host );
	}
	if( printfile == 0 ){
		plp_snprintf( tempbuf, sizeof(tempbuf),
			"%c%s %c %s %s\n", REQ_SECURE,printer, key, user, Use_auth );
	} else {
		plp_snprintf( tempbuf, sizeof(tempbuf),
			"%c%s %c %s %s %s\n",REQ_SECURE,printer,key,user, Use_auth,
				printfile );
	}
	DEBUG3("Send_auth_transfer: sending '%s'", tempbuf );
	status = Link_send( host, sock, transfer_timeout,
		tempbuf, strlen(tempbuf), &ack );
	DEBUG3("Send_auth_transfer: status '%s'", Link_err_str(status) );
	if( status ){
		if(logtransfer){
			setstatus( Cfp_static, "error '%s' sending '%s' to %s@%s",
			Link_err_str(status), tempbuf, printer, host );
		}
		return(status);
	}

	/*
	 * we now use kerberos if we have it built in
	 */
#if defined(HAVE_KRB5_H)
	if( use_kerberos ){
		char *keyfile = 0;
		DEBUG0("Send_auth_transfer: starting kerberos authentication" );
		if( Is_server ){
			keyfile = Kerberos_keytab;
			if( keyfile == 0 ){
				fatal(LOG_ERR,
				"Send_auth_transfer: no server keytab file name" );
			}
		}
		if( client_krb5_auth( keyfile, Kerberos_service, host,
			Kerberos_server_principle,	/* name of the remote server */
			0,	/* options */
			Kerberos_life,	/* lifetime of server ticket */
			Kerberos_renew,	/* renewable time of server ticket */
			*sock,
			tempbuf, sizeof(tempbuf), tempfilename ) ){
			Errorcode = JFAIL;
			fatal( LOG_ERR,
				"Send_auth_transfer: built-in Kerberos failed - '%s'",
					tempbuf );
		}
		/* we are done, return success */
		return(JSUCC);
	}
#endif

	plp_snprintf( tempbuf, sizeof(tempbuf),
		"%s -C -P%s -n%s -A%s -R%s -T%s",
		auth,printer,user, Use_auth, Remote_user, tempfilename );
	DEBUG3("Send_auth_transfer: preliminary '%s'", tempbuf );

	/* now set up the file descriptors:
	 *   FD  Options Purpose
	 *    0  R/W     socket connection to remote host (R/W)
	 *    1  W       pipe or file descriptor,  for responses to client programs
	 *    2  W       error log
	 */

	if( pipe(pipe_fd) <  0 ){
		Errorcode = JFAIL;
		logerr_die( LOG_INFO, "Send_auth_transfer: pipe failed" );
	}
	i = 0;
	fd_list[i++] = *sock;
	fd_list[i++] = pipe_fd[1];
	fd_list[i++] = 0;

	if( Make_passthrough( &Passthrough_send, tempbuf, fd_list, i,
		0, Cfp_static, printcap_entry ) < 0 ){
		Errorcode = JFAIL;
		fatal( LOG_ERR,"Send_auth_transfer: %s", Cfp_static->error );
	}

	close( pipe_fd[1] );
	/* now we wait for the status */
	DEBUG3("Send_auth_transfer: socket %d, pipe_fd %d", *sock, pipe_fd[0]);
	if( *sock != pipe_fd[0] ){
		if( dup2( pipe_fd[0], *sock ) < 0 ){
			Errorcode = JFAIL;
			logerr_die( LOG_INFO, "Send_auth_transfer: dup2 failed" );
		}
		close( pipe_fd[0] );
	}
	DEBUG3("Send_auth_transfer: waiting for data" );
	return( status );
}


/***************************************************************************
 * void Fix_auth() - get the Use_auth value for the remote printer
 ***************************************************************************/
void Fix_auth()
{
	char copyname[LINEBUFFER];
	char *str;
	int change;

	if( Is_server ){
		if( Auth_from ){
			Use_auth = Forward_auth;
		}
	} else {
		if( Use_auth_flag && Use_auth == 0 ){
			Use_auth = Default_auth;
		}
	}
	DEBUG3("Fix_auth: Is_server %d, Auth_from %d, Forward_auth '%s', Use_auth '%s'",
		Is_server, Auth_from, Forward_auth, Use_auth );
	/* now we check the Server_user */
	if(Server_user && strchr( Server_user, '%' ) ){
		/* we need to expand the name */
		change = Expand_percent( Server_user, copyname, copyname+sizeof(copyname)-2 );
		if( change ){
			if( (str = strchr( copyname, '.' )) ) *str = 0;
			Server_user = add_str( &Raw_printcap_files.expanded_str, copyname,__FILE__,__LINE__  );
		}
	}
	if(Remote_user && strchr( Remote_user, '%' ) ){
		/* we need to expand the name */
		change = Expand_percent( Remote_user, copyname, copyname+sizeof(copyname)-2 );
		if( change ){
			if( (str = strchr( copyname, '.' )) ) *str = 0;
			Remote_user = add_str( &Raw_printcap_files.expanded_str, copyname,__FILE__,__LINE__  );
		}
	}
}
