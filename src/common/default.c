/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: default.c
 * PURPOSE: default printcap and configuration information
 **************************************************************************/

static char *const _id =
"$Id: default.c,v 3.7 1996/08/25 22:20:05 papowell Exp papowell $";

char Default_configuration[]
 =
#ifdef GETENV
"allow_getenv yes\n"
#endif
#if !defined(ARCHITECTURE)
#define ARCHITECTURE "unknown"
#endif
"architecture " ARCHITECTURE "\n"
"bk_filter_options $P $w $l $x $y $F $c $L $i $J $C $0n $0h $-a\n"
"bk_of_filter_options $w $l $x $y\n"
"check_for_nonprintable yes\n"
"client_config_file /etc/lpd.conf:/etc/lpd_client.conf:/usr/etc/lpd.conf:/usr/etc/lpd_client.conf:/usr/spool/lpd/lpd.conf:/usr/spool/lpd/lpd_client.conf\n"
"connect_grace 0\n"
"connect_interval 10\n"
"connect_retry 0\n"
"connect_timeout 10\n"
"default_banner_printer\n"
"default_format f\n"
"default_logger_protocol UDP\n"
"default_logger_port 2001\n"
"default_permission ACCEPT\n"
"default_printer\n"
"default_priority A\n"
"default_remote_host %H\n"
"domain_name\n"
"filter_ld_path /lib:/usr/lib:/usr/5lib:/usr/ucblib\n"
"filter_options $C $F $H $J $L $P $Q $R $Z $a $c $d $e $f $h $i $j $k $l $n $s $w $x $y $-a\n"
"filter_path /bin:/usr/bin:/usr/ucb:/usr/sbin:/usr/etc:/etc\n"
"group daemon\n"
"lockfile  /var/spool/lpd/lpd.lock.%h\n"
"logfile  /var/spool/lpd/lpd.log.%h\n"
"longnumber no\n"
"lpd_port printer\n"
"lpd_printcap_path /etc/lpd_printcap:/usr/etc/lpd_printcap:\n"
"mail_operator_on_error\n"
"max_status_size 10\n"
"min_status_size 0\n"
"minfree\n"
"of_filter_options\n"
"originate_port 721 731\n"
"printcap_path /etc/printcap:/usr/etc/printcap:/var/spool/lpd/printcap.%h\n"
"printer_perms_path /etc/lpd.perms:/usr/etc/lpd.perms:/var/spool/lpd/lpd.perms.%h\n"
"send_data_first no\n"
"send_failure_action abort\n"
"send_timeout 6000\n"
"send_try 0\n"
"sendmail /usr/lib/sendmail -oi -t\n"
"server_config_file /etc/lpd.conf:/usr/etc/lpd.conf:/usr/spool/lpd/lpd.conf.%h\n"
"spool_dir_perms  042700\n"
"spool_file_perms 000600\n"
"syslog_device /dev/console\n"
"use_identifier no\n"
"use_info_cache yes\n"
"use_queuename no\n"
"use_shorthost no\n"
"user  daemon\n"
;

/*
 * Default printcap variable values.  If there is not value here,
 * default is 0 for numerical or empty string for string variable
 */

char Default_printcap_var[]
 =
"ae=jobend $H $n $P $k $b $t\n"
"ar\n"
"as=jobstart $H $n $P $k $b $t\n"
"bl=$-'C:$-'n Job: $-'J Date: $-'t\n"
"co#20\n"
"ff=\\f\n"
"fx=flp\n"
"la\n"
"lf=log\n"
"li=NULL\n"
"lo=lock\n"
"mc#1\n"
"ml#32\n"
"mx#1000\n"
"pl#66\n"
"pr=/bin/pr\n"
"pw#80\n"
"rs#300\n"
"rt#3\n"
"st=status\n"
;
