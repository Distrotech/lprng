/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 ***************************************************************************
 * MODULE: lpd.h
 * PURPOSE: Include file for LPR
 * Note: defines global variables that do not need initialization
 *
 * $Id: lpd.h,v 3.2 1996/08/31 21:11:58 papowell Exp papowell $
 **************************************************************************/

#ifndef _LPD_H_
#define _LPD_H_ 1

#ifndef EXTERN
#define EXTERN extern
#endif

#include "lp.h"

/***************************************************************************
 * Information from host environment and defaults
 ***************************************************************************/

EXTERN int Inetd_started;	/* Inetd started the server */
EXTERN char *FQDNRemote;	/* FQDN of Remote host */
EXTERN char *IPRemote;		/* IP address of Remote host */
EXTERN char *ShortRemote;	/* Short form of Remote host */
EXTERN int Foreground;		/* Run lpd in foreground */
EXTERN int Clean;			/* clean out the queues */
EXTERN int Server_pid;		/* PID of server */
EXTERN int Lpd_pipe[2];		/* connection between jobs */

EXTERN struct keywords lpd_parms[]
#ifdef DEFINE
 = {
{ "Clean",  INTEGER_K , &Clean },
{ "Foreground",  INTEGER_K , &Foreground },
{ "FQDNHost",  STRING_K , &FQDNHost },
{ "FQDNRemote",  STRING_K , &FQDNRemote },
{ "Inetd_started",  INTEGER_K , &Inetd_started },
{ "IPRemote",  STRING_K , &IPRemote },
{ "Logname",  STRING_K , &Logname },
{ "Printer",  STRING_K , &Printer },
{ "ShortHost",  STRING_K , &ShortHost },
{ "ShortRemote",  STRING_K , &ShortRemote },
{ 0 }
}
#endif
;

/***************************************************************************
 * LPD configuration file keywords
 ***************************************************************************/
/**************************************************************************
 * Configuration file keywords and options
 **************************************************************************/
extern struct keywords lpd_special[], lpd_config[];
EXTERN struct keywords lpd_special[]
#ifdef DEFINE
  = {
    { (char *)0 }
}
#endif
;

EXTERN struct keywords *lpd_all_config[]
#ifdef DEFINE
  = {
    lpd_special,
    lpd_config,
    0
}
#endif
;

/***************************************************************************
 * LPD variable set - all variables
 ***************************************************************************/
EXTERN struct keywords *lpd_vars[]
#ifdef DEFINE
 = {
    lpd_parms,
    lpd_special,
    lpd_config,
    0
}
#endif
;

/**************************************************************************
 * Command line options
 **************************************************************************/

EXTERN char LPD_optstr[] 	/* LPD options */
#ifdef DEFINE
 = "D:FL:P:Vic"
#endif
;

EXTERN int Inetd_started;	/* started from inetd */

/***************************************************************************
 * Subserver information
 *  used by LPQ and LPD to start subservers
 ***************************************************************************/

struct server_info{
	char *name;         /* printer name for server */
	pid_t pid;			/* pid of server processes */
	int status;			/* 0 = free status of the server process */
	unsigned long time;	/* time it terminated */
};


void Get_subserver_info( struct malloc_list *servers, char *s);

int Receive_job( int *socket, char *input, int maxlen );
int Job_status( int *socket, char *input, int maxlen );
int Job_remove( int *socket, char *input, int maxlen );
int Job_control( int *socket, char *input, int maxlen );
int patselect( struct token *token, struct control_file *cfp,
	struct destination **destination );
void job_count( int *hc, int *cnt );

#endif
