/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: vars.c
 * PURPOSE: variables
 **************************************************************************/

static char *const _id =
"$Id: vars.c,v 3.7 1997/02/17 02:31:27 papowell Exp papowell $";

/* force local definitions */
#define EXTERN
#define DEFINE

#include "lp.h"
#include "printcap.h"
#include "permission.h"
#include "setuid.h"
#include "pr_support.h"
#include "timeout.h"
#include "fileopen.h"
#include "waitchild.h"
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

struct keywords Pc_var_list[] = {
{ "ab",  FLAG_K,  &Always_banner },
   /*  always print banner, ignore lpr -h option */
{ "ac",  STRING_K,  &Allow_class },
   /*  allow these classes to be printed */
{ "achk",  FLAG_K,  &Accounting_check },
   /*  query accounting server when connected */
{ "ae",  STRING_K,  &Accounting_end },
   /*  accounting at end (see also af, la, ar, as) */
{ "af",  STRING_K,  &Accounting_file },
   /*  name of accounting file (see also la, ar) */
{ "ah",  FLAG_K,  &Auto_hold },
   /*  automatically hold all jobs */
{ "allow_getenv", FLAG_K, &Allow_getenv, 1 },
{ "ar",  FLAG_K,  &Accounting_remote },
   /*  write remote transfer accounting (if af is set) */
{ "architecture", STRING_K, &Architecture },
{ "as",  STRING_K,  &Accounting_start },
   /*  accounting at start (see also af, la, ar) */
{ "be",  STRING_K,  &Banner_end },
   /*  end banner printing program overides bp */
{ "bk",  FLAG_K,  &Backwards_compatible },
   /*  backwards-compatible: job file strictly RFC-compliant */
{ "bk_filter_options", STRING_K, &BK_filter_options },
{ "bk_of_filter_options", STRING_K, &BK_of_filter_options },
{ "bkf",  FLAG_K,  &Backwards_compatible_filter },
   /*  backwards-compatible filters: use simple paramters */
{ "bl",  STRING_K,  &Banner_line },
   /*  short banner line sent to banner printer */
{ "bp",  STRING_K,  &Banner_printer },
   /*  banner printing program (see ep) */
{ "bq",  STRING_K,  &Bounce_queue_dest },
   /*  use filters on bounce queue files */
{ "br",  INTEGER_K,  &Baud_rate },
   /*  if lp is a tty, set the baud rate (see ty) */
{ "bs",  STRING_K,  &Banner_start },
   /*  banner printing program overrides bp */
{ "cd",  STRING_K,  &Control_dir },
   /*  control directory */
{ "check_for_nonprintable", FLAG_K, &Check_for_nonprintable },
{ "client_config_file", STRING_K, &Client_config_file, 1 },
{ "cm",  STRING_K,  &Comment_tag },
   /*  comment identifying printer (LPQ) */
{ "co",  INTEGER_K,  &Cost_factor },
   /*  cost in dollars per thousand pages */
{ "connect_failure_action", STRING_K, &Connect_failure_action },
   /* connection failure causes this script to be invoked */
{ "connect_grace", INTEGER_K, &Connect_grace },
   /* connection control for remote printers */
{ "connect_interval", INTEGER_K, &Connect_interval },
   /* connection control for remote printers */
{ "connect_retry", INTEGER_K, &Connect_try },
   /* connection control for remote printers */
{ "connect_timeout", INTEGER_K, &Connect_timeout },
   /* connection control for remote printers */
{ "control_filter", STRING_K, &Control_filter },
   /* control file filter */
{ "db",  STRING_K,  &New_debug },
   /*  debug level set for queue handler */
{ "default_auth", STRING_K, &Default_auth },
   /*  default authentication */
{ "default_banner_printer", STRING_K, &Default_banner_printer },
{ "default_format", STRING_K, &Default_format },
{ "default_logger_port", STRING_K, &Default_logger_port },
{ "default_logger_protocol", STRING_K, &Default_logger_protocol },
{ "default_permission", STRING_K, &Default_permission },
{ "default_printer", STRING_K, &Default_printer },
{ "default_priority", STRING_K, &Default_priority },
{ "default_remote_host", STRING_K, &Default_remote_host },
{ "default_tmp_dir", STRING_K, &Default_tmp_dir },
{ "direct_read", FLAG_K, &Direct_read },
{ "fc",  INTEGER_K,  &Clear_flag_bits },
   /*  if lp is a tty, clear flag bits (see ty) */
{ "fd",  FLAG_K,  &Forwarding_off },
   /*  if true, no forwarded jobs accepted */
{ "ff",  STRING_K,  &Form_feed },
   /*  string to send for a form feed */
{ "filter_control", STRING_K, &Filter_control },
{ "filter_ld_path", STRING_K, &Filter_ld_path, 1 },
{ "filter_options", STRING_K, &Filter_options },
{ "filter_path", STRING_K, &Filter_path, 1 },
{ "fo",  FLAG_K,  &FF_on_open },
   /*  print a form feed when device is opened */
{ "force_queuename", STRING_K, &Force_queuename },
   /*  force use of this queuename if none provided */
{ "forward_auth", STRING_K, &Forward_auth },
   /*  do server to server authentication if authenticted by user */
{ "fq",  FLAG_K,  &FF_on_close },
   /*  print a form feed when device is closed */
{ "fs",  INTEGER_K,  &Set_flag_bits },
   /*  like `fc' but set bits (see ty) */
{ "fx",  STRING_K,  &Formats_allowed },
   /*  valid output filter formats */
{ "generate_banner", FLAG_K, &Generate_banner },
{ "group", STRING_K, &Daemon_group, 1 },
{ "hl",  FLAG_K,  &Banner_last },
   /*  print banner after job instead of before */
{ "if",  STRING_K,  &IF_Filter },
   /*  filter command, run on a per-file basis */
{ "ipv6",  FLAG_K,  &IPV6Protocol },
   /*  filter command, run on a per-file basis */
{ "kerberos_keytab", STRING_K, &Kerberos_keytab },
	/* keytab file location for kerberos, used by server */
{ "kerberos_life", STRING_K, &Kerberos_life },
	/* key lifetime for kerberos, used by server */
{ "kerberos_renew", STRING_K, &Kerberos_renew },
	/* key renewal time for kerberos, used by server */
{ "kerberos_server_principle", STRING_K, &Kerberos_server_principle },
	/* remote server principle, overides default */
{ "kerberos_service", STRING_K, &Kerberos_service },
	/* default service */
{ "la",  FLAG_K,  &Local_accounting },
   /*  write local printer accounting (if af is set) */
{ "ld",  STRING_K,  &Leader_on_open },
   /*  leader string printed on printer open */
{ "lf",  STRING_K,  &Log_file },
   /*  error log file (servers, filters and prefilters) */
{ "lk", FLAG_K,  &Lock_it },
   /* lock the IO device */
{ "localhost", STRING_K, &Localhost, 1 },
{ "lockfile", STRING_K, &Lockfile, 1 },
{ "logfile", STRING_K, &Logfile },
{ "logger_destination",  STRING_K,  &Logger_destination },
   /* where to send status information for logging */
{ "longnumber",  FLAG_K,  &Long_number },
   /*  use long job number when a job is submitted */
{ "lp",  STRING_K,  &Lp_device },
   /*  device name or lp-pipe command to send output to */
{ "lpd_port", STRING_K, &Lpd_port },
{ "lpd_printcap_path", STRING_K, &Lpd_printcap_path, 1 },
{ "lpr_bounce", FLAG_K, &Lpr_bounce },
{ "mail_operator_on_error", STRING_K, &Mail_operator_on_error },
{ "max_servers_active", INTEGER_K, &Max_servers_active },
{ "max_status_size", INTEGER_K, &Max_status_size },
{ "mc",  INTEGER_K,  &Max_copies },
   /*  maximum copies allowed */
{ "mi",  STRING_K,  &Minfree },
   /*  minimum space (Kb) to be left in spool filesystem */
{ "min_status_size", INTEGER_K, &Min_status_size },
{ "minfree", STRING_K, &Minfree },
   /**/
{ "ml",  INTEGER_K,  &Min_printable_count },
   /*  minimum printable characters for printable check */
{ "mx",  INTEGER_K,  &Max_job_size },
   /*  maximum job size (1Kb blocks, 0 = unlimited) */
{ "nb",  INTEGER_K,  &Nonblocking_open },
   /*  use nonblocking open */
{ "nw",  FLAG_K,  &NFS_spool_dir },
   /*  spool dir is on an NFS file system (see rm, rp) */
{ "of",  STRING_K,  &OF_Filter },
   /*  output filter, run once for all output */
{ "of_filter_options", STRING_K, &OF_filter_options },
{ "originate_port", STRING_K, &Originate_port },
{ "pass_env",  STRING_K,  &Pass_env },
   /* if client, pass these environment variables */
{ "pl",  INTEGER_K,  &Page_length },
   /*  page length (in lines) */
{ "poll_time",  INTEGER_K,  &Poll_time },
   /*  interval in secs between starting up all servers */
{ "pr",  STRING_K,  &Pr_program },
   /*  pr program for p format */
{ "printcap_path", STRING_K, &Printcap_path, 1 },
{ "printer_perms_path", STRING_K, &Printer_perms_path, 1 },
{ "ps",  STRING_K,  &Status_file },
   /*  printer status file name */
{ "pw",  INTEGER_K,  &Page_width },
   /*  page width (in characters) */
{ "px",  INTEGER_K,  &Page_x },
   /*  page width in pixels (horizontal) */
{ "py",  INTEGER_K,  &Page_y },
   /*  page length in pixels (vertical) */
{ "qq",  FLAG_K,  &Use_queuename },
   /*  put queue name in control file */
{ "remote_user",  STRING_K,  &Remote_user },
   /*  remote-queue machine (hostname) (with rm) */
{ "rm",  STRING_K,  &RemoteHost },
   /*  remote-queue machine (hostname) (with rm) */
{ "router",  STRING_K,  &Routing_filter },
   /*  routing filter, returns destinations */
{ "rp",  STRING_K,  &RemotePrinter },
   /*  remote-queue printer name (with rp) */
{ "rt",  INTEGER_K,  &Send_try },
   /*  number of times to try printing or transfer (0=infinite) */
{ "rw",  FLAG_K,  &Read_write },
   /*  open the printer for reading and writing */
{ "save_on_error",  FLAG_K,  &Save_on_error },
   /*  save job when an error */
{ "save_when_done",  FLAG_K,  &Save_when_done },
   /*  save job when done */
{ "sb",  FLAG_K,  &Short_banner },
   /*  short banner (one line only) */
{ "sc",  FLAG_K,  &Suppress_copies },
   /*  suppress multiple copies */
{ "sd",  STRING_K,  &Spool_dir },	/* EXPAND */
   /*  spool directory (only ONE printer per directory!) */
{ "send_block_format", FLAG_K, &Send_block_format },
   /* send block of data, rather than individual files */
{ "send_data_first", FLAG_K, &Send_data_first },
   /* failure action to take after send_try attempts failed */
{ "send_failure_action", STRING_K, &Send_failure_action },
   /* failure action to take after send_try attempts failed */
{ "send_timeout", INTEGER_K, &Send_timeout },
   /* timeout for each write to device to complete */
{ "send_try", INTEGER_K, &Send_try },
   /* numbers of times to try sending job - 0 is infinite */
{ "sendmail", STRING_K, &Sendmail, 1 },
{ "server_auth_command", STRING_K, &Server_authentication_command, 1 },
   /*  authenticate transfer command */
{ "server_config_file", STRING_K, &Server_config_file, 1 },
{ "server_tmp_dir", STRING_K, &Server_tmp_dir, 1 },
{ "server_user", STRING_K, &Server_user },
   /* server user for authentication */
{ "sf",  FLAG_K,  &No_FF_separator },
   /*  suppress form feeds separating multiple jobs */
{ "sh",  FLAG_K,  &Suppress_header },
   /*  suppress headers and/or banner page */
{ "spool_dir_perms", INTEGER_K, &Spool_dir_perms, 1 },
{ "spool_file_perms", INTEGER_K, &Spool_file_perms, 1 },
{ "spread_jobs", INTEGER_K, &Spread_jobs },
{ "ss",  STRING_K,  &Server_queue_name },
   /*  name of queue that server serves (with sv) */
{ "sv",  STRING_K,  &Server_names },
   /*  names of servers for queue (with ss) */
{ "sy",  STRING_K,  &Stty_command },
   /*  stty commands to set output line characteristics */
{ "syslog_device", STRING_K, &Syslog_device, 1 },
{ "tr",  STRING_K,  &Trailer_on_close },
   /*  trailer string to print when queue empties */
{ "translate_format",  STRING_K,  &Xlate_format },
   /*  translate format from one to another - similar to tr(1) utility */
{ "ty",  STRING_K,  &Stty_command },
   /*  stty commands to set output line characteristics */
{ "use_auth", STRING_K, &Use_auth },
   /*  use authentication */
{ "use_date",  FLAG_K,  &Use_date },
   /*  put date in control file */
{ "use_identifier",  FLAG_K,  &Use_identifier },
   /*  put identifier in control file */
{ "use_info_cache", FLAG_K, &Use_info_cache },
   /*  read and cache information */
{ "use_queuename", FLAG_K, &Use_queuename },
   /*  put queue name in control file */
{ "use_shorthost",  FLAG_K,  &Use_shorthost },
   /*  Use short hostname for lpr control and data file names */
{ "user", STRING_K, &Daemon_user, 1 },
   /*  server user for SUID purposes */
{ "user_auth_command", STRING_K, &User_authentication_command },
   /*  authenticate transfer command */
{ "xc",  INTEGER_K,  &Clear_local_bits },
   /*  if lp is a tty, clear local mode bits (see ty) */
{ "xs",  INTEGER_K,  &Set_local_bits },
   /*  like `xc' but set bits (see ty) */
{ "xt",  FLAG_K,  &Check_for_nonprintable },
   /*  formats to check for printable files */
{ "xu",  STRING_K,  &Local_permission_file },
   /*  additional permissions file for this queue */
{ (char *)0 }
} ;


struct keywords Lpd_parms[] = {
{ "Clean",  INTEGER_K , &Clean },
{ "Foreground",  INTEGER_K , &Foreground },
{ "FQDNHost",  STRING_K , &FQDNHost },
{ "FQDNRemote",  STRING_K , &FQDNRemote },
{ "Logname",  STRING_K , &Logname },
{ "Printer",  STRING_K , &Printer },
{ "ShortHost",  STRING_K , &ShortHost },
{ "ShortRemote",  STRING_K , &ShortRemote },
{ 0 }
} ;

/* force reference to Copyright */
char **Copyright_ref = Copyright;

struct keywords Keyletter[] = {
	{ "P", STRING_K, &Printer },
	{ "h", STRING_K, &ShortHost },
	{ "H", STRING_K, &FQDNHost },
	{ "a", STRING_K, &Architecture },
	{ "R", STRING_K, &RemotePrinter },
	{ "M", STRING_K, &RemoteHost },
	{ 0 }
};
