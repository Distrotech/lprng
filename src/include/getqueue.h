/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 * $Id: getqueue.h,v 5.2 1999/10/12 18:50:51 papowell Exp papowell $
 ***************************************************************************/



#ifndef _GETQUEUE_H
#define _GETQUEUE_H

EXTERN char *CTRL_A_str DEFINE( = "\001" );

EXTERN const char * ACTION			DEFINE( = "action" );
EXTERN const char * ACTIVE_TIME		DEFINE( = "active_time" );
EXTERN const char * ALL				DEFINE( = "all" );
EXTERN const char * ADDR			DEFINE( = "addr" );
EXTERN const char * ATTEMPT			DEFINE( = "attempt" );
EXTERN const char * AUTHFROM		DEFINE( = "authfrom" );
EXTERN const char * AUTHINFO		DEFINE( = "authinfo" );
EXTERN const char * AUTOHOLD		DEFINE( = "autohold" );
EXTERN const char * AUTHTYPE		DEFINE( = "authtype" );
EXTERN const char * AUTH_CLIENT_ID	DEFINE( = "auth_client_id" );
EXTERN const char * AUTH_FROM_ID	DEFINE( = "auth_from_id" );
EXTERN const char * BNRNAME			DEFINE( = "bnrname" );
EXTERN const char * CALL			DEFINE( = "call" );
EXTERN const char * CF_ESC_IMAGE	DEFINE( = "cf_esc_image" );
EXTERN const char * CF_OUT_IMAGE	DEFINE( = "cf_out_image" );
EXTERN const char * CLASS			DEFINE( = "class" );
EXTERN const char * CLIENT			DEFINE( = "client" );
EXTERN const char * COPIES			DEFINE( = "copies" );
EXTERN const char * COPY_DONE		DEFINE( = "copy_done" );
EXTERN const char * DATALINES		DEFINE( = "datalines" );
EXTERN const char * DATE			DEFINE( = "date" );
EXTERN const char * DEBUG			DEFINE( = "debug" );
EXTERN const char * DEBUGFV			DEFINE( = "debugfv" );
EXTERN const char * DEST			DEFINE( = "dest" );
EXTERN const char * DESTINATION		DEFINE( = "destination" );
EXTERN const char * DESTINATIONS	DEFINE( = "destinations" );
EXTERN const char * DF_NAME			DEFINE( = "df_name" );
EXTERN const char * DMALLOC_OPTIONS	DEFINE( = "DMALLOC_OPTIONS" );
EXTERN const char * DMALLOC_OUTFILE	DEFINE( = "dmalloc_outfile" );
EXTERN const char * DONE_TIME		DEFINE( = "done_time" );
EXTERN const char * DUMP			DEFINE( = "dump" );
EXTERN const char * END				DEFINE( = "end" );
EXTERN const char * ERROR			DEFINE( = "error" );
EXTERN const char * FILE_HOSTNAME	DEFINE( = "file_hostname" );
EXTERN const char * FILENAMES		DEFINE( = "filenames" );
EXTERN const char * FILTER			DEFINE( = "filter" );
EXTERN const char * FORMAT			DEFINE( = "format" );
EXTERN const char * FORMAT_ERROR	DEFINE( = "format_error" );
EXTERN const char * FORWARDING		DEFINE( = "forwarding" );
EXTERN const char * FORWARD_ID		DEFINE( = "forward_id" );
EXTERN const char * FROM			DEFINE( = "from" );
EXTERN const char * FROMHOST		DEFINE( = "fromhost" );
EXTERN const char * HELD			DEFINE( = "held" );
EXTERN const char * HF_IMAGE		DEFINE( = "hf_image" );
EXTERN const char * HF_NAME			DEFINE( = "hf_name" );
EXTERN const char * HOLD_ALL		DEFINE( = "hold_all" );
EXTERN const char * HOLD_CLASS		DEFINE( = "hold_class" );
EXTERN const char * HOLD_TIME		DEFINE( = "hold_time" );
EXTERN const char * HOST			DEFINE( = "host" );
EXTERN const char * IDENTIFIER		DEFINE( = "identifier" );
EXTERN const char * IDLE			DEFINE( = "idle" );
EXTERN const char * INPUT			DEFINE( = "input" );
EXTERN const char * JOBNAME			DEFINE( = "jobname" );
EXTERN const char * JOBSEQ			DEFINE( = "jobseq" );
EXTERN const char * JOB_TIME		DEFINE( = "job_time" );
EXTERN const char * JOB_TIME_USEC	DEFINE( = "job_time_usec" );
EXTERN const char * KERBEROS		DEFINE( = "kerberos" );
EXTERN const char * KERBEROS4		DEFINE( = "kerberos4" );
EXTERN const char * KERBEROS5		DEFINE( = "kerberos5" );
EXTERN const char * KEYID			DEFINE( = "keyid" );
EXTERN const char * LOCALHOST		DEFINE( = "localhost" );
EXTERN const char * LOG				DEFINE( = "log" );
EXTERN const char * LOGGER			DEFINE( = "logger" );
EXTERN const char * LOGNAME			DEFINE( = "logname" );
EXTERN const char * LPC				DEFINE( = "lpc" );
EXTERN const char * LPD				DEFINE( = "lpd" );
EXTERN const char * LPD_ACK_FD		DEFINE( = "lpd_ack_fd" );
EXTERN const char * LPD_CONF		DEFINE( = "LPD_CONF" );
EXTERN const char * LPD_PORT		DEFINE( = "lpd_port" );
EXTERN const char * LPD_REQUEST		DEFINE( = "lpd_request" );
EXTERN const char * MAILNAME		DEFINE( = "mailname" );
EXTERN const char * MAIL_FD			DEFINE( = "mail_fd" );
EXTERN const char * MOVE			DEFINE( = "move" );
EXTERN const char * MOVE_DEST		DEFINE( = "move_dest" );
EXTERN const char * MSG				DEFINE( = "msg" );
EXTERN const char * NAME			DEFINE( = "name" );
EXTERN const char * NEW_DEST		DEFINE( = "new_dest" );
EXTERN const char * NONEP			DEFINE( = "none" );
EXTERN const char * NUMBER			DEFINE( = "number" );
EXTERN const char * OPENNAME		DEFINE( = "openname" );
EXTERN const char * ORIG_IDENTIFIER	DEFINE( = "orig_identifier" );
EXTERN const char * PGP				DEFINE( = "pgp" );
EXTERN const char * PORT			DEFINE( = "port" );
EXTERN const char * PRINTABLE		DEFINE( = "printable" );
EXTERN const char * PRINTER			DEFINE( = "printer" );
EXTERN const char * PRIORITY		DEFINE( = "priority" );
EXTERN const char * PRIORITY_TIME	DEFINE( = "priority_time" );
EXTERN const char * PROCESS			DEFINE( = "process" );
EXTERN const char * PRSTATUS		DEFINE( = "prstatus" );
EXTERN const char * QUEUE			DEFINE( = "queue" );
EXTERN const char * QUEUENAME		DEFINE( = "queuename" );
EXTERN const char * REDIRECT		DEFINE( = "redirect" );
EXTERN const char * REMOTEUSER		DEFINE( = "remoteuser" );
EXTERN const char * REMOVE_TIME		DEFINE( = "remove_time" );
EXTERN const char * SEQUENCE		DEFINE( = "sequence" );
EXTERN const char * SD				DEFINE( = "sd" );
EXTERN const char * SERVER			DEFINE( = "server" );
EXTERN const char * SERVER_ORDER	DEFINE( = "server_order" );
EXTERN const char * SERVICE			DEFINE( = "service" );
EXTERN const char * SIZE			DEFINE( = "size" );
EXTERN const char * SORT_KEY		DEFINE( = "sort_key" );
EXTERN const char * SPOOLDIR		DEFINE( = "spooldir" );
EXTERN const char * STATE			DEFINE( = "state" );
EXTERN const char * START_TIME		DEFINE( = "start_time" );
EXTERN const char * STATUS_FD		DEFINE( = "status_fd" );
EXTERN const char * STATUS_CHANGE	DEFINE( = "status_change" );
EXTERN const char * SUBSERVER		DEFINE( = "subserver" );
EXTERN const char * TRACE			DEFINE( = "trace" );
EXTERN const char * TRANSFERNAME	DEFINE( = "transfername" );
EXTERN const char * UPDATE			DEFINE( = "update" );
EXTERN const char * UPDATE_TIME		DEFINE( = "update_time" );
EXTERN const char * USER			DEFINE( = "user" );
EXTERN const char * VALUE			DEFINE( = "value" );

EXTERN const char * PRINTING_DISABLED DEFINE( = "printing_disabled" );
EXTERN const char * PRINTING_ABORTED DEFINE( = "printing_aborted" );
EXTERN const char * QUEUE_CONTROL_FILE DEFINE( = "queue_control_file" );
EXTERN const char * QUEUE_STATUS_FILE DEFINE( = "queue_status_file" );
EXTERN const char * SPOOLING_DISABLED DEFINE( = "spooling_disabled" );

/* PROTOTYPES */
int Scan_queue( const char *dirpath, struct line_list *spool_control,
	struct line_list *sort_order, int *pprintable, int *pheld, int *pmove,
		int only_pr_and_move );
char *Get_file_image( const char *dir, const char *file, int maxsize );
int Get_file_image_and_split( const char *dir, const char *file,
	int maxsize, int clean,
	struct line_list *l, const char *sep,
	int sort, const char *keysep, int uniq, int trim, int nocomments );
void Is_set_str_value( struct line_list *l, const char *key, char *s);
int Setup_cf_info( const char *dir, struct line_list *cf_line_list,
	struct job *job);
void Setup_job( struct job *job, struct line_list *spool_control, const char *dir,
	const char *cf_name, const char *hf_name,
	struct line_list *cf_line_list );
int Get_hold_class( struct line_list *info, struct line_list *sq );
void Get_datafile_info( const char *dir, struct line_list *cf_line_list, struct job *job );
int Set_hold_file( struct job *job, struct line_list *perm_check );
void Get_spool_control( const char *dir, const char *cf,
	const char *printer, struct line_list *info );
void Set_spool_control(
	struct line_list *perm_check, const char *dir, const char *scf,
	const char *printer, struct line_list *info );
void intval( const char *key, struct line_list *list, struct line_list *info );
void revintval( const char *key, struct line_list *list, struct line_list *info );
void strzval( const char *key, struct line_list *list, struct line_list *info );
void strnzval( const char *key, struct line_list *list, struct line_list *info );
void strval( const char *key, struct line_list *list, struct line_list *info,
	int reverse );
char *Make_sort_key( struct job *job );
int Setup_printer( char *name, char *error, int errlen );
int Read_pid( int fd, char *str, int len );
void Write_pid( int fd, int pid, char *str );
int Patselect( struct line_list *token, struct line_list *cf, int starting );
int Check_format( int type, const char *name, struct job *job );
char *Find_start(char *str, const char *key );
char *Frwarding(struct line_list *l);
int Pr_disabled(struct line_list *l);
int Sp_disabled(struct line_list *l);
int Pr_aborted(struct line_list *l);
int Hld_all(struct line_list *l);
char *Clsses(struct line_list *l);
char *Cntrol_debug(struct line_list *l);
char *Srver_order(struct line_list *l);
void Init_job( struct job *job );
void Free_job( struct job *job );
void Copy_job( struct job *dest, struct job *src );
char *Fix_job_number( struct job *job, int n );
char *Make_identifier( struct job *job );
void Dump_job( char *title, struct job *job );
void Job_printable( struct job *job, struct line_list *spool_control,
	int *pprintable, int *pheld, int *pmove );
int Server_active( char *dir, char *file );
void Update_destination( struct job *job );
int Get_destination( struct job *job, int n );
int Get_destination_by_name( struct job *job, char *name );
int Trim_status_file( char *path, int max, int min );
char *Fix_datafile_info( struct job *job, char *number, char *suffix );
int ordercomp( char *order, const void *left, const void *right );
int BSD_sort( const void *left, const void *right );
int LPRng_sort( const void *left, const void *right );
int Fix_control( struct job *job, char *filter );
int Create_control( struct job *job, char *error, int errlen );
void Init_buf(char **buf, int *max, int *len);
void Put_buf_len( char *s, int cnt, char **buf, int *max, int *len );
void Put_buf_str( char *s, char **buf, int *max, int *len );
void Free_buf(char **buf, int *max, int *len);

#endif
