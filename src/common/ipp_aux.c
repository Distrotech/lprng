/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 *
 * ipp protocol service
 * copyright 2008 Vaclav Michalek, Joint laboratory of Optics, Olomouc
 *
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/


/*#include "ipp.h"*/
#include "ipp_aux.h"
#include "utilities.h"
#include "linelist.h"
#include "getprinter.h"
#include "getqueue.h"
#include "permission.h"
#include "gethostinfo.h"
#include "lpd_remove.h"
#include "fileopen.h"
#include "sendreq.h"
#include "getqueue.h"

#ifdef HAVE_PAM_AUTHENTICATE
#include "security/pam_appl.h"
#include "security/pam_modules.h"
#endif

#ifdef HAVE_GETSPNAM
#include <shadow.h>
#endif

#ifdef HAVE_CRYPT
	char *crypt(const char *key, const char *salt);
#endif

#include <langinfo.h>
#include <iconv.h>

/*various supporting/auxiliary fucntions for ipp*/

static inline char char64decode(char c)
{

	/*for ascii - EBCDIC has groups A-I J-R S-Z a-i j-r s-z*/
	if ((c >= 'A')  && (c <= 'I')) 	return c - 'A';
	if ((c >= 'J')  && (c <= 'R')) 	return c - 'J' + 9;
	if ((c >= 'S')  && (c <= 'Z')) 	return c - 'S' + 18;

	if ((c >= 'a')  && (c <= 'i')) 	return c - 'a' + 26;
	if ((c >= 'j')  && (c <= 'r')) 	return c - 'j' + 35;
	if ((c >= 's')  && (c <= 'z')) 	return c - 's' + 44;
	if ((c >= '0')  && (c <= '9')) 	return c - '0' + 52;
	if (c == '+') return 62;
	if (c == '/') return 63;
	return 0;
	/*slow, but portable - untested*/
	static const char b64t[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	char *r = strchr(b64t, c);
	if (r) return (r - b64t);
	return 0;
}

/* performs base64 decoding
 * returns   length of decoded stream, not including added zero
 * dest      new allocated decoded string, followed by zero
 * */
ssize_t Base64_decode(char **dest, char *src)
{
	/*"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"*/
	ssize_t dl, sl, i, j;
	int pad = 0;

	sl = safestrlen(src);
	if ((sl > 2) && (src[sl - 2] == '=')) pad = 2;
	else if ((sl > 1) && (src[sl - 1] == '=')) pad = 1;

	dl = (sl * 3)/4 - pad;

	*dest = malloc_or_die(dl + 1, __FILE__, __LINE__);

	DEBUGF(DNW4)("b64 decode: src %s, srclen %zd destlen %zd", src, sl, dl);

	for (i = 0, j = 0; i < sl; i += 4, j += 3)
	{
		(*dest)[j] = (char64decode(src[i]) << 2) |
			       (char64decode(src[i+1]) >> 4);
		if ((sl - i > 3) || (pad !=2))
			(*dest)[j+1] =
			       ((char64decode(src[i+1]) & 0xf) << 4) |
			       (char64decode(src[i+2]) >> 2);
		if ((sl - i > 3) || (!pad))
			(*dest)[j+2] =
			       ((char64decode(src[i+2]) & 3) << 6) |
			       (char64decode(src[i+3]) & 0x3f);
	}
	(*dest)[dl] = '\0';
	return dl;
}

#define UTF_8	"UTF-8"

/* convert string in local charset to utf-8
 * return new malloced string
 *
 */
char *Local_to_utf8(const char *s)
{

	/*get the local charset*/
	char *lc = nl_langinfo(CODESET);
	if (!safestrcmp(UTF_8, lc)) {
		return safestrdup(s, __FILE__, __LINE__);
	}
	iconv_t ih = iconv_open(UTF_8, lc);
	if (ih == (iconv_t)(-1)) {
		return safestrdup(s, __FILE__, __LINE__);
	}

	size_t sz = safestrlen(s);
	size_t rz = 2*safestrlen(s)+2;
	char *rs = malloc_or_die(rz, __FILE__, __LINE__);
	char *sc = (char *)s;
	char *rc = rs;
	size_t res = iconv(ih, &sc, &sz, &rc, &rz);
	rc[0] = '\0';

	DEBUGF(DNW4)("Local_to_utf8: return %zd, input %s:%s inbytesleft %zd, output %s, outbytesleft %zd", res, s, lc, sz, rs, rz);

	iconv_close(ih);
	return rs;
}

char *Utf8_to_local(const char *s)
{
	/*get the local charset*/
	char *lc = nl_langinfo(CODESET);
	if (!safestrcmp(UTF_8, lc)) {
		return safestrdup(s, __FILE__, __LINE__);
	}
	iconv_t ih = iconv_open(lc, UTF_8);
	if (ih == (iconv_t)(-1)) {
		return safestrdup(s, __FILE__, __LINE__);
	}

	size_t sz = safestrlen(s);
	size_t rz = 2*safestrlen(s)+2;
	char *ss = safestrdup(s, __FILE__, __LINE__);
	char *sc = ss;
	char *rs = malloc_or_die(rz, __FILE__, __LINE__);
	char *rc = rs;
	size_t res;

	do {
		res = iconv(ih, &sc, &sz, &rc, &rz);
		if (sz) sc[0] = '*';
	} while (sz && (res != (size_t)(-1) || errno != E2BIG));

	rc[0] = '\0';

	DEBUGF(DNW4)("Utf8_to_local: return %zd, input %s inbytesleft %zd, output %s:%s, outbytesleft %zd", res, s, sz, rs, lc, rz);

	if (ss) free(ss);

	iconv_close(ih);
	return rs;

}



#ifdef HAVE_PAM_AUTHENTICATE
/* PAM conversation function
 *   Here we assume (for now, at least) that echo on means login name, and
 *   echo off means password.
 *   */
static int PAM_conv(int num_msg, const struct pam_message **msg, struct pam_response **resp, void *appdata_ptr)
{
        int replies = 0;
        struct pam_response *reply = NULL;
	void *(*data)[2] = appdata_ptr;

	reply = malloc_or_die(sizeof(struct pam_response) * num_msg, __FILE__, __LINE__);
	for (replies = 0; replies < num_msg; replies++) {
#ifdef SOLARIS
		switch ((*msg)[replies].msg_style) {
#else
		switch ((*msg[replies]).msg_style) {
#endif
			case PAM_PROMPT_ECHO_ON:
				/*return PAM_CONV_ERR;*/
				reply[replies].resp_retcode = PAM_SUCCESS;
				reply[replies].resp = safestrdup((char *)(*data)[0], __FILE__, __LINE__);
				break;
			case PAM_PROMPT_ECHO_OFF:
				reply[replies].resp_retcode = PAM_SUCCESS;
				reply[replies].resp = safestrdup((char *)(*data)[1], __FILE__, __LINE__);
				break;
			case PAM_TEXT_INFO:
			case PAM_ERROR_MSG:
				/*ignore*/
				reply[replies].resp_retcode = PAM_SUCCESS;
				reply[replies].resp = NULL;
				break;
			default:
				/*Must be an error of some sort... */
				return PAM_CONV_ERR;
		}
	}
	*resp = reply;
	return PAM_SUCCESS;
}
#endif

/*(Http Basic) password authentication*/
int Ipa_check_password(const char *username, const char *password)
{
	int rs = 0;

#ifdef HAVE_PAM_AUTHENTICATE

	pam_handle_t *pamh = NULL;
	struct pam_conv conv;
	int pam_status;
	void *data[2];

	data[0] = (void *)username;
	data[1] = (void *)password;
	conv.conv = PAM_conv;
	conv.appdata_ptr = data;

	/*make sure that syslog is opened with valid ident*/
	openlog(IPPC_PAM_SERVICE, 0, LOG_LPR);
	pam_status = pam_start(IPPC_PAM_SERVICE, username, &conv, &pamh);
	DEBUGF(DNW4)("Ipa_check_password: pam_start: service %s, username %s, status %d", IPPC_PAM_SERVICE, username, pam_status);
	if (pam_status == PAM_SUCCESS) {
		/*unfortunately PAM must be suid*/
		To_euid_root();
		pam_status = pam_authenticate(pamh, 0);
		if (pam_status != PAM_SUCCESS && geteuid()!=0)
		{
			WARNMSG( _("Effective UID is not root, PAM may not work properly (EUID=%d)"), geteuid());
		}
		To_daemon();
	}
	if (pam_status == PAM_SUCCESS)	{
		To_euid_root();
		pam_status = pam_acct_mgmt(pamh, 0);
		To_daemon();
	}
	rs = (pam_status == PAM_SUCCESS);
	pam_end(pamh, pam_status);
	To_daemon();
	closelog();

#elif HAVE_CRYPT
	struct passwd pwb, *p;
	int sz = sysconf(_SC_GETPW_R_SIZE_MAX);
	char *buf = malloc_or_die(sz, __FILE__, __LINE__);


	if ((rs = getpwnam_r(username, &pwb, buf, sz, &p))) {
		DEBUGF(DNW4)("Ipa_check_password: getpwnam_r failed (username %s) status %d", username, rs);
		goto procend;
	}
	rs = (!safestrcmp(p->pw_passwd, crypt(password, p->pw_passwd)));
#       if defined HAVE_GETSPNAM_R || defined HAVE_GETSPNAM

	struct spwd *s;
#        ifdef HAVE_GETSPNAM_R
	struct spwd swb;
#        endif
	if (!rs) {
		To_euid_root();
#        ifdef HAVE_GETSPNAM_R
		if (getspnam_r(username, &swb, buf, sz, &s)) {
#        else
		if (!(s = getspnam(username))) {
#        endif
			if (geteuid() != 0) {
				WARNMSG( _("Effective UID is not root, shadow password may not work (EUID=%d)"), geteuid());
			}
			To_daemon();
			DEBUGF(DNW4)("Ipa_check_password: getspnam failed (username %s)", username);
			goto procend;
		}
		To_daemon();
		rs = (!safestrcmp(s->sp_pwdp, crypt(password, s->sp_pwdp)));
	}

#       endif /*GETSPNAM_R GETSPNAM*/

 procend:;

	if (buf) free(buf);
#else
	/*no auth function*/
#endif
	DEBUGF(DNW4)("Ipa_check_password: result %d", rs);
	return rs;
}

/*routine for Scan_queue_proc to sort/filter by job number to have first-job-id as first*/
int order_filter_job_number(struct job *job, void *jobnum)
{
	int rs = 1;
	int number;

	intval(NUMBER, &(job->info), job);
	if (jobnum) {
		number = *(int *)(jobnum);
		DEBUGF(DNW4)("order_filter_job_number: jobnum %d, number %d ", number, Find_decimal_value(&(job->info), NUMBER));
		if (number && (Find_decimal_value(&(job->info), NUMBER) == number)) rs = 0; else rs = -1;
	}
	return rs;
}

int Ipa_validate_charset(char **charset, struct ipp_attr *attributes)
{
	struct ipp_attr *a;

	a = Ipp_get_attr(attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_CHARSET, -1);
	if (!a) return CLIENT_ERROR_BAD_REQUEST;

	/*only one value for charset is acceptable*/
	if (!Ipp_get_attr(attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_CHARSET, -1))
		return CLIENT_ERROR_BAD_REQUEST;

	/*we know only utf-8*/
	if (!safestrncmp(a->value, IPPAV_UTF_8, a->value_len)) {
		if (charset) *charset = (char *)IPPAV_UTF_8;
		return SUCCESSFUL_OK;
	}

	return CLIENT_ERROR_CHARSET_NOT_SUPPORTED;

}

int Ipa_validate_language(char **lang, struct ipp_attr *attributes)
{
	struct ipp_attr *a;

	a = Ipp_get_attr(attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_NAT_LANG, -1);
	if (!a) return CLIENT_ERROR_BAD_REQUEST;

	/*only one value for language is acceptable*/
	if (!Ipp_get_attr(attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_NAT_LANG, -1))
		return CLIENT_ERROR_BAD_REQUEST;

	return SUCCESSFUL_OK;
}

/* get printer name from IPP request (printer-uri)
 * for various IPP operation types (printer, job, subscribe ...)
 * the printer is new dynamically alloced string
 * the ppath is new malloced url directory "printers" or "raw"
 * */
int Ipa_get_printer(char **printer, char **ppath, struct ipp_attr *attributes, int auth_scope)
{
	struct ipp_attr *a, *b;

	char uri[IPPC_URI_LEN+1];
	struct line_list uri_parts;
	int i, rs;
	int job_uri = 0, subscription = 0;
	struct line_list pc_entry, pc_alias;
	char *mainname, *c;

	*printer = NULL;
	*ppath = NULL;

	if (auth_scope == AUTHS_DEST_PRINTER)
		/*target printer-uri for CUPS-Move-Job operation*/
		a = Ipp_get_attr(attributes, JOB_ATTRIBUTES_GRP, 0, IPPAN_JOB_PRINTER_URI, -1);
	else {
		/*decide what attribute to use - printer-uri of job-uri (only for job operations) */
		a = Ipp_get_attr(attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_PRINTER_URI, -1);
		if (((auth_scope == AUTHS_JOB) ||
		     (auth_scope == AUTHS_PRINTER_OR_JOB) ||
		     ((auth_scope == AUTHS_ALL_PRINTERS) && (Ipp_getjobs_compat_DYN & 2)))
		     && !a) {
			a = Ipp_get_attr(attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_JOB_URI, -1);
			job_uri = 1;
		}
	}
	if (!a) return CLIENT_ERROR_BAD_REQUEST;
	if (a->value_len > IPPC_URI_LEN)
		return CLIENT_ERROR_REQUEST_VALUE_TOO_LONG;

	/*mandatory attribute for some subscription operations*/
	if (auth_scope == AUTHS_SUBSCRIPTION) {
		b = Ipp_get_attr(attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_NTF_SUBSCRIPTION_ID, -1);
		if (!b || (b->value_len != 4)) return CLIENT_ERROR_BAD_REQUEST;
		subscription = ntoh32(b->value);
	}

	memcpy(uri, a->value, a->value_len); uri[a->value_len] = '\0';
	Init_line_list(&uri_parts);
	Split(&uri_parts, uri, "/", 0, NULL, 0, 0, 0, NULL);
	/*DEBUGFC(DNW4)Dump_line_list("Ipp_get_printer: uri_parts", &uri_parts);
	DEBUGF(DNW4)("Compat val: %d, scope %d", Ipp_getjobs_compat_DYN, auth_scope);*/

	Init_line_list(&pc_entry);
	Init_line_list(&pc_alias);
	/*Get-All-Jobs CUPS uri test  (CUPS printer-uri "ipp://host/" or job-uri "ipp://localhost/jobs") */
	if ((auth_scope == AUTHS_ALL_PRINTERS) &&
	    (((Ipp_getjobs_compat_DYN & 1) && (uri_parts.count == 3)) ||
	     ((Ipp_getjobs_compat_DYN & 2) && (uri_parts.count == 4) && (!strcmp(uri_parts.list[3], IPPC_ALLJOBS_CUPS_PATH)))
	    )
	   ) {
		*printer = safestrdup(IPPC_ALLPRINTERS, __FILE__, __LINE__);
		*ppath = safestrdup(IPPC_PRINTERS_PATH, __FILE__, __LINE__); /*needed valid path - path from uri is returned in printer-uri in ipp response */
		rs = SUCCESSFUL_OK;
		goto endproc;
	}
	/*CUPS job with number: job-uri "ipp://localhost/jobs/N" */
	if ((uri_parts.count == 5) && job_uri && ((auth_scope == AUTHS_JOB) || (auth_scope == AUTHS_PRINTER_OR_JOB)) &&
	    (Ipp_getjobs_compat_DYN & 3) && (!strcmp(uri_parts.list[3], IPPC_ALLJOBS_CUPS_PATH))
	   ) {
		/*select printer by ipp-transformed-to-unique job number*/
		i = strtol(uri_parts.list[4], &c, 10); /*transformed job number*/
		if (*c !=0) {
			rs = CLIENT_ERROR_NOT_FOUND;
			goto endproc;
		}
		Ipa_ipp2jobid(NULL, printer, i);
		*ppath = safestrdup(IPPC_PRINTERS_PATH, __FILE__, __LINE__);
		rs = SUCCESSFUL_OK;
		goto endproc;
	}
	/*notifications CUPS printer-uri "/" test*/
	if ((uri_parts.count == 1) && (auth_scope == AUTHS_SUBSCRIPTION_PRINTERS) &&
	    ((Ipp_getjobs_compat_DYN & 3) && (!strcmp(uri, IPPC_NTF_ALLPRINTERS_CUPS_URI)))
	   ) {
		*printer = safestrdup(IPPC_ALLPRINTERS, __FILE__, __LINE__);
		*ppath = safestrdup(IPPC_PRINTERS_PATH, __FILE__, __LINE__); /*needed valid path - path from uri is returned in printer-uri*/
		rs = SUCCESSFUL_OK;
		goto endproc;
	}
	/*notification CUPS printer-uri "/" with required subscription-id*/
	if ((uri_parts.count == 1) && (auth_scope == AUTHS_SUBSCRIPTION) &&
	    ((Ipp_getjobs_compat_DYN & 3) && (!strcmp(uri, IPPC_NTF_ALLPRINTERS_CUPS_URI)))
	   ) {
		if (is_job_subscription(subscription)) {
			Ipa_ipp2jobid (NULL, printer, subscription_jobid(subscription));
		}
		if (!*printer) *printer = safestrdup(IPPC_ALLPRINTERS, __FILE__, __LINE__);
		*ppath = safestrdup(IPPC_PRINTERS_PATH, __FILE__, __LINE__);
		rs = SUCCESSFUL_OK;
		goto endproc;
	}
	/*notification CUPS printer-uri "/" with possible notify-job-id*/
	if ((uri_parts.count == 1) && (auth_scope == AUTHS_SUBSCRIPTION_ATTRIBUTES) &&
	    ((Ipp_getjobs_compat_DYN & 3) && (!strcmp(uri, IPPC_NTF_ALLPRINTERS_CUPS_URI)))
	   ) {
		b = Ipp_get_attr(attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_NTF_JOB_ID, -1);
		if (b && (b->value_len == 4)) {
			Ipa_ipp2jobid (NULL, printer, ntoh32(b->value));
		}
		if (!*printer) *printer = safestrdup(IPPC_ALLPRINTERS, __FILE__, __LINE__);
		*ppath = safestrdup(IPPC_PRINTERS_PATH, __FILE__, __LINE__);
		rs = SUCCESSFUL_OK;
		goto endproc;
	}

	/*lprng standard printer-uri "scheme://host/printers/PRINTERNAME" or job-uri "scheme://host/printers/PRINTERNAME/jobs/N" */
	if ((uri_parts.count != (job_uri ? 7 : 5)) ||
	    ((strcmp(uri_parts.list[3], IPPC_PRINTERS_PATH) && strcmp(uri_parts.list[3], IPPC_PRINTERS_PATH_RAW))) ||
            (job_uri && strcmp(uri_parts.list[5], IPPC_JOBS_PATH)) ||
	    (safestrlen(uri_parts.list[4]) == 0) ||
	    Is_clean_name(uri_parts.list[4]) )
	{
		rs = CLIENT_ERROR_NOT_FOUND;
		goto endproc;
	}
	/*test printername validity*/
	mainname = Select_pc_info(uri_parts.list[4], &pc_entry, &pc_alias, &PC_names_line_list, &PC_order_line_list, &PC_info_line_list, 0, 0);
	if (!mainname)
	{
		rs = CLIENT_ERROR_NOT_FOUND; /*invalid printer name*/
		goto endproc;
	}
        *printer = safestrdup(mainname, __FILE__, __LINE__);
	*ppath = safestrdup(uri_parts.list[3], __FILE__, __LINE__);
	rs = SUCCESSFUL_OK;

 endproc:


	Free_line_list(&uri_parts);
	Free_line_list(&pc_entry);
	Free_line_list(&pc_alias);
	return rs;

}
/* return job-id from request
 * use job-id attribute, if not found, job-uri attribute
 * */
int Ipa_get_job_id(int *job_id, struct ipp_attr *attributes)
{
	int rs, j;
	struct ipp_attr *a;
	char uri[IPPC_URI_LEN+1];
	struct line_list uri_parts;
	char *c;

	a = Ipp_get_attr(attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_JOB_ID, -1);
	if (a && a->value_len == 4) {
		if (job_id) Ipa_ipp2jobid(job_id, NULL, ntoh32(a->value));
		return SUCCESSFUL_OK;
	}
	a = Ipp_get_attr(attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_JOB_URI, -1);
	if (!a) return CLIENT_ERROR_BAD_REQUEST;
	if (a->value_len > IPPC_URI_LEN)
		return CLIENT_ERROR_REQUEST_VALUE_TOO_LONG;

	memcpy(uri, a->value, a->value_len); uri[a->value_len] = '\0';
	Init_line_list(&uri_parts);
	Split(&uri_parts, uri, "/", 0, NULL, 0, 0, 0, NULL);
	if (uri_parts.count == 7) { /*standard job-uri "scheme://host:port/printers/PRINTERNAME/jobs/N"*/
		j = strtol(uri_parts.list[6], &c, 10);
		if (*c !=0) {
			rs = CLIENT_ERROR_NOT_FOUND;
			goto endproc;
		}
	} else if ((Ipp_getjobs_compat_DYN & 3) && (uri_parts.count == 5) &&
		   (!strcmp(uri_parts.list[3], IPPC_ALLJOBS_CUPS_PATH))
		  ) {  /* CUPS non-standard "ipp://localhost/jobs/N" without printer specification*/
		j = strtol(uri_parts.list[4], &c, 10);
		if (*c !=0) {
			rs = CLIENT_ERROR_NOT_FOUND;
			goto endproc;
		}
		Ipa_ipp2jobid(&j, NULL, j);
	} else {
		rs = CLIENT_ERROR_NOT_FOUND;
		goto endproc;
	}

	if (job_id) *job_id = j;
	rs = SUCCESSFUL_OK;

 endproc:
	Free_line_list(&uri_parts);
	return rs;

}

/* transform (unique) IPP job number (coming from IPP request) into
 *   pair of native job id "ntv_id"
 *           and corresponding printer (new allocated string)
 * this is for CUPS compatibility
 * */
int Ipa_ipp2jobid(int *ntv_id, char **printer, int ipp_id)
{

	if (printer) *printer = NULL;

	if (Ipp_getjobs_compat_DYN & 3) {
		Get_all_printcap_entries();
		if (subscribe_high_jobid(All_line_list.count * Ipp_compat_hrcount_DYN)) {
			/*too many printers - no jobid transformation (printer count*hrcount  cannot fit into job subscription ID size) */
			if (printer) *printer = safestrdup(IPPC_ALLPRINTERS, __FILE__, __LINE__);
			if (ntv_id) *ntv_id = ipp_id;
		} else {
			if (ipp_id > All_line_list.count * Ipp_compat_hrcount_DYN) {
				if (printer) *printer = safestrdup(All_line_list.list[ipp_id % All_line_list.count], __FILE__, __LINE__);
				if (ntv_id) *ntv_id = ipp_id / All_line_list.count + 1;
			} else {
				if (printer) *printer = safestrdup(All_line_list.list[ipp_id / Ipp_compat_hrcount_DYN], __FILE__, __LINE__);
				if (ntv_id) *ntv_id = ipp_id % Ipp_compat_hrcount_DYN;
			}
		}
	} else {
		if (ntv_id) *ntv_id = ipp_id;
	}

	if (DEBUGFSET(DNW4)) {
		if (ntv_id) LOGDEBUG("ipp2 jobid ipp: %d, native %d",ipp_id, *ntv_id);
	}

	return 0;
}

/*transform internal job number job_id to be unique within the whole server for compatibility with
  bad CUPS clients that relies on job-id uniquiness*/
int Ipa_jobid2ipp(const char *printer, int job_id)
{
	int uqj, idx;

	if (Ipp_getjobs_compat_DYN & 3) {
		Get_all_printcap_entries();
		if ((subscribe_high_jobid(job_id * All_line_list.count)) /*too much high original job_id*/
		    || (subscribe_high_jobid(All_line_list.count * Ipp_compat_hrcount_DYN)) /*too much printers*/
		   )
			uqj = job_id;
		else {
			for (idx = All_line_list.count - 1; idx && (safestrcmp(printer, All_line_list.list[idx])); idx-- );
			if (job_id < Ipp_compat_hrcount_DYN)
				uqj = idx * Ipp_compat_hrcount_DYN + job_id;        /*make small job id user friendly; it also will allow sorted job-id (for small job-id) as required by Get-Jobs with CUPS ugly first-job-id attribute */
			else
				uqj = (job_id - 1) * All_line_list.count + idx;
		}
	} else {
		uqj = job_id;
	}
	return uqj;
}

/*construct priter-uri for ipp response*/
int Ipa_get_printer_uri(char **uri, int ipp_version, int port, const char *ppath, const char *printer)
{
	char ury[IPPC_URI_LEN+1];
	char *scheme;

#ifdef	SSL_ENABLE
	if (port == ipp_ippsport) scheme = IPPC_SCHEME_HTTPS;
	else
#endif
	     if (ipp_version < 11) scheme = IPPC_SCHEME_HTTP;
	else scheme = IPPC_SCHEME_IPP;

	plp_snprintf(ury, sizeof(ury), "%s://%s:%d/%s/%s", scheme, FQDNHost_FQDN, port, ppath, printer);
	ury[IPPC_URI_LEN] = '\0';

	*uri = safestrdup(ury, __FILE__, __LINE__);
	return 0;
}
/*construct job-uri for ipp response*/
int Ipa_get_job_uri(char **uri, int ipp_version, int port, const char *ppath, const char *printer, int jobnum)
{
	char ury[IPPC_URI_LEN+1];
	char *scheme;

#ifdef	SSL_ENABLE
	if (port == ipp_ippsport) scheme = IPPC_SCHEME_HTTPS;
	else
#endif
	     if (ipp_version < 11) scheme = IPPC_SCHEME_HTTP;
	else scheme = IPPC_SCHEME_IPP;

	plp_snprintf(ury, sizeof(ury), "%s://%s:%d/%s/%s/%s/%d", scheme, FQDNHost_FQDN, port, ppath, printer, IPPC_JOBS_PATH, jobnum );
	ury[IPPC_URI_LEN] = '\0';

	*uri = safestrdup(ury, __FILE__, __LINE__);
	return 0;
}
/*construct printer-name for ipp response*/
int Ipa_get_printername(char **printername, const char *ppath, const char *printer)
{
	*printername = safestrdup(printer, __FILE__, __LINE__);
	return 0;
}

/*return authentication methods for printer from printcap printer entry in line list */
int Ipa_get_printcap_auth(struct line_list *auths, struct line_list *pc_entry,
		const char *ppath, const char *printer, struct perm_check *perms)
{

	char *k;
	struct line_list l;
	int ds = 0;
	int i;
	struct perm_check pp;

	Free_line_list(auths);

	Init_line_list(&l);
	k = Find_str_value(pc_entry, PC_AUTH);
	Split(&l, k, ",", 0, NULL, 0, 0, 0, NULL);

	memcpy(&pp, perms, sizeof(struct perm_check));
	pp.service = '*';
	pp.printer = printer;
	pp.ppath = ppath;
	pp.remoteuser = NULL;
	pp.user = NULL;
        pp.authuser = NULL;

	for (i = 0; i < l.count; i++)
	{
		if (!safestrcmp(l.list[i], PCV_USRNAME))
		{
			pp.authtype = l.list[i];
			if (Perms_check(&Perm_line_list, &pp, 0, 0) != P_REJECT) {
				Add_line_list(auths, IPPAV_RQ_USRNAME, "", 1, 1);
				ds = 1;
			}
		}
		if (!safestrcmp(l.list[i], PCV_BASIC))
		{
			pp.authtype = l.list[i];
			if (Perms_check(&Perm_line_list, &pp, 0, 0) != P_REJECT) {
				Add_line_list(auths, IPPAV_BASIC, "", 1, 1);
				ds = 1;
			}
		}

	}
	Free_line_list(&l);
	if (!ds) Add_line_list(auths, IPPAV_RQ_USRNAME, "", 1, 1); /*MUST return at least usrname if no auth type defined in printcap*/

	return 0;
}

/*return common authentication types for all printers (if exists; used for CUPS get-all-jobs specific call) */
int Ipa_get_all_printcap_auth(struct line_list *auths, const char *ppath, const char *printer, struct perm_check *perms)
{
	struct line_list sb;
	struct line_list pc_entry, pc_alias;
	int i, j, k;

	Get_all_printcap_entries();
	for (i = 0; i < All_line_list.count; i++) {
		Init_line_list(&pc_entry);
		Init_line_list(&pc_alias);
		Select_pc_info(printer, &pc_entry, &pc_alias, &PC_names_line_list, &PC_order_line_list, &PC_info_line_list, 0, 0);
		if (i) {
			Init_line_list(&sb);
			Ipa_get_printcap_auth(&sb, &pc_entry, ppath, All_line_list.list[i], perms);
			for (j = auths->count - 1; j >= 0 ; j--) {
				for (k = sb.count - 1; k >=0 ; k--) {
					if (!safestrcmp(auths->list[j],sb.list[k])) goto found;
				}
				Remove_line_list(auths, j);
			found:	; /*auth type found*/
			}
			Free_line_list(&sb);
		} else {
			Ipa_get_printcap_auth(auths, &pc_entry, ppath, All_line_list.list[i], perms);
		}
		Free_line_list(&pc_entry);
		Free_line_list(&pc_alias);
	}

	DEBUGFC(DNW4)Dump_line_list("Ipa_get_all_printcap_auth: common auth types for \"all\" printer", auths);

	return 0;
}

/* fills struct perm_check using values provided by auth_info
 * */
int Ipa_set_perm(struct perm_check *persm, char service, struct line_list *auth_info)
{
	persm->service = service;
	persm->printer = Find_str_value(auth_info, KWA_PRINTER);
	persm->ppath = Find_str_value(auth_info, KWA_PPATH);
	persm->remoteuser = Find_str_value(auth_info, KWA_USER);
	persm->authtype = Find_str_value(auth_info, KWA_AUTHTYPE);
	if (persm->authtype) {
		persm->authuser = persm->remoteuser;
		if (!persm->authfrom) persm->authfrom = Find_str_value(auth_info, KWA_AUTHFROM);
		if (!persm->authfrom) persm->authca = Find_str_value(auth_info, KWA_AUTHCA);
	}
	persm->host = 0;
	return 0;
}

/* set some debugflag and operation specific lprng debug environment
 * */
void Ipa_prase_debug(int mask)
{

	int db = Debug;
	int dbflag = DbgFlag;
	char *s = Find_str_value(&Spool_control,DEBUG);
	int fd;

	if( !s ) s = New_debug_DYN;
	Parse_debug( s, 0 );
	if( !(mask & DbgFlag) ){
		Debug = db;
		DbgFlag = dbflag;
	} else {
		int odb, odbf;
		odb = Debug;
		odbf = DbgFlag;
		Debug = db;
		DbgFlag = dbflag;
		if( Log_file_DYN ){
			fd = Trim_status_file( -1, Log_file_DYN, Max_log_file_size_DYN,
				Min_log_file_size_DYN );
			if( fd > 0 && fd != 2 ){
				dup2(fd,2);
				close(fd);
				close(fd);
			}
		}
		Debug = odb;
		DbgFlag = odbf;
	}
}


/* input: printer is identified by previous Setup_printer() call
 * returns ipp printer-state as integer,
 *             printer-state-reason (only one string)
 *             printer-is-accepting-jobs
 *             queued-job-count
 * */
int Ipa_get_printer_state(int *state, char **reason, int *job_accept, int *job_count)
{
	int held, move, err, done;

	Free_line_list(&Spool_control);
	Get_spool_control(Queue_control_file_DYN, &Spool_control);
	if (Scan_queue(&Spool_control, NULL, /*&Sort_order,*/ job_count, &held, &move,
			1, &err, &done, 0, 0)) {
		return SERVER_ERROR_INTERNAL_ERROR;
	}
	/*Free_line_list(&Sort_order);*/

	/*DEBUGFC(DNW4)Dump_line_list("Ipp_get_printer_state: spool_control", &Spool_control);*/

	*state = IPPAV_PRS_IDLE;
	*reason = (char *)IPPAV_PRSR_NONE;
	if (*job_count) *state = IPPAV_PRS_PROCESSING;
	if (Pr_disabled(&Spool_control))  {
		*state = IPPAV_PRS_STOPPED;
		*reason = (char *)IPPAV_PRSR_PAUSED;
	}

	*job_accept = !Sp_disabled(&Spool_control);

	return SUCCESSFUL_OK;
}

int Ipa_get_job_state(int *jobstate, char **reason, struct job *sjob, struct line_list *spool_control)
{
	int printable, held, move, err, done, i, remove;

	Job_printable(sjob, spool_control, &printable, &held, &move, &err, &done);
	remove = Find_flag_value(&sjob->info, REMOVE_TIME);

	if (remove && done) *jobstate = IPPAV_JRS_COMPLETED;
	else if (remove && err && !(done = Find_flag_value(&sjob->info, DONE_TIME)) ) *jobstate = IPPAV_JRS_ABORTED; /*if error=1, Job_printabel does not test "done"*/
	else if (remove && err && done) *jobstate = IPPAV_JRS_CANCELED;
	else if (err) *jobstate = IPPAV_JRS_STOPPED;
	else if (held) *jobstate = IPPAV_JRS_HELD;
	else if (printable && ((i = Find_flag_value(&sjob->info, SERVER)) /*active or stalled - see Job_printable*/
			       && (kill(i, 0) == 0)))
		*jobstate = IPPAV_JRS_PROCESSING;
	else if (printable) *jobstate = IPPAV_JRS_PENDING;
	else *jobstate = IPPAV_JRS_STOPPED;

	if (reason) *reason = (char *)IPPAV_JSR_NONE;

	return SUCCESSFUL_OK;
}

/*this is copy of lpd_remove: Get_queue_remove*/
int Ipa_remove_job(const char *printer,struct line_list *auth_info, int jobnum, struct line_list *done_list)
{
	int localq;    /*printer is local*/
	int control_perm, pid,  count, removed, permission, fd;
        int jticket_fd = -1;
	char error[SMALLBUFFER];
	int errlen = sizeof(error);
	struct job job;
	int rs = SUCCESSFUL_OK;
	struct stat statb;
	struct line_list active_pid, aux_list;
	char *rlist;
	char *username;


	username = Find_str_value(auth_info, KWA_USER);
	if (!username) username = IPPC_USER_ANONYMOUS;

	Init_job(&job);
	Init_line_list(&active_pid);
	Init_line_list(&aux_list);


	DEBUGF(DNW2)("Ipa_remove_job: printer %s username %s job %d", printer, username, jobnum);

	localq = (safestrchr(printer, '@') == NULL);
	if (!localq) {
		Set_DYN(&Printer_DYN, printer);
		Fix_Rm_Rp_info(NULL, 0);
		localq = Find_fqdn(&LookupHost_IP, RemoteHost_DYN) &&
			 ( !Same_host(&LookupHost_IP, &Host_IP) ||
			   !Same_host(&LookupHost_IP, &Localhost_IP));
	}
	if (!localq) {
		/*remote remove*/
		/*unfortunately UNTESTED*/
		int fd;
		struct line_list tokens;
		char cnum[SMALLBUFFER];
		struct stat statb;
		int outfd;
		Init_line_list(&tokens);
		Add_line_list(&tokens, username, "", 0, 0);
		plp_snprintf(cnum, sizeof(cnum), "%d", jobnum); cnum[sizeof(cnum)-1] = '\0';
		Add_line_list(&tokens, cnum, "", 0, 0);
		DEBUGF(DNW4)("Ipa_remove_job: lprm to Remote printer %s@%s", RemotePrinter_DYN, RemoteHost_DYN);
		outfd = Checkwrite ("/dev/null", &statb, 0, 0, 0);
		fd = Send_request('M', REQ_REMOVE, tokens.list, Connect_timeout_DYN, Send_query_rw_timeout_DYN, outfd);
		close(outfd);
		if (fd >=0 ) {
			shutdown(fd, 1);
			close(fd);
		}
		Free_line_list(&tokens);
		rs = SUCCESSFUL_OK;
		goto rmend;
	}
	if (Find_exists_value(done_list, printer, Hash_value_sep)) {
		rs = SUCCESSFUL_OK;
		goto rmend;
	}
	Add_line_list(done_list, Printer_DYN, Hash_value_sep, 1, 1);

	if (Setup_printer((char *)printer, error, errlen, 0))	{
		DEBUGF(DLPRM2)("Ipa_remove_job: cannot setup printer %s", Printer_DYN);
		rs = SERVER_ERROR_INTERNAL_ERROR;
		goto rmend;
	}

	Perm_check.service = 'C';
	Perm_check.printer = Printer_DYN;
	Perm_check.ppath = Find_str_value(auth_info, KWA_PPATH);
	Perm_check.user = NULL;
	control_perm = (Perms_check(&Perm_line_list, &Perm_check, 0, 0) == P_ACCEPT);

	/*try find & remove*/
	Free_line_list(&Spool_control);
	Get_spool_control(Queue_control_file_DYN, &Spool_control);
	Scan_queue_proc(&Spool_control, &Sort_order, 0, 0, 0, 0, 0, 0, 0, 0, order_filter_job_number, &jobnum);

	DEBUGF(DNW2)("Ipa_remove_job: printer %s username %s job %d", printer, username, jobnum);
	for(count = 0, removed = 0; count < Sort_order.count; count++) {
		/*get job info*/
		Free_job(&job);
		if (jticket_fd > 0) close(jticket_fd); jticket_fd = -1;
		Get_job_ticket_file(&jticket_fd, &job, Sort_order.list[count]);
		if (!job.info.count) {
			/*got lpd_status.c*/
			continue;
		}
		/*filtered by order_filter_job_number
		if (jobnum != Find_decimal_value(&job.info, NUMBER)) {
			continue;
		}*/
		/*check permission*/
		if (!control_perm) {
			char *s;
			struct host_information *h;
			h = Perm_check.host;
			Perm_check.service = 'M';
			Perm_check.user = Find_str_value(&job.info, LOGNAME);
			Perm_check.host = 0;
			if ((s = Find_str_value(&job.info, FROMHOST)) &&
			    Find_fqdn(&PermHost_IP, s)) {
				Perm_check.host = &PermHost_IP;
			}
			permission = Perms_check(&Perm_line_list, &Perm_check, &job, 1);
			Perm_check.host = h;
			if (permission != P_ACCEPT) {
				DEBUGF(DLPRM3)("Ipa_remove_job: permisson denied (job %d)", jobnum);
				rs = CLIENT_ERROR_NOT_AUTHENTICATED;
				goto rmend;
			}
		}
		/*do remove*/
		pid = Find_flag_value(&job.info, INCOMING_PID);
		if (Find_flag_value(&job.info, INCOMING_TIME) && pid && kill(pid, SIGINT)) {
			DEBUGF(DLPRM4)("Ipa_remove_job: remove incoming job %d", jobnum);
		} else {
			DEBUGF(DLPRM4)("Ipa_remove_job: remove job %d", jobnum);
		}
		setmessage(&job, "IPP-LPRM", "start");
		if (Find_flag_value(&job.info, REMOVE_TIME)) {
			if (Remove_job(&job)) {
				setmessage(&job, "IPP-LPRM", "error");
				rs = SERVER_ERROR_INTERNAL_ERROR;
				goto rmend;
			}
		} else {
			Set_flag_value(&job.info, ERROR_TIME, time((void *)0));
			Set_str_value(&job.info, ERROR, "Job canceled by user");
			Set_flag_value(&job.info, DONE_TIME, time((void *)0));
			Set_flag_value(&job.info, REMOVE_TIME, time((void *)0));
			Free_line_list(&aux_list);
			Perm_check_to_list(&aux_list, &Perm_check);
			if (Set_job_ticket_file(&job, &aux_list, jticket_fd)) {
				rs = SERVER_ERROR_INTERNAL_ERROR;
				DEBUGF(DLPRM4)("Ipa_remove_job: Set_job_ticket file error");
				goto rmend;
			}
			if (Server_queue_name_DYN) Set_flag_value(&Spool_control, CHANGE, 1);
			Set_spool_control(&aux_list, Queue_control_file_DYN, &Spool_control);
		}
		setmessage(&job, "IPP-LPRM", "success");
		/*job in active state*/
		if ((pid = Find_flag_value(&job.info, SERVER))) {
			DEBUGF(DLPRM4)("Ipa_remove_job: active server pid %d", pid);
			if (!kill(pid, 0)) {
				Check_max(&active_pid, 1);
				active_pid.list[active_pid.count++] = Cast_int_to_voidstar(pid);/*krpa*/
			}
		}
		removed++;
		break;
	}

	Free_line_list(&Sort_order);
	if (removed) {
		for (count = 0; count < active_pid.count; count++) {
			pid = Cast_ptr_to_int(active_pid.list[count]);
			DEBUGF(DLPRM2)("Ipa_remove_job: killing server pid %d SIGHUP/SIGINT/SIGQUIT/SIGCONT", pid);
			killpg(pid, SIGHUP);  kill(pid, SIGHUP);
			killpg(pid, SIGINT);  kill(pid, SIGINT);
			killpg(pid, SIGQUIT); kill(pid, SIGQUIT);
			killpg(pid, SIGCONT); kill(pid, SIGCONT);
		}
		/*kill spooler*/
		pid = 0;
		if ((fd = Checkread(Queue_lock_file_DYN, &statb)) >= 0) {
			pid = Read_pid(fd);
			close(fd);
		}
		DEBUGF(DLPRM2)("Ipa_remove_job: checking spooler pid %d", pid);
		if (pid) kill(pid, SIGUSR2);
	}
	/*dequeque load-balance servers and routed destinations */
	rlist = Server_names_DYN;
	if (!rlist) rlist = Destinations_DYN;
	if (rlist) {
		/*recursive subprocess remove*/
		Free_line_list(&aux_list);
		Split(&aux_list, rlist, File_sep, 0, 0, 0, 0, 0, 0);
		for (count = 0; count < aux_list.count; count++) {
			DEBUGF(DLPRM2)("Ipa_remove_job: subserver/destination call %s", aux_list.list[count]);
			rs = Ipa_remove_job(aux_list.list[count], auth_info, jobnum, done_list);
			DEBUGF(DLPRM2)("Ipa_remove_job: subserver/destination finished %s", aux_list.list[count]);
		}
	}

 rmend:
	Free_line_list(&aux_list);
	if (jticket_fd > 0) close(jticket_fd);
	/*active_pid line list does not contain strings, but casted numbers - nothing to free*/
	for (count = 0; count < active_pid.count; count++)  active_pid.list[count] = NULL;
	Free_line_list(&active_pid);
	Free_job(&job);
	return rs;
}

/*identify whetehr the uri is ppd file GET request; return malloced printer name or null */
char *Iph_CUPS_ppd_uri(const char *uri)
{
	char *s = NULL;
	struct line_list uri_parts;
	int p, e;
	struct line_list pc_entry, pc_alias;
	char *mainname;

	Init_line_list(&uri_parts);
	Init_line_list(&pc_entry);
	Init_line_list(&pc_alias);

	Split(&uri_parts, uri, "/", 0, NULL, 0, 0, 0, NULL);
	/*DEBUGFC(DNW4)Dump_line_list("Iph_CUPS_ppd_uri: uri_parts", &uri_parts);*/
	if ((uri_parts.count == 3) && (!safestrcmp(uri_parts.list[1], IPPC_PRINTERS_PATH)) &&
	    ((p = safestrlen(uri_parts.list[2])) > (e = safestrlen(IPPC_CUPS_PPD_EXT)))) {
	    /*CUPS ppd file candidate, extract printer name */
		uri_parts.list[2][p-e] = '\0'; /*remove *.ppd extension*/
		mainname = Select_pc_info(uri_parts.list[2], &pc_entry, &pc_alias, &PC_names_line_list, &PC_order_line_list, &PC_info_line_list, 0, 0);
		if (mainname) {
			s = safestrdup(mainname, __FILE__, __LINE__);
		}
	}
	Free_line_list(&uri_parts);
	Free_line_list(&pc_entry);
	Free_line_list(&pc_alias);

	return s;
}

/*open PPD file for a printer*/
int Ipa_ppd_fd(int *fd, struct stat *fstat, const char *ppath, const char *printer)
{
	struct line_list pc_entry, pc_alias;
	char *ppd = NULL;

	*fd = -1;
	Init_line_list(&pc_entry);
	Init_line_list(&pc_alias);
	Select_pc_info(printer, &pc_entry, &pc_alias, &PC_names_line_list, &PC_order_line_list, &PC_info_line_list, 0, 0);
	ppd = Find_str_value(&pc_entry, PC_IPPD);

	if (ppd) *fd = Checkread(ppd, fstat);

	Free_line_list(&pc_entry);
	Free_line_list(&pc_alias);

	return (*fd >= 0);
}

/*Write content of file descriptor fd to http connection */
size_t Ipa_copy_fd(const struct http_conn *out_conn, int in_fd, size_t size)
{
	char buf[LARGEBUFFER];
	size_t sent = 0, bs;
	int c;

	do {
		bs = (size - sent);
		if (bs > sizeof(buf)) bs = sizeof(buf);
		/*DEBUGF(DNW4)("Ipa_copy_fd: size %ld, sent %ld, count %d, bs %ld", size, sent, c, bs);*/
		c = Read_fd_len_timeout(out_conn->timeout, in_fd, buf, bs);
		/*DEBUGF(DNW4)("Ipa_copy_fd: size %ld, sent %ld, count %d", size, sent, c);*/
		if (c <= 0) break;
		sent += c;
		c = (out_conn->iofunc->write_conn_len_timeout)(out_conn, buf, bs);
		if (c < 0) break;
	} while (sent < size);
	return sent;
}

/*convert Lprng priority into IPP standard*/
int Ipa_int_priority(char priority, const int reverse) {

	int rs = 1;
	priority = toupper(priority);
	if (priority > 'Z') priority = 'Z';
	if (priority < 'A') priority = 'A';

	rs = (100*( 'Z' - priority + 1))/26;

	return reverse ? rs : 101 - rs;

}
/*transform IPP priority to Lprng */
char Ipa_char_priority(int priority, int reverse) {

	char rs;

	if (priority > 100) priority = 100;
	if (priority < 1) priority = 1;
	if (!reverse) priority = 101 - priority;

	rs = 'A' + (26*(100 - priority))/100;
	if (rs < 'A') rs = 'A';
	if (rs > 'Z') rs = 'Z';
	/*DEBUGF(DNW2)("Ipa_char_priority priority %d reverse %d char %c", priority, reverse, rs);*/
	return rs;
}



