/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: vars.c,v 5.5 1999/10/28 01:28:36 papowell Exp papowell $";


/* force local definitions */
#define EXTERN
#define DEFINE(X) X
#define DEFS

#include "lp.h"
#include "child.h"
#include "gethostinfo.h"
#include "getqueue.h"
#include "accounting.h"
#include "permission.h"
#include "printjob.h"
/**** ENDINCLUDE ****/

/***************************************************************************

Commentary:
Patrick Powell Tue Nov 26 08:10:12 PST 1996
Put all of the variables in a separate file.

 ***************************************************************************/

/*
 * printcap variables used by LPD for printing
 * THESE MUST BE IN SORTED ORDER
 *  NOTE:  the maxval field is used to suppress clearing
 *   these values when initializing the printcap variable
 *   values.
 */
#ifdef GETENV
#define ENV "1"
#else
#define ENV
#endif

#if !defined(ARCHITECTURE)
#define ARCHITECTURE "unknown"
#endif
#if !defined(LPD_CONF_PATH)
#error Missing LPD_CONF_PATH definition
#endif
#if !defined(LPD_PERMS_PATH)
#error Missing LPD_PERMS_PATH definition
#endif
#if !defined(PRINTCAP_PATH)
#error Missing PRINTCAP_PATH definition
#endif
#if !defined(LPD_PRINTCAP_PATH)
#error Missing LPD_PRINTCAP_PATH definition
#endif
#if !defined(FORCE_LOCALHOST)
#error Missing FORCE_LOCALHOST definition
#endif
#if !defined(REQUIRE_CONFIGFILES)
#error Missing REQUIRE_CONFIGFILES definition
#endif

struct keywords Pc_var_list[] = {

/* XXSTARTXX */
   /*  always print banner, ignore lpr -h option */
{ "ab",  FLAG_K,  &Always_banner_DYN,0,0},
   /*  query accounting server when connected */
{ "achk",  FLAG_K,  &Accounting_check_DYN,0,0},
   /*  accounting at end (see also af, la, ar, as) */
{ "ae",  STRING_K,  &Accounting_end_DYN,0,0,"=jobend $H $n $P $k $b $t"},
   /*  name of accounting file (see also la, ar) */
{ "af",  STRING_K,  &Accounting_file_DYN,0,0,"=acct"},
   /*  automatically hold all jobs */
{ "ah",  FLAG_K,  &Auto_hold_DYN,0,0},
   /* Allow duplicate command line arguments (legacy requirement) */
{ "allow_duplicate_args", FLAG_K, &Allow_duplicate_args_DYN,0,0,"1"},
   /* Allow use of LPD_CONF environment variable */
{ "allow_getenv", FLAG_K, &Allow_getenv_DYN,1,0,ENV},
   /* allow users to request logging info using lpr -mhost%port */
{ "allow_user_logging", FLAG_K, &Allow_user_logging_DYN,0,0},
   /* allow these users or UIDs to set owner of job.  For Samba front ending */
{ "allow_user_setting", STRING_K, &Allow_user_setting_DYN,0,0},
   /*  write remote transfer accounting (if af is set) */
{ "ar",  FLAG_K,  &Accounting_remote_DYN,0,0,"1"},
   /* host architecture */
{ "architecture", STRING_K, &Architecture_DYN,1,0,ARCHITECTURE},
   /*  accounting at start (see also af, la, ar) */
{ "as",  STRING_K,  &Accounting_start_DYN,0,0,"=jobstart $H $n $P $k $b $t"},
	/* authentication type to use to send to server */
{ "auth",  STRING_K, &Auth_DYN,0,0 },
   /*  client to server authentication filter */
{ "auth_client_filter", STRING_K, &Auth_client_filter_DYN,0,0},
   /*  authentication for forwarding */
{ "auth_forward", STRING_K, &Auth_forward_DYN,0,0},
   /*  server to server authentication remote server id */
{ "auth_forward_id", STRING_K, &Auth_forward_id_DYN,0,0},
   /*  server to server authentication transfer filter */
{ "auth_forward_filter", STRING_K, &Auth_forward_filter_DYN,0,0},
   /*  authentication server send filter */
{ "auth_receive_filter", STRING_K, &Auth_receive_filter_DYN,0,0},
   /* authentication server id */
{ "auth_server_id", STRING_K, &Auth_server_id_DYN,0,0},
   /*  end banner printing program overides bp */
{ "be",  STRING_K,  &Banner_end_DYN,0,0},
   /*  Berkeley LPD: job file strictly RFC-compliant */
{ "bk",  FLAG_K,  &Backwards_compatible_DYN,0,0},
   /*  Berkeley LPD filter options */
{ "bk_filter_options", STRING_K, &BK_filter_options_DYN,0,0,"=$P $w $l $x $y $F $c $L $i $J $C $0n $0h $-a"},
   /*  Berkeley LPD OF filter options */
{ "bk_of_filter_options", STRING_K, &BK_of_filter_options_DYN,0,0,"=$w $l $x $y"},
   /*  backwards-compatible filters: use simple paramters */
{ "bkf",  FLAG_K,  &Backwards_compatible_filter_DYN,0,0},
   /*  short banner line sent to banner printer */
{ "bl",  STRING_K,  &Banner_line_DYN,0,0,"=$-C:$-n Job: $-J Date: $-t"},
   /*  banner printing program (see bs, be) */
{ "bp",  STRING_K,  &Banner_printer_DYN,0,0},
   /*  use filters on bounce queue files */
{ "bq",  STRING_K,  &Bounce_queue_dest_DYN,0,0},
   /*  format for bounce queue output */
{ "bq_format",  STRING_K,  &Bounce_queue_format_DYN,0,0,"=l"},
   /*  if lp is a tty, set the baud rate (see ty) */
{ "br",  INTEGER_K,  &Baud_rate_DYN,0,0},
   /* do not set priority from class name */
{ "break_classname_priority_link",  FLAG_K,  &Break_classname_priority_link_DYN,0,0},
   /*  banner printing program overrides bp */
{ "bs",  STRING_K,  &Banner_start_DYN,0,0},
   /* check for nonprintable file */
{ "check_for_nonprintable", FLAG_K, &Check_for_nonprintable_DYN,0,0,"1"},
   /* check for RFC1179 protocol violations */
{ "check_for_protocol_violations", FLAG_K, &Check_for_protocol_violations_DYN,0,0},
   /* check for idle printer */
{ "check_idle", STRING_K, &Check_idle_DYN,0,0},
   /* show classname in status display */
{ "class_in_status", FLAG_K, &Class_in_status_DYN,0,0},
   /*  comment identifying printer (LPQ) */
{ "cm",  STRING_K,  &Comment_tag_DYN,0,0},
   /* configuration file */
{ "config_file", STRING_K, &Config_file_DYN,1,0,"=" LPD_CONF_PATH},
   /* connection control for remote printers */
{ "connect_grace", INTEGER_K, &Connect_grace_DYN,0,0,"=0"},
   /* connection control for remote printers */
{ "connect_interval", INTEGER_K, &Connect_interval_DYN,0,0,"=10"},
   /* connection control for remote printers */
{ "connect_timeout", INTEGER_K, &Connect_timeout_DYN,0,0,"=10"},
   /* control file filter */
{ "control_filter", STRING_K, &Control_filter_DYN,0,0},
   /* create files in spool directory */
{ "create_files", FLAG_K, &Create_files_DYN,0,0},
   /*  debug level set for queue handler */
{ "db",  STRING_K,  &New_debug_DYN,0,0},
   /* default job format */
{ "default_format", STRING_K, &Default_format_DYN,0,0,"=f"},
   /* default permission for files */
{ "default_permission", STRING_K, &Default_permission_DYN,0,0,"=ACCEPT"},
   /* default printer */
{ "default_printer", STRING_K, &Default_printer_DYN,0,0,"=missingprinter"},
   /* printer for lpd to use when not in printcap */
{ "default_printer_when_unknown", STRING_K, &Default_printer_when_unknown,0,0},
   /* default job priority */
{ "default_priority", STRING_K, &Default_priority_DYN,0,0,"=A"},
   /* default remote host */
{ "default_remote_host", STRING_K, &Default_remote_host_DYN,0,0,"=localhost"},
   /* default temp directory for temp files */
{ "default_tmp_dir", STRING_K, &Default_tmp_dir_DYN,0,0,"=/tmp"},
   /* printers that a route filter may return and we should query */
{ "destinations", STRING_K, &Destinations_DYN,0,0},
   /* list of keys used to search config for environment variable setup */
{ "env_names", STRING_K, &Env_names_DYN,0,0},
   /* exit linger timeout to wait for socket to close */
{ "exit_linger_timeout", INTEGER_K, &Exit_linger_timeout_DYN,0,0,"=600"},
   /*  string to send for a form feed */
{ "ff",  STRING_K,  &Form_feed_DYN,0,0,"=\\f"},
   /* default filter */
{ "filter", STRING_K, &Filter_DYN,0,0},
   /* filter LD_LIBRARY_PATH value */
{ "filter_ld_path", STRING_K, &Filter_ld_path_DYN,0,0,"=/lib:/usr/lib:/usr/5lib:/usr/ucblib"},
   /* filter options */
{ "filter_options", STRING_K, &Filter_options_DYN,0,0,"=$C $A $F $H $J $K $L $P $Q $R $Z $a $c $d $e $f $h $i $j $k $l $n $p $r $s $w $x $y $-a"},
   /* filter PATH environment variable */
{ "filter_path", STRING_K, &Filter_path_DYN,0,0,"=/bin:/usr/bin:/usr/local/bin:/usr/ucb:/usr/sbin:/usr/etc:/etc"},
   /* interval at which to check OF filter for error status */
{ "filter_poll_interval", INTEGER_K, &Filter_poll_interval_DYN,0,0,"=30"},
   /*  print a form feed when device is opened */
{ "fo",  FLAG_K,  &FF_on_open_DYN,0,0},
   /* force FQDN HOST value in control file */
{ "force_fqdn_hostname",  FLAG_K,  &Force_FQDN_hostname_DYN,0,0},
   /* force clients to send all requests to localhost */
{ "force_localhost",  FLAG_K,  &Force_localhost_DYN,0,0,FORCE_LOCALHOST},
   /*  force lpq status format for specified hostnames */
{ "force_lpq_status", STRING_K, &Force_lpq_status_DYN,0,0},
   /*  force use of this queuename if none provided */
{ "force_queuename", STRING_K, &Force_queuename_DYN,0,0},
   /*  print a form feed when device is closed */
{ "fq",  FLAG_K,  &FF_on_close_DYN,0,0},
   /* full or complete time format */
{ "full_time",  FLAG_K,  &Full_time_DYN,0,0},
   /*  valid output filter formats */
{ "fx",  STRING_K,  &Formats_allowed_DYN,0,0},
   /* generate a banner when forwarding job */
{ "generate_banner", FLAG_K, &Generate_banner_DYN,0,0},
   /* group to run SUID ROOT programs */
{ "group", STRING_K, &Daemon_group_DYN,1,0,"=daemon"},
   /*  print banner after job instead of before */
{ "hl",  FLAG_K,  &Banner_last_DYN,0,0},
   /*  filter command, run on a per-file basis */
{ "if",  STRING_K,  &IF_Filter_DYN,0,0},
   /*  ignore requested user priority */
{ "ignore_requested_user_priority",  FLAG_K,  &Ignore_requested_user_priority_DYN,0,0},
   /*  Running IPV6 */
{ "ipv6",  FLAG_K,  &IPV6Protocol_DYN,0,0},
	/* TCP keepalive enabled */
{ "keepalive", FLAG_K, &Keepalive_DYN,0,0,"1"},
	/* keytab file location for kerberos, used by server */
{ "kerberos_keytab", STRING_K, &Kerberos_keytab_DYN,0,0,"=/etc/lpd.keytab"},
	/* key lifetime for kerberos, used by server */
{ "kerberos_life", STRING_K, &Kerberos_life_DYN,0,0},
	/* key renewal time for kerberos, used by server */
{ "kerberos_renew", STRING_K, &Kerberos_renew_DYN,0,0},
	/* remote server principal for server to server forwarding */
{ "kerberos_forward_principal", STRING_K, &Kerberos_forward_principal_DYN,0,0},
	/* remote server principle, overides default */
{ "kerberos_server_principal", STRING_K, &Kerberos_server_principal_DYN,0,0},
	/* default service */
{ "kerberos_service", STRING_K, &Kerberos_service_DYN,0,0,"=lpr"},
   /*  write local printer accounting (if af is set) */
{ "la",  FLAG_K,  &Local_accounting_DYN,0,0,"1"},
   /*  leader string printed on printer open */
{ "ld",  STRING_K,  &Leader_on_open_DYN,0,0},
   /*  error log file (servers, filters and prefilters) */
{ "lf",  STRING_K,  &Log_file_DYN,0,0,"=log"},
   /* lock the IO device */
{ "lk", FLAG_K,  &Lock_it_DYN,0,0},
   /* lpd lock file */
{ "lockfile", STRING_K, &Lockfile_DYN,1,0,"=/var/run/lpd"},
   /* where to send status information for logging */
{ "logger_destination",  STRING_K,  &Logger_destination_DYN,0,0},
   /* maximum size in K of logger file */
{ "logger_max_size",  INTEGER_K,  &Logger_max_size_DYN,0,0},
   /* path of file to hold logger information */
{ "logger_path",  STRING_K,  &Logger_path_DYN,0,0},
   /* timeout between connection attempts to remote logger */
{ "logger_timeout",  INTEGER_K,  &Logger_timeout_DYN,0,0},
   /*  use long job number when a job is submitted */
{ "longnumber",  FLAG_K,  &Long_number_DYN,0,0},
   /*  device name or lp-pipe command to send output to */
{ "lp",  STRING_K,  &Lp_device_DYN,0,0},
   /* force lpd to filter jobs (bounce) before sending to remote queue */
{ "lpd_bounce", FLAG_K, &Lpd_bounce_DYN,0,0},
   /* force a poll operation */
{ "lpd_force_poll", FLAG_K, &Force_poll_DYN,0,0},
   /*  lpd pathname for server use */
{ "lpd_path",  STRING_K,  &Lpd_path_DYN,0,0},
   /*  interval in secs between starting up all servers */
{ "lpd_poll_time",  INTEGER_K,  &Poll_time_DYN,0,0,"=600"},
   /* lpd port */
{ "lpd_port", STRING_K, &Lpd_port_DYN,0,0,"=printer"},
   /* lpd printcap path */
{ "lpd_printcap_path", STRING_K, &Lpd_printcap_path_DYN,1,0,"=" LPD_PRINTCAP_PATH},
   /* use lpr filtering as in bounce queue */
{ "lpr_bounce", FLAG_K, &Lpr_bounce_DYN,0,0},
   /* BSD LPR -m flag, does not require mail address */
{ "lpr_bsd", FLAG_K, &LPR_bsd_DYN,0,0},
   /* from address to use in mail messages */
{ "mail_from", STRING_K, &Mail_from_DYN,0,0},
   /* mail to this operator on error */
{ "mail_operator_on_error", STRING_K, &Mail_operator_on_error_DYN,0,0},
   /* maximum connection interval */
{ "max_connect_interval", INTEGER_K, &Max_connect_interval_DYN,0,0,"=60"},
   /* maximum log file size in Kbytes */
{ "max_log_file_size", INTEGER_K, &Max_log_file_size_DYN,0,0,"=1000"},
   /* maximum number of servers that can be active */
{ "max_servers_active", INTEGER_K, &Max_servers_active_DYN,1,0},
   /* maximum length of status line */
{ "max_status_line", INTEGER_K, &Max_status_line_DYN,0,0,"=79"},
   /* maximum size (in K) of status file */
{ "max_status_size", INTEGER_K, &Max_status_size_DYN,0,0,"=10"},
   /*  maximum copies allowed */
{ "mc",  INTEGER_K,  &Max_copies_DYN,0,0,"=1"},
   /* minimum log file size in Kbytes */
{ "min_log_file_size", INTEGER_K, &Min_log_file_size_DYN,0,0},
   /* minimum status file size in Kbytes */
{ "min_status_size", INTEGER_K, &Min_status_size_DYN,0,0},
   /* minimum amount of free space needed in K bytes */
{ "minfree", INTEGER_K, &Minfree_DYN,0,0},
   /*  minimum number of printable characters for printable check */
{ "ml",  INTEGER_K,  &Min_printable_count_DYN,0,0},
   /* millisecond time resolution */
{ "ms_time_resolution",  FLAG_K,  &Ms_time_resolution_DYN,0,0,"=1"},
   /*  maximum job size (1Kb blocks, 0 = unlimited) */
{ "mx",  INTEGER_K,  &Max_job_size_DYN,0,0},
   /*  use nonblocking open */
{ "nb",  FLAG_K,  &Nonblocking_open_DYN,0,0},
   /* connection control for remote network printers */
{ "network_connect_grace", INTEGER_K, &Network_connect_grace_DYN,0,0},
   /*  N line after cfA000... line in control file */
{ "nline_after_file",  FLAG_K,  &Nline_after_file_DYN,0,0},
   /*  output filter, run once for all output */
{ "of",  STRING_K,  &OF_Filter_DYN,0,0},
   /* OF filter options */
{ "of_filter_options", STRING_K, &OF_filter_options_DYN,0,0},
   /* orginate connections from these ports */
{ "originate_port", STRING_K, &Originate_port_DYN,0,0,"=512 1023"},
   /* if client, pass these environment variables */
{ "pass_env",  STRING_K,  &Pass_env_DYN,0,0,"=PGPPASS,PGPPATH,PGPPASSFD"},
   /* lpd.perms files */
{ "perms_path", STRING_K, &Printer_perms_path_DYN,1,0,"=" LPD_PERMS_PATH },
   /* pathname of PGP program */
{ "pgp_path", STRING_K, &Pgp_path_DYN,0,0},
   /* client passphrase file for built in PGP authentication */
{ "pgp_passphrasefile", STRING_K, &Pgp_passphrasefile_DYN,0,0,"clientkey"},
   /* server passphrasefile PGP authentication */
{ "pgp_server_passphrasefile", STRING_K, &Pgp_server_passphrasefile_DYN,0,0},
   /*  page length (in lines) */
{ "pl",  INTEGER_K,  &Page_length_DYN,0,0,"=66"},
   /*  pr program for p format */
{ "pr",  STRING_K,  &Pr_program_DYN,0,0,"=/bin/pr"},
   /* /etc/printcap files */
{ "printcap_path", STRING_K, &Printcap_path_DYN,1,0,"=" PRINTCAP_PATH},
   /*  printer status file name */
{ "ps",  STRING_K,  &Status_file_DYN,0,0,"=status"},
   /*  page width (in characters) */
{ "pw",  INTEGER_K,  &Page_width_DYN,0,0,"=80"},
   /*  page width in pixels (horizontal) */
{ "px",  INTEGER_K,  &Page_x_DYN,0,0},
   /*  page length in pixels (vertical) */
{ "py",  INTEGER_K,  &Page_y_DYN,0,0},
   /*  print queue lock file name */
{ "queue_lock_file",  STRING_K,  &Queue_lock_file_DYN,0,0,"=%P"},
   /*  print queue control file name */
{ "queue_control_file",  STRING_K,  &Queue_control_file_DYN,0,0,"=control.%P"},
   /*  print queue status file name */
{ "queue_status_file",  STRING_K,  &Queue_status_file_DYN,0,0,"=status.%P"},
   /*  print queue unspooler pid file name */
{ "queue_unspooler_file",  STRING_K,  &Queue_unspooler_file_DYN,0,0,"=unspooler.%P"},
   /*  put queue name in control file */
{ "qq",  FLAG_K,  &Use_queuename_DYN,0,0,"=1"},
   /*  operations allowed to remote host */
{ "remote_support",  STRING_K,  &Remote_support_DYN,0,0,"=RMQVC"},
   /*  report server as this value for LPQ status */
{ "report_server_as",  STRING_K,  &Report_server_as_DYN,0,0},
   /*  client requires lpd.conf, printcap */
{ "require_configfiles",  FLAG_K,  &Require_configfiles_DYN,0,0,"=" REQUIRE_CONFIGFILES},
   /*  retry on ECONNREFUSED error */
{ "retry_econnrefused",  FLAG_K,  &Retry_ECONNREFUSED_DYN,0,0,"1"},
   /*  retry on NOLINK connection */
{ "retry_nolink",  FLAG_K,  &Retry_NOLINK_DYN,0,0,"1"},
   /*  return short status when specified remotehost */
{ "return_short_status",  STRING_K,  &Return_short_status_DYN,0,0},
   /*  set SO_REUSEADDR on outgoing ports */
{ "reuse_addr",  FLAG_K,  &Reuse_addr_DYN,0,0},
   /*  reverse LPQ status format when specified remotehost */
{ "reverse_lpq_status",  STRING_K,  &Reverse_lpq_status_DYN,0,0},
   /*  reverse priority order, z-aZ-A, so A is lowest */
{ "reverse_priority_order",  FLAG_K,  &Reverse_priority_order_DYN,0,0},
   /*  remote-queue machine (hostname) (with rp) */
{ "rm",  STRING_K,  &RemoteHost_DYN,0,0},
   /*  routing filter, returns destinations */
{ "router",  STRING_K,  &Routing_filter_DYN,0,0},
   /*  remote-queue printer name (with rp) */
{ "rp",  STRING_K,  &RemotePrinter_DYN,0,0},
   /*  open the printer for reading and writing */
{ "rw",  FLAG_K,  &Read_write_DYN,0,0},
   /*  additional safe characters to use in control files */
{ "safe_chars",  STRING_K,  &Safe_chars_DYN,0,0},
   /*  save job when an error */
{ "save_on_error",  FLAG_K,  &Save_on_error_DYN,0,0},
   /*  save job when done */
{ "save_when_done",  FLAG_K,  &Save_when_done_DYN,0,0},
   /*  short banner (one line only) */
{ "sb",  FLAG_K,  &Short_banner_DYN,0,0},
   /*  spool directory (only ONE printer per directory!) */
{ "sd",  STRING_K,  &Spool_dir_DYN,0,0},
   /* send block of data, rather than individual files */
{ "send_block_format", FLAG_K, &Send_block_format_DYN,0,0},
   /* send data files first, then control file */
{ "send_data_first", FLAG_K, &Send_data_first_DYN,0,0},
   /* failure action to take after send_try attempts failed */
{ "send_failure_action", STRING_K, &Send_failure_action_DYN,0,0,"=remove"},
   /* timeout for read/write lpr IO operatons */
{ "send_job_rw_timeout", INTEGER_K, &Send_job_rw_timeout_DYN,0,0,"=6000"},
   /* timeout for read/write status or control operatons */
{ "send_query_rw_timeout", INTEGER_K, &Send_query_rw_timeout_DYN,0,0,"=30"},
   /* numbers of times to try sending job - 0 is infinite */
{ "send_try", INTEGER_K, &Send_try_DYN,0,0,"=3"},
   /* sendmail program */
{ "sendmail", STRING_K, &Sendmail_DYN,1,0,"=/usr/sbin/sendmail -oi -t"},
   /* server temporary file directory */
{ "server_tmp_dir", STRING_K, &Server_tmp_dir_DYN,0,0,"=/tmp"},
   /*  suppress form feeds separating multiple jobs */
{ "sf",  FLAG_K,  &No_FF_separator_DYN,0,0},
   /*  suppress headers and/or banner page */
{ "sh",  FLAG_K,  &Suppress_header_DYN,0,0},
   /*  SHELL enviornment variable value for filters */
{ "shell",  STRING_K,  &Shell_DYN,0,0,"=/bin/sh"},
   /*  short status date enabled or disabled */
{ "short_status_date",  FLAG_K,  &Short_status_date_DYN,0,0,"=1"},
   /*  short status length in lines */
{ "short_status_length",  INTEGER_K,  &Short_status_length_DYN,0,0,"=3"},
   /* set the SO_LINGER socket option */
{ "socket_linger", INTEGER_K,  &Socket_linger_DYN,0,0,"=10"},
   /* spool directory permissions */
{ "spool_dir_perms", INTEGER_K, &Spool_dir_perms_DYN,0,0,"=000700"},
   /* spool file permissions */
{ "spool_file_perms", INTEGER_K, &Spool_file_perms_DYN,0,0,"=000600"},
   /* amount to spread jobs to avoid collisions */
{ "spread_jobs", INTEGER_K, &Spread_jobs_DYN,0,0,"=0"},
   /*  name of queue that server serves (with sv) */
{ "ss",  STRING_K,  &Server_queue_name_DYN,0,0},
   /*  stalled job timeout */
{ "stalled_time", INTEGER_K, &Stalled_time_DYN,0,0,"=120"},
   /*  stop processing queue on filter abort */
{ "stop_on_abort",  FLAG_K,  &Stop_on_abort_DYN,0,0},
   /*  stty commands to set output line characteristics */
{ "stty",  STRING_K,  &Stty_command_DYN,0,0},
   /*  names of servers for queue (with ss) */
{ "sv",  STRING_K,  &Server_names_DYN,0,0},
   /* name of syslog device */
{ "syslog_device", STRING_K, &Syslog_device_DYN,0,0,"=/dev/console"},
   /*  trailer string to print when queue empties */
{ "tr",  STRING_K,  &Trailer_on_close_DYN,0,0},
   /*  translate format from one to another - similar to tr(1) utility */
{ "translate_format",  STRING_K,  &Xlate_format_DYN,0,0},
   /*  put date in control file */
{ "use_date",  FLAG_K,  &Use_date_DYN,0,0,"1"},
   /*  put identifier in control file */
{ "use_identifier",  FLAG_K,  &Use_identifier_DYN,0,0,"1"},
   /*  read and cache information */
{ "use_info_cache", FLAG_K, &Use_info_cache_DYN,0,0,"1"},
   /*  put queue name in control file */
{ "use_shorthost",  FLAG_K,  &Use_shorthost_DYN,0,0},
   /*  server user for SUID purposes */
{ "user", STRING_K, &Daemon_user_DYN,1,0,"=daemon"},
   /*  wait for EOF on device before closing */
{ "wait_for_eof", FLAG_K, &Wait_for_eof_DYN,0,0,"1"},
/* END */
{ (char *)0 }
} ;

struct keywords DYN_var_list[] = {
{ "Logname_DYN", STRING_K, &Logname_DYN},
{ "ShortHost_FQDN", STRING_K, &ShortHost_FQDN},
{ "FQDNHost_FQDN", STRING_K, &FQDNHost_FQDN},
{ "Printer_DYN",  STRING_K, &Printer_DYN },
{ "Queue_name_DYN",  STRING_K, &Queue_name_DYN },
{ "Lp_device_DYN",  STRING_K, &Lp_device_DYN },
{ "RemotePrinter_DYN",  STRING_K, &RemotePrinter_DYN },
{ "RemoteHost_DYN",  STRING_K, &RemoteHost_DYN },
{ "FQDNRemote_FQDN",  STRING_K, &FQDNRemote_FQDN },
{ "ShortRemote_FQDN",  STRING_K, &ShortRemote_FQDN },

{ "Auth_client_id_DYN",  STRING_K, &Auth_client_id_DYN },
{ "Auth_dest_id_DYN",  STRING_K, &Auth_dest_id_DYN },
{ "Auth_filter_DYN",  STRING_K, &Auth_filter_DYN },
{ "Auth_id_DYN",  STRING_K, &Auth_id_DYN },
{ "Auth_received_id_DYN",  STRING_K, &Auth_received_id_DYN },
{ "Auth_sender_id_DYN",  STRING_K, &Auth_sender_id_DYN },

{ "esc_Auth_DYN",  STRING_K, &esc_Auth_DYN },
{ "esc_Auth_client_id_DYN",  STRING_K, &esc_Auth_client_id_DYN },
{ "esc_Auth_dest_id_DYN",  STRING_K, &esc_Auth_dest_id_DYN },
{ "esc_Auth_filter_DYN",  STRING_K, &esc_Auth_filter_DYN },
{ "esc_Auth_id_DYN",  STRING_K, &esc_Auth_id_DYN },
{ "esc_Auth_received_id_DYN",  STRING_K, &esc_Auth_received_id_DYN },
{ "esc_Auth_sender_id_DYN",  STRING_K, &esc_Auth_sender_id_DYN },

{ "Current_date_DYN",  STRING_K, &Current_date_DYN },

{ 0 }
} ;
