/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: sclient.c,v 5.1 1999/09/12 21:32:30 papowell Exp papowell $";


/*
 * Simple Kerberos client tester.
 * Derived from the Kerberos 5 SCLIENT code -
 *   Note carefully that there are NO MIT copyrights here.
 */

#include "lp.h"
#include "child.h"
#include "krb5_auth.h"

char *msg[] = {
	"[-D options] [-p port] [-s service] [-k keytab] [-P principal] host file",
	"  -D turns debugging on",
	0
};

char *progname;

void
usage()
{   
	int i;
	fprintf(stderr, "usage: %s %s\n", progname, msg[0]);
	for( i = 1; msg[i]; ++i ){
		fprintf(stderr, "%s\n", msg[i]);
	}
}  


int
main(argc, argv)
int argc;
char *argv[];
{
    struct hostent *host_ent;
    struct sockaddr_in sin;
    int sock;
	int port = 1234;
	char *host;
	int c;
	extern int opterr, optind, getopt();
	extern char * optarg;
	char buffer[SMALLBUFFER];
	char msg[SMALLBUFFER];
	char *file;
	char *keytab = 0;
	char *principal = 0;

	progname = argv[0];
	while( (c = getopt(argc, argv, "D:p:s:k:P:")) != EOF){
		switch(c){
		default: 
			fprintf(stderr,"bad option '%c'\n", c );
			usage(progname); exit(1); break;
		case 'k': keytab = optarg; break;
		case 'D': Parse_debug(optarg,1); break;
		case 'p': port= atoi(optarg); break;
		case 's': Set_DYN(&Kerberos_service_DYN,optarg); break;
		case 'P': principal = optarg; break;
		}
	}
	if( argc - optind != 2 ){
		fprintf(stderr,"missing host or file name\n" );
		usage(progname);
		exit(1);
	}
	host = argv[optind++];
	file = argv[optind++];

    /* clear out the structure first */
    (void) memset((char *)&sin, 0, sizeof(sin));
	if(Kerberos_service_DYN == 0 ) Set_DYN(&Kerberos_service_DYN,"lpr");
	if( principal ){
		fprintf(stderr, "using '%s'\n", principal );
	} else {
		remote_principal_krb5( Kerberos_service_DYN, host, buffer, sizeof(buffer) );
		fprintf(stderr, "default remote principal '%s'\n", buffer );
		principal = buffer;
	}

    /* look up the server host */
    host_ent = gethostbyname(host);
    if(host_ent == 0){
		fprintf(stderr, "unknown host %s\n",host);
		exit(1);
    }

    /* set up the address of the foreign socket for connect() */
    sin.sin_family = host_ent->h_addrtype;
    (void) memcpy((char *)&sin.sin_addr, (char *)host_ent->h_addr,
		 sizeof(host_ent->h_addr));
	sin.sin_port = htons(port);

    /* open a TCP socket */
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if( sock < 0 ){
		perror("socket");
		exit(1);
    }

    /* connect to the server */
    if( connect(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0 ){
		perror("connect");
		close(sock);
		exit(1);
    }

	buffer[0] = 0;
	if( client_krb5_auth( keytab, Kerberos_service_DYN, host,
		0, 0, 0, 0,
		sock, buffer, sizeof(buffer), file ) ){
		fprintf( stderr, "client_krb5_auth failed: %s\n", buffer );
		exit(1);
	}
	fflush(stdout);
	fflush(stderr);
	plp_snprintf(msg, sizeof(msg),"starting read from %d\n", sock );
	write(1,msg, strlen(msg) );
	while( (c = read( sock, buffer, sizeof(buffer) ) ) > 0 ){
		buffer[c] = 0;
		plp_snprintf(msg, sizeof(msg),
			"read %d from fd %d '%s'\n", c, sock, buffer );
		write( 1, msg, strlen(msg) );
	}
	plp_snprintf(msg, sizeof(msg),
		"last read status %d from fd %d\n", c, sock );
	write( 1, msg, strlen(msg) );
    return(0);
}

/* VARARGS2 */
#ifdef HAVE_STDARGS
void setstatus (struct job *job,char *fmt,...)
#else
void setstatus (va_alist) va_dcl
#endif
{
#ifndef HAVE_STDARGS
    struct job *job;
    char *fmt;
#endif
	char msg[LARGEBUFFER];
    VA_LOCAL_DECL

    VA_START (fmt);
    VA_SHIFT (job, struct job * );
    VA_SHIFT (fmt, char *);

	msg[0] = 0;
	if( Verbose ){
		(void) plp_vsnprintf( msg, sizeof(msg)-2, fmt, ap);
		strcat( msg,"\n" );
		if( Write_fd_str( 2, msg ) < 0 ) cleanup(0);
	}
	VA_END;
	return;
}

void send_to_logger (struct job *job,const char *header, char *fmt){;}
/* VARARGS2 */
#ifdef HAVE_STDARGS
void setmessage (struct job *job,const char *header, char *fmt,...)
#else
void setmessage (va_alist) va_dcl
#endif
{
#ifndef HAVE_STDARGS
    struct job *job;
    char *fmt, *header;
#endif
	char msg[LARGEBUFFER];
    VA_LOCAL_DECL

    VA_START (fmt);
    VA_SHIFT (job, struct job * );
    VA_SHIFT (header, char * );
    VA_SHIFT (fmt, char *);

	msg[0] = 0;
	if( Verbose ){
		(void) plp_vsnprintf( msg, sizeof(msg)-2, fmt, ap);
		strcat( msg,"\n" );
		if( Write_fd_str( 2, msg ) < 0 ) cleanup(0);
	}
	VA_END;
	return;
}

