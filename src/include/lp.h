/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lp.h
 * PURPOSE: general type definitions that are used by all LPX facilities.
 * $Id: lp.h,v 3.3 1996/08/25 22:20:05 papowell Exp papowell $
 **************************************************************************/

#ifndef _LP_H_
#define _LP_H_ 1

#ifndef EXTERN
#define EXTERN extern
#endif

/*****************************************************************
 * get the portability information and configuration 
 *****************************************************************/

#include "portable.h"

/*****************************************************************
 * Global variables and routines that will be common to all programs
 *****************************************************************/

/*****************************************************************
 * Initialize() performs all of the system and necessary
 * initialization.  It was created to handle the problems
 * of different systems needing dynamic initialization calls.
 *****************************************************************/
void Initialize();

/*****************************************************************
 * strncpy/strncat(s1,s2,len)
 * The bsdi-compat versions of strncat and strncpy
 * only copy up to the end of string, and a terminating 0.
 *****************************************************************/

#define safestrncat( s1, s2 ) strncat(s1,s2,sizeof(s1)-strlen(s1)-1)
#define safestrncpy( s1, s2 ) strncpy(s1,s2,sizeof(s1));

/* VARARGS3 */
#ifdef HAVE_STDARGS
int	plp_snprintf (char *str, size_t count, const char *fmt, ...);
int	vplp_snprintf (char *str, size_t count, const char *fmt, va_list arg);
#  if !defined(HAVE_SETPROCTITLE)
void setproctitle( const char *fmt, ... );
#  endif
#else
int plp_snprintf ();
int vplp_snprintf ();
#  if !defined(HAVE_SETPROCTITLE)
void setproctitle();
#  endif
#endif

#define STR(X) #X

/* safestrcmp( char *, char *) - takes NULL pointers safely */
int safestrcmp( const char *, const char *);

/* safe version of strdup */
char *safestrdup (const char *p);
/* safe version of strdup that adds extra chars */
char *safexstrdup (const char *p, int extra );

/***************************************************************************
 * Pathname handling -
 * We define a data structure that contains a directory pathname, its
 *  length,  and the maximum allowed length.  This can then be modified
 *  as needed, copied, etc.
 * Add_path( struct dpathname *p, char *s1 ) - appends s1 to end of path
 * Add2_path( struct dpathname *p, char *s1, char *s2 ) - appends s1, s2
 * Clear_path( struct dpathname *p ) - clears the path name of files
 * Init_path( struct dpathname *p, char *s1 ) - sets that path
 ***************************************************************************/

struct dpathname{
	char pathname[MAXPATHLEN];
	int pathlen;
};

char * Add_path( struct dpathname *p, char *s1 );
char * Add2_path( struct dpathname *p, char *s1, char *s2 );
char * Clear_path( struct dpathname *p );
void Init_path( struct dpathname *p, char *s1 );
char * Expand_path( struct dpathname *p, char *s1 );

/* note: most of the time the Control directory pathname
 * and the spool directory pathname are the same
 * However, for tighter security in NFS mounted systems, you can
 * make them different
 */
EXTERN struct dpathname *CDpathname;	/* control directory pathname */
EXTERN struct dpathname *SDpathname;	/* spool directory pathname */

/***************************************************************************
 * tokens, parameters, and keywords
 * The struct token{} information is used to parse input lines and
 *    extract values.  The start field is the start of the token;
 *    the length field is the length.
 *
 * The struct keywords{} information is used to represent name/value
 *   pairs of information.
 *   The keyword field is a pointer to a string, i.e.- the keyword;
 *   The type field indicates the type of value we have:
 *     INTEGER is integer value,  STRING is string value,
 *     LIST is a list of values,  which is actually an array of pointers
 *     to strings.
 *   The variable field is the address of a variable associated with the
 *     keyword
 *   The maxval field holds the maximum value (length? magnitude?) of variable
 *   The flags field is for holding flags associated with the variable
 ***************************************************************************/

enum key_type { FLAG_K, INTEGER_K, STRING_K, LIST_K };

struct token {
	char *start;
	int length;
};

struct keywords{
    char *keyword;		/* name of keyword */
    enum key_type type;	/* type of entry */
    void *variable;		/* address of variable */
	int  maxval;		/* maximum length or value of variable */
	int  flag;			/* flag for variable */
};

int Crackline( char *line, struct token *token, int max ); /* crack line */

/*****************************************************************
 * Simple keyword to value translation:
 *  - strings such as 'yes', 'no'
 *  value is the address of the variable;
 *  returns 0 on failure, 1 on success
 *****************************************************************/

int Check_str_keyword( char *name, int *value );
int Check_token_keyword( struct token *name, int *value );
void Check_lpd_config( void );

/*****************************************************************
 * Debugging Information
 *****************************************************************/
#include "debug.h"

/*****************************************************************
 * BUFFER SIZES
 * 
 * A common size is 1024 bytes;
 * However,  it appears that this is overkill for most purposes.
 * 180 bytes appears to be satisfactory for a line,
 * 512 for a small buffer, 1024 for a large buffer
 *
 *****************************************************************/

#define LINEBUFFER 180
#define SMALLBUFFER 512
#define LARGEBUFFER 1024
 
/*****************************************************************
 * Signal handlers
 * plp_signal(int, plp_sigfunc_t);
 *  SIGALRM should be the only signal that terminates system calls
 *  with EINTR error code ; all other signals should NOT terminate
 *  them.  Note that the signal handler code should assume this.
 *  (Justin Mason, July 1994)
 *  (Ref: Advanced Programming in the UNIX Environment, Stevens)
 * plp_block_signals()
 *  blocks the set of signals used by PLP code; May need this in
 *  places where you do not want any further signals, such as termination
 *  code.
 * plp_unblock_signals() unblocks them.
 *****************************************************************/

typedef plp_signal_t (*plp_sigfunc_t)(int) ;
extern  plp_sigfunc_t plp_signal (int, plp_sigfunc_t);
extern void plp_block_signals ( void );
extern void plp_unblock_signals ( void );

/*
 *----------------------------------------------------------------------
 *
 * waitpid --
 *
 *      This procedure emulates the functionality of the POSIX
 *      waitpid kernel call, using the BSD wait3 kernel call.
 *      Note:  it doesn't emulate absolutely all of the waitpid
 *      functionality, in that it doesn't support pid's of 0
 *      or < -1.
 *
 * Results:
 *      -1 is returned if there is an error in the wait kernel call.
 *      Otherwise the pid of an exited or suspended process is
 *      returned and *statusPtr is set to the status value of the
 *      process.
 *
 * Side effects:
 *      None.
 * NOTE: you must call Setup_waitpid() first to enable the
 * signal handling mechanism
 *
 *----------------------------------------------------------------------
 */

pid_t plp_waitpid(pid_t pid, plp_status_t *statusPtr, int options);
void Setup_waitpid(void);

/*****************************************************************
 * General variables use in the common routines;
 * while in principle we could segragate these, it is not worth it
 * as the number is relatively small
 *****************************************************************/
extern char *Copyright[];	/* copyright info */
EXTERN char *Logname;		/* Username for logging */
EXTERN char *ShortHost;		/* Short hostname for logging */
EXTERN char *FQDNHost;		/* FQDN hostname */
EXTERN char *Printer;		/* Printer name for logging */
EXTERN char *Queue_name;	/* Queue name used for spooling */
EXTERN unsigned long HostIP;	/* local host ip address in network order */
EXTERN char *Forwarding;	/* Forwarding to remote host */
EXTERN char *Classes;		/* Classes for printing */
EXTERN int Destination_port;	/* Destination port for connection */
EXTERN char *Server_order;	/* order servers should be used in */

/* use this for printing lists */
void Printlist( char **m, FILE *f );

/*****************************************************************
 * Command line options and Debugging information
 * Getopt is a modified version of the standard getopt(3) command
 *  line parsing routine. See getopt.c for details
 *****************************************************************/

/* use this before any error printing, sets up program Name */
int Getopt( int argc, char *argv[], char *optstring );
extern int Optind, Opterr;
extern char *Optarg;
extern char *Name;			/* program name */

/*****************************************************************
 * option checking assistance functions, see getparms.c for details
 *****************************************************************/
void Dienoarg(int option);
void Check_int_dup (int option, int *value, char *arg, int maxvalue);
void Check_str_dup(int option, char **value, char *arg, int maxlen );
void Check_dup(int option, int *value);

/*****************************************************************
 * Get Fully Qualified Domain Name (FQDN) host name
 * The 'domain' information is appended to the host name only in
 * desperation cases and only if it is non-null.
 * If the FQDN cannot be found,  then you fall back on the host name.
 *****************************************************************/
void Get_local_host( void ); /* get local hostname */
char *Get_local_fqdn( char *domain ); /* get local hostname */
char *Find_fqdn( char *shorthost, char *domain ); /* get FQDN */
/* get IP in network order */
unsigned long Find_ip( char *shorthost );

/*****************************************************************
 * Get User Name
 * Does a lookup in the password information,
 *   or defaults to 'nobody'
 *****************************************************************/
char *Get_user_information( void );
int Root_perms( void );

/***************************************************************************
 * Dynamic memory allocation control
 * the idea is that you allocate lists or blocks,  and then you keep track
 * of how much you have allocated
 ***************************************************************************/

struct malloc_list{
	char **list;	/* array of pointers to allocated blocks */
	int count;		/* number of entries in list */
	int max;		/* max number of entries in list */
	int size;		/* size of each entry in list: error checking */
};

void extend_malloc_list( struct malloc_list *buffers, int element, int incr );
char *add_buffer( struct malloc_list *buffers, int len );
void clear_malloc_list( struct malloc_list *buffers, int free_list );

/* list of control files for status information */
EXTERN struct malloc_list C_files_list;

/**************************************************************************
 *      Control file format
 *      First character is kind of entry, remainder of line is
 *          the argument.
 *
 *		1 -- "R font file" for troff -ignore
 *		2 -- "I font file" for troff -ignore
 *		3 -- "B font file" for troff -ignore
 *		4 -- "S font file" for troff -ignore
 *		C -- "class name" on banner page (priority)
 *		D -- "Date" job submitted
 *		H -- "Host name" of machine where lpr was done
 *		I -- "indent" amount to indent output
 *		J -- "job name" on banner page
 *		L -- "literal" user's name to print on banner
 *		M -- "mail" to user when done printing
 *		N -- "name" of file (used by lpq)
 *		P -- "Person" user's login name
 *		Q -- "Queue Name" used for spooling
 *		R -- account id  for charging
 *		U -- "unlink" name of file to remove after we print it
 *		W -- "width" page width for PR
 *		Z -- xtra options to filters
 *
 *		Lower case letters are formats
 *		f -- "file name" name of text file to print
 *		l -- "file name" text file with control chars
 *		p -- "file name" text file to print with pr(1)
 *		t -- "file name" troff(1) file to print
 *		n -- "file name" ditroff(1) file to print
 *		d -- "file name" dvi file to print
 *		g -- "file name" plot(1G) file to print
 *		v -- "file name" plain raster file to print
 *		c -- "file name" cifplot file to print
 *
 */


/*****************************************************************
 * Control File Information
 *
 *  Patrick Powell Sat Apr  8 08:04:28 PDT 1995
 *
 * The struct data_file{} is used to record the data files in
 * a print job.
 *  - name field is the 'N' option in the control file
 *  - datafile field is the name of the data file
 *  - size field is the size (in bytes) of the data file
 *
 * The struct control_file{} is used to record control file information
 * about a print job.   This data structure is meant to be used
 * in most of the LPR software,  and has a slight amount of overkill
 * in terms of its facilities.
 * 1. the name and stat fields are used when determining job order
 *    and priority.
 * 2. the options and option_count fields are used when parsing the
 *    control file, and determining the various options in the file.
 * 3. the capoptions[] array points to the various options starting
 *    with capital letters and the digitoptions with digit letters
 * 4. the datafile[] field points to an array of data file structures,
 *    each of which corresponds to a data file in a job.
 * 5. the buffer field points to a block of memory which holds the
 *    control file.
 *
 * Commentary:
 *  It is a lot easier to simply read a control file into memory,
 *  then parse it and put pointers to the various things that are
 *  needed.  Note that when you do a scan of a directory for job
 *  information,  you will need to read the control file anyways.
 *  Thus,  if you simply read them into memory,  you will have
 *  all of the information available,  and only need to read them
 *  once until they change.
 *
 *  Rescanning the spool directory becomes almost trivial;  you simply
 *  stat each file in order,  check to see if it is in the list,
 *  and check the stat information.  If the entry is not in the list
 *  then you add it.  If an entry has changed or is not found on the
 *  scan,  then you remove the current entry and add it again.
 *
 *  When transferring a job (forwarding) to another LPD server,  you
 *  may have to reformat the information in the control file. Note
 *  that this usually consists of reordering the fields and/or
 *  eliminating some fields.  By having a cracked version of this
 *  file available you make life simpler.
 *
 *  Note that when you send the control file,  you will have to
 *  calculate the lengths of all of the fields to send.  You can
 *  do this by rescanning the options[] array and using strlen()
 *  on each of the lines in the control file.  Note that this
 *  has to be done only once,  so it is not an insurmountable overhead.
 *
 *  Critics of this method point out that you will now need to do a
 *  'line by line' write to the socket rather than doing a block write;
 *  many of these forget that you are dealing with TCP/IP, which will
 *  invoke the Van Jacobsen and Slow Start algorithm,  and transfer
 *  maximum size blocks (see RFC1720, RFC1722, and the references
 *  listed in them).  So there is effectively no impact on the TCP/IP
 *  throughput.  If you are really worried about overhead,  you can
 *  play games with dup(), fdopen(),  and fprintf(),  and use the
 *  STDIO library buffering functions.   I have experimented with
 *  this and found that there was only 3% performance change in the
 *  PLP performance;  apparently the cost of calls to dirent() overwhelm the
 *  calls to 'write',  so you really don't gain much for the huge effort
 *  in coding.
 *
 *  One of the interesting effects of this strategy is to cause a change
 *  in the way that status information for queues is obtained.  In
 *  most LPD implemtations,  the LPD server will fork a process that
 *  will then scan the spool queues.  Each request has a rather
 *  substantial overhead,  and has led to some rather nasty denial of
 *  service network attacks.  A different method would be to allow
 *  the LPD daemon to keep the information,  and then when a
 *  request arrives to rescan the spool queues.  Once the spool queues
 *  are rescanned,  then the LPD daemon would fork a process to handle
 *  the reporting of the information.   This would make the LPD
 *  daemon process large (in terms of memory),  but reduce the load
 *  on systems which are suffering from denial of service attacks.
 *  An alternative to this method would be to put a throttle on the
 *  number of processes allowed to be created for reporting spool queue
 *  information.
 *****************************************************************/

struct data_file {
	char *openname;		/* Data file name- dfXXXhost, tempfile, userfile */
	char *transfername;		/* transfer information, i.e.- dfXXXhost */
	char *Ninfo;		/* 'N' Option information: userfile, (stdin) */
	char *Uinfo;		/* 'U' Option information */
	int format;			/* format */
	int fd;				/* file descriptor for reading */
	int line;			/* file line in control file 'lines' array */
	int uline;			/* U line in control file 'lines' array */
	int linecount;		/* numbers of lines for this entry */
	struct stat statb;	/* stat information */
	int flags;			/* flags */
#define PIPE_FLAG		0x01	/* pipe */
	int copies;			/* if non-zero, there are more copies of this entry */
};


struct control_file {
	char *name;				/* name of control file */
	int  number;			/* job number */
	int  number_len;		/* number of digits in job number */
	int  jobsize;			/* size of job in bytes */
	char *filehostname;		/* hostname part of the control file name */
	char *realhostname;		/* hostname in the control file */
	struct stat statb;		/* stat of control file information */ 
	struct stat hstatb;		/* stat of hold file information */ 
	int active;				/* pid of server if it is active */
	int held_class;			/* held class for printing */
	int flags;				/* flags used to indicate an error or status */
#define BAD_FLAG       0x01
#define OLD_FLAG       0x02
#define ACTIVE_FLAG    0x04
#define FOUND_FLAG     0x08
	long priority_time;		/* priority time */
	long hold_time;			/* hold time */
	long remove_time;		/* removal time */
	long done_time;			/* finished time */
	long move_time;			/* time when moved */
	long routed_time;		/* routed to destinations */
	char *hold_file;		/* pathname of hold file */
	char *spool_dir;		/* pathname of spool dir */
	char *control_dir;		/* pathname of control dir */
	char *identifier;	/* identifier */
	char ident_buffer[LINEBUFFER];	/* identifier */
	char redirect[LINEBUFFER];	/* redirection */
	char error[LINEBUFFER];	/* error message for bad control file */
	int print_attempts;		/* number of print attempts */
	int  control_info;		/* number of lines before data file information
								in control file */
	char *capoptions[26]; 	/* capital letter options */
	char *digitoptions[10];	/* digit options */
	char *cf_info;			/* control file image */
	char *cf_copy;			/* copy of control file with CONTROL_FILE= */
	struct malloc_list control_file;	/* allocated area for
											in memory control file
											and name */
	struct malloc_list data_file;		/* allocated area for data file list */
	struct malloc_list info_lines;		/* allocated area for line list */
	struct malloc_list status_file;		/* allocated area for status file */
	struct malloc_list destination_lines;	/* allocated area for destination lines */
	struct malloc_list destination_list;	/* allocated area for destination list */
};

struct destination {
	char destination[LINEBUFFER];	/* destination of job */
	char identifier[LINEBUFFER];	/* new identifier of job */
	char error[LINEBUFFER];			/* error message for bad control file */
	int arg_start;					/* option lines to add/change */
	int arg_count;					/* number of lines */
	int copies;						/* number of copies */
	int copy_done;					/* copies done */
	int status;						/* status of copy */
	int hold;						/* held */
	int active;						/* active */
	int done;						/* active */
	int attempt;					/* attempt */
	int ignore;						/* ignore */
};

int Parse_cf( struct dpathname *dpath,
    struct control_file *cf, int check_df );

int Fix_control( struct control_file *cf, char **line );
char *Add_job_line( struct control_file *cf, char *str );
char *Prefix_job_line( struct control_file *cf, char *str );

/*
 * spool queue status reporting:
 * array of pointers to a set of control files,  one per job
 */
void Scan_queue(  int check_df );
int Remove_job( struct control_file *cfp );

/*
 * clear a control_file structure
 */
void Clear_control_file( struct control_file *cf );

/***************************************************************************
 * ACKs from the remote host on job transfers
 ***************************************************************************/
#define ACK_SUCCESS 0   /* succeeded; delete remote copy */
#define ACK_STOP_Q  1   /* failed; no spooling to the remote queue */
#define ACK_RETRY   2   /* failed; retry later */
#define ACK_FAIL    3   /* failed; bad job */

 
/*
 * macros for the names and maximum lengths for the various fields
 */

#define M_DEFAULT		131				/* default limit on line length */
#define IDENTIFIER capoptions['A'-'A']	/* LPRng - unique job ID */
#define M_IDENTIFIER	131				/* default limit on line length */
#define CLASSNAME capoptions['C'-'A']	/* RFC: 31 char limit */
#define M_CLASSNAME		31
#define FROMHOST  capoptions['H'-'A']	/* RFC: 31 char limit */
#define M_FROMHOST		31
#define INDENT    capoptions['I'-'A']	/* RFC: number */
#define M_INDENT		31
#define JOBNAME   capoptions['J'-'A']	/* RFC: 99 char limit */
#define M_JOBNAME		99
#define BNRNAME   capoptions['L'-'A']	/* RFC: ?? char limit */
#define M_BNRNAME		31
#define FILENAME  capoptions['N'-'A']	/* RFC: 131 char limit */
#define M_FILENAME		131
#define MAILNAME  capoptions['M'-'A']	/* RFC: ?? char limit */
#define M_MAILNAME		131
#define M_NAME			131				/* RFC: 131 char limit on 'N' entries */
#define LOGNAME   capoptions['P'-'A']	/* RFC: 31 char limit */
#define M_LOGNAME		31
#define QUEUENAME capoptions['Q'-'A']	/* PLP: 31 char limit */
#define M_QUEUENAME		31
#define ACCNTNAME capoptions['R'-'A']	/* PLP: info for accounting: 131 */
#define M_ACCNTNAME		131
#define SLINKDATA capoptions['S'-'A']	/* RFC: number " " number */
#define M_SLINKDATA		131
#define PRTITLE   capoptions['T'-'A']	/* RFC: 79 char limit */
#define M_PRTITLE		79
#define UNLNKFILE capoptions['U'-'A']	/* RFC: flag */
#define M_UNLNKFILE		131
#define PWIDTH    capoptions['W'-'A']	/* RFC: number */
#define M_PWIDTH		31
#define ZOPTS     capoptions['Z'-'A']	/* PLP */
#define M_ZOPTS			131
#define FONT1     digitoptions['1'-'0']	/* RFC: ?? char limit */
#define FONT2     digitoptions['2'-'0']	/* RFC: ?? char limit */
#define FONT3     digitoptions['3'-'0']	/* RFC: ?? char limit */
#define FONT4     digitoptions['4'-'0']	/* RFC: ?? char limit */
#define M_FONT		131

/*****************************************************************
 * Command file option checking:
 *  We specify the allowed order of options in the control file.
 *  Note that '*' is a wild card
 *   Berkeley-            HPJCLIMWT1234
 *   PLP-                 HPJCLIMWT1234*
 *****************************************************************/

/*****************************************************************
 * Connection support code
 *  See extensive comments in link_support.c
 *****************************************************************/

int Link_setreuse( int sock );
int Link_getreuse( int sock );
int Link_dest_port_num( void );
int Link_listen(void);
int Link_open(char *host, int retry, int timeout );
int Link_open_type(char *host, int retry, int timeout, int port, int connection_type );
void Link_close( int *sock );
int Link_ack( char *host, int *socket, int timeout, int sendc, int *ack );
int Link_send ( char *host, int *socket, int timeout,
    int ch, char *send, int lf, int *ack );
int Link_copy( char *host, int *socket, int readtimeout, int writetimeout,
    char *src, int fd, int count);
int Link_get( char *host, int *socket, int timeout, char *dest, FILE *fp );
int Link_line_read(char *host, int *socket, int timeout,
      char *buf, int *count );
int Link_read(char *host, int *socket, int timeout,
      char *buf, int *count );
int Link_file_read(char *host, int *socket, int readtimeout, int writetimeout,
      int fd, int *count, int *ack );
const char *Link_err_str (int n);
const char *Ack_err_str (int n);

/* error codes for return values */

#define LINK_OPEN_FAIL		-1		/* open or connect */
#define LINK_TRANSFER_FAIL	-2		/* transfer failed */
#define LINK_ACK_FAIL		-3		/* non-zero ACK on operation */
#define LINK_FILE_READ_FAIL	-4		/* read from a file failed */
#define LINK_LONG_LINE_FAIL	-5		/* a line was too long to read */
#define LINK_BIND_FAIL		-6		/* cannot bind to port */
#define LINK_PERM_FAIL		-7		/* permission failure, remove job */

/*****************************************************************
 * Status reporting function
 *  Note that this can be a logging routine to STDOUT or to
 *  a log file depending on system that is using it.
 *****************************************************************/

#define NORMAL (0)
#ifdef HAVE_STDARGS
void setstatus( struct control_file *cfp, char *fmt, ... );
void setmessage( struct control_file *cfp, char *msg, char *fmt, ... );
#else
void setstatus( va_alist );
void setmessage( va_alist );
#endif
void send_to_logger( char *msg );


/*****************************************************************
 * File open functions
 * These perform extensive checking for permissions and types
 *  see fileopen.c for details
 *****************************************************************/
int Checkread( char *file, struct stat *statb );
int Checkwrite( char *file, struct stat *statb, int rw, int create, int del );
int Open_path_file( char **pathlist, char *file, int nofind, char **name,
	struct stat *statb );

/*****************************************************************
 * LPD Protocol Information
 *****************************************************************/
/*****************************************************************
 * Request types
 * A request sent to the LPD daemon has the format:
 * \Xprinter [options],  where \X is a single character or byte value.
 * The following are the values and commands
 *****************************************************************/

#define REQ_START   1   /* start printer */
#define REQ_RECV    2   /* transfer a printer job */
#define REQ_DSHORT  3   /* print short form of queue status */
#define REQ_DLONG   4   /* print long form of queue status */
#define REQ_REMOVE  5   /* remove jobs */
#define REQ_CONTROL 6   /* do control operation */


#define CONTROL_FILE 2       /* \2<count> <cfname>\n */
#define DATA_FILE    3       /* \3<count> <dfname>\n */

/*****************************************************************
 * LPD Spool Queue Files
 *****************************************************************/
/* any created file should have these permissions */
int Lockf (char *filename, int *lock, int *create, struct stat *statb );
int Lock_fd( int fd, char *filename, int *lock, struct stat *statb );
int Do_lock (int fd, const char *filename, int block );
int LockDevice(int fd, char *devname);

/*****************************************************************
 * Serial line configuration
 *****************************************************************/
void Do_stty( int fd );

/*****************************************************************
 * LPD Control file information
 * control file has the format:
 *   spooling on/off
 *   printing on/off
 *****************************************************************/

EXTERN int Printing_disabled;	/* No printing */
EXTERN int Spooling_disabled;	/* No spooling */

/*****************************************************************
 * char *Clean_name( char *s )
 *  scan input line for non-alphanumeric, _ characters
 *  return pointer to bad character found
 * char *Clean_FQDNname( char *s )
 *  scan input line for non-alphanumeric, _, -, @ characters
 *
 * char *Find_meta( char *s )
 *  scan input line for meta character or non-printable character
 *  return pointer to bad character found
 * void Clean_meta( char *s )
 *  scan input line for meta character or non-printable character
 *  and replace with '_'
 *
 * Check_format( int kind, char *name )
 *  check to see that control and data file names match;
 *  if kind = 0, reset match check
 *****************************************************************/
char *Clean_name( char *s );
char *Clean_FQDNname( char *s );
char *Find_meta( char *s );
void Clean_meta( char *s );
int Check_format( int kind, char *name, struct control_file *cfp );



/***************************************************************************
 * Send_statusrequest: open a connection for status information
 ***************************************************************************/

void Send_statusrequest( char *remoteprinter, char *host, int format,
	char **options,	int connect_timeout, int transfer_timeout,
	int output );

/***************************************************************************
 * Send_lprmrequest: open a connection for lprm action
 ***************************************************************************/

void Send_lprmrequest( char *remoteprinter, char *host, char *logname,
	char **options,	int connect_timeout, int transfer_timeout,
	int output );




/*****************************************************************
 * Pipe set to communicate with the LPD process from some poor
 * low level process.  The idea here is that if the LPD process
 * needs to be informed of some action,  it will be sent a command
 * from the low level process.  The only commands sent should be
 * start printer.
 *****************************************************************/
void Process_jobs( int *socket, char *input, int maxlen );


#include "errorcodes.h"

/*****************************************************************
 * Process forking support 
 * Idea: we generate the command line for the process from pieces;
 *   we check to make sure that each piece has no problems.
 * The struct args {} data structure is used to set this up.
 *****************************************************************/

struct filter {
	int pid;		/* PID of filter process */
	int input;	 	/* input file descriptor */
	int output;	 	/* output file descriptor of filter */
	char *cmd;		/* command string - malloc */
	char *copy;		/* copy of command string - malloc */
	struct malloc_list args;		/* argument list */
};


int dofork( void );	/* fork and create child as process group leader */

void Write_pid( int fd, int pid, char *str );
int Read_pid( int fd, char *buffer, int len );

/*****************************************************************
 * Error Printing
 * errormsg.c file
 *****************************************************************/

EXTERN int Errorcode;		/* Exit code for an error */
EXTERN int Interactive;		/* Interactive or Server mode */
EXTERN int Verbose;			/* Verbose logging mode */
EXTERN int Echo_on_fd;		/* Echo Error on fd */
EXTERN int Syslog_fd;		/* syslog device if no syslog() facility */
EXTERN char *RemoteHost;	/* Remote Host */
EXTERN char *RemotePrinter;	/* Remote Printer */
EXTERN int Status_fd;		/* status file file descriptor */

/*****************************************************************
 * Routing Support
 *****************************************************************/

EXTERN struct destination *Destination;	/* Destination for routing */


#include "errormsg.h"

#define malloc_or_die(x,y)  if (((x) = malloc((unsigned) y)) == 0) { \
	Malloc_failed((unsigned)y); \
}

#define roundup_2(x,y) ((x+((1<<y)-1))&~((1<<y)-1))

/*****************************************************************
 * Handy utilty functions: write (bombproof) to a file descriptor
 *****************************************************************/

int Write_fd_str( int fd, char *msg );
int Write_fd_len( int fd, char *msg, int len );


/***************************************************************************
 * char *Get_realhostname( struct control_file *cfp )
 *  Find the real hostname of the job
 *  sets the cfp->realhostname entry as well
 ***************************************************************************/
char *Get_realhostname( struct control_file *cfp );

/***************************************************************************
 * void trunc_str( char *s ) - truncate trailing whitespace unless escaped
 ***************************************************************************/
void trunc_str( char *s );

#endif
