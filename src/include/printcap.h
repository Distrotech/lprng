/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: printcap.h
 * PURPOSE:
 * $Id: printcap.h,v 3.5 1996/09/09 14:24:41 papowell Exp papowell $
 **************************************************************************/

#ifndef _PRINTCAP_H
#define _PRINTCAP_H
/*
 * See printcap.c for details on the follow datastructures
 * each printcap entry has the following information
 */

struct printcap{
	int name;		/* index in array of pointers to printcap names */
	int namecount;	/* number of names  */
	int ordered;	/* sorted or non-sorted */
	int options;	/* index in array of pointers to options */
	int optioncount;/* number of options */
	struct printcapfile *pcf; /* the printcap file */
};

/*
 * The printcap files are stored using the following data structures
 * The fields field points to the lines in the printcap files
 * The entry
 */

struct printcapfile{
	int init;					/* initialized */
	struct malloc_list pcs;		/* printcap entries */
	struct malloc_list lines;	/* lines in printcap files */
	struct malloc_list buffers;	/* list of buffers */
	struct malloc_list filters;	/* list of filters to use if no printer */
	struct dpathname dpath;		/* last extra one read in */
};
 
/*
 * The printcap variables are referenced either by a list or by
 * the following data structure which points to the list and has
 * its size
 */

struct pc_var_list {
	struct keywords *names;	/* list of printcap variables */
	int count;	/* number of elements in list */
};

struct pc_used {
	struct malloc_list pc_used;	/* list of all printcap entries */
	struct malloc_list pc_list;	/* linearized list of all the entries */
	int size;					/* size of allocated block */
	char *block;				/* environment variable form */
};

void dump_printcap( char *title,  struct printcap *pcf );
void dump_printcapfile( char *title,  struct printcapfile *pcf );
void Getprintcap( struct printcapfile *pcf, char *path, int nofilter );

char *Get_first_printer( struct printcapfile *pcf );

int Readprintcap( struct printcapfile *pcf, char *file, int fd,
	struct stat *statb );
struct printcap *Filterprintcap( char *name, struct printcapfile *pcf,
	int *index );
int Bufferprintcap( struct printcapfile *pcf, char *file, char *buffer );
void dump_printcap( char *title,  struct printcap *pc );
void dump_printcapfile( char *title,  struct printcapfile *pcf );
void Free_pcf( struct printcapfile *pcf );
char *Get_pc_option( char *str, struct printcap *pc );
struct printcap *Get_pc_name( char *str, struct printcapfile *pcf,
	int *index );
int Get_pc_vars( struct pc_var_list *keylist, struct printcap *pc,
	char *error, int errlen, struct pc_used *pc_used );
void Initialize_pc_vars( struct printcapfile *pcf,
	struct pc_var_list *vars, char *init );
char *Get_printer_vars( char *name, char *error, int errlen,
	struct printcapfile *printcapfile,
	struct pc_var_list *var_list, char *defval,
	struct pc_used *pc_used );
char *Get_printer_comment( struct printcapfile *printcapfile, char *name );
char *Search_option_val( char *option, struct pc_used *pc_used );
char *Linearize_pc_list( struct pc_used *pc_used, char *parm_name );
void Clear_pc_used( struct pc_used *pc_used );
char *Filter_read( char *name, struct malloc_list *list, char *filter );
char *Find_filter( int key, struct pc_used *pc_used );


int Setup_printer( char *name, char *error, int errlen, struct pc_used *pc_used,
	struct keywords *debug_list, int info_only,
	struct stat *control_statb );
void Check_remotehost( int cyclecheck );
void Check_pc_table( void );
void Fix_update( struct keywords *debug_list, int info_only );

int Make_identifier( struct control_file *cfp );

/* Print job on local printer */
int Print_job( struct control_file *cfp, struct pc_used *pc_used );

EXTERN struct printcapfile Printcapfile;     /* printcap file information */

void Clear_pc_used( struct pc_used *pc_used );
char *Linearize_pc_list( struct pc_used *pc_used, char *title );
void Sendmail_to_user( int status, struct control_file *cfp,
	struct pc_used *pc_used );


/***************************************************************************
 * Variables whose values are defined by entries in the printcap file
 *
 * We extract these from the printcap file when we need them
 ***************************************************************************/




EXTERN int Accounting_check; /* check accounting at start */
EXTERN char* Accounting_end;/* accounting at start (see also af, la, ar) */
EXTERN char* Accounting_file; /* name of accounting file (see also la, ar) */
EXTERN int Accounting_remote; /* write remote transfer accounting (if af is set) */
EXTERN char* Accounting_start;/* accounting at start (see also af, la, ar) */
EXTERN char* All_list;	 /* all printers list */
EXTERN char* Allow_class; /* allow these classes to be printed */
EXTERN int Always_banner; /* always print banner, ignore lpr -h option */
EXTERN int Auto_hold;	 /* automatically hold all jobs */
EXTERN int Backwards_compatible; /* backwards-compatible: job file format */
EXTERN int Backwards_compatible_filter; /* backwards-compatible: filter parameters */
EXTERN char* Banner_end;	 /* end banner printing program overrides bp */
EXTERN int Banner_last; /* print banner after job instead of before */
EXTERN char* Banner_line;	 /* short banner line sent to banner printer */
EXTERN char* Banner_printer; /* banner printing program (see ep) */
EXTERN char* Banner_start;	 /* start banner printing program overrides bp */
EXTERN int Baud_rate; /* if lp is a tty, set the baud rate (see ty) */
EXTERN char* Bounce_queue_dest; /* destination for bounce queue files */
EXTERN int Clear_flag_bits; /* if lp is a tty, clear flag bits (see ty) */
EXTERN int Clear_local_bits; /* if lp is a tty, clear local mode bits (see ty) */
EXTERN char* Comment_tag; /* comment identifying printer (LPQ) */
EXTERN int Connect_grace; /* grace period for reconnections */
EXTERN char* Control_dir; /* Control directory */
EXTERN int Cost_factor; /* cost in dollars per thousand pages */
EXTERN char* Default_logger_port;	/* default logger port */
EXTERN char* Default_logger_protocol;	/* default logger protocol */
EXTERN char* Default_priority;	/* default priority */
EXTERN char* Default_format;	/* default format */
EXTERN int FF_on_close; /* print a form feed when device is closed */
EXTERN int FF_on_open; /* print a form feed when device is opened */
EXTERN char* Form_feed; /* string to send for a form feed */
EXTERN char* Formats_allowed; /* valid output filter formats */
EXTERN int Forwarding_off; /* if true, no forwarded jobs accepted */
EXTERN char* IF_Filter; /* filter command, run on a per-file basis */
EXTERN char* Leader_on_open; /* leader string printed on printer open */
EXTERN int Local_accounting; /* write local printer accounting (if af is set) */
EXTERN char* Local_permission_file; /* additional permissions file for this queue */
EXTERN int Lock_it; /* lock the IO device */
EXTERN char* Log_file; /* error log file (servers, filters and prefilters) */
EXTERN char* Logger_destination; /* logger process host */
EXTERN int Long_number; /* long job number (6 digits) */
EXTERN char* Lp_device; /* device name or lp-pipe command to send output to */
EXTERN int Max_copies; /* maximum copies allowed */
EXTERN int Max_job_size; /* maximum job size (1Kb blocks, 0 = unlimited) */
EXTERN int Min_printable_count; /* minimum printable characters for printable check */
EXTERN char* Minfree; /* minimum space (Kb) to be left in spool filesystem */
EXTERN int NFS_spool_dir; /* spool dir is on an NFS file system (see rm, rp) */
EXTERN char* New_debug; /* debug level set for queue handler */
EXTERN int No_FF_separator; /* suppress form feeds separating multiple jobs */
EXTERN int Nonblocking_open; /* nonblocking open on io device */
EXTERN char* OF_Filter; /* output filter, run once for all output */
EXTERN int Page_length; /* page length (in lines) */
EXTERN int Page_width; /* page width (in characters) */
EXTERN int Page_x; /* page width in pixels (horizontal) */
EXTERN int Page_y; /* page length in pixels (vertical) */
EXTERN char* Pr_program; /* pr program for p format */
EXTERN int Read_write; /* open the printer for reading and writing */
EXTERN char* RemoteHost; /* remote-queue machine (hostname) (with rm) */
EXTERN char* RemotePrinter; /* remote-queue printer name (with rp) */
EXTERN int Rescan_time; /* max time between queue rescans */
EXTERN char* Routing_filter; /* filter to determine routing of jobs */
EXTERN int Save_when_done; /* save this job when done */
EXTERN char* Server_names; /* names of servers for queue (with ss) */
EXTERN char* Server_queue_name; /* name of queue that server serves (with sv) */
EXTERN int Send_data_first; /* send data files first */
EXTERN int Set_flag_bits; /* like `fc' but set bits (see ty) */
EXTERN int Set_local_bits; /* like `xc' but set bits (see ty) */
EXTERN int Short_banner; /* short banner (one line only) */
EXTERN char* Spool_dir; /* spool directory (only ONE printer per directory!) */
EXTERN char* Status_file; /* printer status file name */
EXTERN char* Stty_command; /* stty commands to set output line characteristics */
EXTERN int Suppress_copies; /* suppress multiple copies */
EXTERN int Suppress_header; /* suppress headers and/or banner page */
EXTERN char* Trailer_on_close; /* trailer string to print when queue empties */
EXTERN int Use_queuename;	/* put queuename in control file */
EXTERN int Use_identifier;	/* put identifier in control file */
EXTERN int Use_shorthost;	/* Use short hostname in control file information */
EXTERN char* Xlate_format;	/* translate format ids */


/*
 * printcap variables used by LPR and LPD
 * NOTE: Lists must be sorted alphabetically as the
 *  printcap variable lookup assumes this
 */

extern struct pc_var_list Pc_var_list;

/*
 * Default printcap variable values.  If there is not value here,
 * default is 0 or empty line
 */

extern char Default_printcap_var[];

#endif
