/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 * $Id: lpr.h,v 5.1 1999/09/12 21:33:03 papowell Exp papowell $
 ***************************************************************************/



#ifndef _LPR_1_
#define _LPR_1_

EXTERN char *Accntname_JOB; /* Accounting name: PLP 'R' control file option */
EXTERN int Binary;      /* Binary format: 'l' Format */
EXTERN char *Bnrname_JOB;   /* Banner name: RFC 'L' option */
EXTERN char *Classname_JOB; /* Class name:  RFC 'C' option */
EXTERN int Copies;      /* Copies */
EXTERN int Format;    /* format for printing: lower case letter */
EXTERN char *Font1_JOB;     /* Font information 1 */
EXTERN char *Font2_JOB;     /* Font information 2 */
EXTERN char *Font3_JOB;     /* Font information 3 */
EXTERN char *Font4_JOB;     /* Font information 4 */
EXTERN int Indent;      /* indent:      RFC 'I' option */
EXTERN char *Jobname_JOB;   /* Job name:    RFC 'J' option */
EXTERN char *Mailname_JOB;  /* Mail name:   RFC 'M' option */
EXTERN int No_header;   /* No header flag: no L option in control file */
EXTERN int Priority;	/* Priority */
EXTERN char *Prtitle_JOB;   /* Pr title:    RFC 'T' option */
EXTERN int Pwidth;	    /* Width paper: RFC 'W' option */
EXTERN int Removefiles;	    /* Remove files */
EXTERN char *Username_JOB;	/* Specified with the -U option */
EXTERN int Secure;		/* Secure filter option */
EXTERN int Setup_mailaddress;   /* Set up mail address */
EXTERN char *Zopts_JOB;     /* Z options */

EXTERN int DevNullFD;	/* DevNull File descriptor */
extern struct jobwords Lpr_parms[]; /* parameters for LPR */
EXTERN int LP_mode;		/* look like LP */


EXTERN int Silent;			/* lp -s option */

/* PROTOTYPES */
int main(int argc, char *argv[], char *envp[]);
void Get_parms(int argc, char *argv[] );
void usage(void);
int Make_job( struct job *job );
void get_job_number( struct job *job );
double Copy_stdin( struct job *job );
double Check_files( struct job *job );
int Check_lpr_printable(char *file, int fd, struct stat *statb, int format );
int is_exec( char *buf, int n);
int is_arch(char *buf, int n);
void Dienoarg(int option);
void Check_int_dup (int option, int *value, char *arg, int maxvalue);
void Check_str_dup(int option, char **value, char *arg, int maxlen );
void Check_dup(int option, int *value);
int Start_worker( struct line_list *l, int fd );

#endif
