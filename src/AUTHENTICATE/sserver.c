/*
 * 
 * Simple authentictor test for kerberos.
 *  Based on the SSERVER code in the Kerberos5 distibution.
 *  See Copyrights, etc.
 */

#include "lp.h"
#include "krb5_auth.h"

char *progname;

void usage(void)
{
	fprintf(stderr,
	"usage: %s [-D] [-p port] [-s service] [-S keytab] file\n"
		"  -D turns debugging on\n", progname);
}   

int
main(int argc, char *argv[])
{
	struct sockaddr_in peername;
	int namelen = sizeof(peername);
	int sock = -1;          /* incoming connection fd */
	short port = 1234;     /* If user specifies port */
	extern int opterr, optind, getopt(), atoi();
	extern char * optarg;
	int ch;
	int on = 1;
	int acc;
	struct sockaddr_in sin;
	char auth[128];
	char err[128];
	char *file;

	progname = argv[0];

	/*
	 * Parse command line arguments
	 *  
	 */
	opterr = 0;
	while ((ch = getopt(argc, argv, "Dp:S:s:")) != EOF)
	switch (ch) {
	case 'D': Debug = 9; break;
	case 'p': port = atoi(optarg); break;
	case 's': Kerberos_service  = optarg; break;
	case 'S': Kerberos_keytab = optarg; break;
	default: usage(); exit(1); break;
	}
	Spool_file_perms = 0600;

	if( argc - optind != 1 ){
		usage();
		exit(1);
	}
	file = argv[optind++];

	if( Kerberos_keytab == 0 ) Kerberos_keytab = ETCSTR "/lpd.keytab";
	if( Kerberos_service == 0 ) Kerberos_service = "lpr";
	if( port == 0 ){
		fprintf( stderr, "bad port specified\n" );
		exit(1);
	}
	/*
	 * If user specified a port, then listen on that port; otherwise,
	 * assume we've been started out of inetd. 
	 */

	remote_principal_krb5( Kerberos_service, 0, auth, sizeof(auth));
	fprintf(stderr, "server principal '%s'\n", auth );

	if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "socket: %s\n", Errormsg(errno));
		exit(3);
	}
	/* Let the socket be reused right away */
	(void) setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
			  sizeof(on));

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = 0;
	sin.sin_port = htons(port);
	if (bind(sock, (struct sockaddr *) &sin, sizeof(sin))) {
		fprintf(stderr, "bind: %s\n", Errormsg(errno));
		exit(3);
	}
	if (listen(sock, 1) == -1) {
		fprintf(stderr, "listen: %s", Errormsg(errno));
		exit(3);
	}
	while(1){
		if ((acc = accept(sock, (struct sockaddr *)&peername,
				&namelen)) == -1){
			fprintf(stderr, "accept: %s\n", Errormsg(errno));
			exit(3);
		}

		err[0] = 0;
		auth[0] = 0;
		if( server_krb5_auth( Kerberos_keytab, Kerberos_service, acc,
			auth, sizeof(auth), err, sizeof(err), file ) ){
			fprintf( stderr, "error '%s'\n", err );
		} else {
			sprintf( err, "authorized '%s'\n", auth );
			fprintf( stderr, "writing to %d - '%s'\n", acc, err );
			ch = write( acc, err, strlen(err) );
			fprintf( stderr, "write returned %d\n", ch );
			
		}
		close(acc);
	}
	exit(0);
}
