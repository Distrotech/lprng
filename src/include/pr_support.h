/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: printer_support.h
 * PURPOSE: printer support routines
 * "pr_support.h,v 3.4 1998/01/12 20:29:33 papowell Exp"
 **************************************************************************/

/*****************************************************************
 * Printer Support Routines
 *****************************************************************/

int Print_open( struct filter *filter,
	struct control_file *cf, int timeout, int interval, int grace,
	int max_try, struct printcap_entry *printcap_entry, int accounting_port );

void Print_close( struct control_file *cfp, int timeout );
void Print_kill( int signal );
void Print_abort( void );
int of_start( struct filter *filter );
int of_stop( struct filter *filter, int timeout, struct control_file *cfp );
int Print_string( struct filter *filter, char *str, int len, int timeout );
int Print_copy( struct control_file *cfp, int fd, struct stat *statb,
	struct filter *filter, int timeout, int status, char *file_name );

EXTERN struct filter Device_fd_info; 	/* output device or pipe */
EXTERN struct filter OF_fd_info;		/* of filter */
EXTERN struct filter XF_fd_info;		/* format filter */
EXTERN struct filter Pr_fd_info;		/* pr filter */
EXTERN struct filter Pc_fd_info;		/* Printcap filter */
EXTERN struct filter Pe_fd_info;		/* Permission filter */
EXTERN struct filter Af_fd_info;		/* Accounting filter */
EXTERN struct filter As_fd_info;		/* Accounting filter */
EXTERN char *Orig_Lp_device;	/* original LP, RH, RP from printcap */
EXTERN char *Orig_RemoteHost;
EXTERN char *Orig_RemotePrinter;
EXTERN int Accounting_port;				/* Accounting server port */

