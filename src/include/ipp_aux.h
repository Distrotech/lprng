/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * IPP internet printing protocol
 * copyright 2008 Vaclav Michalek
 *
 * See LICENSE for conditions of use.
 ***************************************************************************/


#ifndef _IPP_AUX_H_
#define _IPP_AUX_H_

#include "lp.h"
#include "ipp.h"
#include "permission.h"

/*ipp subscription constatns & defines*/
#define	EVENT_PRINTER			0x0000
#define	EVENT_JOB			0x8000
#define	EVENT_DATA			0x4000
#define EVENT_PR_STATECHANGE		0x0400
#define	EVENT_PR_STOPPED		0x0200
#define	EVENT_JOB_STATECHANGE		0x2000
#define	EVENT_JOB_CREATED		0x1000
#define	EVENT_JOB_COMPLETED		0x0800

/*mask for jobid extraction*/
#define	NTF_MAX_EVENTS			8
#define EVENTS_PRINTER			(EVENT_PR_STATECHANGE | EVENT_PR_STATECHANGE)
#define EVENTS_JOB			(EVENT_JOB_STATECHANGE | EVENT_JOB_CREATED | EVENT_JOB_COMPLETED)
#define	EVENT_JOBID_MASK		(EVENT_JOB | EVENT_DATA | EVENTS_JOB)

#define subscribe_high_jobid(X)		((X) >= EVENT_JOBID_MASK)
#define is_printer_subscription(X)	(!((X) & EVENT_JOB))
#define is_job_subscription(X)		((X) & EVENT_JOB)
#define is_data_subscription(X)		((X) & EVENT_DATA)
#define	subscription_jobid(X)		((X) & EVENT_JOBID_MASK)

/*various functions for ipp*/
int Ipa_check_password(const char *username, const char *password);
ssize_t Base64_decode(char **dest, char *src);
char *Local_to_utf8(const char *s);
char *Utf8_to_local(const char *s);

int order_filter_job_number(struct job *job, void *jobnum);

int Ipa_validate_charset(char **charset, struct ipp_attr *attributes);
int Ipa_validate_language(char **lang, struct ipp_attr *attributes);
int Ipa_get_printer(char **printer, char **ppath, struct ipp_attr *attributes, int auth_scope);
int Ipa_get_job_id(int *job_id, struct ipp_attr *attributes);
int Ipa_get_printer_uri(char **uri, int ipp_version, int port, const char *ppath, const char *printer);
int Ipa_get_printername(char **printername, const char *ppath, const char *printer);
int Ipa_get_printcap_auth(struct line_list *auths, struct line_list *pc_entry,
		const char *ppath, const char *printer, struct perm_check *perms);
int Ipa_get_all_printcap_auth(struct line_list *auths, const char *ppath, const char *printer, struct perm_check *perms);
int Ipa_get_job_uri(char **uri, int ipp_version, int port, const char *ppath, const char *printer, int jobnum);
int Ipa_ipp2jobid(int *ntv_id, char **printer, int ipp_id);
int Ipa_jobid2ipp(const char *printer, int job_id);
int Ipa_set_perm(struct perm_check *persm, char service, struct line_list *auth_info);
void Ipa_prase_debug(int mask);
int Ipa_get_printer_state(int *state, char **reason, int *job_accept, int *job_count);
int Ipa_get_job_state(int *jobstate, char **reason, struct job *sjob, struct line_list *spool_control);
int Ipa_remove_job(const char *printer, struct line_list *auth_info, int jobnum, struct line_list *done_list);

char *Iph_CUPS_ppd_uri(const char *uri);
int Ipa_ppd_fd(int *fd, struct stat *fstat, const char *ppath, const char *printer);
size_t Ipa_copy_fd(const struct http_conn *out_conn, int in_fd, size_t size) ;

int Ipa_int_priority(char priority, int reverse);
char Ipa_char_priority(int priority, int reverse);

#endif
