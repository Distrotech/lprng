/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lp_config.h
 * PURPOSE: define a set of configuration parameters used by all
 *  modules.  Note that this file contains not only the definitions
 *  of the variables,  but also the initialization array needed
 *  to read them from the configuration file.
 *
 * "$Id: lp_config.h,v 3.0 1996/05/19 04:06:23 papowell Exp $";
 **************************************************************************/
#ifndef _LP_CONFIG_
#define _LP_CONFIG_

EXTERN char * Architecture;
EXTERN int Allow_getenv;
EXTERN char * BK_filter_options;	/* backwards compatible filter options */
EXTERN char * BK_of_filter_options;	/* backwards compatible OF filter options */
EXTERN int Check_for_nonprintable;	/* lpr check for nonprintable file */
EXTERN char * Client_config_file;
EXTERN int Connect_interval;
EXTERN int Connect_try;
EXTERN int Connect_timeout;
EXTERN char * Default_banner_printer;	/* default banner printer */
EXTERN char * Default_permission;	/* default permission */
EXTERN char * Default_printer;	/* default printer */
EXTERN char * Default_remote_host;
EXTERN char * Domain_name;
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
EXTERN char * Originate_port;
EXTERN char * OF_filter_options;
EXTERN char * Printcap_path;
EXTERN char * Printer_perms_path;
EXTERN char * Sendmail;
EXTERN char *Send_failure_action;
EXTERN int Send_try;
EXTERN int Send_timeout;
EXTERN char * Server_config_file;
EXTERN int Spool_dir_perms;
EXTERN int Spool_file_perms;
EXTERN char *Syslog_device;	/* default syslog device if no syslog() facility */
EXTERN int Use_info_cache;
EXTERN char * Server_group;
EXTERN char * Server_user;

extern struct keywords lpd_config[];

extern char Default_configuration[];

EXTERN struct malloc_list Config_buffers;

void Getconfig( char * filename, struct keywords **keys,
	struct malloc_list *buffers );
void Parsebuffer( char *filename, char *buffer, struct keywords **keys,
	struct malloc_list *buffers );
void Expandconfig( struct keywords **keylist, struct malloc_list *buffers );

#endif
