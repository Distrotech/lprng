/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: printer_support.h
 * PURPOSE: printer support routines
 * "$Id: pr_support.h,v 3.3 1996/08/31 21:11:58 papowell Exp papowell $"
 **************************************************************************/

/*****************************************************************
 * Printer Support Routines
 *****************************************************************/

int Print_open( struct filter *filter,
	struct control_file *cf, int timeout, int interval, int grace,
	int max_try, struct pc_used *pc_used, int accounting_port );

void Kill_filter( struct filter *filter, int signal );
void Flush_filter( struct filter *filter );
int Close_filter( struct filter *filter, int timeout );
void Print_close( int timeout );
void Print_kill( int signal );
void Print_abort( void );
int of_start( struct filter *filter );
int of_stop( struct filter *filter, int timeout );
int Print_string( struct filter *filter, char *str, int len, int timeout );
int Print_copy( struct control_file *cfp, int fd, struct stat *statb,
	struct filter *filter, int timeout, int status );

int Setup_filter( int fmt, struct control_file *cf,
    char *filtername, struct filter *filter, int noextra,
	struct data_file *data_file );
int Make_filter( int key,
    struct control_file *cf,
    struct filter *filter, char *line, int noextra,
    int read_write, int print_fd, struct pc_used *pc_used,
	struct data_file *data_file, int account_port, int status_to_logger );

int Do_accounting( int end, char *command, struct control_file *cfp,
	int timeout, struct pc_used *pc_used, int filter_out );
void Setup_accounting( struct control_file *cfp, struct pc_used *pc_used );

char *Expand_command( struct control_file *cfp, char *bp, char *ep,
	char *s, int fmt, struct data_file *df );

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
EXTERN int DevNullFD;					/* DevNull File descriptor */
