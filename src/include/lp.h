/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lp.h
 * PURPOSE: general type definitions that are used by all LPX facilities.
 * $Id: lp.h,v 3.30 1998/01/18 00:10:32 papowell Exp papowell $
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

/*****************************************************
 * Internationalisation of messages, using GNU gettext
 *****************************************************/

#if HAVE_LOCALE_H
# include <locale.h>
#endif

#if ENABLE_NLS
# include <libintl.h>
# define _(Text) gettext (Text)
#else
# define _(Text) Text
#endif
#define N_(Text) Text

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
#else
int plp_snprintf ();
int vplp_snprintf ();
#endif


/* VARARGS3 */
#if !defined(HAVE_SETPROCTITLE) || !defined(HAVE_SETPROCTITLE_DEF)
#  ifdef HAVE_STDARGS
void setproctitle( const char *fmt, ... );
#  else
void setproctitle();
#  endif
#endif

#define STR(X) #X

/***************************************************************************
 * Pathname handling -
 ***************************************************************************/

struct dpathname{
	char pathname[MAXPATHLEN];
	int pathlen;
};
EXTERN struct dpathname *Tempfile;		/* temporary file */

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
						/* also used to suppress clearing value */
	int  flag;			/* flag for variable */
	char *default_value;		/* default value */
};
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
 * General variables use in the common routines;
 * while in principle we could segragate these, it is not worth it
 * as the number is relatively small
 *****************************************************************/
extern char *Copyright[];	/* copyright info */
EXTERN int Init_done;		/* initialization done */
EXTERN char *Logname;		/* Username for logging */
/* EXTERN char *Host;			/ * Hostname */
EXTERN char *ShortHost;		/* Short hostname for logging */
EXTERN char *FQDNHost;		/* FQDN hostname */
EXTERN char *Localhost;		/* Name to be used for localhost lookup */
EXTERN char *Printer;		/* Printer name for logging */
EXTERN char *Orig_printer;	/* Printer name for logging */
EXTERN char *Queue_name;	/* Queue name used for spooling */
EXTERN char *Forwarding;	/* Forwarding to remote host */
EXTERN char *Classes;		/* Classes for printing */
EXTERN int Destination_port;	/* Destination port for connection */
EXTERN char *Server_order;	/* order servers should be used in */
EXTERN int Max_servers;		/* max servers currently active */
EXTERN int Max_connect_interval;	/* maximum connect interval */
EXTERN int Max_servers_active;	/* maximum number of servers active */
EXTERN int IPV6Protocol;	/* IPV4 or IPV6 protocol */
extern int AF_Protocol;		/* AF protocol */
EXTERN char* Kerberos_service;	/* kerberos service */
EXTERN char* Kerberos_keytab;	/* kerberos keytab file */
EXTERN char* Kerberos_life;	/* kerberos lifetime */
EXTERN char* Kerberos_renew;	/* kerberos newal time */
EXTERN char* Kerberos_server_principle;	/* kerberos server principle */
EXTERN char* Reverse_lpq_status;	/* change lpq format when from host */
EXTERN char* Return_short_status;	/* return short status */
EXTERN int Short_status_length;	/* short status length */

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
	char openname[MAXPATHLEN];		/* full pathname of datafile */
	char transfername[LINEBUFFER];	/* transfername of data file - short */
	char original[LINEBUFFER];	/* original name of data file - short */
	char Ninfo[LINEBUFFER];	/* 'N' Option information: userfile, (stdin) */
	char Uinfo[LINEBUFFER];	/* 'U' Option information */
	int format;			/* format */
	int fd;				/* file descriptor for reading */
	struct stat statb;	/* stat information */
	int d_flags;			/* flags */
	int found;			/* job found in control file and sent */
#define PIPE_FLAG		0x01	/* pipe */
	int copies;			/* if non-zero, this is the transfered copy */
	off_t offset;		/* offset from the start of the block format file */
	off_t length;		/* length file */
};


struct hold_file {
	int  attempt;			/* number of print attempts */
	int  not_printable;		/* printable status */
	int  held_class;		/* held class for printing */
	long priority_time;		/* priority time */
	long hold_time;			/* hold time */
	long remove_time;		/* removal time */
	long done_time;			/* finished time */
	long routed_time;		/* routed to destinations */
	long active_time;		/* time started printing */
	int  server;			/* pid of server if it is active */
	int  subserver;			/* pid of server if it is active */
	char redirect[LINEBUFFER];	/* redirection */
};

struct control_file {
	char openname[MAXPATHLEN];	/* full pathname of control file - used in open/close */
	char transfername[LINEBUFFER];	/* control file - used in transfers  */
	char original[LINEBUFFER];	/* original file name - received from LPR or LPD */
	int  number;			/* job number */
	int  number_len;		/* number of digits in job number */
	int  max_number;		/* maximum job number */
	int  recvd_number;		/* original number of job */
	int  recvd_number_len;	/* original digits of number of job */
	int  jobsize;			/* size of job in bytes */
	int  copynumber;		/* copy number */
	char filehostname[LINEBUFFER];	/* hostname part of the control file name */
	struct stat statb;		/* stat of control file information */ 
	char hold_file[MAXPATHLEN];	/* full pathname of hold file */
	struct stat hstatb;		/* stat of hold file information */ 
	int priority;			/* priority from the job ID */
	int flags;				/* flags used to indicate an error or status */
#define LAST_DATA_FILE_FIRST 0x2
	int found;				/* found in the directory structure */
	int remove_on_exit;		/* if remove job files on exit */
	char auth_id[LINEBUFFER];	/* authenticated information */
	char forward_id[LINEBUFFER];
	char authtype[LINEBUFFER];	/* authentication type */
#define BAD_FLAG       0x01
#define OLD_FLAG       0x02
#define ACTIVE_FLAG    0x04
#define FOUND_FLAG     0x08
	/* block format for files */
	int block_format;		/* block format flag */
	off_t start;			/* offset from start */
	off_t len;				/* length of the control file */
	char identifier[LINEBUFFER];/* identifier */
	char *orig_identifier;	/* original identifier in control file */
	char error[LINEBUFFER];	/* error message for bad control file */
	int  control_info;		/* number of lines before data file information
								in control file */
	char *capoptions[26]; 	/* capital letter options */
	char *digitoptions[10];	/* digit options */
	char *cf_info;			/* control file image */
	/* from the hold file */
	int   destination_info_start;	/* destination information starts here */
	struct hold_file hold_info;
	struct malloc_list control_file_image;	/* control file name and image (buffers) */
	struct malloc_list data_file_list;		/* allocated area for data file list */
	struct malloc_list control_file_lines;		/* allocated area for line list */
	struct malloc_list control_file_copy;	/* allocated area for control file copy */
	struct malloc_list control_file_print;	/* printable copy of control file */
	char * hold_file_info;					/* allocated area for hold file */
	struct malloc_list hold_file_lines;	/* allocated area for hold file lines */
	struct malloc_list hold_file_print;		/* printable copy of control file */
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
	int server;						/* server */
	int subserver;					/* subserver */
	int done;						/* done */
	int attempt;					/* attempt */
	int ignore;						/* ignore */
	int sequence_number;			/* sequence number to add to job */
	int not_printable;
	int priority;					/* new priority from control file */
};

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
#define M_DATE			31
#define DATE      capoptions['D'-'A']	/* LPRng - date job started */
#define M_CLASSNAME		1024
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
/* error codes for return values */

#define LINK_OPEN_FAIL		-1		/* open or connect */
#define LINK_TRANSFER_FAIL	-2		/* transfer failed */
#define LINK_ACK_FAIL		-3		/* non-zero ACK on operation */
#define LINK_FILE_READ_FAIL	-4		/* read from a file failed */
#define LINK_LONG_LINE_FAIL	-5		/* a line was too long to read */
#define LINK_BIND_FAIL		-6		/* cannot bind to port */
#define LINK_PERM_FAIL		-7		/* permission failure, remove job */
#define LINK_ECONNREFUSED	-8		/* connection refused */
#define LINK_ETIMEDOUT     	-9		/* connection timedout */

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
#define REQ_BLOCK   7   /* transfer a block format print job */
#define REQ_SECURE  8   /* secure command transfer */
#define REQ_VERBOSE 9   /* verbose status information */


#define ABORT_XFER   1       /* \1\n - abort transfer */
#define CONTROL_FILE 2       /* \2<count> <cfname>\n */
#define DATA_FILE    3       /* \3<count> <dfname>\n */

/*****************************************************************
 * The expanded printcap entry is created by running through
 * all of the printcap entries and extracting the required
 * information from them.  The raw entries are extracted from
 * the printcap files themselves;  the expanded entries are
 * the  combined information.
 *  Note that the raw_list is the address of the pointer to the
 *  start of the raw list information;  nameindex and optionindex
 *  are the indices into this raw list.  When the printcap
 *  is expanded,  the namelines and line entries will be used
 *  to hold the array of pointers to names and entries.  This is
 *  done because the original raw list can grow and may need to
 *  be relocated in memory.  Once it has been relocated, the
 *  raw_list value is set to 0.
 *****************************************************************/

struct printcap_entry {
	char **names;		/* start of array of printcap names */
	int namecount;		/* number of names  */
	char **options;		/* start of array of options */
	int optioncount;	/* number of options */
	char *key;			/* key for checking spool dirs */
	int status_done;	/* status reported */
	int checked;		/* printcap already checked */
	struct malloc_list namelines;	/* names and options in list */
	struct malloc_list lines;	/* names and options in list */
		/* count is number of lines in entry */
		/* list is an array of char */
};

/*
 * each printcap file is read into a buffer, and then
 * parsed for lines and printcap entries.  The include
 * processing is done at this point;  the file and 
 * include file data is stored in the files data structure
 * 
 * The file data is scanned,  and broken down into lines and
 * then into entries.  The entries field will be an array of
 * pointers to the first character in each field.
 *
 * After the data file has been scanned for entries, it
 * will be rescanned for printcap entries.  Each one of
 * these will be put into the printcaps field.
 */

struct file_entry {
	int initialized;			/* initialized flag */
	struct malloc_list files;	/* storage for file data and include */
		/* list is an array of char * (must be freed) */
	struct malloc_list entries;	/* storage for entries */
		/* list is an array of char ** */
	struct malloc_list filters;	/* storage for filters */
		/* list is an array of char * (must be freed ) */
	struct malloc_list printcaps;	/* storage for printcaps */
		/* list is an array of struct printcap */
	struct malloc_list expanded_str;	/* storage for expanded strings */
};

/*****************************************************************
 * LPD Control file information
 * control file has the format:
 *   spooling on/off
 *   printing on/off
 *****************************************************************/

EXTERN int Printing_disabled;	/* No printing */
EXTERN int Spooling_disabled;	/* No spooling */


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
	struct dpathname exec_path;	/* pathname */
	struct malloc_list args;		/* argument list */
	struct malloc_list env;			/* environment variables */
	struct malloc_list envp;		/* pointers */
};


/*****************************************************************
 * Error Printing
 * errormsg.c file
 *****************************************************************/

EXTERN int Errorcode;		/* Exit code for an error */
EXTERN int Interactive;		/* Interactive or Server mode */
EXTERN int Verbose;			/* Verbose logging mode */
EXTERN int Echo_on_fd;		/* Echo Error on fd */
EXTERN int Syslog_fd;		/* syslog device if no syslog() facility */
EXTERN int Status_fd;		/* status file file descriptor */
EXTERN int Logger_fd, Mail_fd; /* logger and mail sockets */

/*****************************************************************
 * Routing Support
 *****************************************************************/

EXTERN struct destination *Destination;	/* Destination for routing */

#define malloc_or_die(x,y)  if (((x) = malloc((unsigned) y)) == 0) { \
	Malloc_failed((unsigned)y); \
}

#define roundup_2(x,y) ((x+((1<<y)-1))&~((1<<y)-1))

/***************************************************************************
 * Subserver information
 *  used by LPQ and LPD to start subservers
 ***************************************************************************/

struct server_info{
    char name[LINEBUFFER];  /* printer name for server */  
    pid_t pid;          /* pid of server processes */ 
	int need_to_start;	/* does it need to start */
    int status;         /* last exit status of the server process */
    int initial;        /* 1 = initial c leanup server */
	int printing_disabled;	/* printing disabled */
    unsigned long time; /* time it terminated */ 
	char transfername[LINEBUFFER];	/* transfername of file */
	struct dpathname spooldir;	/* spool directory */
	struct dpathname controldir;	/* control directory */
	int check_for_idle;	/* check for an idle condition */ 
};

/*****************************************************************
 * Include files that are so common they should be included anyways
 *****************************************************************/

#include "bsd-compat.h"
#include "errormsg.h"
#include "debug.h"
#include "utilities.h"

/***************************************************************************
 * Variables whose values are defined by entries in the printcap file
 *
 * We extract these from the printcap file when we need them
 ***************************************************************************/

/*
 * note: most of the time the Control directory pathname
 * and the spool directory pathname are the same
 * However, for tighter security in NFS mounted systems, you can
 * make them different
 */

EXTERN int Is_server;				/* LPD sets to non-zero */
EXTERN int Auth_from;				/* LPD sets to type of authentication */
EXTERN struct dpathname *CDpathname;	/* control directory pathname */
EXTERN struct dpathname *SDpathname;	/* spool directory pathname */

EXTERN int Accounting_check; /* check accounting at start */
EXTERN char* Accounting_end;/* accounting at start (see also af, la, ar) */
EXTERN char* Accounting_file; /* name of accounting file (see also la, ar) */
EXTERN int Accounting_remote; /* write remote transfer accounting (if af is set) */
EXTERN char* Accounting_start;/* accounting at start (see also af, la, ar) */
EXTERN int Allow_user_logging; /* allow users to get log info */
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
EXTERN int Lpr_bounce; /* allow LPR to do bounce queue filtering */
EXTERN char * Check_idle;	/* lpd checks for idle printer */
EXTERN int Clear_flag_bits; /* if lp is a tty, clear flag bits (see ty) */
EXTERN int Clear_local_bits; /* if lp is a tty, clear local mode bits (see ty) */
EXTERN char* Comment_tag; /* comment identifying printer (LPQ) */
EXTERN int Connect_grace; /* grace period for reconnections */
EXTERN int Network_connect_grace; /* grace period for reconnections */
EXTERN char* Control_dir; /* Control directory */
EXTERN char Control_dir_expanded[MAXPATHLEN]; /* Control directory expanded */
EXTERN char* Control_filter; /* Control filter */
EXTERN int Cost_factor; /* cost in dollars per thousand pages */
EXTERN char* Default_auth;	/* default authentication type */
EXTERN char* Default_logger_port;	/* default logger port */
EXTERN char* Default_logger_protocol;	/* default logger protocol */
EXTERN char* Default_priority;	/* default priority */
EXTERN char* Default_format;	/* default format */
EXTERN char* Destinations; /* printers that a route filter may return and we should query */
EXTERN int Direct_read;	/* filter reads directly from a file */
EXTERN int FF_on_close; /* print a form feed when device is closed */
EXTERN int FF_on_open; /* print a form feed when device is opened */
EXTERN char* Force_queuename; /* force the use of this queue name */
EXTERN int Force_poll; /* force polling job queues */
EXTERN char* Form_feed; /* string to send for a form feed */
EXTERN char* Formats_allowed; /* valid output filter formats */
EXTERN int Forwarding_off; /* if true, no forwarded jobs accepted */
EXTERN int Fix_bad_job; /* if true, try to fix bad job files */
EXTERN int Full_time; /* full or complete time format in messages */
EXTERN int Generate_banner; /* generate a banner when sending to remote */
EXTERN int Hold_all;	 /* hold all jobs */
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
EXTERN int Max_status_line; /* maximum status line size */
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
EXTERN int Poll_time; /* force polling job queues */
EXTERN char* Pr_program; /* pr program for p format */
EXTERN int Read_write; /* open the printer for reading and writing */
EXTERN char* RemoteHost; /* remote-queue machine (hostname) (with rm) */
EXTERN char* RemotePrinter; /* remote-queue printer name (with rp) */
EXTERN int Reuse_addr; /* set SO_REUSEADDR on outgoing ports */
EXTERN int Retry_ECONNREFUSED; /* retry on ECONNREFUSED  */
EXTERN int Retry_NOLINK; /* retry on link connection failure */
EXTERN char* Routing_filter; /* filter to determine routing of jobs */
EXTERN char* Safe_chars; /* safe characters in control file */
EXTERN int Save_on_error; /* save this job when an error */
EXTERN int Save_when_done; /* save this job when done */
EXTERN char* Server_authentication_command; /* authentication command */
EXTERN char* Server_names; /* names of servers for queue (with ss) */
EXTERN char* Server_queue_name; /* name of queue that server serves (with sv) */
EXTERN int Send_block_format; /* send block of data */
EXTERN int Send_data_first; /* send data files first */
EXTERN int Set_flag_bits; /* like `fc' but set bits (see ty) */
EXTERN int Set_local_bits; /* like `xc' but set bits (see ty) */
EXTERN int Short_banner; /* short banner (one line only) */
EXTERN char* Spool_dir; /* spool directory (only ONE printer per directory!) */
EXTERN char Spool_dir_expanded[MAXPATHLEN]; /* expanded Spool Dir */
EXTERN int Spread_jobs; /* spread job numbers out by this factor */
EXTERN char* Status_file; /* printer status file name */
EXTERN int Stalled_time; /* amount of time before reporing stalled job */
EXTERN int Stop_on_abort; /* stop when job aborts */
EXTERN char* Stty_command; /* stty commands to set output line characteristics */
EXTERN int Suppress_copies; /* suppress multiple copies */
EXTERN int Suppress_header; /* suppress headers and/or banner page */
EXTERN char* Trailer_on_close; /* trailer string to print when queue empties */
EXTERN int Use_date;		/* put date in control file */
EXTERN int Use_queuename;	/* put queuename in control file */
EXTERN int Use_identifier;	/* put identifier in control file */
EXTERN int Use_shorthost;	/* Use short hostname in control file information */
EXTERN char* Use_auth;		/* clients use authentication */
EXTERN int Use_auth_flag;	/* clients forcing authentication */
EXTERN char* Forward_auth;	/* server use authentication when forwarding */
EXTERN char* Xlate_format;	/* translate format ids */
EXTERN char* Pass_env;		/* pass these environment variables */
EXTERN char* Top_of_mem;	/* top of allocated memory */
EXTERN int Force_localhost;	/* force localhost for client job transfer */
EXTERN int Socket_linger;	/* set SO_linger for connections to remote hosts */
EXTERN char* Remote_support; /* Operations allowed to remote system */
EXTERN char* Restricted_group; /* Restricted group for clients */

/***************************************************************************
 * Configuration information
 ***************************************************************************/
EXTERN char * Architecture;
EXTERN int Allow_duplicate_args;	/* Legacy requirement */
EXTERN int Allow_getenv;
EXTERN char * BK_filter_options;	/* backwards compatible filter options */
EXTERN char * BK_of_filter_options;	/* backwards compatible OF filter options */
EXTERN int Check_for_nonprintable;	/* lpr check for nonprintable file */
EXTERN char * Config_file;
EXTERN int Connect_interval;
EXTERN int Connect_timeout;
EXTERN char * Default_permission;	/* default permission */
EXTERN char * Default_printer;	/* default printer */
EXTERN char * Default_remote_host;
EXTERN char * Default_tmp_dir;	/* default temporary file directory */
EXTERN char * Server_tmp_dir;	/* default temporary file directory */
EXTERN char * Filter_ld_path;
EXTERN char * Filter_options;
EXTERN char * Filter_path;
EXTERN char * Lockfile;
EXTERN char * Logfile;
EXTERN char * Lpd_port;
EXTERN char * Lpd_printcap_path;
EXTERN char * Mail_operator_on_error;
EXTERN int Max_status_size;
EXTERN int Min_status_size;
EXTERN char * Minfree; /**/
EXTERN int Ms_time_resolution;
EXTERN char * Originate_port;
EXTERN char * OF_filter_options;
EXTERN char * Printcap_path;
EXTERN char * Checkpc_Printcap_path;
EXTERN char * Printer_perms_path;
EXTERN char * Sendmail;
EXTERN char *Send_failure_action;
EXTERN int Send_try;
EXTERN int Send_job_rw_timeout;
EXTERN int Send_query_rw_timeout;
EXTERN int Spool_dir_perms;
EXTERN int Spool_file_perms;
EXTERN char *Syslog_device;	/* default syslog device if no syslog() facility */
EXTERN int Use_info_cache;
EXTERN char * User_authentication_command;
EXTERN char * User_lpc;
EXTERN char * Daemon_group;
EXTERN char * Server_user;
EXTERN char * Remote_user;
EXTERN char * Daemon_user;

/***************************************************************************
 * lpr variables
 ***************************************************************************/

EXTERN char *Accntname; /* Accounting name: PLP 'R' control file option */
EXTERN int Binary;      /* Binary format: 'l' Format */
EXTERN char *Bnrname;   /* Banner name: RFC 'L' option */
EXTERN int Break_classname_priority_link;	/* Legacy requirement */
EXTERN char *Classname; /* Class name:  RFC 'C' option */
EXTERN int Classname_length; /* Control max length of Classname (sort of) */
EXTERN int Copies;      /* Copies */
EXTERN char *Format;    /* format for printing: lower case letter */
EXTERN char *Font1;     /* Font information 1 */
EXTERN char *Font2;     /* Font information 2 */
EXTERN char *Font3;     /* Font information 3 */
EXTERN char *Font4;     /* Font information 4 */
EXTERN int Indent;      /* indent:      RFC 'I' option */
EXTERN char *Jobname;   /* Job name:    RFC 'J' option */
EXTERN char *Mailname;  /* Mail name:   RFC 'M' option */
EXTERN int No_header;   /* No header flag: no L option in control file */
EXTERN char *Option_order;	/* Option order in control file */
EXTERN char *Printer;	/* printer name */
EXTERN int Priority;	/* Priority */
EXTERN char *Prtitle;   /* Pr title:    RFC 'T' option */
EXTERN int Pwidth;	    /* Width paper: RFC 'W' option */
EXTERN int Removefiles;	    /* Remove files */
EXTERN char *Username;	/* Specified with the -U option */
EXTERN int Use_queuename_flag;	/* Specified with the -Q option */
EXTERN int Secure;		/* Secure filter option */
EXTERN int Setup_mailaddress;   /* Set up mail address */
EXTERN char *Zopts;     /* Z options */

EXTERN int Filecount;   /* number of files to print */ 
EXTERN char **Files;    /* pointer to array of file names */
EXTERN int DevNullFD;	/* DevNull File descriptor */

/***************************************************************************
 * Information from host environment and defaults
 ***************************************************************************/

EXTERN char *FQDNRemote;    /* FQDN of Remote host */
EXTERN char *ShortRemote;   /* Short form of Remote host */
EXTERN int Foreground;      /* Run lpd in foreground */
EXTERN int Clean;           /* clean out the queues */
EXTERN int Server_pid;      /* PID of server */
EXTERN int Lpd_pipe[2];     /* connection between jobs */
EXTERN int Clear_scr;       /* clear screen */
EXTERN int Interval;        /* display interval */
EXTERN int Longformat;      /* Long format */
EXTERN int Displayformat;   /* Display format */
EXTERN int All_printers;    /* show all printers */
EXTERN int Status_line_count; /* number of status lines */
EXTERN struct keywords Lpd_parms[]; /* lpd parameters */
extern char LPC_optstr[];   /* LPD options */
extern char LPD_optstr[];   /* LPD options */
extern char LPQ_optstr[];   /* LPQ options */
extern char LPR_optstr[];   /* LPQ options */
extern char LPRM_optstr[];  /* LPQ options */
extern struct keywords Lpr_parms[]; /* parameters for LPR */
EXTERN int LP_mode;			/* LP mode */
EXTERN int Lp_getlist;		/* show default printer */
EXTERN int Lp_sched;		/* print scheduler information */
EXTERN int Lp_status;		/* print scheduler information */
EXTERN int Lp_showjobs;		/* print scheduler information */
EXTERN int Lp_accepting;	/* print scheduler information */
EXTERN int Lp_default;		/* print scheduler information */
EXTERN int Lp_summary;		/* print scheduler information */
EXTERN struct malloc_list Lp_pr_list;


/***************************************************************************
 * For printcap and configuration information
 ***************************************************************************/
EXTERN struct control_file *Cfp_static;	/* control file for job */
EXTERN struct malloc_list Data_files; /* list of data files received */

EXTERN char *Control_debug;


/*****************************************************************
 * Host name and address information
 *  The gethostbyname function returns a pointer to a structure
 *  which contains all the host information.  But this
 *  value disappears or is modified on the next call.  We save
 *  the information  using this structure.  Also,  we look
 *  forward to the IPV6 structure,  where we need a structure
 *  for an address.
 *****************************************************************/

struct host_information{
	struct malloc_list host_names;	/* official name of host is first */
	int host_addrtype;		/* address type */
	int host_addrlength;	/* address length */
	struct malloc_list host_addr_list;	/* address list */
	char shorthost[LINEBUFFER];
	char *fqdn;
};

EXTERN struct host_information LocalhostIP;	/* IP from localhost lookup */
EXTERN struct host_information RemoteHostIP;	/* IP from localhost lookup */
EXTERN struct host_information HostIP;	/* current host ip */
EXTERN struct host_information LookupHostIP;	/* for searches */
EXTERN struct host_information PermcheckHostIP;	/* for searches */

/***************************************************************************
 * inet_ntop_sockaddr()
 *  - interface to inet_ntop that does checking
 ***************************************************************************/
char *inet_ntop_sockaddr( struct sockaddr *addr, char *str, int len );

/***************************************************************************
 * you need a place for this stuff
 ***************************************************************************/

void Get_subserver_info( struct malloc_list *servers, char *s);
int Receive_job( int *socket, char *input, int maxlen, int transfer_timeout );
int Receive_secure( int *socket, char *input, int maxlen, int transfer_timeout );
int Receive_block_job( int *socket, char *input, int maxlen, int transfer_timeout );
int Job_status( int *socket, char *input, int maxlen );
int Job_remove( int *socket, char *input, int maxlen );
int Job_control( int *socket, char *input, int maxlen );
void Get_parms(int argc,char *argv[]);
off_t Copy_stdin( struct control_file *cf );    /* copy stdin to a file */
off_t Check_files( struct control_file *cf,  char **files, int filecount );
int Make_job( struct control_file *cf );
void Process_jobs( int *socket, char *input, int maxlen );
int Start_all( void );
int Start_idle_server( int n );
void Start_particular_server( char *name );
int Scan_block_file( int fd, struct control_file *cfp );
int Check_for_missing_files( struct control_file *cfp,
	struct malloc_list *data_files_list,
	char *orig_name, char *authentication, int *hold_fd,
	struct printcap_entry *pc_entry);
int Do_perm_check( struct control_file *cfp );
void Do_queue_jobs( char *name );
void Sendmail_to_user( int status, struct control_file *cfp,
    struct printcap_entry *pc );
void Start_new_server( void );

#endif
