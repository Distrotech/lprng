/*
 * Simple Kerberos client tester.
 * Derived from the Kerberos 5 SCLIENT code -
 *   Note carefully that there are NO MIT copyrights here.
 */

#include "lp.h"
#include "krb5_auth.h"

char *progname;
void
usage()
{   
	fprintf(stderr, "usage: %s [-D] [-p port] [-s service] [-k keytab] [-P principal] host file\n"
		"  -D turns debugging on\n", progname);
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
	char buffer[128];
	char *file;
	char *keytab = 0;
	char *principal = 0;

	progname = argv[0];
	while( (c = getopt(argc, argv, "Dp:s:k:")) != EOF){
		switch(c){
		default: usage(progname); exit(1); break;
		case 'k': keytab = optarg; break;
		case 'D': Debug = 9; break;
		case 'p': port= atoi(optarg); break;
		case 's': Kerberos_service = optarg; break;
		case 'P': principal = optarg; break;
		}
	}
	if( argc - optind != 2 ){
		usage(progname); 
		exit(1);
	}
	host = argv[optind++];
	file = argv[optind++];

    /* clear out the structure first */
    (void) memset((char *)&sin, 0, sizeof(sin));
	if(Kerberos_service == 0 ) Kerberos_service = "lpr";
	remote_principal_krb5( Kerberos_service, host, buffer, sizeof(buffer) );
	fprintf(stderr, "default remote principal '%s'\n", buffer );
	if( principal ){
		fprintf(stderr, "using '%s'\n", principal );
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
	if( client_krb5_auth( keytab, Kerberos_service, host,
		0, 0, 0, 0,
		sock, buffer, sizeof(buffer), file ) ){
		fprintf( stderr, "authentication failed: %s\n", buffer );
		exit(1);
	}
	fprintf(stderr, "starting read from %d\n", sock );
	while( (c = read( sock, buffer, sizeof(buffer) ) ) > 0 ){
		fprintf(stderr, "read %d from %d\n", c, sock );
		buffer[c] = 0;
		write( 1, buffer, c );
	}
	fprintf(stderr, "finished read from %d\n", sock );
    return(0);
}
