/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 ***************************************************************************
 * MODULE: lpr.h
 * PURPOSE: Include file for LPR
 * Note: defines global variables that do not need initialization
 *
 * $Id: lpr.h,v 3.3 1996/06/30 17:12:44 papowell Exp $
 **************************************************************************/

#ifndef _LPR_H_
#define _LPR_H_ 1

#ifndef EXTERN
#define EXTERN extern
#endif

#include "lp.h"

/***************************************************************************
 * Information from host environment and defaults
 ***************************************************************************/

EXTERN char *FQDNHost;	/* host fully qualified domain name */
EXTERN char *Host;		/* host name to be used in control file 'H' */
EXTERN char *ShortHost;	/* short host name */
EXTERN char *Logname;		/* users login */
EXTERN char *Tempfile;		/* temporary file */

/***************************************************************************
 * Information from command line options
 ***************************************************************************/

EXTERN char *Accntname; /* Accounting name: PLP 'R' control file option */
EXTERN int Binary;      /* Binary format: 'l' Format */
EXTERN char *Bnrname;   /* Banner name: RFC 'L' option */
EXTERN char *Classname; /* Class name:  RFC 'C' option */
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

/***************************************************************************
 * Additional parameters needed to process job
 ***************************************************************************/

EXTERN char *RemoteHost;		/* where to send the job */
/***************************************************************************
 * Configuration file information
 ***************************************************************************/

EXTERN char *Default_tmp_dir;	/* default temporary file directory */


EXTERN struct keywords lpr_parms[]
#ifdef DEFINE
 = {
{ "Accntname",  STRING_K , &Accntname, M_ACCNTNAME, 'R' },
{ "Binary",  INTEGER_K , &Binary },
{ "Bnrname",  STRING_K , &Bnrname, M_BNRNAME, 'L' },
{ "Classname",  STRING_K , &Classname, M_CLASSNAME, 'C' },
{ "Copies",  INTEGER_K , &Copies },
{ "Format",  STRING_K , &Format },
{ "Font1",  STRING_K , &Font1, M_FONT, '1' },
{ "Font2",  STRING_K , &Font2, M_FONT, '2' },
{ "Font3",  STRING_K , &Font3, M_FONT, '3' },
{ "Font4",  STRING_K , &Font4, M_FONT, '4' },
{ "Host",  STRING_K , &Host, M_FROMHOST, 'H' },
{ "FQDNHost",  STRING_K , &FQDNHost },
{ "Indent",  INTEGER_K , &Indent, M_INDENT, 'I' },
{ "Jobname",  STRING_K , &Jobname, M_JOBNAME, 'J' },
{ "Logname",  STRING_K , &Logname, M_BNRNAME, 'P' },
{ "Mailname",  STRING_K , &Mailname, M_MAILNAME, 'M' },
{ "No_header",  INTEGER_K , &No_header },
{ "Option_order",  STRING_K , &Option_order },
{ "Printer",  STRING_K , &Printer },
{ "Priority",  INTEGER_K , &Priority },
{ "Prtitle",  STRING_K , &Prtitle, M_PRTITLE, 'T' },
{ "Pwidth",  INTEGER_K , &Pwidth, M_PWIDTH, 'W' },
{ "Queue_name",  STRING_K , &Queue_name, M_QUEUENAME, 'Q' },
{ "RemoteHost",  STRING_K , &RemoteHost },
{ "Removefiles",  INTEGER_K , &Removefiles },
{ "ShortHost",  STRING_K , &ShortHost },
{ "Setup_mailaddress",  INTEGER_K , &Setup_mailaddress },
{ "Secure", INTEGER_K , &Secure },
{ "Tempfile",  STRING_K , &Tempfile },
{ "Username",  STRING_K , &Username },
{ "Use_shorthost",  INTEGER_K , &Use_shorthost },
{ "Zopts",  STRING_K , &Zopts, M_ZOPTS, 'Z' },
{ "Filecount",  INTEGER_K , &Filecount },
{ "Files",  LIST_K , &Files },
{ 0 }
}
#endif
;

/***************************************************************************
 * LPR configuration file keywords
 ***************************************************************************/
/**************************************************************************
 * Configuration file keywords and options
 **************************************************************************/
extern char *Default_printer;
EXTERN struct keywords lpr_special[]
#ifdef DEFINE
  = {
    { "default_printer",STRING_K,(void *)&Default_printer },
	{ "default_priority", STRING_K, &Default_priority },
	{ "default_format", STRING_K, &Default_format },
	{ "default_tmp_dir", STRING_K, &Default_tmp_dir },
    { (char *)0 }
}
#endif
;

extern struct keywords lpd_config[];
EXTERN struct keywords *lpr_config[]
#ifdef DEFINE
  = {
    lpr_special,
    lpd_config,
    0
}
#endif
;

/***************************************************************************
 * LPR variable set - all variables
 ***************************************************************************/
EXTERN struct keywords *lpr_vars[]
#ifdef DEFINE
 = {
    lpr_parms,
    lpr_special,
    lpd_config,
    0
}
#endif
;

/**************************************************************************
 * Command line options
 **************************************************************************/

EXTERN char LPR_optstr[] 	/* LPR options */
#ifdef DEFINE
 = "1:2:3:4:#:C:D:F:J:K:NP:QR:T:U:VZ:bcdfghi:lkm:nprstvw:"
#endif
;

void Get_parms(int argc,char *argv[]);

off_t Copy_stdin( struct control_file *cf );	/* copy stdin to a file */
off_t Check_files( struct control_file *cf,  char **files, int filecount );

void Make_job( struct control_file *cf );
void Check_parms();

#endif
