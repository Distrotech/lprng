/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: permission.h
 * PURPOSE: permission file parsing
 * $Id: permission.h,v 3.1 1996/08/25 22:20:05 papowell Exp papowell $
 **************************************************************************/

#ifndef _PERMISSION_H
#define _PERMISSION_H
/*
 * See permission.c for details on the follow datastructures
 * each permission ntry has the following information
 */

struct perm_val {
	int key;			/* keyword of permssion entry */
	char *token;		/* token for it */
	struct malloc_list values;	/* values of strings */
};

struct perm_line {
	int name;	/* permssion name */
	int flag;	/* flags */
	struct malloc_list options;	/* options on line */
};

/*
 * The printcap files are stored using the following data structures
 * The fields field points to the lines in the printcap files
 * The entry
 */

struct perm_file {
	struct malloc_list lines;	/* lines in permisson */
	struct malloc_list buffers;	/* list of buffers */
};

EXTERN struct perm_file Perm_file;

/***************************************************************************
 * Permissions keywords
 ***************************************************************************/

#define REJECT		-1
#define ACCEPT		1
#define NOT			2	/* invert test condition */
#define SERVICE		3	/* Service listed below */
#define USER		4	/* USER field from control file (LPR) or command */
						/* if a command, then the user name sent with command */
#define HOST		5	/* HOST field from control file */
						/* if not a printing operation, then host name
							sent with command */
#define IP			6	/* IP address of HOST */
#define PORT		7	/* remote connect */
#define REMOTEHOST	8	/* remote end of connnection host name */
						/* if printing, has the same value as HOST */
#define REMOTEIP	9	/* remote end of connnection IP address */
						/* if printing, has the same value as IP */
#define PRINTER		10	/* printer */
#define DEFAULT		11
#define FORWARD		12	/* forward - REMOTE IP != IP */
#define SAMEHOST	13	/* same host - REMOTE IP == IP */
#define SAMEUSER	14	/* remote user name on command line == user in file */
#define CONTROLLINE	15	/* line from control file */
#define GROUP	 	16	/* user is in named group - uses getpwname() */
#define SERVER	 	17	/* request is from the server */
#define REMOTEUSER 	18	/* USER from control information */

/*
 * First character of protocol to letter mappings
 */

#define STARTPR     'P'  /* 1  - from lPc */
#define RECVJOB     'R'  /* 2  - from lpR, connection for printer */
#define TRANSFERJOB 'T'  /* 2  - from lpR, user information in job */
#define SHORTSTAT   'Q'  /* 3  - from lpQ */
#define LONGSTAT    'Q'  /* 4  - from lpQ */
#define REMOVEJOB   'M'  /* 5  - from lprM */
#define CONNECTION  'X'  /* connection from remote host */

struct perm_check {
	char *user;				/* USER field from control file */
							/* or REMOTEUSER from command line */
	char *remoteuser;		/* remote user name sent on command line */
							/* or USER field if no command line */
	char *host;				/* HOST field from control file */
							/* or REMOTEHOST if no control file */
	char *remotehost;		/* remote HOST name making connection */
							/* or HOST if no control file */
	int	port;				/* port for remote connection */
	unsigned long ip;		/* IP address of HOST */
	unsigned long remoteip;	/* IP address of REMOTEHOST */
	char *printer;			/* printer name */
	int service;			/* first character service */
};

EXTERN struct perm_file Perm_file;
EXTERN struct perm_file Local_perm_file;
EXTERN struct perm_file Perm_queue;
EXTERN struct perm_check Perm_check;
EXTERN int Last_default_perm;	/* last default permission */

void dump_perm_file( char *title,  struct perm_file *perms );
void dump_perm_check( char *title,  struct perm_check *check );
char *perm_str( int val );
void Get_perms( char *name, struct perm_file *perms, char *path );
int Filter_perms( char *name, struct perm_file *perms, char *filter );

int Buffer_perms( struct perm_file *perms, char *file, char *buffer );
char *perm_str( int val );
void dump_perm_line( char *title,  struct perm_line *perms );
void dump_perm_file( char *title,  struct perm_file *perms );
void dump_perm_check( char *title,  struct perm_check *check );

void Init_perms_check();
void Free_perms( struct perm_file *perms );
int Perms_check( struct perm_file *perms, struct perm_check *check,
	struct control_file *cf );

#endif
