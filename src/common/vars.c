/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: vars.c
 * PURPOSE: variables
 **************************************************************************/

static char *const _id =
"$Id: vars.c,v 3.20 1998/01/12 20:29:26 papowell Exp $";

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
#ifdef GETENV
#define ENV "1"
#else
#define ENV
#endif

#if !defined(ARCHITECTURE)
#define ARCHITECTURE "unknown"
#endif


struct keywords Pc_var_list[] = {

/* START */
   /*  always print banner, ignore lpr -h option */
{ "ab",  FLAG_K,  &Always_banner,0,0},
   /*  query accounting server when connected */
{ "achk",  FLAG_K,  &Accounting_check,0,0},
   /*  accounting at end (see also af, la, ar, as) */
{ "ae",  STRING_K,  &Accounting_end,0,0,"=jobend $H $n $P $k $b $t"},
   /*  name of accounting file (see also la, ar) */
{ "af",  STRING_K,  &Accounting_file,0,0},
   /*  automatically hold all jobs */
{ "ah",  FLAG_K,  &Auto_hold,0,0},
   /* Allow duplicate command line arguments (legacy requirement) */
{ "allow_duplicate_args", FLAG_K, &Allow_duplicate_args,0,0},
   /* Allow use of LPD_CONF */
{ "allow_getenv", FLAG_K, &Allow_getenv,1,0,ENV},
   /* allow users to request logging info using lpr -mhost%port */
{ "allow_user_logging", FLAG_K, &Allow_user_logging,0,0},
   /*  write remote transfer accounting (if af is set) */
{ "ar",  FLAG_K,  &Accounting_remote,0,0,"1"},
   /* host architecture */
{ "architecture", STRING_K, &Architecture,1,0,ARCHITECTURE},
   /*  accounting at start (see also af, la, ar) */
{ "as",  STRING_K,  &Accounting_start,0,0,"=jobstart $H $n $P $k $b $t"},
   /*  end banner printing program overides bp */
{ "be",  STRING_K,  &Banner_end,0,0},
   /*  Berkeley LPD: job file strictly RFC-compliant */
{ "bk",  FLAG_K,  &Backwards_compatible,0,0},
   /*  Berkeley LPD filter options */
{ "bk_filter_options", STRING_K, &BK_filter_options,0,0,"=$P $w $l $x $y $F $c $L $i $J $C $0n $0h $-a"},
   /*  Berkeley LPD OF filter options */
{ "bk_of_filter_options", STRING_K, &BK_of_filter_options,0,0,"=$w $l $x $y"},
   /*  backwards-compatible filters: use simple paramters */
{ "bkf",  FLAG_K,  &Backwards_compatible_filter,0,0},
   /*  short banner line sent to banner printer */
{ "bl",  STRING_K,  &Banner_line,0,0,"=$-'C:$-'n Job: $-'J Date: $-'t"},
   /*  banner printing program (see bs, be) */
{ "bp",  STRING_K,  &Banner_printer,0,0},
   /*  use filters on bounce queue files */
{ "bq",  STRING_K,  &Bounce_queue_dest,0,0},
   /*  if lp is a tty, set the baud rate (see ty) */
{ "br",  INTEGER_K,  &Baud_rate,0,0},
   /* Don't default priority from classname (legacy requirement) */
{ "break_classname_priority_link", FLAG_K, &Break_classname_priority_link,0,0},
   /*  banner printing program overrides bp */
{ "bs",  STRING_K,  &Banner_start,0,0},
   /*  control directory */
{ "cd",  STRING_K,  &Control_dir,0,0},
   /* check for nonprintable file */
{ "check_for_nonprintable", FLAG_K, &Check_for_nonprintable,0,0,"1"},
   /* check for idle printer */
{ "check_idle", STRING_K, &Check_idle,0,0},
   /* Maximum length of classname argument (legacy requirement) */
{ "classname_length", INTEGER_K, &Classname_length,0,0,"#31"},
   /*  comment identifying printer (LPQ) */
{ "cm",  STRING_K,  &Comment_tag,0,0},
   /*  cost in dollars per thousand pages */
{ "co",  INTEGER_K,  &Cost_factor,0,0,"#20"},
   /* configuration file */
{ "config_file", STRING_K, &Config_file,1,0,"=/etc/lpd.conf:/usr/etc/lpd.conf"},
   /* connection control for remote printers */
{ "connect_grace", INTEGER_K, &Connect_grace,0,0,"#0"},
   /* connection control for remote printers */
{ "connect_interval", INTEGER_K, &Connect_interval,0,0,"#10"},
   /* connection control for remote printers */
{ "connect_timeout", INTEGER_K, &Connect_timeout,0,0,"#10"},
   /* control file filter */
{ "control_filter", STRING_K, &Control_filter,0,0},
   /*  debug level set for queue handler */
{ "db",  STRING_K,  &New_debug,0,0},
   /*  default authentication */
{ "default_auth", STRING_K, &Default_auth,0,0},
   /* default job format */
{ "default_format", STRING_K, &Default_format,0,0,"=f"},
   /* default port for logging info */
{ "default_logger_port", STRING_K, &Default_logger_port,0,0,"=2001"},
   /* default protocol for logging info */
{ "default_logger_protocol", STRING_K, &Default_logger_protocol,0,0,"=UDP"},
   /* default permission for files */
{ "default_permission", STRING_K, &Default_permission,0,0,"=ACCEPT"},
   /* default printer */
{ "default_printer", STRING_K, &Default_printer,0,0,"=lp"},
   /* default job priority */
{ "default_priority", STRING_K, &Default_priority,0,0,"=A"},
   /* default remote host */
{ "default_remote_host", STRING_K, &Default_remote_host,0,0,"=localhost"},
   /* default temp directory for temp files */
{ "default_tmp_dir", STRING_K, &Default_tmp_dir,0,0,"=/tmp"},
   /* printers that a route filter may return and we should query */
{ "destinations", STRING_K, &Destinations,0,0},
   /* input for filter is direct to file */
{ "direct_read", FLAG_K, &Direct_read,0,0},
   /*  if lp is a tty, clear flag bits (see ty) */
{ "fc",  INTEGER_K,  &Clear_flag_bits,0,0},
   /*  if true, no forwarded jobs accepted */
{ "fd",  FLAG_K,  &Forwarding_off,0,0},
   /*  string to send for a form feed */
{ "ff",  STRING_K,  &Form_feed,0,0,"=\\f"},
   /* filter LD_LIBRARY_PATH value */
{ "filter_ld_path", STRING_K, &Filter_ld_path,0,0,"=/lib:/usr/lib:/usr/5lib:/usr/ucblib"},
   /* filter options */
{ "filter_options", STRING_K, &Filter_options,0,0,"=$C $A $F $H $J $L $P $Q $R $Z $a $c $d $e $f $h $i $j $k $l $n $p $r $s $w $x $y $-a"},
   /* filter PATH environment variable */
{ "filter_path", STRING_K, &Filter_path,0,0,"=/bin:/usr/bin:/usr/ucb:/usr/sbin:/usr/etc:/etc"},
   /* fix bad job files */
{ "fix_bad_job", FLAG_K, &Fix_bad_job,0,0},
   /*  print a form feed when device is opened */
{ "fo",  FLAG_K,  &FF_on_open,0,0},
   /* force clients to send all requests to localhost */
{ "force_localhost",  FLAG_K,  &Force_localhost,0,0},
   /*  force use of this queuename if none provided */
{ "force_queuename", STRING_K, &Force_queuename,0,0},
   /*  do server to server authentication if authenticted by user */
{ "forward_auth", STRING_K, &Forward_auth,0,0},
   /*  print a form feed when device is closed */
{ "fq",  FLAG_K,  &FF_on_close,0,0},
   /*  like `fc' but set bits (see ty) */
{ "fs",  INTEGER_K,  &Set_flag_bits,0,0},
   /* full or complete time format */
{ "full_time",  FLAG_K,  &Full_time,0,0},
   /*  valid output filter formats */
{ "fx",  STRING_K,  &Formats_allowed,0,0,"=flp"},
   /* force a banner to be generated */
{ "generate_banner", FLAG_K, &Generate_banner,0,0},
   /* group to run SUID ROOT programs */
{ "group", STRING_K, &Daemon_group,1,0,"=daemon"},
   /*  print banner after job instead of before */
{ "hl",  FLAG_K,  &Banner_last,0,0},
   /*  filter command, run on a per-file basis */
{ "if",  STRING_K,  &IF_Filter,0,0},
   /*  filter command, run on a per-file basis */
{ "ipv6",  FLAG_K,  &IPV6Protocol,0,0},
	/* keytab file location for kerberos, used by server */
{ "kerberos_keytab", STRING_K, &Kerberos_keytab,0,0,"=/etc/lpd.keytab"},
	/* key lifetime for kerberos, used by server */
{ "kerberos_life", STRING_K, &Kerberos_life,0,0},
	/* key renewal time for kerberos, used by server */
{ "kerberos_renew", STRING_K, &Kerberos_renew,0,0},
	/* remote server principle, overides default */
{ "kerberos_server_principle", STRING_K, &Kerberos_server_principle,0,0},
	/* default service */
{ "kerberos_service", STRING_K, &Kerberos_service,0,0,"=lpr"},
   /*  write local printer accounting (if af is set) */
{ "la",  FLAG_K,  &Local_accounting,0,0,"1"},
   /*  leader string printed on printer open */
{ "ld",  STRING_K,  &Leader_on_open,0,0},
   /*  error log file (servers, filters and prefilters) */
{ "lf",  STRING_K,  &Log_file,0,0,"=log"},
   /* lock the IO device */
{ "lk", FLAG_K,  &Lock_it,0,0},
   /* name of localhost */
{ "localhost", STRING_K, &Localhost,1,0,"=localhost"},
   /* lpd lock file */
{ "lockfile", STRING_K, &Lockfile,1,0,"=/var/spool/lpd/lpd.lock.%h"},
   /* lpd log file */
{ "logfile", STRING_K, &Logfile,0,0,"=/var/spool/lpd/lpd.log.%h"},
   /* where to send status information for logging */
{ "logger_destination",  STRING_K,  &Logger_destination,0,0},
   /*  use long job number when a job is submitted */
{ "longnumber",  FLAG_K,  &Long_number,0,0},
   /*  device name or lp-pipe command to send output to */
{ "lp",  STRING_K,  &Lp_device,0,0},
   /* force a poll operation */
{ "lpd_force_poll", FLAG_K, &Force_poll,0,0,"1"},
   /*  interval in secs between starting up all servers */
{ "lpd_poll_time",  INTEGER_K,  &Poll_time,0,0,"#600"},
   /* lpd port */
{ "lpd_port", STRING_K, &Lpd_port,0,0,"=printer"},
   /* lpd printcap path */
{ "lpd_printcap_path", STRING_K, &Lpd_printcap_path,1,0,"=/etc/lpd_printcap:/usr/etc/lpd_printcap"},
   /* use lpr filtering as in bounce queue */
{ "lpr_bounce", FLAG_K, &Lpr_bounce,0,0},
   /* mail to this operator on error */
{ "mail_operator_on_error", STRING_K, &Mail_operator_on_error,0,0},
   /* maximum connection interval */
{ "max_connect_interval", INTEGER_K, &Max_connect_interval,0,0,"#60"},
   /* maximum number of servers that can be active */
{ "max_servers_active", INTEGER_K, &Max_servers_active,1,0},
   /* maximum length of status line */
{ "max_status_line", INTEGER_K, &Max_status_line,0,0,"#79"},
   /* maximum size (in K) of status file */
{ "max_status_size", INTEGER_K, &Max_status_size,0,0,"#10"},
   /*  maximum copies allowed */
{ "mc",  INTEGER_K,  &Max_copies,0,0,"#1"},
   /*  minimum space (Kb) to be left in spool filesystem */
{ "mi",  STRING_K,  &Minfree,0,0},
   /* minimum size to reduce status file to */
{ "min_status_size", INTEGER_K, &Min_status_size,0,0},
   /* minimum amount of free space needed */
{ "minfree", STRING_K, &Minfree,0,0,"=0"},
   /*  minimum number of printable characters for printable check */
{ "ml",  INTEGER_K,  &Min_printable_count,0,0},
   /*  stty commands to set output line characteristics */
{ "ms",  STRING_K,  &Stty_command,0,0},
   /* millisecond time resolution */
{ "ms_time_resolution",  FLAG_K,  &Ms_time_resolution,0,0},
   /*  maximum job size (1Kb blocks, 0 = unlimited) */
{ "mx",  INTEGER_K,  &Max_job_size,0,0},
   /*  use nonblocking open */
{ "nb",  INTEGER_K,  &Nonblocking_open,0,0},
   /* connection control for remote network printers */
{ "network_connect_grace", INTEGER_K, &Network_connect_grace,0,0,"#0"},
   /*  spool dir is on an NFS file system (see rm, rp) */
{ "nw",  FLAG_K,  &NFS_spool_dir,0,0},
   /*  output filter, run once for all output */
{ "of",  STRING_K,  &OF_Filter,0,0},
   /* OF filter options */
{ "of_filter_options", STRING_K, &OF_filter_options,0,0},
   /* orginate connections from these ports */
{ "originate_port", STRING_K, &Originate_port,0,0,"=512 1023"},
   /* if client, pass these environment variables */
{ "pass_env",  STRING_K,  &Pass_env,0,0,"=PGPPASS,PGPPATH"},
   /* lpd.perms files */
{ "perms_path", STRING_K, &Printer_perms_path,1,0,"=/etc/lpd.perms:/usr/etc/lpd.perms"},
   /*  page length (in lines) */
{ "pl",  INTEGER_K,  &Page_length,0,0,"#66"},
   /*  pr program for p format */
{ "pr",  STRING_K,  &Pr_program,0,0,"=/bin/pr"},
   /* /etc/printcap files */
{ "printcap_path", STRING_K, &Printcap_path,1,0,"=/etc/printcap:/usr/etc/printcap"},
   /*  printer status file name */
{ "ps",  STRING_K,  &Status_file,0,0,"=status"},
   /*  page width (in characters) */
{ "pw",  INTEGER_K,  &Page_width,0,0,"#80"},
   /*  page width in pixels (horizontal) */
{ "px",  INTEGER_K,  &Page_x,0,0},
   /*  page length in pixels (vertical) */
{ "py",  INTEGER_K,  &Page_y,0,0},
   /*  put queue name in control file */
{ "qq",  FLAG_K,  &Use_queuename,0,0},
   /*  operations allowed to remote host */
{ "remote_support",  STRING_K,  &Remote_support,0,0,"=RMQVC"},
   /*  remote-user name for authentication */
{ "remote_user",  STRING_K,  &Remote_user,0,0},
   /*  retry on ECONNREFUSED error */
{ "retry_econnrefused",  FLAG_K,  &Retry_ECONNREFUSED,0,0,"1"},
   /*  retry on NOLINK connection */
{ "retry_nolink",  FLAG_K,  &Retry_NOLINK,0,0,"1"},
   /*  return short status when specified remotehost */
{ "return_short_status",  STRING_K,  &Return_short_status,0,0},
   /*  set SO_REUSEADDR on outgoing ports */
{ "reuse_addr",  FLAG_K,  &Reuse_addr,0,0},
   /*  reverse LPQ status format when specified remotehost */
{ "reverse_lpq_status",  STRING_K,  &Reverse_lpq_status,0,0},
   /*  restrict client users to members of group */
{ "rg",  STRING_K,  &Restricted_group,0,0},
   /*  remote-queue machine (hostname) (with rp) */
{ "rm",  STRING_K,  &RemoteHost,0,0},
   /*  routing filter, returns destinations */
{ "router",  STRING_K,  &Routing_filter,0,0},
   /*  remote-queue printer name (with rp) */
{ "rp",  STRING_K,  &RemotePrinter,0,0},
   /*  number of times to try printing or transfer (0=infinite) */
{ "rt",  INTEGER_K,  &Send_try,0,0},
   /*  open the printer for reading and writing */
{ "rw",  FLAG_K,  &Read_write,0,0},
   /*  additional safe characters to use in control files */
{ "safe_chars",  STRING_K,  &Safe_chars,1,0},
   /*  save job when an error */
{ "save_on_error",  FLAG_K,  &Save_on_error,0,0},
   /*  save job when done */
{ "save_when_done",  FLAG_K,  &Save_when_done,0,0},
   /*  short banner (one line only) */
{ "sb",  FLAG_K,  &Short_banner,0,0},
   /*  suppress multiple copies */
{ "sc",  FLAG_K,  &Suppress_copies,0,0},
   /*  spool directory (only ONE printer per directory!) */
{ "sd",  STRING_K,  &Spool_dir,0,0},	/* EXPAND */
   /* send block of data, rather than individual files */
{ "send_block_format", FLAG_K, &Send_block_format,0,0},
   /* failure action to take after send_try attempts failed */
{ "send_data_first", FLAG_K, &Send_data_first,0,0},
   /* failure action to take after send_try attempts failed */
{ "send_failure_action", STRING_K, &Send_failure_action,0,0,"=remove"},
   /* timeout for read/write lpr IO operatons */
{ "send_job_rw_timeout", INTEGER_K, &Send_job_rw_timeout,0,0,"#6000"},
   /* timeout for read/write status or control operatons */
{ "send_query_rw_timeout", INTEGER_K, &Send_query_rw_timeout,0,0,"#30"},
   /* numbers of times to try sending job - 0 is infinite */
{ "send_try", INTEGER_K, &Send_try,0,0,"#3"},
   /* sendmail program */
{ "sendmail", STRING_K, &Sendmail,1,0,"=/usr/sbin/sendmail -oi -t"},
   /*  authenticate transfer command */
{ "server_auth_command", STRING_K, &Server_authentication_command,0,0},
   /* server temporary file directory */
{ "server_tmp_dir", STRING_K, &Server_tmp_dir,1,0,"=/tmp"},
   /* server user for authentication */
{ "server_user", STRING_K, &Server_user,0,0,"=daemon"},
   /*  suppress form feeds separating multiple jobs */
{ "sf",  FLAG_K,  &No_FF_separator,0,0},
   /*  suppress headers and/or banner page */
{ "sh",  FLAG_K,  &Suppress_header,0,0},
   /*  short status length in lines */
{ "short_status_length",  INTEGER_K,  &Short_status_length,0,0,"#3"},
   /* set the SO_LINGER socket option */
{ "socket_linger", INTEGER_K,  &Socket_linger,0,0,"#10"},
   /* spool directory permissions */
{ "spool_dir_perms", INTEGER_K, &Spool_dir_perms,1,0,"#042700"},
   /* spool file permissions */
{ "spool_file_perms", INTEGER_K, &Spool_file_perms,1,0,"#000600"},
   /* amount to spread jobs to avoid collisions */
{ "spread_jobs", INTEGER_K, &Spread_jobs,0,0,"#0"},
   /*  name of queue that server serves (with sv) */
{ "ss",  STRING_K,  &Server_queue_name,0,0},
   /*  stalled job timeout */
{ "stalled_time", INTEGER_K, &Stalled_time,0,0,"#120"},
   /*  stop processing queue on filter abort */
{ "stop_on_abort",  FLAG_K,  &Stop_on_abort,0,0,"1"},
   /*  names of servers for queue (with ss) */
{ "sv",  STRING_K,  &Server_names,0,0},
   /*  stty commands to set output line characteristics */
{ "sy",  STRING_K,  &Stty_command,0,0},
   /* name of syslog device */
{ "syslog_device", STRING_K, &Syslog_device,1,0,"=/dev/console"},
   /*  trailer string to print when queue empties */
{ "tr",  STRING_K,  &Trailer_on_close,0,0},
   /*  translate format from one to another - similar to tr(1) utility */
{ "translate_format",  STRING_K,  &Xlate_format,0,0},
   /*  stty commands to set output line characteristics */
{ "ty",  STRING_K,  &Stty_command,0,0},
   /*  use authentication */
{ "use_auth", STRING_K, &Use_auth,0,0},
   /*  put date in control file */
{ "use_date",  FLAG_K,  &Use_date,0,0},
   /*  put identifier in control file */
{ "use_identifier",  FLAG_K,  &Use_identifier,0,0},
   /*  read and cache information */
{ "use_info_cache", FLAG_K, &Use_info_cache,0,0,"1"},
   /*  put queue name in control file */
{ "use_queuename", FLAG_K, &Use_queuename,0,0},
   /*  Use short hostname for lpr control and data file names */
{ "use_shorthost",  FLAG_K,  &Use_shorthost,0,0},
   /*  server user for SUID purposes */
{ "user", STRING_K, &Daemon_user,1,0,"=daemon"},
   /*  authenticate transfer command */
{ "user_auth_command", STRING_K, &User_authentication_command,0,0},
   /* LPC commands for SERVICE=U permissions checks */
{ "user_lpc", STRING_K, &User_lpc,1,0},
   /*  if lp is a tty, clear local mode bits (see ty) */
{ "xc",  INTEGER_K,  &Clear_local_bits,0,0},
   /*  like `xc' but set bits (see ty) */
{ "xs",  INTEGER_K,  &Set_local_bits,0,0},
   /*  formats to check for printable files */
{ "xt",  FLAG_K,  &Check_for_nonprintable,0,0},
   /*  additional permissions file for this queue */
{ "xu",  STRING_K,  &Local_permission_file,0,0},
/* END */
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
