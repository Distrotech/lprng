/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 *
 * ipp protocol service
 * copyright 2008 Vaclav Michalek, Joint Laboratory of Optics, Olomouc
 *
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

#include "lp.h"

#include "linelist.h"
#include "ipp.h"
#include "ipp_proc.h"
#include "ipp_aux.h"
#include "getprinter.h"
#include "getqueue.h"
#include "lpd_jobs.h"
#include <time.h>
#include "gethostinfo.h"
#include "fileopen.h"
#include "lockfile.h"
#include "lpd_rcvjob.h"
#include "lpd_remove.h"
#include "errormsg.h"

/*IPP operations
 firstly see the bottom of this file
 */

/*Ipp operation sending SERVER_ERROR_OPERATION_NOT_SUPPORTED only*/
int Ipp_op_unknown(struct line_list *http_request, struct line_list *http_headers, const struct http_conn *conn,
                   ssize_t *body_rest_len, struct ipp_operation *ipp_request,
		   struct line_list *auth_info)
{
	struct ipp_operation response;

	Ipp_init_operation(&response);

	response.version = ipp_request->version;
	response.request_id = ipp_request->request_id;
	response.op_id_status = SERVER_ERROR_OPERATION_NOT_SUPPORTED;
	/*mandatory attributes*/
	Ipp_set_attr(&response.attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_CHARSET, IPPDT_CHARSET, 0,
		        IPPAV_UTF_8, safestrlen(IPPAV_UTF_8));
	Ipp_set_attr(&response.attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_NAT_LANG, IPPDT_NAT_LANG, 0,
		        IPPAV_EN_US, safestrlen(IPPAV_EN_US));

	Ipp_send_response(conn, http_headers, body_rest_len, &response, auth_info, 0, NULL);
	Ipp_free_operation(&response);
	return 0;
}

/*Ipp operation sending SUCCESSFUL_OK only*/
static int Ipp_op_empty_ok(struct line_list *http_request, struct line_list *http_headers, const struct http_conn *conn,
                   ssize_t *body_rest_len, struct ipp_operation *ipp_request,
		   struct line_list *auth_info)
{
	struct ipp_operation response;

	Ipp_init_operation(&response);

	response.version = ipp_request->version;
	response.request_id = ipp_request->request_id;
	response.op_id_status = SUCCESSFUL_OK;
	/*mandatory attributes*/
	Ipp_set_attr(&response.attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_CHARSET, IPPDT_CHARSET, 0,
		        IPPAV_UTF_8, safestrlen(IPPAV_UTF_8));
	Ipp_set_attr(&response.attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_NAT_LANG, IPPDT_NAT_LANG, 0,
		        IPPAV_EN_US, safestrlen(IPPAV_EN_US));

	Ipp_send_response(conn, http_headers, body_rest_len, &response, auth_info, 0, NULL);
	Ipp_free_operation(&response);
	return 0;
}


static int Ipp_create_subscriptions(struct ipp_operation *ipp_response, struct ipp_operation *ipp_request,
		const char *printer, struct job *sjob, int validate_only);

#define	HOLD_INVALID	0
#define	HOLD_NONE	1
#define	HOLD_INDEFINITE	2

#define	CREATE_HOLD_UNTIL	"create-hold"
#define	CREATE_COPIES		"create-copies"

static int Ipp_op_print_validate_job(struct line_list *http_request, struct line_list *http_headers, const struct http_conn *conn,
                   ssize_t *body_rest_len, struct ipp_operation *ipp_request,
		   struct line_list *auth_info)
{
	struct ipp_operation response;
	char *ppath, *printer;
	struct ipp_attr *a;
	char error[SMALLBUFFER];
	int errlen = sizeof(error);
	int permission;
	int fifo_fd = -1;
	char *fifo_path = NULL;
	char buf[LARGEBUFFER];             	/*receiving buffer*/
	ssize_t cont_len;                  	/*http content length*/
	ssize_t rd_len, rdb;               	/*number of read data bytes from ipp print request*/
	int tmp_fd;                        	/*temporary file fd for received data*/
	char *tempfile;                    	/*temporary file name for input job stream*/
	struct job job;
	int job_ticket_fd = -1;
	struct line_list files;            	/*list of incoming datafiles*/
	char *s, *jobname, *jreason, *format;   /* s = temporary string */
	int jobnum, jobstate;              	/*new job numnber, new job state*/
	int copies, max_copies, lprng_copies;  	/*requested copies, maximum allowed copies, copies handling flag*/
	char *Zopts;                       	/*job template options passed to filter*/
	struct line_list pc_entry, pc_alias; 	/*printcap option string lists*/
	char priority[2];
	int validate_only;
	int create_only;
	int job_hold_until;			/*attribute job-hold-until and hod permission OK*/

	Ipp_init_operation(&response);
	Init_job(&job);
	Init_line_list(&files);
	Init_line_list(&pc_entry);
	Init_line_list(&pc_alias);
	response.version = ipp_request->version;
	response.request_id = ipp_request->request_id;
	/*mandatory response attributes*/
	Ipp_set_attr(&response.attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_CHARSET, IPPDT_CHARSET, 0,
		        IPPAV_UTF_8, safestrlen(IPPAV_UTF_8));
	Ipp_set_attr(&response.attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_NAT_LANG, IPPDT_NAT_LANG, 0,
		        IPPAV_EN_US, safestrlen(IPPAV_EN_US));
	/*validate charset, language et cetera*/
	response.op_id_status = Ipa_validate_charset(NULL, ipp_request->attributes);
	if (response.op_id_status != SUCCESSFUL_OK) goto ipperr;
	response.op_id_status = Ipa_validate_language(NULL, ipp_request->attributes);
	if (response.op_id_status != SUCCESSFUL_OK) goto ipperr;
	a = Ipp_get_attr(ipp_request->attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_COMPRESSION, -1);
	if (a && safestrncmp(a->value, IPPAV_COMPR_NONE, a->value_len)) {
		response.op_id_status = CLIENT_ERROR_COMPRESSION_NOT_SUPPORTED;
		goto ipperr;
	}
	/*mandatory value*/
	/*unfortunately, CUPS ipp backend does not send job-name when copies_supported<=1, so we must not check */
	/*a = Ipp_get_attr(ipp_request->attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_JOB_NAME, -1);
	if (!a || !(a->value_len)) {
		response.op_id_status = CLIENT_ERROR_BAD_REQUEST;
		goto ipperr;
	}*/
	printer = Find_str_value(auth_info, KWA_PRINTER);
	ppath = Find_str_value(auth_info, KWA_PPATH);
	if (!printer || !ppath) {
		response.op_id_status = CLIENT_ERROR_NOT_FOUND;
		goto ipperr;
	}
	DEBUGF(DNW2)("Ipp_op_print_validate_job: printer: %s path %s", printer, ppath);
	if (Setup_printer(printer, error, errlen, 0))	{
		response.op_id_status = SERVER_ERROR_INTERNAL_ERROR;
		goto ipperr;
	}

	validate_only = (ipp_request->op_id_status == IPPOP_VALIDATE_JOB);
	create_only = (ipp_request->op_id_status == IPPOP_CREATE_JOB);

	Ipa_prase_debug(DRECVMASK);

	Ipa_set_perm(&Perm_check, 'R', auth_info);
	Perm_check.user = Find_str_value(auth_info, KWA_USER);
	Perm_check.host = &RemoteHost_IP;
	permission = Perms_check(&Perm_line_list, &Perm_check, 0, 0);
	DEBUGF(DLPQ1)("Ipa_op_print_validate_job: permission '%s'", perm_str(permission));
	if (permission == P_REJECT)
	{
		response.op_id_status = CLIENT_ERROR_NOT_AUTHENTICATED;
		goto ipperr;
	}

	Free_line_list(&Spool_control);
	Get_spool_control(Queue_control_file_DYN, &Spool_control);
	if (Sp_disabled(&Spool_control)) {
		response.op_id_status = SERVER_ERROR_NOT_ACCEPTING_JOBS;
		goto ipperr;
	}
	/*see lpd_recvjob.c*/
	/*fifo order force*/
	if (Fifo_DYN) {
		struct stat statb;
		char *p = Make_pathname(Spool_dir_DYN, Fifo_lock_file_DYN);
		fifo_path = safestrdup3(p, ".", RemoteHost_IP.fqdn, __FILE__, __LINE__);
		DEBUGF(DRECV1)("Ipp_op_print_validate_job: checking fifo lock %s", fifo_path);
		fifo_fd = Checkwrite(fifo_path, &statb, O_RDWR, 1, 0);
		if ((fifo_fd < 0) || (Do_lock(fifo_fd, 1) < 0)) {
			logerr_die(LOG_ERR, _("Ipp_op_print_validate_job: lock error (file %s)"), fifo_path);
		}
		if (p) free(p);
	}
	response.op_id_status = SUCCESSFUL_OK;
	/*validate job-hold-until attribute
	  if invalid, still allow printing with SUCCESSFUL_OK_IGNORED_SUBSTITUED*/
	job_hold_until = HOLD_INVALID;
	a = Ipp_get_attr(ipp_request->attributes, JOB_ATTRIBUTES_GRP, 0, IPPAN_JOB_HOLD_UNTIL, -1);
	if (a) {
		if (safestrncmp(a->value, IPPAV_NO_HOLD, a->value_len) &&
		    safestrncmp(a->value, IPPAV_INDEFINITE, a->value_len)) {
			response.op_id_status = SUCCESSFUL_OK_IGNORED_SUBSTITUED;
		} else 	if (!safestrncmp(a->value, IPPAV_NO_HOLD, a->value_len) && Auto_hold_DYN) {
			Perm_check.service = 'C';
			Perm_check.lpc = ILPC_RELEASE;
			if (Perms_check(&Perm_line_list, &Perm_check, 0, 0) == P_REJECT)
				response.op_id_status = SUCCESSFUL_OK_IGNORED_SUBSTITUED;
			else job_hold_until = HOLD_NONE;
		} else if (!safestrncmp(a->value, IPPAV_INDEFINITE, a->value_len) && !Auto_hold_DYN) {
			Perm_check.service = 'C';
			Perm_check.lpc = ILPC_HOLD;
			if (Perms_check(&Perm_line_list, &Perm_check, 0, 0) == P_REJECT)
				response.op_id_status = SUCCESSFUL_OK_IGNORED_SUBSTITUED;
			else job_hold_until = HOLD_INDEFINITE;
		}
	}

	/*validation done - no further processing needed */
	if (validate_only) {
		goto subscriptions;
	}



	/* other unimplemented IPP - operation attributes
	 *  ipp-attribute-fidelity (R*)  - simply ignored as we cannot determinate to the future filters etc.
            document-name (R*)           - not needed for us, we use job name
        */

	/*add job attributes*/
	Free_job(&job);
	Init_line_list(&job.info);
	Set_str_value(&job.info, LOGNAME, Find_str_value(auth_info, KWA_USER));
	Set_str_value(&job.info, FROMHOST, RemoteHost_IP.fqdn);
	Set_str_value(&job.info, AUTHUSER, Find_str_value(auth_info, KWA_USER));
	Set_str_value(&job.info, AUTHFROM, Find_str_value(auth_info, KWA_AUTHFROM));
	Set_str_value(&job.info, AUTHTYPE, Find_str_value(auth_info, KWA_AUTHTYPE));
	Set_str_value(&job.info, AUTHCA, Find_str_value(auth_info, KWA_AUTHCA));
	/*job name*/
	a = Ipp_get_attr(ipp_request->attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_JOB_NAME, -1);
	if (!a) a = Ipp_get_attr(ipp_request->attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_DOC_NAME, -1);
	if (a) {
		s = malloc_or_die(a->value_len + 1, __FILE__, __LINE__);
		memcpy(s, a->value, a->value_len); s[a->value_len] = '\0';
		jobname = Utf8_to_local(s);
		if (s) free(s);
	} else {
		jobname = safestrdup4("IPP: ", Find_str_value(auth_info, KWA_USER), "@", RemoteHost_IP.fqdn, __FILE__, __LINE__);
	}
	Set_str_value(&job.info, JOBNAME, jobname);
	if (jobname) free(jobname);

	/*other unrecognized attributes are Z options*/
	Select_pc_info(printer, &pc_entry, &pc_alias, &PC_names_line_list, &PC_order_line_list, &PC_info_line_list, 0, 0);
	lprng_copies = safestrcmp(Find_str_value(&pc_entry, PC_IMC), PCV_IMC_IF);
	copies = 1;
	max_copies = Max_copies_DYN < 1 ? 1 : Max_copies_DYN;
	priority[0] = priority[1] = '\0';
	Zopts = NULL;
	s = NULL;
	for (a = Ipp_get_attr(ipp_request->attributes, JOB_ATTRIBUTES_GRP, 0, NULL, -1); a; a = a->next) {
		if (!safestrcmp(a->name, IPPAN_JOB_HOLD_UNTIL)) continue;
		switch (a->type) {
			case IPPDT_BOOLEAN:
				s = malloc_or_die(6, __FILE__, __LINE__);
				plp_snprintf(s, 6, "%s", ((char)(*(a->value)) ? "true" : "false"));
				break;
			case IPPDT_INTEGER:
			case IPPDT_ENUM: ;
				int val = ntoh32(a->value);
				if (!safestrcmp(a->name, IPPAN_COPIES)) {
					if (val <= 0) val = 1;
					if (val > max_copies) val = max_copies;
					if (lprng_copies) { /*copies handled by lpd*/
						copies = val;
						continue;
					}
				} else if (!safestrcmp(a->name, IPPAN_JOB_PRI)) {
					if (!Ignore_requested_user_priority_DYN)
						priority[0] = Ipa_char_priority(val, Reverse_priority_order_DYN);
					continue;
				}
				/*if copies handled by lpd, do not pass some values to further processing*/
				if (lprng_copies && !safestrcmp(a->name, IPPAN_MULT_DOC_HNDL)) continue;
				s = malloc_or_die(12, __FILE__, __LINE__);
				plp_snprintf(s, 12, "%d", val);
				break;
			case IPPDT_RANGE_INT: ;
				int lval = ntoh32(a->value);
				int hval = ntoh32(a->value + 4);
				s = malloc_or_die(25, __FILE__, __LINE__);
				plp_snprintf(s, 25, "%d-%d", lval, hval);
				break;
			default:
				if (!a->value || !a->value_len) continue;
				s = malloc_or_die(a->value_len*2 + 3, __FILE__, __LINE__); /* enough space for possible quoted chars + term zero*/
				memcpy(s, a->value, a->value_len); s[a->value_len] = '\0'; /*make terminated string from unterminated*/
				/*quote if needed*/
				size_t i;
				for (i = 0; i < strlen(s); i++) {
					if (isalnum(s[i]) || strchr("+-_.:;/", s[i])) continue;
					if (s[i] == '"') {
						memmove(s+i+1, s+i, strlen(s) - i);
						s[i] = '\\';
						i++;
					}
					if (s[0] != '"') {
						memmove(s+1, s, strlen(s));
						s[0] = '"';
					}
				}
				if (s[0] == '"') strcat(s+strlen(s)-1, "\"");
		}
		DEBUGF(DNW4)("Ipp_op_print_validate_job: Z option %s=%s", a->name, s);
		if (s) {
			if (Zopts) Zopts = safeextend5(Zopts, ",", a->name, "=", s, __FILE__, __LINE__);
			else Zopts = safestrdup3(a->name, "=", s, __FILE__, __LINE__);
			free(s); s = NULL;
		}
	}
	if (Zopts) {
		Set_str_value(&job.info, "Z", Zopts);
		free(Zopts);
	}
	if (priority[0] != 0 ) {
		Set_str_value(&job.info, CLASS, priority);
		Set_str_value(&job.info, PRIORITY, priority);
	}

	/*add job files - we want to set proper format and number of copies, so hand-add is necessary*/
	job_ticket_fd = Setup_temporary_job_ticket_file(&job, 0, 0, 0, error, errlen); /*clears datafiles, so we must add them after*/
	if (job_ticket_fd < 0) {
			response.op_id_status = SERVER_ERROR_INTERNAL_ERROR;
			goto ipperr;
	}

	if (!create_only) {
	        /*read datafile; we have no reliable information about job size*/
		tmp_fd = Make_temp_fd(&tempfile);
		cont_len = Http_content_length(http_headers);
		DEBUGF(DNW2)("Ipp_op_print_validate_job: rest_len %zd, cont_len %zd", *body_rest_len, cont_len);
		rd_len = 0;
		do {
			rdb = Http_read_body(conn, cont_len, body_rest_len, buf, sizeof(buf), http_headers);
			if (rdb < 0) {
				DEBUGF(DRECV1)("Ipp_op_print_validate_job: Http_read_body failed (%zd)", rdb);
				response.op_id_status = CLIENT_ERROR_BAD_REQUEST;
				goto ipperr;
			}
			rd_len += rdb;
			if (Max_job_size_DYN && (rd_len > Max_job_size_DYN)) {
				DEBUGF(DRECV1)("Ipp_op_print_validate_job: Maximum job size (%zd) exceeded", rd_len);
				response.op_id_status = CLIENT_ERROR_REQUEST_ENTITY_TOO_LARGE;
				goto ipperr;
			}
			if (!Check_space(rd_len, Minfree_DYN, Spool_dir_DYN)) {
				DEBUGF(DRECV1)("Ipp_op_print_validate_job: insufficient space - needed %zd bytes in %s", rd_len, Spool_dir_DYN);
				response.op_id_status = SERVER_ERROR_TEMPORARY_ERROR;
				goto ipperr;
			}
			if (Write_fd_len_timeout(conn->timeout, tmp_fd, buf, rdb) < 0) {
				DEBUGF(DRECV1)("Ipp_op_print_validate_job: cannot write to temp file %s rest len %zd, rdb %zd", tempfile, *body_rest_len, rdb);
				response.op_id_status = SERVER_ERROR_INTERNAL_ERROR;
				goto ipperr;
			}
		} while (*body_rest_len);

		Set_casekey_str_value(&files, tempfile, tempfile);
		Check_max(&job.datafiles, 2);
		job.datafiles.list[0] = malloc_or_die(sizeof(struct line_list), __FILE__, __LINE__);
		Init_line_list((void *)job.datafiles.list[0]);
		Set_str_value((void *)job.datafiles.list[0], OTRANSFERNAME, tempfile);
		/* the file format is depemndent on the printer-uri and document-format
		 * if ppath = raw or document format is application/vnd.cups-raw, set the Binary (literal) print format
		 * * */
		a = Ipp_get_attr(ipp_request->attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_DOC_FORMAT, -1);
		format = !safestrcmp(ppath, IPPC_PRINTERS_PATH_RAW) ||
			 (a && !safestrncmp(a->value, IPPAV_APP_CUPS_RAW, a->value_len)) ? "l" : "f";
		Set_str_value((void *)job.datafiles.list[0], FORMAT, format);
		Set_flag_value((void *)job.datafiles.list[0], COPIES, copies);
		job.datafiles.count = 1;
		/*job set done*/
	} else {
		job.datafiles.count = 0;
		Set_flag_value(&job.info, ERROR_TIME, time((void *)0));
		Set_flag_value(&job.info, REMOVE_TIME, time((void *)0));    /*automatically remove missing Send-Document*/
		Set_flag_value(&job.info, CREATE_HOLD_UNTIL, job_hold_until);
		Set_flag_value(&job.info, CREATE_COPIES, copies);
		Set_job_ticket_file(&job, NULL, job_ticket_fd);

	}

	DEBUGFC(DRECV3)Dump_job("Ipp_op_print_validate_job: new job", &job);
	DEBUGFC(DNW1)Dump_job("Ipp_op_print_validate_job: new job", &job);

	if (Check_for_missing_files(&job, &files, error, errlen, 0, job_ticket_fd)){
			response.op_id_status = SERVER_ERROR_INTERNAL_ERROR;
			goto ipperr;
	}
	DEBUGF(DNW4)("Ipp_op_print_validate_job: Hold_change %d", job_hold_until);
	/*change hold if needed*/
	switch (job_hold_until) {
		case HOLD_NONE:
			Set_flag_value(&job.info, HOLD_TIME, 0);
			Set_job_ticket_file(&job, NULL, job_ticket_fd);
			break;
		case HOLD_INDEFINITE:
			Set_flag_value(&job.info, HOLD_TIME, time((void *)0));
			Set_job_ticket_file(&job, NULL, job_ticket_fd);
			break;
	}

	DEBUGFC(DRECV2)Dump_job("Ipp_op_print_validate_job: job after Check_for_missing_files()", &job);
	DEBUGFC(DNW1)Dump_job("Ipp_op_print_validate_job: job after Check_for_missing_files()", &job);

	/*update spool queue status*/
	if (job_ticket_fd > 0) {
		close(job_ticket_fd); job_ticket_fd = -1;
	}
	Free_line_list(&Spool_control);
	Get_spool_control(Queue_control_file_DYN, &Spool_control);
	Set_flag_value(&Spool_control, CHANGE, 1);
	Set_spool_control(0, Queue_control_file_DYN, &Spool_control);
	if (Lpq_status_file_DYN) unlink(Lpq_status_file_DYN);

	/*tell server to start working*/
	s = Server_queue_name_DYN;
	if (!s) s = Printer_DYN;
	DEBUGF(DRECV1)("Ipp_op_print_validate_job: Lpd_request fd %d, data %s", Lpd_request, s);
	if (Write_fd_str(Lpd_request, s) < 0 ||
	    Write_fd_str(Lpd_request, "\n") < 0) {
		logerr_die(LOG_ERR, _("Ipp_op_print_validate_job: write to fd %d failed"), Lpd_request);
		response.op_id_status = SERVER_ERROR_INTERNAL_ERROR;
		goto ipperr;
	}

	/*fill IPP response*/
	/*unsupported group*/
	Ipp_set_attr(&response.attributes, UNSUPPORTED_ATTRIBUTES_GRP, 0, NULL, 0, 0, NULL, 0);
	/*mandatory return attributes  - job-id*/
	jobnum = Find_decimal_value(&job.info, NUMBER);
	hton32(buf, Ipa_jobid2ipp(printer, jobnum));
	Ipp_set_attr(&response.attributes, JOB_ATTRIBUTES_GRP, 0, IPPAN_JOB_ID, IPPDT_INTEGER, 0, buf, 4);
	Ipa_get_job_uri(&s, ipp_request->version, conn->port, ppath, printer, jobnum);
	/*job-uri*/
	Ipp_set_attr(&response.attributes, JOB_ATTRIBUTES_GRP, 0, IPPAN_JOB_URI, IPPDT_URI, 0, s, strlen(s));
	if (s) free(s);
	/*job-state, job-state-reasons*/
	Ipa_get_job_state(&jobstate, &jreason, &job, &Spool_control);
	hton32(buf, jobstate);
	Ipp_set_attr(&response.attributes, JOB_ATTRIBUTES_GRP, 0, IPPAN_JOB_STATE, IPPDT_ENUM, 0, buf, 4);
	Ipp_set_attr(&response.attributes, JOB_ATTRIBUTES_GRP, 0, IPPAN_JOB_SREASONS, IPPDT_KEYWORD, 0, jreason, strlen(jreason));

	DEBUGF(DRECV1)("Ipp_op_print_validate_job: new job id %d, state %d, reason %s", jobnum, jobstate, jreason);
 subscriptions:
	Ipp_create_subscriptions(&response, ipp_request, printer, &job, validate_only);
 ipperr:

	/*if fail, remove */
	if ((response.op_id_status != SUCCESSFUL_OK) &&
	   (s = Find_str_value(&job.info, HF_NAME))) {
		unlink(s);
		Remove_job(&job);
	}
	Remove_tempfiles();
	Free_job(&job);
	Free_line_list(&files);
	Free_line_list(&pc_entry);
	Free_line_list(&pc_alias);
	if (fifo_fd > 0) {
		close(fifo_fd);
	}
	if (fifo_path) free(fifo_path);
	if (job_ticket_fd > 0) close(job_ticket_fd);
	Ipp_send_response(conn, http_headers, body_rest_len, &response, auth_info, 0, NULL);
	Ipp_free_operation(&response);
	return 0;

}

static int Ipp_op_send_document(struct line_list *http_request, struct line_list *http_headers, const struct http_conn *conn,
             ssize_t *body_rest_len, struct ipp_operation *ipp_request, struct line_list *auth_info)
{

	struct ipp_operation response;
	char *printer = NULL;
	char *ppath = NULL;
	struct ipp_attr *a;
	char error[SMALLBUFFER];
	int errlen = sizeof(error);
	int count;
	struct job job;
	int printable, move, held, err, done;
	int jobid = 0, jobstate, jticket_fd = -1;
	char *username, *jreason;       /*job username, job state reason*/
	char buf[LARGEBUFFER];        	/*see op_print_job*/
	ssize_t cont_len;
	ssize_t rd_len, rdb;
	int tmp_fd;
	char *tempfile;
	int permission;
	char *s;                        /*temporary pointer*/
	int dcount;			/*job datafiles count*/
	struct line_list files;
	char *format;
	int copies;
	int lastdoc;

	Init_job(&job);
	Ipp_init_operation(&response);
	response.version = ipp_request->version;
	response.request_id = ipp_request->request_id;
	Init_line_list(&files);

	/*set response mandatory attributes*/
	Ipp_set_attr(&response.attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_CHARSET, IPPDT_CHARSET, 0,
		        IPPAV_UTF_8, safestrlen(IPPAV_UTF_8));
	Ipp_set_attr(&response.attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_NAT_LANG, IPPDT_NAT_LANG, 0,
		        IPPAV_EN_US, safestrlen(IPPAV_EN_US));
	/*validate request - charset and language*/
	response.op_id_status = Ipa_validate_charset(NULL, ipp_request->attributes);
	if (response.op_id_status != SUCCESSFUL_OK) goto ipperr;
	response.op_id_status = Ipa_validate_language(NULL, ipp_request->attributes);
	if (response.op_id_status != SUCCESSFUL_OK) goto ipperr;
	a = Ipp_get_attr(ipp_request->attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_COMPRESSION, -1);
	if (a && safestrncmp(a->value, IPPAV_COMPR_NONE, a->value_len)) {
		response.op_id_status = CLIENT_ERROR_COMPRESSION_NOT_SUPPORTED;
		goto ipperr;
	}
	printer = Find_str_value(auth_info, KWA_PRINTER);
	ppath = Find_str_value(auth_info, KWA_PPATH);
	if (!printer || !ppath) {
		response.op_id_status = CLIENT_ERROR_NOT_FOUND;
		goto ipperr;
	}
	DEBUGF(DNW2)("Ipp_op_send_document: printer: %s path %s", printer, ppath);
	if (Setup_printer(printer, error, errlen, 0))	{
		response.op_id_status = SERVER_ERROR_INTERNAL_ERROR;
		goto ipperr;
	}
	/*job-id required*/
	response.op_id_status = Ipa_get_job_id(&jobid, ipp_request->attributes);
	DEBUGF(DNW2)("Ipp_op_send_document: requested job-id: %d", jobid);
	if (response.op_id_status != SUCCESSFUL_OK) goto ipperr;

	Ipa_prase_debug(DRECVMASK);
	response.op_id_status = SUCCESSFUL_OK;

	/*requested & unsupported attributes*/
	Ipp_set_attr(&response.attributes, UNSUPPORTED_ATTRIBUTES_GRP, 0, NULL, 0, 0, NULL, 0);
	for (a = ipp_request->attributes; a; a = a->next) {
		if ((a->group == OPERATION_ATTRIBUTES_GRP) &&
		    (!safestrcmp(a->name, IPPAN_CHARSET) ||
		     !safestrcmp(a->name, IPPAN_NAT_LANG) ||
		     !safestrcmp(a->name, IPPAN_RQ_USRNAME) ||
		     !safestrcmp(a->name, IPPAN_PRINTER_URI) ||
		     !safestrcmp(a->name, IPPAN_JOB_URI) ||
		     !safestrcmp(a->name, IPPAN_JOB_ID) ||
		     !safestrcmp(a->name, IPPAN_DOC_NAME) ||
		     !safestrcmp(a->name, IPPAN_COMPRESSION) ||
		     !safestrcmp(a->name, IPPAN_DOC_FORMAT) ||
		     !safestrcmp(a->name, IPPAN_LAST_DOCUMENT)
		     )
		   ) continue;
		Ipp_set_attr(&response.attributes, UNSUPPORTED_ATTRIBUTES_GRP, 0, a->name, IPPDT_UNSUPPORTED, 0, NULL, 0);
		response.op_id_status = SUCCESSFUL_OK_IGNORED_SUBSTITUED;
	}

	/* search the queue, process request  */
	Free_line_list(&Spool_control);
	Get_spool_control(Queue_control_file_DYN, &Spool_control);
	if (Sp_disabled(&Spool_control)) {
		response.op_id_status = SERVER_ERROR_NOT_ACCEPTING_JOBS;
		goto ipperr;
	}
	Scan_queue_proc(&Spool_control, &Sort_order, &printable, &held, &move, 0, &err, &done, 0, 0, order_filter_job_number, &jobid);
	/*remove done jobs - depends on Sort_order global variable*/
	if (Remove_done_jobs()) {
		Scan_queue_proc(&Spool_control, &Sort_order, &printable, &held, &move, 0, &err, &done, 0, 0, order_filter_job_number, &jobid);
	}

	Ipa_set_perm(&Perm_check, 'R', auth_info);

	/*count should be 0 or 1 - filtered by Scan_queue_proc + job number*/
	for (count = 0; count < Sort_order.count; count++) {
		/*get job info*/
		Free_job(&job);
		if (jticket_fd > 0) close(jticket_fd); jticket_fd = -1;
		Get_job_ticket_file(&jticket_fd, &job, Sort_order.list[count]);
		if (!job.info.count) /*got lpd_status.c*/continue;

		DEBUGFC(DRECV4)Dump_job("Ipa_send_document - info", &job);
		/*DEBUGFC(DNW4)Dump_job("Ipa_set_job_attributes - info", &job);*/

		Ipa_get_job_state(&jobstate, NULL, &job, &Spool_control);
		if ((jobstate == IPPAV_JRS_COMPLETED) || /*Create-Job leaves [empty] jobs in ABORTED state ...*/
		    (jobstate == IPPAV_JRS_CANCELED) ||
		    (jobstate == IPPAV_JRS_PROCESSING)) {
			response.op_id_status = CLIENT_ERROR_NOT_POSSIBLE;
			goto ipperr;
		}

		/*test permission*/
		Perm_check.user = Find_str_value(&job.info, LOGNAME);
		Perm_check.host = ((s = Find_str_value(&job.info, FROMHOST)) && Find_fqdn(&PermHost_IP, s)) ?
			Perm_check.host = &PermHost_IP : 0;
		permission = Perms_check(&Perm_line_list, &Perm_check, &job, 1);
		DEBUGF(DRECV1)("Ipa_op_send_document: permission '%s'", perm_str(permission));
		if (permission == P_REJECT)
		{
			response.op_id_status = CLIENT_ERROR_NOT_AUTHENTICATED;
			goto ipperr;
		}

		goto job_found;
	}

	response.op_id_status = CLIENT_ERROR_NOT_FOUND;
	goto ipperr;

job_found:

	/*read datafile; we have no reliable information about job size*/
	/*see op_print_job*/
	tmp_fd = Make_temp_fd(&tempfile);
	cont_len = Http_content_length(http_headers);
	DEBUGF(DNW2)("Ipp_op_send_document: rest_len %zd, cont_len %zd", *body_rest_len, cont_len);
	rd_len = 0;
	do {
		rdb = Http_read_body(conn, cont_len, body_rest_len, buf, sizeof(buf), http_headers);
		if (rdb < 0) {
			DEBUGF(DRECV1)("Ipp_op_send_document: Http_read_body failed (%zd)", rdb);
			response.op_id_status = CLIENT_ERROR_BAD_REQUEST;
			goto ipperr;
		}
		rd_len += rdb;
		if (Max_job_size_DYN && (rd_len > Max_job_size_DYN)) {
			DEBUGF(DRECV1)("Ipp_op_send_document: Maximum job size (%zd) exceeded", rd_len);
			response.op_id_status = CLIENT_ERROR_REQUEST_ENTITY_TOO_LARGE;
			goto ipperr;
		}
		if (!Check_space(rd_len, Minfree_DYN, Spool_dir_DYN)) {
			DEBUGF(DRECV1)("Ipp_op_send_document: insufficient space - needed %zd bytes in %s", rd_len, Spool_dir_DYN);
			response.op_id_status = SERVER_ERROR_TEMPORARY_ERROR;
			goto ipperr;
		}
		if (Write_fd_len_timeout(conn->timeout, tmp_fd, buf, rdb) < 0) {
			DEBUGF(DRECV1)("Ipp_op_send_document: cannot write to temp file %s rest len %zd, rdb %zd", tempfile, *body_rest_len, rdb);
			response.op_id_status = SERVER_ERROR_INTERNAL_ERROR;
			goto ipperr;
		}
	} while (*body_rest_len);

	if (rd_len) {
		Set_casekey_str_value(&files, tempfile, tempfile);
		dcount = job.datafiles.count;
		Check_max(&job.datafiles, dcount + 2);
		job.datafiles.list[count] = malloc_or_die(sizeof(struct line_list), __FILE__, __LINE__);
		Init_line_list((void *)job.datafiles.list[dcount]);
		Set_str_value((void *)job.datafiles.list[dcount], OTRANSFERNAME, tempfile);
		a = Ipp_get_attr(ipp_request->attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_DOC_FORMAT, -1);
		format = !safestrcmp(ppath, IPPC_PRINTERS_PATH_RAW) ||
			 (a && !safestrncmp(a->value, IPPAV_APP_CUPS_RAW, a->value_len)) ? "l" : "f";
		Set_str_value((void *)job.datafiles.list[dcount], FORMAT, format);
		copies = Find_flag_value(&job.info, CREATE_COPIES);
		Set_flag_value((void *)job.datafiles.list[dcount], COPIES, copies ? copies : 1);
		job.datafiles.count = dcount + 1;
		/*job set done*/

		if (Check_for_missing_files(&job, &files, error, errlen, 0, jticket_fd)){
			response.op_id_status = SERVER_ERROR_INTERNAL_ERROR;
			goto ipperr;
		}
	}

	a = Ipp_get_attr(ipp_request->attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_LAST_DOCUMENT, -1);
	lastdoc = !a || !(a->value) || (a->value[0]);

	Free_line_list(&Spool_control);
	Get_spool_control(Queue_control_file_DYN, &Spool_control);

	if (lastdoc) {
		switch (Find_flag_value(&job.info, CREATE_HOLD_UNTIL)) {
			case HOLD_NONE:
				Set_flag_value(&job.info, HOLD_TIME, 0); break;
			case HOLD_INDEFINITE:
				Set_flag_value(&job.info, HOLD_TIME, time((void *)0)); break;
		}
		Set_str_value(&job.info, ERROR, 0);
		Set_flag_value(&job.info, ERROR_TIME, 0);
		Set_flag_value(&job.info, REMOVE_TIME, 0);
		Set_flag_value(&job.info, CREATE_HOLD_UNTIL, 0);
		Set_flag_value(&job.info, CREATE_COPIES, 0);
		Set_job_ticket_file(&job, NULL, jticket_fd);

		/*queue changed*/
		Set_flag_value(&Spool_control, CHANGE, 1);
		Set_spool_control(0, Queue_control_file_DYN, &Spool_control);
		if (Lpq_status_file_DYN) unlink(Lpq_status_file_DYN);

		/*tell server to start working*/
		s = Server_queue_name_DYN;
		if (!s) s = Printer_DYN;
		DEBUGF(DRECV1)("Ipp_op_send_document: Lpd_request fd %d, data %s", Lpd_request, s);
		if (Write_fd_str(Lpd_request, s) < 0 ||
		    Write_fd_str(Lpd_request, "\n") < 0) {
			logerr_die(LOG_ERR, _("Ipp_op_send_document: write to fd %d failed"), Lpd_request);
			response.op_id_status = SERVER_ERROR_INTERNAL_ERROR;
			goto ipperr;
		}
	}

	/*fill IPP response*/
	/*mandatory return attributes  - job-id*/
	hton32(buf, Ipa_jobid2ipp(printer, jobid));
	Ipp_set_attr(&response.attributes, JOB_ATTRIBUTES_GRP, 0, IPPAN_JOB_ID, IPPDT_INTEGER, 0, buf, 4);
	/*job-uri*/
	Ipa_get_job_uri(&s, ipp_request->version, conn->port, ppath, printer, jobid);
	Ipp_set_attr(&response.attributes, JOB_ATTRIBUTES_GRP, 0, IPPAN_JOB_URI, IPPDT_URI, 0, s, strlen(s));
	if (s) free(s);
	/*job-state, job-state-reasons*/
	Ipa_get_job_state(&jobstate, &jreason, &job, &Spool_control);
	hton32(buf, jobstate);
	Ipp_set_attr(&response.attributes, JOB_ATTRIBUTES_GRP, 0, IPPAN_JOB_STATE, IPPDT_ENUM, 0, buf, 4);
	Ipp_set_attr(&response.attributes, JOB_ATTRIBUTES_GRP, 0, IPPAN_JOB_SREASONS, IPPDT_KEYWORD, 0, jreason, strlen(jreason));

	DEBUGF(DRECV1)("Ipp_op_send_document: job id %d, state %d, reason %s", jobid, jobstate, jreason);



ipperr:

	if (jticket_fd > 0) close (jticket_fd);
	Free_line_list(&files);
	Remove_tempfiles();
	Free_job(&job);
	Ipp_send_response(conn, http_headers, body_rest_len, &response, auth_info, 0, NULL);
	Ipp_free_operation(&response);

	return 0;
}


static int Ipp_op_cancel_job(struct line_list *http_request, struct line_list *http_headers, const struct http_conn *conn,
                   ssize_t *body_rest_len, struct ipp_operation *ipp_request,
		   struct line_list *auth_info)
{

	struct ipp_operation response;
	char *printer, *ppath;
	int jobnum;
	struct line_list printers_done;
	/*struct ipp_attr *a;
	int purge;*/

	Init_line_list(&printers_done);
	Ipp_init_operation(&response);

	response.version = ipp_request->version;
	response.request_id = ipp_request->request_id;

	/*mandatory response attributes*/
	Ipp_set_attr(&response.attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_CHARSET, IPPDT_CHARSET, 0,
		        IPPAV_UTF_8, safestrlen(IPPAV_UTF_8));
	Ipp_set_attr(&response.attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_NAT_LANG, IPPDT_NAT_LANG, 0,
		        IPPAV_EN_US, safestrlen(IPPAV_EN_US));

	/*validate charset and language*/
	response.op_id_status = Ipa_validate_charset(NULL, ipp_request->attributes);
	if (response.op_id_status != SUCCESSFUL_OK) goto ipperr;
	response.op_id_status = Ipa_validate_language(NULL, ipp_request->attributes);
	if (response.op_id_status != SUCCESSFUL_OK) goto ipperr;

	/*get and validate printer */
	printer = Find_str_value(auth_info, KWA_PRINTER);
	ppath = Find_str_value(auth_info, KWA_PPATH);
	if (!printer || !ppath) {
		response.op_id_status = CLIENT_ERROR_NOT_FOUND;
		goto ipperr;
	}
	DEBUGF(DNW3)("Ipp_op_cancel_job: printer: %s path %s", printer, ppath);

	response.op_id_status = Ipa_get_job_id(&jobnum, ipp_request->attributes);
	DEBUGF(DNW2)("Ipp_op_cancel_job: requested job-id for operation Cancel-job: %d", jobnum);
	if (response.op_id_status != SUCCESSFUL_OK) goto ipperr;

	Ipa_prase_debug(DLPRMMASK);

	/* do not purge since job-removed event cannot be generated
	a = Ipp_get_attr(ipp_request->attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_PURGE_JOB, -1);
	purge = (a && a->value_len == 1 && ( *(char *)(a->value) & 0xff));*/

	Ipa_set_perm(&Perm_check, 'C', auth_info);
	/*try find & remove*/
	response.op_id_status = Ipa_remove_job(printer, auth_info, jobnum, &printers_done);

	/*if ((response.op_id_status == SUCCESSFUL_OK) && purge)
		response.op_id_status = Ipa_remove_job(printer, auth_info, jobnum, &printers_done);*/

 ipperr:
	Ipp_send_response(conn, http_headers, body_rest_len, &response, auth_info, 0, NULL);
	Ipp_free_operation(&response);
	Free_line_list(&printers_done);

	return 0;
}


#define RQ_TRUE "1"
#define RQ_SUPP "S"

static int Ipp_op_get_printer_attributes(struct line_list *http_request, struct line_list *http_headers, const struct http_conn *conn,
              ssize_t *body_rest_len, struct ipp_operation *ipp_request, struct line_list *auth_info)
{

	struct ipp_operation response;
	char *printer = NULL;	/*pointer*/
	char *ppath = NULL;  	/*pointer*/
	char *default_printer = NULL;	/*allocated string*/
	struct ipp_attr *a;
	struct line_list pc_entry, pc_alias, auths;
	char error[SMALLBUFFER];
	int errlen = sizeof(error);
	int pstate;
	char *preason = "";
	int j_accept, j_count;
	struct line_list printers, ppaths, requested_attributes;
	int limit, prcount, pthcount, group;
	int lprng_copies;
	int i;

	Ipp_init_operation(&response);
	response.version = ipp_request->version;
	response.request_id = ipp_request->request_id;
	/*mandatory attributes*/
	Ipp_set_attr(&response.attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_CHARSET, IPPDT_CHARSET, 0,
		        IPPAV_UTF_8, safestrlen(IPPAV_UTF_8));
	Ipp_set_attr(&response.attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_NAT_LANG, IPPDT_NAT_LANG, 0,
		        IPPAV_EN_US, safestrlen(IPPAV_EN_US));

	Init_line_list(&printers);
	Init_line_list(&ppaths);
	Init_line_list(&requested_attributes);
	Init_line_list(&pc_entry);
	Init_line_list(&pc_alias);
	Init_line_list(&auths);

	/*validate charset and language*/
	response.op_id_status = Ipa_validate_charset(NULL, ipp_request->attributes);
	if (response.op_id_status != SUCCESSFUL_OK) goto ipperr;
	response.op_id_status = Ipa_validate_language(NULL, ipp_request->attributes);
	if (response.op_id_status != SUCCESSFUL_OK) goto ipperr;

	response.op_id_status = SUCCESSFUL_OK;
	/*add unsupported group 2*/
	Ipp_set_attr(&response.attributes, UNSUPPORTED_ATTRIBUTES_GRP, 0, NULL, 0, 0, NULL, 0);

	/*find/get the requested attributes list*/
	a = Ipp_get_attr(ipp_request->attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_REQUESTED_ATTRIBUTES, -1);
	do {
		if (!a || !safestrncmp(a->value, IPPAV_ALL, a->value_len)) {
			Set_casekey_str_value(&requested_attributes, IPPAN_PRINTER_URI_SUPPORTED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_URI_SEC_SUPPORTED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_URI_AUTH_SUPPORTED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_PRINTER_NAME, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_PRINTER_LOCATION, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_PRINTER_INFO, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_PRINTER_MAKE_MODEL, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_PRINTER_STATE, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_PRINTER_SREASONS, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_IPP_VER_SUPPORTED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_OPERATIONS_SUPPORTED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_MULT_DOC_JOBS_SUPP, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_CHARSET_CONFIGURED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_CHARSET_SUPPORTED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NL_CONFIGURED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NL_SUPPORTED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_DOC_FORMAT_DEFAULT, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_DOC_FORMAT_SUPPORTED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_PRINTER_JOB_ACCEPTING, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_QUEUED_JOBS, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_PDL_OVRD_SUPPORTED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_PRINTER_UP_TIME, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_MULT_OP_TIMEOUT, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_COMPRESSION_SUPPORTED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_PRI_SUPPORTED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_PRI_DEFAULT, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_COPIES_SUPPORTED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_COPIES_DEFAULT, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_MULT_DOC_HNDL_DEFAULT, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_MULT_DOC_HNDL_SUPP, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_HOLD_UNTIL_D, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_HOLD_UNTIL_SUPP, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_SETTABLE_ATTRS_S, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_PULL_METHOD_SUPP, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_EVENTS_DEFAULT, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_EVENTS_SUPPORTED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_MAX_EVENTS_SUPP, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_LEASE_DURATION_D, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_LEASE_DURATION_S, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_IPPGET_EVENT_LIFE, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_AUTH_INFO_REQUIRED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_PRINTER_TYPE, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_USER_RIGHTS, RQ_TRUE);
			break;
		}
		else if (!safestrncmp(a->value, IPPAV_PRINTER_DESCRIPTION, a->value_len)) {
			Set_casekey_str_value(&requested_attributes, IPPAN_PRINTER_URI_SUPPORTED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_URI_SEC_SUPPORTED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_URI_AUTH_SUPPORTED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_PRINTER_NAME, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_PRINTER_LOCATION, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_PRINTER_INFO, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_PRINTER_MAKE_MODEL, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_PRINTER_STATE, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_PRINTER_SREASONS, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_IPP_VER_SUPPORTED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_OPERATIONS_SUPPORTED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_MULT_DOC_JOBS_SUPP, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_CHARSET_CONFIGURED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_CHARSET_SUPPORTED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NL_CONFIGURED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NL_SUPPORTED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_DOC_FORMAT_DEFAULT, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_DOC_FORMAT_SUPPORTED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_PRINTER_JOB_ACCEPTING, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_QUEUED_JOBS, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_PDL_OVRD_SUPPORTED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_PRINTER_UP_TIME, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_MULT_OP_TIMEOUT, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_COMPRESSION_SUPPORTED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_SETTABLE_ATTRS_S, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_AUTH_INFO_REQUIRED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_PRINTER_TYPE, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_USER_RIGHTS, RQ_TRUE);
		}
		else if (!safestrncmp(a->value, IPPAV_JOB_TEMPLATE, a->value_len)) {
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_PRI_SUPPORTED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_PRI_DEFAULT, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_COPIES_SUPPORTED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_COPIES_DEFAULT, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_MULT_DOC_HNDL_DEFAULT, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_MULT_DOC_HNDL_SUPP, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_HOLD_UNTIL_D, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_HOLD_UNTIL_SUPP, RQ_TRUE);
		}
		else if (!safestrncmp(a->value, IPPAV_SUBSCRIPTION_TEMPLATE, a->value_len)) {
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_PULL_METHOD_SUPP, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_EVENTS_DEFAULT, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_EVENTS_SUPPORTED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_MAX_EVENTS_SUPP, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_LEASE_DURATION_D, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_LEASE_DURATION_S, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_IPPGET_EVENT_LIFE, RQ_TRUE);
		}
		else {
			char *buf = malloc_or_die(a->value_len + 1, __FILE__, __LINE__);
			memcpy(buf, a->value, a->value_len); buf[a->value_len] = '\0';
			Set_casekey_str_value(&requested_attributes, buf, RQ_TRUE);
			free(buf);
		}
		if (a) a = Ipp_get_attr(a->next, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_REQUESTED_ATTRIBUTES, -1);
	} while (a);

	/*get the default printer*/
	if ((ipp_request->op_id_status == IPPOP_CUPS_GET_DEFAULT) ||
	    Find_casekey_str_value(&requested_attributes, IPPAN_PRINTER_TYPE, Hash_value_sep)) {
		Get_all_printcap_entries();
		/*filter default todo*/
		Ipa_set_perm(&Perm_check, 'd', auth_info);
		for (prcount = 0; prcount < All_line_list.count; prcount++) {
			Perm_check.printer = All_line_list.list[prcount];
			Perm_check.ppath = IPPC_PRINTERS_PATH;
			if (Perms_check(&Perm_line_list, &Perm_check, 0, 0) == P_ACCEPT) {
				default_printer = safestrdup(All_line_list.list[prcount], __FILE__, __LINE__);
				goto default_printer_found;
			}
		}
		/*system default printer*/
		for (prcount = 0; prcount < All_line_list.count; prcount++) {
			if (!safestrcmp(All_line_list.list[prcount], Default_printer_DYN)) {
				default_printer = safestrdup(Default_printer_DYN, __FILE__, __LINE__);
				goto default_printer_found;
			}
		}
default_printer_found: ;
		Perm_check.printer = 0;
		Perm_check.ppath = 0;
	}

	/*build printer list*/
	limit = 0;
	if ((ipp_request->op_id_status == IPPOP_CUPS_GET_PRINTERS) ||
	    (ipp_request->op_id_status == IPPOP_CUPS_GET_CLASSES) ||
	    (ipp_request->op_id_status == IPPOP_CUPS_GET_DEVICES)) {
		Get_all_printcap_entries();
		Merge_line_list(&printers, &All_line_list, "", 0, 1);
		DEBUGFC(DNW4)Dump_line_list("Ipp_op_get_printer_attributes: printers", &printers);
		Add_line_list(&ppaths, IPPC_PRINTERS_PATH, "", 0, 1); /*for CUPS-specific operation, advertize only CUPS-compatible uri*/
		a = Ipp_get_attr(ipp_request->attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_LIMIT, -1);
		if (a && a->value_len == 4)
			limit = ntoh32(a->value);
	} else if (ipp_request->op_id_status == IPPOP_CUPS_GET_DEFAULT) {
		DEBUGFC(DNW4)Dump_line_list("Ipp_op_get_printer_attributes: printers", &printers);
		if (default_printer) Add_line_list(&printers, default_printer, "", 1, 1);
		Add_line_list(&ppaths, IPPC_PRINTERS_PATH, "", 1, 1); /*for CUPS-specific operation, advertize only CUPS-compatible uri*/
	} else {
		printer = Find_str_value(auth_info, KWA_PRINTER);
		ppath = Find_str_value(auth_info, KWA_PPATH);
		if (!printer || !ppath) {
			response.op_id_status = CLIENT_ERROR_NOT_FOUND;
			goto ipperr;
		}
		DEBUGF(DNW2)("Ipp_op_get_printer_attributes: printer: %s path %s", printer, ppath);
		Add_line_list(&printers, printer, "", 1, 1);
		Add_line_list(&ppaths, ppath, "", 1, 1);
	}

	for (prcount = 0, group = 0; prcount < printers.count; prcount++) {
		printer = printers.list[prcount];
		if (Setup_printer(printer, error, errlen, 0))	{
			response.op_id_status = SERVER_ERROR_INTERNAL_ERROR;
			goto ipperr;
		}
		Ipa_set_perm(&Perm_check, 'S', auth_info);
		if (Perms_check(&Perm_line_list, &Perm_check, 0, 0) == P_REJECT)
		{
			response.op_id_status = CLIENT_ERROR_NOT_AUTHENTICATED;
			goto ipperr;
		}

		/*get some info from printcap*/
		Select_pc_info(printer, &pc_entry, &pc_alias, &PC_names_line_list, &PC_order_line_list, &PC_info_line_list, 0, 0);
		DEBUGFC(DNW4)Dump_line_list("Ipp_op_get_printer_attributes: printcap aliases", &pc_alias);
		DEBUGFC(DNW4)Dump_line_list("Ipp_op_get_printer_attributes: printcap entry", &pc_entry);
		Ipa_get_printcap_auth(&auths, &pc_entry, ppath, printer, &Perm_check);
		DEBUGFC(DNW4)Dump_line_list("Ipp_op_get_printer_attributes: printcap auths", &auths);
		if (auths.count < 1) {
			DEBUGF(DNW4)("Ipp_op_get_printer_attributes: no valid authentication type for printer %s", printer);
			continue;
		}
		lprng_copies = safestrcmp(Find_str_value(&pc_entry, PC_IMC), PCV_IMC_IF);
		Ipa_get_printer_state(&pstate, &preason, &j_accept, &j_count);
		DEBUGF(DNW4)("Ipp_op_get_printer_attributes: state: %s", preason);

		for (pthcount = 0; pthcount < ppaths.count; pthcount++) {
			ppath = ppaths.list[pthcount];
			/*requested attribute values*/
			if (Find_casekey_str_value(&requested_attributes, IPPAN_PRINTER_URI_SUPPORTED, Hash_value_sep)) {
				/*send only printer-uri you are connected, CUPS clients do not like more different uris like http+https*/
				char *uri = NULL;
				Ipa_get_printer_uri(&uri, ipp_request->version, conn->port, ppath, printer);
				int i;
				for (i = 0; i < auths.count; i++)
					Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_PRINTER_URI_SUPPORTED,
						IPPDT_URI, i, uri, strlen(uri));
				if (uri)  free(uri);
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_PRINTER_URI_SUPPORTED, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_URI_SEC_SUPPORTED, Hash_value_sep)) {
#ifdef	SSL_ENABLE
				int i;
				for (i = 0; i < auths.count; i++)
					if (ipp_ssl_available || conn->ssl)
						Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_URI_SEC_SUPPORTED,
							IPPDT_KEYWORD, i, IPPAV_SEC_TLS, strlen(IPPAV_SEC_TLS));
					else
						Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_URI_SEC_SUPPORTED,
							IPPDT_KEYWORD, i, IPPAV_SEC_NONE, strlen(IPPAV_SEC_NONE));
#else	/*SSL_ENABLE*/
				int i;
				for (i = 0; i < auths.count; i++)
					Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_URI_SEC_SUPPORTED,
						IPPDT_KEYWORD, i, IPPAV_SEC_NONE, strlen(IPPAV_SEC_NONE));
#endif	/*SSL_ENABLE*/
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_URI_SEC_SUPPORTED, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_URI_AUTH_SUPPORTED, Hash_value_sep)) {
				int i;
				for (i = 0; i < auths.count; i++)
					Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_URI_AUTH_SUPPORTED,
						IPPDT_KEYWORD, i, auths.list[i], strlen(auths.list[i]));
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_URI_AUTH_SUPPORTED, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_AUTH_INFO_REQUIRED, Hash_value_sep)) {
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_AUTH_INFO_REQUIRED,
						IPPDT_KEYWORD, 0, IPPAV_AUTH_NONE, strlen(IPPAV_AUTH_NONE));
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_AUTH_INFO_REQUIRED, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_PRINTER_NAME, Hash_value_sep)) {
				char *name = NULL;
				Ipa_get_printername(&name, ppath, printer);
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_PRINTER_NAME,
						IPPDT_NAME_WITHOUT_LANG, 0, name, safestrlen(name));
				if (name) free(name);
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_PRINTER_NAME, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_PRINTER_LOCATION, Hash_value_sep)) {
				int i = pc_alias.count;
				char *loc = i > 1 ? pc_alias.list[i-1] : "" ;
				char *loc8 = Local_to_utf8(loc);
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_PRINTER_LOCATION,
						IPPDT_TEXT_WITHOUT_LANG, 0, loc8, safestrlen(loc8));
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_PRINTER_LOCATION, RQ_SUPP);
				if (loc8) free(loc8);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_PRINTER_INFO, Hash_value_sep)) {
				char *info = Comment_tag_DYN ? Comment_tag_DYN : "";
				char *info8 = Local_to_utf8(info);
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_PRINTER_INFO,
						IPPDT_TEXT_WITHOUT_LANG, 0, info8, safestrlen(info8));
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_PRINTER_INFO, RQ_SUPP);
				if (info8) free(info8);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_PRINTER_MAKE_MODEL, Hash_value_sep)) {
				int i = pc_alias.count;
				char *info = i > 1 ? pc_alias.list[i-1] : "" ;
				char *info8 = Local_to_utf8(info);
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_PRINTER_MAKE_MODEL,
						IPPDT_TEXT_WITHOUT_LANG, 0, info8, safestrlen(info8));
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_PRINTER_MAKE_MODEL, RQ_SUPP);
				if (info8) free (info8);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_PRINTER_STATE, Hash_value_sep)) {
				/*show the value only to authenticated users ... kprinter does not like this* /
				if (ipp_request->op_id_status != IPPOP_GET_PRINTER_ATTRIBUTES) {
					Ipp_set_attr(&response.attributes, UNSUPPORTED_ATTRIBUTES_GRP, group, IPPAN_PRINTER_STATE,
						IPPDT_UNKNOWN, 0, NULL, 0);
				} else */ {
					char buf[4];
					hton32(buf, pstate);
					Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_PRINTER_STATE,
						IPPDT_ENUM, 0, buf, 4);
				}
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_PRINTER_STATE, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_PRINTER_SREASONS, Hash_value_sep)) {
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_PRINTER_SREASONS,
						IPPDT_KEYWORD, 0, preason, strlen(preason));
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_PRINTER_SREASONS, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_IPP_VER_SUPPORTED, Hash_value_sep)) {
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_IPP_VER_SUPPORTED,
						IPPDT_KEYWORD, 0, IPPAV_VER_11, strlen(IPPAV_VER_11));
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_IPP_VER_SUPPORTED, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_OPERATIONS_SUPPORTED, Hash_value_sep)) {
				int i;
				struct ipp_procs *s;
				char buf[4];
				for (i = 0, s = OperationsSupported; s->op_code; i++, s++) {
					hton32(buf, s->op_code);
					Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_OPERATIONS_SUPPORTED,
						IPPDT_ENUM, i, buf, 4);
				}
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_OPERATIONS_SUPPORTED, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_MULT_DOC_JOBS_SUPP, Hash_value_sep)) {
				char b = 1;
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_MULT_DOC_JOBS_SUPP,
						IPPDT_BOOLEAN, 0, &b, 1);
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_MULT_DOC_JOBS_SUPP, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_CHARSET_CONFIGURED, Hash_value_sep)) {
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_CHARSET_CONFIGURED,
						IPPDT_CHARSET, 0, IPPAV_UTF_8, safestrlen(IPPAV_UTF_8));
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_CHARSET_CONFIGURED, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_CHARSET_SUPPORTED, Hash_value_sep)) {
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_CHARSET_SUPPORTED,
						IPPDT_CHARSET, 0, IPPAV_UTF_8, strlen(IPPAV_UTF_8));
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_CHARSET_SUPPORTED, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_NL_CONFIGURED, Hash_value_sep)) {
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_NL_CONFIGURED,
						IPPDT_NAT_LANG, 0, IPPAV_EN_US, strlen(IPPAV_EN_US));
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_NL_CONFIGURED, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_NL_SUPPORTED, Hash_value_sep)) {
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_NL_SUPPORTED,
						IPPDT_NAT_LANG, 0, IPPAV_EN_US, strlen(IPPAV_EN_US));
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_NL_SUPPORTED, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_DOC_FORMAT_DEFAULT, Hash_value_sep)) {
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_DOC_FORMAT_DEFAULT,
						IPPDT_MIME_MEDIA_TYPE, 0, IPPAV_APP_OCTET_STREAM, strlen(IPPAV_APP_OCTET_STREAM));
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_DOC_FORMAT_DEFAULT, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_DOC_FORMAT_SUPPORTED, Hash_value_sep)) {
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_DOC_FORMAT_SUPPORTED,
						IPPDT_MIME_MEDIA_TYPE, 0, IPPAV_APP_OCTET_STREAM, strlen(IPPAV_APP_OCTET_STREAM));
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_DOC_FORMAT_SUPPORTED,
						IPPDT_MIME_MEDIA_TYPE, 1, IPPAV_APP_CUPS_RAW, strlen(IPPAV_APP_CUPS_RAW));
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_DOC_FORMAT_SUPPORTED, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_PRINTER_JOB_ACCEPTING, Hash_value_sep)) {
				char b = j_accept ? 1 : 0;
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_PRINTER_JOB_ACCEPTING,
						IPPDT_BOOLEAN, 0, &b, 1);
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_PRINTER_JOB_ACCEPTING, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_QUEUED_JOBS, Hash_value_sep)) {
				char buf[4];
				hton32(buf, j_count);
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_QUEUED_JOBS,
						IPPDT_INTEGER, 0, buf, 4);
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_QUEUED_JOBS, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_PDL_OVRD_SUPPORTED, Hash_value_sep)) {
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_PDL_OVRD_SUPPORTED,
						IPPDT_KEYWORD, 0, IPPAV_NOT_ATTEMPTED, strlen(IPPAV_NOT_ATTEMPTED));
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_PDL_OVRD_SUPPORTED, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_PRINTER_UP_TIME, Hash_value_sep)) {
				char buf[4];
				time_t t;
				time(&t);
				hton32(buf, t);
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_PRINTER_UP_TIME,
						IPPDT_INTEGER, 0, buf, 4);
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_PRINTER_UP_TIME, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_MULT_OP_TIMEOUT, Hash_value_sep)) {
				char buf[4];
				hton32(buf, Done_jobs_max_age_DYN);
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_MULT_OP_TIMEOUT,
						IPPDT_INTEGER, 0, buf, 4);
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_MULT_OP_TIMEOUT, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_COMPRESSION_SUPPORTED, Hash_value_sep)) {
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_COMPRESSION_SUPPORTED,
						IPPDT_KEYWORD, 0, IPPAV_COMPR_NONE, strlen(IPPAV_COMPR_NONE));
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_COMPRESSION_SUPPORTED, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_JOB_SETTABLE_ATTRS_S, Hash_value_sep)) {
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_JOB_SETTABLE_ATTRS_S,
						IPPDT_KEYWORD, 0, IPPAN_JOB_PRI, strlen(IPPAN_JOB_PRI));
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_JOB_SETTABLE_ATTRS_S,
						IPPDT_KEYWORD, 1, IPPAN_JOB_HOLD_UNTIL, strlen(IPPAN_JOB_HOLD_UNTIL));
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_JOB_SETTABLE_ATTRS_S, RQ_SUPP);
			}
			/*CUPS-specific printer-description attributes*/
			if (Find_casekey_str_value(&requested_attributes, IPPAN_PRINTER_TYPE, Hash_value_sep)) {
				char buf[4];
				int prtype = 0;
				if (!Lp_device_DYN)                        prtype |= 0x00000002; /*network remote printer*/
				if (!safestrcmp(printer, default_printer)) prtype |= 0x00020000; /*printer is default*/
				if (!j_accept)                             prtype |= 0x00080000; /*printer is rejecting*/
				hton32(buf, prtype);
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_PRINTER_TYPE,
						IPPDT_ENUM, 0, buf, 4);
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_PRINTER_TYPE, RQ_SUPP);
			}
			/*Novell-specific printer-description attributes*/
			if (Find_casekey_str_value(&requested_attributes, IPPAN_USER_RIGHTS, Hash_value_sep)) {
				char buf[4];
				hton32(buf, 0x1);
				/* guess:
				   0 = Follow Windows standards and only allow useers with sufficient rights to install the printer to the desktop. This allows  ALL USERS to see/use this printer.
				   1 = If current user doesn't have rights to add a WORKSTATION PRINTER, automatically add  the printer so that ONLY THIS USER can Add/View/Modify/Delete this printer. In essence this will be a PRIVATE OR USER PRINTER.
				   2 = Only add USER PRINTERS. No rights required.  All users (including Administrator or power user) that  add a printer will be adding a printer that only they can Add/View/Modify/Delete. (ALL PRINTERS ARE PRIVATE OR USER PRINTERS).
				   3 = Only add WORKSTATION PRINTERS. No rights required.*/
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_USER_RIGHTS,
						IPPDT_INTEGER, 0, buf, 4);
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_USER_RIGHTS, RQ_SUPP);
			}
			/*Job-template [default] attributes*/
			if (Find_casekey_str_value(&requested_attributes, IPPAN_JOB_PRI_SUPPORTED, Hash_value_sep)) {
				char buf[4];
				hton32(buf, Ignore_requested_user_priority_DYN ? 1: 26);
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_JOB_PRI_SUPPORTED,
						IPPDT_INTEGER, 0, buf, 4);
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_JOB_PRI_SUPPORTED, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_JOB_PRI_DEFAULT, Hash_value_sep)) {
				char cpri = Default_priority_DYN ? Default_priority_DYN[0] : 'A';
				char buf[4];
				hton32(buf, Ipa_int_priority(cpri, Reverse_priority_order_DYN));
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_JOB_PRI_DEFAULT,
						IPPDT_INTEGER, 0, buf, 4);
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_JOB_PRI_DEFAULT, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_COPIES_SUPPORTED, Hash_value_sep)) {
				char buf[8];
				int l = 1;
				int h = Max_copies_DYN < 1 ? 1 : Max_copies_DYN;
				hton32(buf, l);
				hton32(buf+4, h);
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_COPIES_SUPPORTED,
						IPPDT_RANGE_INT, 0, buf, 8);
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_COPIES_SUPPORTED, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_COPIES_DEFAULT, Hash_value_sep)) {
				if (lprng_copies) {
					char buf[4];
					hton32(buf, 1);
					Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_COPIES_DEFAULT,
						IPPDT_INTEGER, 0, buf, 4);
				} else {
					Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_COPIES_DEFAULT,
						IPPDT_UNKNOWN, 0, NULL, 0);
				}
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_COPIES_DEFAULT, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_MULT_DOC_HNDL_SUPP, Hash_value_sep)) {
				if (lprng_copies) {
					Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_MULT_DOC_HNDL_SUPP,
						IPPDT_KEYWORD, 0, IPPAV_SDC_COLLATED, safestrlen(IPPAV_SDC_COLLATED));
				} else {
					Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_MULT_DOC_HNDL_SUPP,
						IPPDT_UNKNOWN, 0, NULL, 0);
				}
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_MULT_DOC_HNDL_SUPP, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_MULT_DOC_HNDL_DEFAULT, Hash_value_sep)) {
				if (lprng_copies) {
					Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_MULT_DOC_HNDL_DEFAULT,
						IPPDT_KEYWORD, 0, IPPAV_SDC_COLLATED, safestrlen(IPPAV_SDC_COLLATED));
				} else {
					Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_MULT_DOC_HNDL_DEFAULT,
						IPPDT_UNKNOWN, 0, NULL, 0);
				}
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_MULT_DOC_HNDL_DEFAULT, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_JOB_HOLD_UNTIL_SUPP, Hash_value_sep)) {
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_JOB_HOLD_UNTIL_SUPP,
						IPPDT_KEYWORD, 0, IPPAV_NO_HOLD, safestrlen(IPPAV_NO_HOLD));
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_JOB_HOLD_UNTIL_SUPP,
						IPPDT_KEYWORD, 1, IPPAV_INDEFINITE, safestrlen(IPPAV_INDEFINITE));
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_JOB_HOLD_UNTIL_SUPP, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_JOB_HOLD_UNTIL_D, Hash_value_sep)) {
				if (Auto_hold_DYN) {
					Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_JOB_HOLD_UNTIL_D,
						IPPDT_KEYWORD, 0, IPPAV_INDEFINITE, safestrlen(IPPAV_INDEFINITE));
				} else {
					Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_JOB_HOLD_UNTIL_D,
						IPPDT_KEYWORD, 0, IPPAV_NO_HOLD, safestrlen(IPPAV_NO_HOLD));
				}
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_JOB_HOLD_UNTIL_D, RQ_SUPP);
			}
			/*Subscription*/
			if (Find_casekey_str_value(&requested_attributes, IPPAN_NTF_PULL_METHOD_SUPP, Hash_value_sep)) {
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_NTF_PULL_METHOD_SUPP,
						IPPDT_KEYWORD, 0, IPPAV_IPPGET, strlen(IPPAV_IPPGET));
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_NTF_PULL_METHOD_SUPP, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_NTF_EVENTS_DEFAULT, Hash_value_sep)) {
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_NTF_EVENTS_DEFAULT,
						IPPDT_KEYWORD, 0, IPPAV_EVENT_NONE, strlen(IPPAV_EVENT_NONE));
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_NTF_EVENTS_DEFAULT, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_NTF_EVENTS_SUPPORTED, Hash_value_sep)) {
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_NTF_EVENTS_SUPPORTED,
						IPPDT_KEYWORD, 0, IPPAV_EVENT_PR_STATECHANGE, strlen(IPPAV_EVENT_PR_STATECHANGE));
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_NTF_EVENTS_SUPPORTED,
						IPPDT_KEYWORD, 1, IPPAV_EVENT_PR_STOPPED, strlen(IPPAV_EVENT_PR_STOPPED));
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_NTF_EVENTS_SUPPORTED,
						IPPDT_KEYWORD, 2, IPPAV_EVENT_JOB_STATECHANGE, strlen(IPPAV_EVENT_JOB_STATECHANGE));
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_NTF_EVENTS_SUPPORTED,
						IPPDT_KEYWORD, 3, IPPAV_EVENT_JOB_CREATED, strlen(IPPAV_EVENT_JOB_CREATED));
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_NTF_EVENTS_SUPPORTED,
						IPPDT_KEYWORD, 4, IPPAV_EVENT_JOB_COMPLETED, strlen(IPPAV_EVENT_JOB_COMPLETED));
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_NTF_EVENTS_SUPPORTED, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_NTF_MAX_EVENTS_SUPP, Hash_value_sep)) {
				char buf[4];
				hton32(buf, NTF_MAX_EVENTS);
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_NTF_MAX_EVENTS_SUPP,
						IPPDT_INTEGER, 0, buf, 4);
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_NTF_MAX_EVENTS_SUPP, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_NTF_LEASE_DURATION_D, Hash_value_sep)) {
				char buf[4];
				hton32(buf, 0);
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_NTF_LEASE_DURATION_D,
						IPPDT_INTEGER, 0, buf, 4);
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_NTF_LEASE_DURATION_D, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_NTF_LEASE_DURATION_S, Hash_value_sep)) {
				char buf[4];
				hton32(buf, 0);
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_NTF_LEASE_DURATION_S,
						IPPDT_INTEGER, 0, buf, 4);
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_NTF_LEASE_DURATION_S, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_IPPGET_EVENT_LIFE, Hash_value_sep)) {
				char buf[4];
				hton32(buf, Done_jobs_max_age_DYN);
				Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_IPPGET_EVENT_LIFE,
						IPPDT_INTEGER, 0, buf, 4);
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_IPPGET_EVENT_LIFE, RQ_SUPP);
			}
			/*unsupported attributes*/
			if (!group) {
				for (i = requested_attributes.count - 1; i >= 0 ; i--) {
					char *c, s, *key;
					c = safestrpbrk(requested_attributes.list[i], Hash_value_sep);
					if (!c) continue;
					s = *c;	*c = '\0';
					key = safestrdup(requested_attributes.list[i], __FILE__, __LINE__);
					*c = s;
					if (!safestrcmp(Find_casekey_str_value(&requested_attributes, key, Hash_value_sep), RQ_TRUE)) {
						Ipp_set_attr(&response.attributes, UNSUPPORTED_ATTRIBUTES_GRP, 0, key, IPPDT_UNSUPPORTED, 0, NULL, 0);
						Set_casekey_str_value(&requested_attributes, key, NULL);
						response.op_id_status = SUCCESSFUL_OK_IGNORED_SUBSTITUED;
					}
					if (key) free(key);
				}
			}
			group++;
			if (limit && (limit == group)) goto ccend;
		}
	}

 ccend:
 ipperr:
	Ipp_send_response(conn, http_headers, body_rest_len, &response, auth_info, 0, NULL);

	Free_line_list(&requested_attributes);
	Free_line_list(&auths);
	Free_line_list(&pc_entry);
	Free_line_list(&pc_alias);
	if (default_printer) free(default_printer);
	Free_line_list(&ppaths);
	Free_line_list(&printers);
	Ipp_free_operation(&response);

	return 0;
}

#define	JOBS_ALL		0
#define	JOBS_COMPLETED		1
#define	JOBS_NOT_COMPLETED	2

static int Ipp_op_get_jobs(struct line_list *http_request, struct line_list *http_headers, const struct http_conn *conn,
             ssize_t *body_rest_len, struct ipp_operation *ipp_request, struct line_list *auth_info)
{

	struct ipp_operation response;
	char *printer = NULL;
	char *ppath = NULL;
	struct ipp_attr *a;
	char error[SMALLBUFFER];
	int errlen = sizeof(error);
	int permission;
	int printable, move, held, err, done, count, limit, group, jobnum;
	struct job job;
	struct line_list requested_attributes, pc_entry, pc_alias;
	int i, jobid = 0, myjobs, whichjobs, jobstate, lprng_copies;
	char *username, *jreason;
	struct line_list printers;
	char *uagent;
	int prcount, ipp_first_job_id;
	void *sort_param;


	Init_job(&job);
	Ipp_init_operation(&response);
	response.version = ipp_request->version;
	response.request_id = ipp_request->request_id;
	Init_line_list(&requested_attributes);
	Init_line_list(&pc_entry);
	Init_line_list(&pc_alias);
	Init_line_list(&printers);

	/*set response mandatory attributes*/
	Ipp_set_attr(&response.attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_CHARSET, IPPDT_CHARSET, 0,
		        IPPAV_UTF_8, safestrlen(IPPAV_UTF_8));
	Ipp_set_attr(&response.attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_NAT_LANG, IPPDT_NAT_LANG, 0,
		        IPPAV_EN_US, safestrlen(IPPAV_EN_US));
	/*validate request - charset and language*/
	response.op_id_status = Ipa_validate_charset(NULL, ipp_request->attributes);
	if (response.op_id_status != SUCCESSFUL_OK) goto ipperr;
	response.op_id_status = Ipa_validate_language(NULL, ipp_request->attributes);
	if (response.op_id_status != SUCCESSFUL_OK) goto ipperr;

	printer = Find_str_value(auth_info, KWA_PRINTER);
	ppath = Find_str_value(auth_info, KWA_PPATH);
	if (!printer || !ppath) {
		response.op_id_status = CLIENT_ERROR_NOT_FOUND;
		goto ipperr;
	}
	DEBUGF(DNW2)("Ipp_op_get_jobs: printer: %s path %s", printer, ppath);
	/*this procedure is common to two IPP operations*/
	if (ipp_request->op_id_status == IPPOP_GET_JOB_ATTRIBUTES)
	{
		response.op_id_status = Ipa_get_job_id(&jobid, ipp_request->attributes);
		DEBUGF(DNW2)("Ipp_op_get_jobs: requested job-id for operation Get-job-attributes: %d", jobid);
		if (response.op_id_status != SUCCESSFUL_OK) goto ipperr;
	}

	Ipa_prase_debug(DLPQMASK);


	response.op_id_status = SUCCESSFUL_OK;
	/*unsupported attributes*/
	/*add unsupported group mumber 2*/
	Ipp_set_attr(&response.attributes, UNSUPPORTED_ATTRIBUTES_GRP, 0, NULL, 0, 0, NULL, 0);
	for (a = ipp_request->attributes; a; a = a->next) {
		if ((!safestrcmp(a->name, IPPAN_CHARSET)) ||
		    (!safestrcmp(a->name, IPPAN_NAT_LANG)) ||
		    (!safestrcmp(a->name, IPPAN_RQ_USRNAME)) ||
		    (!safestrcmp(a->name, IPPAN_PRINTER_URI)) ||
		    (!safestrcmp(a->name, IPPAN_JOB_URI)) ||
		    (!safestrcmp(a->name, IPPAN_JOB_ID)) ||
		    (!safestrcmp(a->name, IPPAN_REQUESTED_ATTRIBUTES)) ||
		    (!safestrcmp(a->name, IPPAN_LIMIT)) ||
		    (!safestrcmp(a->name, IPPAN_MY_JOBS)) ||
		    (!safestrcmp(a->name, IPPAN_WHICH_JOBS)) ||
		    (!safestrcmp(a->name, IPPAN_FIRST_JOB_ID))) continue;
		Ipp_set_attr(&response.attributes, UNSUPPORTED_ATTRIBUTES_GRP, 0, a->name, IPPDT_UNSUPPORTED, 0, NULL, 0);
		response.op_id_status = SUCCESSFUL_OK_IGNORED_SUBSTITUED;
	}

	/*find/get the requested attributes list*/
	a = Ipp_get_attr(ipp_request->attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_REQUESTED_ATTRIBUTES, -1);
	do {
		if (!a || !safestrncmp(a->value, IPPAV_ALL, a->value_len)) {
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_URI, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_ID, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_PRINTER_URI, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_NAME, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_ORIG_USERNAME, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_STATE, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_SREASONS, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_DETAILED_STATUS, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_TIME_AT_CREATION, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_TIME_AT_PROCESSING, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_TIME_AT_COMPLETED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_PRINTER_UP_TIME, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_KOCTETS, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_KOCTETS_PROCESSED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_PRI, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_COPIES, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_MULT_DOC_HNDL, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_HOLD_UNTIL, RQ_TRUE);
			/*see also Ipp_op_set_job_attributes()*/
			break;
		}
		else if (!safestrncmp(a->value, IPPAV_JOB_DESCRIPTION, a->value_len)) {
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_URI, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_ID, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_PRINTER_URI, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_NAME, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_ORIG_USERNAME, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_STATE, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_SREASONS, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_DETAILED_STATUS, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_TIME_AT_CREATION, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_TIME_AT_PROCESSING, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_TIME_AT_COMPLETED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_PRINTER_UP_TIME, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_KOCTETS, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_KOCTETS_PROCESSED, RQ_TRUE);
		}
		else if (!safestrncmp(a->value, IPPAV_JOB_TEMPLATE, a->value_len)) {
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_PRI, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_COPIES, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_MULT_DOC_HNDL, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_JOB_HOLD_UNTIL, RQ_TRUE);
		}
		else {
			char *buf = malloc_or_die(a->value_len + 1, __FILE__, __LINE__);
			memcpy(buf, a->value, a->value_len); buf[a->value_len] = '\0';
			Set_casekey_str_value(&requested_attributes, buf, RQ_TRUE);
			free(buf);
		}
		if (a) a = Ipp_get_attr(a->next, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_REQUESTED_ATTRIBUTES, -1);
	} while (a);

	/*limit requested ?*/
	limit = 0;
	a = Ipp_get_attr(ipp_request->attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_LIMIT, -1);
	if (a && a->value_len == 4) limit = ntoh32(a->value);
	/*my-jobs requested ?*/
	a = Ipp_get_attr(ipp_request->attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_MY_JOBS, -1);
	myjobs = (a && a->value_len == 1 && ( *(char *)(a->value) & 0xff));
	/*which-jobs requested ?*/
	if (ipp_request->op_id_status == IPPOP_GET_JOB_ATTRIBUTES)
		whichjobs = JOBS_ALL;
	else {
		a = Ipp_get_attr(ipp_request->attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_WHICH_JOBS, -1);
		if (a && a->value) {
			if (!safestrncmp(a->value, IPPAV_COMPLETED, a->value_len) )
				whichjobs = JOBS_COMPLETED;
			else if (!safestrncmp(a->value, IPPAV_NOT_COMPLETED, a->value_len) )
				whichjobs = JOBS_NOT_COMPLETED;
			else if (!safestrncmp(a->value, IPPAV_ALL, a->value_len) )
				whichjobs = JOBS_ALL;
			else {
				Ipp_set_attr(&response.attributes, UNSUPPORTED_ATTRIBUTES_GRP, 0, IPPAN_WHICH_JOBS, a->type, 0, a->value, a->value_len);
				response.op_id_status = CLIENT_ERROR_ATTRS_OR_VALS_NOT_SUPPORTED;
				goto ipperr;
			}
		} else {
			uagent = Find_str_value(http_headers, HTTPH_USER_AGENT);
			if (uagent && (Ipp_getjobs_compat_DYN & 0x100) &&
			    (!safestrcmp(uagent, HTTPV_UA_MS) || !safestrncmp(uagent, HTTPV_UA_NOVELL, safestrlen(HTTPV_UA_NOVELL)))
			   )
				whichjobs = JOBS_ALL;
			else
				whichjobs = JOBS_NOT_COMPLETED;
		}
	}
	/*CUPS specific anoying limit first-job-id - jobs are in sort order only for first CUPS_TRANS_HRCOUNT values */
	ipp_first_job_id = 0;
	a = Ipp_get_attr(ipp_request->attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_FIRST_JOB_ID, -1);
	if (a) {
		if (!(Ipp_getjobs_compat_DYN & 3) || (a->value_len != 4) ||
		     (ipp_first_job_id = ntoh32(a->value)) >= (All_line_list.count * Ipp_compat_hrcount_DYN) ||
		     (limit > Ipp_compat_hrcount_DYN)
		   ) {
			ipp_first_job_id = 0;
			Ipp_set_attr(&response.attributes, UNSUPPORTED_ATTRIBUTES_GRP, 0, IPPAN_FIRST_JOB_ID, IPPDT_UNSUPPORTED, 0, NULL, 0);
		}
	}

	DEBUGF(DNW4)("Ipp_get_jobs: limit %d myjobs %d whichjobs %d first-job-id %d", limit, myjobs, whichjobs, ipp_first_job_id);
	DEBUGF(DLPQ4)("Ipp_get_jobs: limit %d myjobs %d whichjobs %d first-job-id %d", limit, myjobs, whichjobs, ipp_first_job_id);

	/*make printer list*/
	if (!safestrcmp(printer, IPPC_ALLPRINTERS)) {
		Get_all_printcap_entries();
		Merge_line_list(&printers, &All_line_list, "", 0, 1); /*copying must preserve order to preserve ipp-uniqe-jobid for ipp_first_job_id ugly request*/
	} else {
		Add_line_list(&printers, printer, "", 1, 1);
	}

	for (prcount = 0, group = 0; prcount < printers.count; prcount++) {
		printer = printers.list[prcount];
		if (Setup_printer(printer, error, errlen, 0))	{
			response.op_id_status = SERVER_ERROR_INTERNAL_ERROR;
			goto ipperr;
		}
		Ipa_set_perm(&Perm_check, 'Q', auth_info);
		permission = Perms_check(&Perm_line_list, &Perm_check, 0, 0);
		DEBUGF(DLPQ1)("Ipa_op_get_jobs: permission '%s'", perm_str(permission));
		if (permission == P_REJECT)
		{
			response.op_id_status = CLIENT_ERROR_NOT_AUTHENTICATED;
			goto ipperr;
		}

		Select_pc_info(printer, &pc_entry, &pc_alias, &PC_names_line_list, &PC_order_line_list, &PC_info_line_list, 0, 0);
		lprng_copies = safestrcmp(Find_str_value(&pc_entry, PC_IMC), PCV_IMC_IF);
		/* we do not need status so the procedure is simplified
		 * the remote queues are ignored (todo)
		 * */
		Free_line_list(&Spool_control);
		Get_spool_control(Queue_control_file_DYN, &Spool_control);
		sort_param = (ipp_request->op_id_status == IPPOP_GET_JOB_ATTRIBUTES) ? &jobid : NULL;
		Scan_queue_proc(&Spool_control, &Sort_order, &printable, &held, &move, 0, &err, &done, 0, 0, order_filter_job_number, sort_param);
		/*remove done jobs - depends on Sort_order global variable*/
		if (Remove_done_jobs()) {
			Scan_queue_proc(&Spool_control, &Sort_order, &printable, &held, &move, 0, &err, &done, 0, 0, order_filter_job_number, sort_param);
		}
		/* first-job-id is the first job in Sort_order (sorted by order_filter_job_number)
		 */
		for(count = 0; count < Sort_order.count; count++) {
			/*get job info*/
			Free_job(&job);
			Get_job_ticket_file(0, &job, Sort_order.list[count]);
			if (!job.info.count) {
				/*got lpd_status.c*/
				continue;
			}
			jobnum = Find_decimal_value(&job.info, NUMBER);
			/*CUPS first-job-id*/
			DEBUGF(DNW4)("Ipa_get_jobs - ipp_jobnum %d",Ipa_jobid2ipp(printer, jobnum));
			if (ipp_first_job_id && (Ipa_jobid2ipp(printer, jobnum) < ipp_first_job_id)) {
				continue;
			}
			Job_printable(&job, &Spool_control, &printable, &held, &move, &err, &done);

			DEBUGFC(DLPQ4)Dump_job("Ipa_get_jobs - info", &job);

			/*fitler owner name - my-jobs*/
			username = Find_str_value(&job.info, LOGNAME);
			if (myjobs && safestrcmp(username, Find_str_value(auth_info, KWA_USER)))
				continue;
			/*jobid filter for get-job-attributes operation*/
			/*filtered by Scan_queue_proc*/
			/*if ((ipp_request->op_id_status == IPPOP_GET_JOB_ATTRIBUTES) &&
			    (jobid != jobnum)) {
				continue;
			}
			*/
			/*here we could check per-job Permission, but lpq command does nothing similar*/

			/*state filter*/
			Ipa_get_job_state(&jobstate, &jreason, &job, &Spool_control);
			if ((whichjobs == JOBS_COMPLETED &&
			      (jobstate != IPPAV_JRS_COMPLETED) && (jobstate != IPPAV_JRS_ABORTED) && (jobstate != IPPAV_JRS_CANCELED) ) ||
			    (whichjobs == JOBS_NOT_COMPLETED &&
			      ((jobstate == IPPAV_JRS_COMPLETED) || (jobstate == IPPAV_JRS_ABORTED) || (jobstate == IPPAV_JRS_CANCELED))) )
				continue;
			DEBUGF(DNW4)("Ipa_get_jobs - job %d printable %d held %d move %d err %d  done %d state %d",
					jobnum, printable, held, move, err, done, jobstate);

			/*fill requested attributes*/
			if (Find_casekey_str_value(&requested_attributes, IPPAN_JOB_URI, Hash_value_sep)) {
				char *uri = NULL;
				Ipa_get_job_uri(&uri, ipp_request->version, conn->port, ppath, printer, jobnum);
				Ipp_set_attr(&response.attributes, JOB_ATTRIBUTES_GRP, group, IPPAN_JOB_URI,
						IPPDT_URI, 0, uri, strlen(uri));
				if (uri) free(uri);
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_JOB_URI, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_JOB_ID, Hash_value_sep)) {
				char buf[4];
				hton32(buf, Ipa_jobid2ipp(printer, jobnum));
				Ipp_set_attr(&response.attributes, JOB_ATTRIBUTES_GRP, group, IPPAN_JOB_ID,
						IPPDT_INTEGER, 0, buf, 4);
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_JOB_ID, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_JOB_PRINTER_URI, Hash_value_sep)) {
				char *uri = NULL;
				Ipa_get_printer_uri(&uri, ipp_request->version, conn->port, ppath, printer);
				Ipp_set_attr(&response.attributes, JOB_ATTRIBUTES_GRP, group, IPPAN_JOB_PRINTER_URI,
						IPPDT_URI, 0, uri, strlen(uri));
				if (uri) free(uri);
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_JOB_PRINTER_URI, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_JOB_NAME, Hash_value_sep)) {
				char *jobname8 = Local_to_utf8(Find_str_value(&job.info, JOBNAME)); /*utf-8*/
				Ipp_set_attr(&response.attributes, JOB_ATTRIBUTES_GRP, group, IPPAN_JOB_NAME,
						IPPDT_NAME_WITHOUT_LANG, 0, jobname8, safestrlen(jobname8));
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_JOB_NAME, RQ_SUPP);
				if (jobname8) free(jobname8);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_ORIG_USERNAME, Hash_value_sep)) {
				Ipp_set_attr(&response.attributes, JOB_ATTRIBUTES_GRP, group, IPPAN_ORIG_USERNAME,
						IPPDT_NAME_WITHOUT_LANG, 0, username, safestrlen(username));
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_ORIG_USERNAME, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_JOB_STATE, Hash_value_sep)) {
				char buf[4];
				hton32(buf, jobstate);
				Ipp_set_attr(&response.attributes, JOB_ATTRIBUTES_GRP, group, IPPAN_JOB_STATE,
						IPPDT_ENUM, 0, buf, 4);
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_JOB_STATE, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_JOB_SREASONS, Hash_value_sep)) {
				Ipp_set_attr(&response.attributes, JOB_ATTRIBUTES_GRP, group, IPPAN_JOB_SREASONS,
						IPPDT_KEYWORD, 0, jreason, strlen(jreason));
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_JOB_SREASONS, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_JOB_DETAILED_STATUS, Hash_value_sep)) {
				char *status8 = Local_to_utf8(Find_str_value(&job.info, ERROR)); /*utf-8*/
				Ipp_set_attr(&response.attributes, JOB_ATTRIBUTES_GRP, group, IPPAN_JOB_DETAILED_STATUS,
						IPPDT_TEXT_WITHOUT_LANG, 0, status8, safestrlen(status8));
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_JOB_DETAILED_STATUS, RQ_SUPP);
				if (status8) free(status8);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_TIME_AT_CREATION, Hash_value_sep)) {
				char buf[4];
				time_t t = Convert_to_time_t(Find_str_value(&job.info, JOB_TIME));
				hton32(buf, t);
				Ipp_set_attr(&response.attributes, JOB_ATTRIBUTES_GRP, group, IPPAN_TIME_AT_CREATION,
						IPPDT_INTEGER, 0, buf, 4);
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_TIME_AT_CREATION, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_TIME_AT_PROCESSING, Hash_value_sep)) {
				char *p, buf[4];
				p = Find_str_value(&job.info, START_TIME);
				if (p) {
					time_t t = Convert_to_time_t(p);
					hton32(buf, t);
					Ipp_set_attr(&response.attributes, JOB_ATTRIBUTES_GRP, group, IPPAN_TIME_AT_PROCESSING,
							IPPDT_INTEGER, 0, buf, 4);
				} else {
					Ipp_set_attr(&response.attributes, JOB_ATTRIBUTES_GRP, group, IPPAN_TIME_AT_PROCESSING,
							IPPDT_NO_VALUE, 0, NULL, 0);
				}
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_TIME_AT_PROCESSING, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_TIME_AT_COMPLETED, Hash_value_sep)) {
				char buf[4];
				time_t t = Convert_to_time_t(Find_str_value(&job.info, DONE_TIME));
				if (t) {
					hton32(buf, t);
					Ipp_set_attr(&response.attributes, JOB_ATTRIBUTES_GRP, group, IPPAN_TIME_AT_COMPLETED,
							IPPDT_INTEGER, 0, buf, 4);
				} else {
					Ipp_set_attr(&response.attributes, JOB_ATTRIBUTES_GRP, group, IPPAN_TIME_AT_COMPLETED,
							IPPDT_NO_VALUE, 0, NULL, 0);
				}
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_TIME_AT_COMPLETED, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_JOB_PRINTER_UP_TIME, Hash_value_sep)) {
				char buf[4];
				time_t t;
				time(&t);
				hton32(buf, t);
				Ipp_set_attr(&response.attributes, JOB_ATTRIBUTES_GRP, group, IPPAN_JOB_PRINTER_UP_TIME,
						IPPDT_INTEGER, 0, buf, 4);
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_JOB_PRINTER_UP_TIME, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_JOB_KOCTETS, Hash_value_sep)) {
				char buf[4];
				ssize_t sz = Find_double_value(&job.info, SIZE);
				sz = (sz + 1023) / 1024;
				hton32(buf, sz);
				Ipp_set_attr(&response.attributes, JOB_ATTRIBUTES_GRP, group, IPPAN_JOB_KOCTETS,
						IPPDT_INTEGER, 0, buf, 4);
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_JOB_KOCTETS, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_JOB_KOCTETS_PROCESSED, Hash_value_sep)) {
				char buf[4];
				ssize_t sz = Find_double_value(&job.info, SIZE);
				int copies = Find_flag_value(&job.info, COPIES);
				if (!copies) copies = 1;
				sz = copies * (sz + 1023) / 1024;
				hton32(buf, sz);
				Ipp_set_attr(&response.attributes, JOB_ATTRIBUTES_GRP, group, IPPAN_JOB_KOCTETS_PROCESSED,
						IPPDT_INTEGER, 0, buf, 4);
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_JOB_KOCTETS_PROCESSED, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_JOB_PRI, Hash_value_sep)) {
				char buf[4];
				char *pri = Find_str_value(&job.info, PRIORITY);
				if (pri) {
					hton32(buf, Ipa_int_priority(pri[0], Reverse_priority_order_DYN));
					Ipp_set_attr(&response.attributes, JOB_ATTRIBUTES_GRP, group, IPPAN_JOB_PRI,
							IPPDT_INTEGER, 0, buf, 4);
				} else {
					Ipp_set_attr(&response.attributes, JOB_ATTRIBUTES_GRP, group, IPPAN_JOB_PRI,
							IPPDT_UNKNOWN, 0, NULL, 0);
				}
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_JOB_PRI, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_COPIES, Hash_value_sep)) {
				if (lprng_copies) {
					int copies = 0;
					char buf[4];
					/*count copies */
					for (i = 0; i < job.datafiles.count; i++) {
						copies += Find_flag_value((struct line_list *)job.datafiles.list[i], COPIES);
					}
					hton32(buf, copies);
					Ipp_set_attr(&response.attributes, JOB_ATTRIBUTES_GRP, group, IPPAN_COPIES,
							IPPDT_INTEGER, 0, buf, 4);
				} else {
					Ipp_set_attr(&response.attributes, JOB_ATTRIBUTES_GRP, group, IPPAN_COPIES,
							IPPDT_UNKNOWN, 0, NULL, 0);
				}
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_COPIES, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_MULT_DOC_HNDL, Hash_value_sep)) {
				if (lprng_copies) {
					Ipp_set_attr(&response.attributes, JOB_ATTRIBUTES_GRP, group, IPPAN_MULT_DOC_HNDL,
						IPPDT_KEYWORD, 0, IPPAV_SDC_COLLATED, safestrlen(IPPAV_SDC_COLLATED));
				} else {
					Ipp_set_attr(&response.attributes, JOB_ATTRIBUTES_GRP, group, IPPAN_MULT_DOC_HNDL,
						IPPDT_UNKNOWN, 0, NULL, 0);
				}
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_MULT_DOC_HNDL, RQ_SUPP);
			}
			if (Find_casekey_str_value(&requested_attributes, IPPAN_JOB_HOLD_UNTIL, Hash_value_sep)) {
				if (jobstate == IPPAV_JRS_HELD) {
					Ipp_set_attr(&response.attributes, JOB_ATTRIBUTES_GRP, group, IPPAN_JOB_HOLD_UNTIL,
						IPPDT_KEYWORD, 0, IPPAV_INDEFINITE, safestrlen(IPPAV_INDEFINITE));
				} else {
					Ipp_set_attr(&response.attributes, JOB_ATTRIBUTES_GRP, group, IPPAN_JOB_HOLD_UNTIL,
						IPPDT_KEYWORD, 0, IPPAV_NO_HOLD, safestrlen(IPPAV_NO_HOLD));
				}
				if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_JOB_HOLD_UNTIL, RQ_SUPP);
			}
			/*unsupported attributes*/
			if (!group) {
				for (i = requested_attributes.count - 1; i >= 0 ; i--) {
					char *c, s, *key;
					c = safestrpbrk(requested_attributes.list[i], Hash_value_sep);
					if (!c) continue;
					s = *c;	*c = '\0';
					key = safestrdup(requested_attributes.list[i], __FILE__, __LINE__);
					*c = s;
					if (!safestrcmp(Find_casekey_str_value(&requested_attributes, key, Hash_value_sep), RQ_TRUE)) {
						Ipp_set_attr(&response.attributes, UNSUPPORTED_ATTRIBUTES_GRP, 0, key, IPPDT_UNSUPPORTED, 0, NULL, 0);
						Set_casekey_str_value(&requested_attributes, key, NULL);
						response.op_id_status = SUCCESSFUL_OK_IGNORED_SUBSTITUED;
					}
					if (key) free(key);
				}
			}
			group++;
			if ((ipp_request->op_id_status == IPPOP_GET_JOB_ATTRIBUTES)) {
				/*send some printer info if error*/
				if ((jobstate == IPPAV_JRS_PROCESSING) ||
				    (jobstate == IPPAV_JRS_ABORTED) || (jobstate == IPPAV_JRS_CANCELED)) {
					struct line_list l;
					char *image = Get_file_image(Queue_status_file_DYN, 4 /*kB*/);
					char stats[1024], *t;
					int i, ilen, sstart = 1023; /*maximum 1023 size of TEXT datatype*/
					stats[sstart] = '\0';
					Init_line_list(&l);
					Split(&l,image,Line_ends, 0,0,0,0,0,0);

					for (i = l.count-1; i >=0; i--) {
						if ( (t = strstr(l.list[i], " ## ")) ) *t = '\0';
						ilen = safestrlen(l.list[i]);
						if (sstart < ilen + 1 /*\n character*/) break;
						if ((stats[sstart] != '\0') && (stats[sstart] != '\n')) { sstart--; stats[sstart] = '\n'; }
						sstart -= ilen;
						memcpy(&stats[sstart], l.list[i], ilen);
						/*DEBUGF(DNW4)("stats: %s start/ilen: %d/%d l.list: %s\n", &stats[sstart], sstart, ilen, l.list[i]);*/
					}
					Ipp_set_attr(&response.attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_DETAILED_STATUS,
						IPPDT_TEXT_WITHOUT_LANG, 0, &stats[sstart], safestrlen(&stats[sstart]));

					Free_line_list(&l);
					if (image) free(image);
				}
				/*the response consists of attributes for maximum 1 job - clients often rely on it*/
				break;
			}
			if (limit && (limit == group)) goto joblimit;

		} /* count<Sort_order.count*/
	}

	/*if the job-id is not found then send a note*/
	if ((ipp_request->op_id_status == IPPOP_GET_JOB_ATTRIBUTES) && !group &&
	    ((response.op_id_status == SUCCESSFUL_OK) ||
	     (response.op_id_status == SUCCESSFUL_OK_IGNORED_SUBSTITUED))
	   )
		response.op_id_status = CLIENT_ERROR_NOT_FOUND;

joblimit:

 ipperr:

	Free_job(&job);
	Free_line_list(&requested_attributes);
	Free_line_list(&pc_entry);
	Free_line_list(&pc_alias);
	Free_line_list(&printers);
	Ipp_send_response(conn, http_headers, body_rest_len, &response, auth_info, 0, NULL);
	Ipp_free_operation(&response);

	return 0;
}

#define	CHR_REQ		"0"		/*request for change*/
#define	CHR_RO		"RO"		/*request for read-only in this implementation*/
#define	CHR_INVAL	"INVALID"	/*attribute request invalid*/

#define	CHHOLD_NO_CHANGE		0
#define	CHHOLD_NO_HOLD			1
#define	CHHOLD_INDEFINITE		2
#define	CHHOLD_RESTART_NO_HOLD		3
#define	CHHOLD_RESTART_INDEFINITE	4

static int Ipp_op_set_job_attributes(struct line_list *http_request, struct line_list *http_headers, const struct http_conn *conn,
             ssize_t *body_rest_len, struct ipp_operation *ipp_request, struct line_list *auth_info)
{

	struct ipp_operation response;
	char *printer = NULL;
	char *ppath = NULL;
	struct ipp_attr *a;
	char error[SMALLBUFFER];
	int errlen = sizeof(error);
	int printable, move, held, err, done, count, jobnum;
	struct job job;
	int jobid = 0, jobstate, jticket_fd = -1;
	char *username, *jreason;       /*job username, job state reason*/
	int job_changed;                /*flag whether job update is needed*/
	struct line_list l;             /*temporary line list*/
	struct line_list change_attributes;
	char *s;                        /*temporary pointer*/
	int server_pid;
	int ffd;
	char line[LINEBUFFER];
	int change_hold;		/*value for new hold-state*/
	char change_priority[2];	/*value for new priority*/
	int i;				/*generic iteration index*/
	char *move_printer = NULL;     	/*target printer for cups-move-job operation*/
	char *move_ppath = NULL;


	Init_job(&job);
	Ipp_init_operation(&response);
	response.version = ipp_request->version;
	response.request_id = ipp_request->request_id;
	Init_line_list(&l);
	Init_line_list(&change_attributes);

	/*set response mandatory attributes*/
	Ipp_set_attr(&response.attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_CHARSET, IPPDT_CHARSET, 0,
		        IPPAV_UTF_8, safestrlen(IPPAV_UTF_8));
	Ipp_set_attr(&response.attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_NAT_LANG, IPPDT_NAT_LANG, 0,
		        IPPAV_EN_US, safestrlen(IPPAV_EN_US));
	/*validate request - charset and language*/
	response.op_id_status = Ipa_validate_charset(NULL, ipp_request->attributes);
	if (response.op_id_status != SUCCESSFUL_OK) goto procend;
	response.op_id_status = Ipa_validate_language(NULL, ipp_request->attributes);
	if (response.op_id_status != SUCCESSFUL_OK) goto procend;

	printer = Find_str_value(auth_info, KWA_PRINTER);
	ppath = Find_str_value(auth_info, KWA_PPATH);
	if (!printer || !ppath) {
		response.op_id_status = CLIENT_ERROR_NOT_FOUND;
		goto procend;
	}
	DEBUGF(DNW2)("Ipp_op_set_job_attributes: printer: %s path %s", printer, ppath);
	if (Setup_printer(printer, error, errlen, 0))	{
		response.op_id_status = SERVER_ERROR_INTERNAL_ERROR;
		goto procend;
	}

	/*job-id required for all operations except CUPS_MOVE_JOB, which have job identification optional */
	if ((ipp_request->op_id_status != IPPOP_CUPS_MOVE_JOB) ||
	    Ipp_get_attr(ipp_request->attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_JOB_URI, -1) ||
	    Ipp_get_attr(ipp_request->attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_JOB_ID, -1)) {
		response.op_id_status = Ipa_get_job_id(&jobid, ipp_request->attributes);
		DEBUGF(DNW2)("Ipp_op_set_job_attributes: requested job-id: %d", jobid);
		if (response.op_id_status != SUCCESSFUL_OK) goto procend;
	}

	/*CUPS-Move-Job requires job-printer-uri*/
	if (ipp_request->op_id_status == IPPOP_CUPS_MOVE_JOB) {
		response.op_id_status = Ipa_get_printer(&move_printer, &move_ppath, ipp_request->attributes, AUTHS_DEST_PRINTER);
		if (response.op_id_status != SUCCESSFUL_OK) goto procend;
	}


	Ipa_prase_debug(DCTRLMASK);

	Ipa_set_perm(&Perm_check, 'C', auth_info);

	response.op_id_status = SUCCESSFUL_OK;
	/*requested & unsupported attributes*/
	for (a = ipp_request->attributes; a; a = a->next) {
		if ((a->group == OPERATION_ATTRIBUTES_GRP) &&
		    (!safestrcmp(a->name, IPPAN_CHARSET) ||
		     !safestrcmp(a->name, IPPAN_NAT_LANG) ||
		     !safestrcmp(a->name, IPPAN_RQ_USRNAME) ||
		     !safestrcmp(a->name, IPPAN_PRINTER_URI) ||
		     !safestrcmp(a->name, IPPAN_JOB_URI) ||
		     !safestrcmp(a->name, IPPAN_JOB_ID) ||
		     (((ipp_request->op_id_status == IPPOP_HOLD_JOB) || (ipp_request->op_id_status == IPPOP_RESTART_JOB))
		       && !safestrcmp(a->name, IPPAN_JOB_HOLD_UNTIL))
		    )
		   ) {
			continue;
		} else if (
		   (a->group == JOB_ATTRIBUTES_GRP) && (ipp_request->op_id_status == IPPOP_CHANGE_JOB_ATTRIBUTES) &&
		   (!safestrcmp(a->name, IPPAN_JOB_URI) 	|| !safestrcmp(a->name, IPPAN_JOB_ID) ||
		    !safestrcmp(a->name, IPPAN_JOB_PRINTER_URI) || !safestrcmp(a->name, IPPAN_JOB_NAME) ||
		    !safestrcmp(a->name, IPPAN_ORIG_USERNAME) 	|| !safestrcmp(a->name, IPPAN_JOB_STATE) ||
		    !safestrcmp(a->name, IPPAN_JOB_SREASONS) 	|| !safestrcmp(a->name, IPPAN_TIME_AT_CREATION) ||
		    !safestrcmp(a->name, IPPAN_TIME_AT_PROCESSING) || !safestrcmp(a->name, IPPAN_TIME_AT_COMPLETED) ||
		    !safestrcmp(a->name, IPPAN_JOB_PRINTER_UP_TIME) || !safestrcmp(a->name, IPPAN_JOB_KOCTETS) ||
		    !safestrcmp(a->name, IPPAN_JOB_KOCTETS_PROCESSED) || !safestrcmp(a->name, IPPAN_COPIES) ||
		    !safestrcmp(a->name, IPPAN_MULT_DOC_HNDL)
		   )      ) {
			Set_casekey_str_value(&change_attributes, a->name, CHR_RO); /*read-only attribute*/
			response.op_id_status = CLIENT_ERROR_ATTRIBUTES_NOT_SETTABLE;
		} else if ( /*no reason to set attributes other than can be set thru lpd - lpc*/
		   (a->group == JOB_ATTRIBUTES_GRP) && (ipp_request->op_id_status == IPPOP_CHANGE_JOB_ATTRIBUTES) &&
		   (!safestrcmp(a->name, IPPAN_JOB_PRI) 	|| !safestrcmp(a->name, IPPAN_JOB_HOLD_UNTIL)
		   )      ) {
			Set_casekey_str_value(&change_attributes, a->name, CHR_REQ); /*settable*/
		} else if (
		   (a->group == JOB_ATTRIBUTES_GRP) && (ipp_request->op_id_status == IPPOP_CUPS_MOVE_JOB) &&
		   !safestrcmp(a->name, IPPAN_JOB_PRINTER_URI)
		          ) {
			continue;
		}  else {
			Ipp_set_attr(&response.attributes, UNSUPPORTED_ATTRIBUTES_GRP, 0, a->name, IPPDT_UNSUPPORTED, 0, NULL, 0);
			if (response.op_id_status == SUCCESSFUL_OK)
				response.op_id_status = SUCCESSFUL_OK_IGNORED_SUBSTITUED;
		}
	}
	/* search the queue, process request
	 * */
	Free_line_list(&Spool_control);
	Get_spool_control(Queue_control_file_DYN, &Spool_control);
	/*if jobid = 0, we have all-jobs operation (=printer operation) */
	Scan_queue_proc(&Spool_control, &Sort_order, &printable, &held, &move, 0, &err, &done, 0, 0, jobid ? order_filter_job_number : NULL, &jobid);
	/*remove done jobs - depends on Sort_order global variable*/
	if (Remove_done_jobs()) {
		Scan_queue_proc(&Spool_control, &Sort_order, &printable, &held, &move, 0, &err, &done, 0, 0, jobid ? order_filter_job_number : NULL, &jobid);
	}

	for (count = 0; count < Sort_order.count; count++) {
		/*get job info*/
		Free_job(&job);
		if (jticket_fd > 0) close(jticket_fd); jticket_fd = -1;
		Get_job_ticket_file(&jticket_fd, &job, Sort_order.list[count]);
		if (!job.info.count) {
			/*got lpd_status.c*/
			continue;
		}
		/* filtered by Scan_queue_proc & order_filter_job_number
		jobnum = Find_decimal_value(&job.info, NUMBER);
		/ *jobid filter* /
		if (jobid != jobnum) continue;
		*/
		DEBUGFC(DCTRL4)Dump_job("Ipa_set_job_attributes - info", &job);
		/*DEBUGFC(DNW4)Dump_job("Ipa_set_job_attributes - info", &job);*/
		Ipa_get_job_state(&jobstate, &jreason, &job, &Spool_control);

		/*test permission & validate settable values*/
		change_hold = CHHOLD_NO_CHANGE;
		memset(change_priority, 0, sizeof(change_priority));
		Perm_check.user = Find_str_value(&job.info, LOGNAME);
		Perm_check.host = 0;
		if ((s = Find_str_value(&job.info, FROMHOST)) &&
		    Find_fqdn(&PermHost_IP, s)) {
			Perm_check.host = &PermHost_IP;
		}
		switch (ipp_request->op_id_status) {
			case IPPOP_HOLD_JOB:
				a = Ipp_get_attr(ipp_request->attributes, JOB_ATTRIBUTES_GRP, 0, IPPAN_JOB_HOLD_UNTIL, -1);
				if (a && !safestrncmp(a->value, IPPAV_NO_HOLD, a->value_len)) { /*in reality, we do RELEASE*/
					Perm_check.lpc = ILPC_RELEASE;
					change_hold = CHHOLD_NO_HOLD;
				} else {
					if (a && safestrncmp(a->value, IPPAV_INDEFINITE, a->value_len) && (response.op_id_status == SUCCESSFUL_OK))
						response.op_id_status = SUCCESSFUL_OK_IGNORED_SUBSTITUED;
					Perm_check.lpc = ILPC_HOLD;
					change_hold = CHHOLD_INDEFINITE;
				}
				if (Perms_check(&Perm_line_list, &Perm_check, &job, 1) != P_ACCEPT) {
					response.op_id_status = CLIENT_ERROR_NOT_AUTHENTICATED;
					goto procend;
				}
				break;
			case IPPOP_RESTART_JOB:
				a = Ipp_get_attr(ipp_request->attributes, JOB_ATTRIBUTES_GRP, 0, IPPAN_JOB_HOLD_UNTIL, -1);
				if (a && !safestrncmp(a->value, IPPAV_INDEFINITE, a->value_len)) { /*in reality, we do RELEASE*/
					Perm_check.lpc = ILPC_HOLD;
					change_hold = CHHOLD_RESTART_INDEFINITE;
				} else {
					if (a && safestrncmp(a->value, IPPAV_NO_HOLD, a->value_len) && (response.op_id_status == SUCCESSFUL_OK))
						response.op_id_status = SUCCESSFUL_OK_IGNORED_SUBSTITUED;
					Perm_check.lpc = ILPC_RELEASE;
					change_hold = CHHOLD_RESTART_NO_HOLD;
				}
				if (Perms_check(&Perm_line_list, &Perm_check, &job, 1) != P_ACCEPT) {
					response.op_id_status = CLIENT_ERROR_NOT_AUTHENTICATED;
					goto procend;
				}
				break;
			case IPPOP_RELEASE_JOB:
				Perm_check.lpc = ILPC_RELEASE;
				if (Perms_check(&Perm_line_list, &Perm_check, &job, 1) != P_ACCEPT) {
					response.op_id_status = CLIENT_ERROR_NOT_AUTHENTICATED;
					goto procend;
				}
				change_hold = CHHOLD_NO_HOLD;
				break;
			case IPPOP_CHANGE_JOB_ATTRIBUTES:
				if (Find_casekey_str_value(&change_attributes, IPPAN_JOB_PRI, Hash_value_sep)) {
					Perm_check.lpc = ILPC_CHATTR_JOB_PRI;
					if (Perms_check(&Perm_line_list, &Perm_check, &job, 1) != P_ACCEPT) {
						response.op_id_status = CLIENT_ERROR_NOT_AUTHENTICATED;
						goto procend;
					}
					if (Ignore_requested_user_priority_DYN) {
						Set_casekey_str_value(&change_attributes, IPPAN_JOB_PRI, CHR_RO);
						response.op_id_status = CLIENT_ERROR_ATTRIBUTES_NOT_SETTABLE;
					} else {
						a = Ipp_get_attr(ipp_request->attributes, JOB_ATTRIBUTES_GRP, 0, IPPAN_JOB_PRI, -1);
						if (!a || (a->type == IPPDT_DELETE)) {
							change_priority[0] = Default_priority_DYN ? Default_priority_DYN[0] : 'A';
						} else if (a->value_len != 4)  {
							Set_casekey_str_value(&change_attributes, IPPAN_JOB_PRI, CHR_INVAL);
							response.op_id_status = CLIENT_ERROR_ATTRS_OR_VALS_NOT_SUPPORTED;
						}
						else change_priority[0] = Ipa_char_priority(ntoh32(a->value), Reverse_priority_order_DYN);
					}
				}
				if (Find_casekey_str_value(&change_attributes, IPPAN_JOB_HOLD_UNTIL, Hash_value_sep)) {
					a = Ipp_get_attr(ipp_request->attributes, JOB_ATTRIBUTES_GRP, -1, IPPAN_JOB_HOLD_UNTIL, -1);
					if (a && (((a->type == IPPDT_DELETE) && !Auto_hold_DYN) || !safestrncmp(a->value, IPPAV_NO_HOLD, a->value_len))) {
						Perm_check.lpc = ILPC_RELEASE;
						if (Perms_check(&Perm_line_list, &Perm_check, &job, 1) != P_ACCEPT) {
							response.op_id_status = CLIENT_ERROR_NOT_AUTHENTICATED;
							goto procend;
						}
						change_hold = CHHOLD_NO_HOLD;
					} else if (a && (((a->type == IPPDT_DELETE) && Auto_hold_DYN) || !safestrncmp(a->value, IPPAV_INDEFINITE, a->value_len))) {
						Perm_check.lpc = ILPC_HOLD;
						if (Perms_check(&Perm_line_list, &Perm_check, &job, 1) != P_ACCEPT) {
							response.op_id_status = CLIENT_ERROR_NOT_AUTHENTICATED;
							goto procend;
						}
						change_hold = CHHOLD_INDEFINITE;
					} else {
						Set_casekey_str_value(&change_attributes, IPPAN_JOB_HOLD_UNTIL, CHR_INVAL);
						response.op_id_status = CLIENT_ERROR_ATTRS_OR_VALS_NOT_SUPPORTED;
					}
				}
				break;
			case IPPOP_CUPS_MOVE_JOB:
				Perm_check.lpc = ILPC_MOVE;
				if (Perms_check(&Perm_line_list, &Perm_check, &job, 1) != P_ACCEPT) {
					response.op_id_status = CLIENT_ERROR_NOT_AUTHENTICATED;
					goto procend;
				}
				/*lpc utility does not check dest permission, but here it would be good*/
				const char *save_printer = Perm_check.printer;
				const char *save_ppath = Perm_check.ppath;
				const char *save_lpc = Perm_check.lpc;
				char save_service = Perm_check.service;
				Perm_check.printer = move_printer;
				Perm_check.ppath = move_ppath;
				Perm_check.lpc = NULL;
				Perm_check.service = 'P';
				if (Perms_check(&Perm_line_list, &Perm_check, &job, 1) != P_ACCEPT) {
					response.op_id_status = CLIENT_ERROR_NOT_AUTHENTICATED;
					goto procend;
				}
				Perm_check.printer = save_printer;
				Perm_check.ppath = save_ppath;
				Perm_check.lpc = save_lpc;
				Perm_check.service = save_service;
				break;
		}
		/*state filter after authentication: allow change only for not-completed jobs only*/
		switch (ipp_request->op_id_status) {
			case IPPOP_RESTART_JOB:
				if (jobstate != IPPAV_JRS_COMPLETED) {
					response.op_id_status = CLIENT_ERROR_NOT_POSSIBLE;
					goto procend;
				}
				break;
			case IPPOP_CUPS_MOVE_JOB:
				if ((jobstate == IPPAV_JRS_PROCESSING) || (jobstate == IPPAV_JRS_COMPLETED)) {
					response.op_id_status = CLIENT_ERROR_NOT_POSSIBLE;
					goto procend;
				}
				break;
			default:
				if ((jobstate == IPPAV_JRS_PROCESSING) || (jobstate == IPPAV_JRS_COMPLETED) ||
				    (jobstate == IPPAV_JRS_ABORTED) || (jobstate == IPPAV_JRS_CANCELED)	) {
					response.op_id_status = CLIENT_ERROR_NOT_POSSIBLE;
					goto procend;
				}
				break;
		}
		/*send error if attribute unsupported: not valid/settable */
		if ((ipp_request->op_id_status == IPPOP_CHANGE_JOB_ATTRIBUTES) &&
		    (response.op_id_status != SUCCESSFUL_OK) && (response.op_id_status != SUCCESSFUL_OK_IGNORED_SUBSTITUED)) {
			for (i = change_attributes.count - 1; i >= 0; i--) {
				char *sval, sep1, *key;
				sval = safestrpbrk(change_attributes.list[i], Hash_value_sep);
				if (!sval) continue;
				sep1 = *sval; *sval = '\0';
				key = safestrdup(change_attributes.list[i], __FILE__, __LINE__);
				*sval = sep1;
				sval = Find_casekey_str_value(&change_attributes, key, Hash_value_sep);
				DEBUGF(DNW4)("Ipp_op_set_job_attributes: key %s sval %s", key, sval ? sval : "NULL" );
				if (!safestrcmp(sval, CHR_RO)) {  /*read-only attribute*/
					Ipp_set_attr(&response.attributes, UNSUPPORTED_ATTRIBUTES_GRP, 0, key, IPPDT_NOT_SETTABLE, 0, NULL, 0);
				} else if (!safestrcmp(sval, CHR_INVAL)) { /*settable attribute, but invalid value requested*/
					a = Ipp_get_attr(ipp_request->attributes, JOB_ATTRIBUTES_GRP, 0, key, -1);
					/*kjobviewer sends more mixed request group adn we cannot deal with it*/
					DEBUGF(DNW4)("Ipp_op_set_job_attributes: key %s attr %s sval %s", key, a ? a->name : "NULL", sval ? sval : "NULL" );
					if (a) Ipp_set_attr(&response.attributes, UNSUPPORTED_ATTRIBUTES_GRP, 0, key, a->type, 0, a->value, a->value_len);
				}
				Set_casekey_str_value(&change_attributes, key, NULL);	/*delete to speedup*/
				if (key) free(key);
			}
			goto procend;
		}

		/*finally, we can change job values*/
		job_changed = 0;
		switch (change_hold) {
			case CHHOLD_RESTART_NO_HOLD:	Set_flag_value(&job.info, DONE_TIME, 0);
							Set_flag_value(&job.info, REMOVE_TIME, 0);
							job_changed = 1;
			case CHHOLD_NO_HOLD:		if (Find_flag_value(&job.info, HOLD_TIME)) {
								Set_flag_value(&job.info, HOLD_TIME, 0);
								Set_flag_value(&job.info, ATTEMPT, 0);
								job_changed = 1;
							}
							break;
			case CHHOLD_RESTART_INDEFINITE:	Set_flag_value(&job.info, DONE_TIME, 0);
							Set_flag_value(&job.info, REMOVE_TIME, 0);
							job_changed = 1;
			case CHHOLD_INDEFINITE:		if (!Find_flag_value(&job.info, HOLD_TIME)) {
								Set_flag_value(&job.info, HOLD_TIME, time((void *)0));
								job_changed = 1;
							}
							break;
			default: ; /*no request change for hold*/
		}
		if (change_priority[0] != 0 ) {
			Set_str_value(&job.info, CLASS, change_priority);
			Set_str_value(&job.info, PRIORITY, change_priority);
			job_changed = 1;
		}
		if (move_printer) {
			Set_str_value(&job.info, MOVE, move_printer);
			Set_flag_value(&job.info, HOLD_TIME, 0);
			Set_flag_value(&job.info, PRIORITY_TIME, 0);
			Set_flag_value(&job.info, DONE_TIME, 0);
			Set_flag_value(&job.info, REMOVE_TIME, 0);
			/*change job authentication information  to current identity in accordance with Perm_check of destination printer */
			Set_str_value(&job.info, AUTHUSER, Find_str_value(auth_info, KWA_USER));
			Set_str_value(&job.info, AUTHFROM, Find_str_value(auth_info, KWA_AUTHFROM));
			Set_str_value(&job.info, AUTHTYPE, Find_str_value(auth_info, KWA_AUTHTYPE));
			Set_str_value(&job.info, AUTHCA, Find_str_value(auth_info, KWA_AUTHCA));
			job_changed = 1;
		}
		Set_str_value(&job.info, ERROR, 0);
		Set_flag_value(&job.info, ERROR_TIME, 0);
		if (job_changed) {
			Perm_check_to_list(&l, &Perm_check);
			if (Set_job_ticket_file(&job, &l, jticket_fd)) {
				response.op_id_status = SERVER_ERROR_INTERNAL_ERROR;
				DEBUGF(DCTRL4)("Ipp_op_set_job_attributes: Set_job_ticket file error");
				goto procend;
			}
			if (Server_queue_name_DYN) Set_flag_value(&Spool_control, CHANGE, 1);
			Set_spool_control(&l, Queue_control_file_DYN, &Spool_control);
			server_pid = 0;
			struct stat statb;
			if ((ffd = Checkread(Queue_lock_file_DYN, &statb)) >= 0) {
				server_pid = Read_pid(ffd);
				close(ffd);
				if (server_pid && !kill(server_pid, SIGUSR1)) {
					DEBUGF(DCTRL4)("Ipp_op_set_job_attributes: kill server pid %d SIGUSR1", server_pid);
				}
			}
			if (Server_queue_name_DYN) {
				if (Setup_printer(Server_queue_name_DYN, error, errlen, 0)) {
					response.op_id_status = SERVER_ERROR_INTERNAL_ERROR;
					goto procend;
				}
				server_pid = 0;
				if ((ffd = Checkread(Queue_lock_file_DYN, &statb)) >= 0) {
					server_pid = Read_pid(ffd);
					close(ffd);
					if (server_pid && !kill(server_pid, SIGUSR1)) {
						DEBUGF(DCTRL4)("Ipp_op_set_job_attributes: kill server pid %d SIGUSR1", server_pid);
					}
				}

			}
			if (!server_pid) {
				plp_snprintf(line, sizeof(line), "%s\n", Printer_DYN);
				DEBUGF(DCTRL4)("Ipp_op_set_job_attributes: Lpd_request fd %d, data %s", Lpd_request, s);
				if (Write_fd_str(Lpd_request, line) < 0) {
					logerr_die(LOG_ERR, _("Ipp_op_set_job_attributes: write to fd %d failed"), Lpd_request);
					response.op_id_status = SERVER_ERROR_INTERNAL_ERROR;
					goto procend;
				}
			}

		}
		if (jobid) goto procend;

	} /* count<Sort_order.count*/

	if (jobid) response.op_id_status = CLIENT_ERROR_NOT_FOUND; /*jobid contained in the request not found in the queue*/

procend:

	if (jticket_fd > 0) close(jticket_fd);
	Free_job(&job);
	Free_line_list(&l);
	Free_line_list(&change_attributes);
	if (move_printer) free(move_printer);
	if (move_ppath) free(move_ppath);
	Ipp_send_response(conn, http_headers, body_rest_len, &response, auth_info, 0, NULL);
	Ipp_free_operation(&response);

	return 0;
}

static int Ipp_create_subscriptions(struct ipp_operation *ipp_response, struct ipp_operation *ipp_request,
		const char *printer, struct job *sjob, int validate_only) {

	/*warning - job subscription creation untested*/
	int group;
	int supp, srs, events, ecount, enone, jobid, subscription_id, lease;
	struct ipp_attr *a, *b;

	if (sjob) {
		jobid = Ipa_jobid2ipp(printer, Find_decimal_value(&(sjob->info), NUMBER));
		if (!jobid) return 1;
	} else {
		jobid = 0;
	}

	/*cycle subscription groups*/
	for (group = 0; (a = Ipp_get_attr(ipp_request->attributes, SUBSCRIPTION_ATTRIBUTES_GRP, group, NULL, -1)); group++) {
		srs = SUCCESSFUL_OK;
		DEBUGF(DNW4)("Ipp_create_subscriptions: subscription group %d", group);
		/*mandatory attributes check*/
		if ((b = Ipp_get_attr(a, SUBSCRIPTION_ATTRIBUTES_GRP, group, IPPAN_NTF_PULL_METHOD, -1)) &&
		    (!safestrcmp(b->name, IPPAN_NTF_PULL_METHOD) && safestrncmp(b->value, IPPAV_IPPGET, b->value_len))) {
			srs = CLIENT_ERROR_ATTRS_OR_VALS_NOT_SUPPORTED;	/*invalid method*/
			goto sserr;
		}
		/*as we have default event "none", the "notify-events" attribute is mandatory */
		if (!Ipp_get_attr(a, SUBSCRIPTION_ATTRIBUTES_GRP, group, IPPAN_NTF_EVENTS, -1)) {
			srs = CLIENT_ERROR_ATTRS_OR_VALS_NOT_SUPPORTED;
			goto sserr;
		}
		ecount = 0; enone = 0;
		events = jobid ? EVENT_JOB : EVENT_PRINTER;
		subscription_id = 0;
		if (jobid) {
			if (subscribe_high_jobid(jobid)) {
				srs = CLIENT_ERROR_TOO_MANY_SUBSCRIPTIONS;
				goto sserr;
			} else subscription_id |= jobid;
		}
		lease = 0;
		for (b = a; b; b = Ipp_get_attr(b->next, SUBSCRIPTION_ATTRIBUTES_GRP, group, NULL, -1)) {
			supp = 0;
			/*DEBUGF(DNW4)("Ipp_create_subscriptions: attribute %s", b->name);*/
			/*process template attributes list*/
			if (!safestrcmp(b->name, IPPAN_NTF_PULL_METHOD)) supp = 1;
			else if (!safestrcmp(b->name, IPPAN_NTF_EVENTS)) {
				supp = 1;
				if (!safestrncmp(b->value, IPPAV_EVENT_NONE, b->value_len)) enone = 1;
				else if (!safestrncmp(b->value, IPPAV_EVENT_PR_STATECHANGE, b->value_len) && !jobid)  events |= EVENT_PR_STATECHANGE;
				else if (!safestrncmp(b->value, IPPAV_EVENT_PR_STOPPED, b->value_len) && !jobid)  events |= EVENT_PR_STOPPED;
				else if (!safestrncmp(b->value, IPPAV_EVENT_JOB_STATECHANGE, b->value_len))  events |= EVENT_JOB_STATECHANGE;
				else if (!safestrncmp(b->value, IPPAV_EVENT_JOB_CREATED, b->value_len))  events |= EVENT_JOB_CREATED;
				else if (!safestrncmp(b->value, IPPAV_EVENT_JOB_COMPLETED, b->value_len))  events |= EVENT_JOB_COMPLETED;
				else srs = SUCCESSFUL_OK_IGNORED_SUBSTITUED; /*unsupported event*/
			} else if (!safestrcmp(b->name, IPPAN_NTF_USER_DATA)) {
				supp = 1;
				srs = CLIENT_ERROR_TOO_MANY_SUBSCRIPTIONS;
				goto sserr;
				/*
				if (!safestrcmp(printer, IPPC_APLLPRINTERS))  we do not know the printer, go away todo ... -> NOT_CREATED
				events |= EVENT_DATA;
				struct line_list Spool_control;
				Init_line_list(&Spool_control);
				if (!jobid) { if !validate ....
				Get_spool_control(Queue_control_file_DYN, &Spool_control);
				Set_flag_value(&Spool_control, "event_data%n" , data+subscribe_id); / *todo* /
				Set_spool_control(0, Queue_control_file_DYN, &Spool_control);
				Free_line_list(&Spool_control);
				}
				lease = something nonzero
				*/
			} else if (!safestrcmp(b->name, IPPAN_NTF_CHARSET)) supp = 1;  /*only utf-8, en-us sypported*/
			  else if (!safestrcmp(b->name, IPPAN_NTF_NAT_LANG)) supp = 1;
			  else if (!safestrcmp(b->name, IPPAN_NTF_LEASE_DURATION) && !jobid) {
				supp = 1;
			}
			/*unsupported attribute*/
			if (!supp) {
				Ipp_set_attr(&(ipp_response->attributes), SUBSCRIPTION_ATTRIBUTES_GRP, group, b->name,
						IPPDT_UNSUPPORTED, 0, NULL, 0);
				if (srs != SUCCESSFUL_OK) srs = SUCCESSFUL_OK_IGNORED_SUBSTITUED;

			}
		}
		/*event check*/
		DEBUGF(DNW4)("Ipp_create_subscriptions: events %d, count %d", events, ecount);
		if (ecount > NTF_MAX_EVENTS) {
			srs = SUCCESSFUL_OK_TOO_MANY_EVENTS;
		}
		if (enone) {
			if (ecount == 1) {  /*sole "none" event*/
				srs = CLIENT_ERROR_ATTRS_OR_VALS_NOT_SUPPORTED;	/*the status code not defined in rfc*/
				goto sserr;
			} else {
				if (events) srs = SUCCESSFUL_OK_IGNORED_SUBSTITUED;
				else {
					srs = CLIENT_ERROR_ATTRS_OR_VALS_NOT_SUPPORTED;	/*any usable event not found*/
					goto sserr;
				}
			}
		}
		/*mandatory response attrs todo*/
		/*notify-subscription-id*/
                /*notify-lease-duration*/
		subscription_id |= events;
		if (!subscription_id) {
			/*probably bad events*/
			srs = CLIENT_ERROR_ATTRS_OR_VALS_NOT_SUPPORTED;
			goto sserr;
		}
		char buf[4];
		hton32(buf, subscription_id);
		Ipp_set_attr(&(ipp_response->attributes), SUBSCRIPTION_ATTRIBUTES_GRP, group, IPPAN_NTF_SUBSCRIPTION_ID,
				IPPDT_INTEGER, 0, buf, 4);
		hton32(buf, lease);
		Ipp_set_attr(&(ipp_response->attributes), SUBSCRIPTION_ATTRIBUTES_GRP, group, IPPAN_NTF_LEASE_DURATION,
				IPPDT_INTEGER, 0, buf, 4);

  sserr:
		/*set subscription group result here*/
		if (srs != SUCCESSFUL_OK) {
			char buf[4];
			hton32(buf, srs);
			Ipp_set_attr(&(ipp_response->attributes), SUBSCRIPTION_ATTRIBUTES_GRP, group, IPPAN_NTF_STATUS_CODE,
						IPPDT_ENUM, 0, buf, 4);
		}
	}

	return 0;
}

static int Ipp_op_create_printer_subscriptions(struct line_list *http_request, struct line_list *http_headers, const struct http_conn *conn,
              ssize_t *body_rest_len, struct ipp_operation *ipp_request, struct line_list *auth_info)
{

	struct ipp_operation response;
	char *printer = NULL;
	char *ppath = NULL;
	char error[SMALLBUFFER];
	int errlen = sizeof(error);
	struct line_list printers;
	int prcount;
	struct ipp_attr *a;

	Ipp_init_operation(&response);
	response.version = ipp_request->version;
	response.request_id = ipp_request->request_id;
	/*mandatory attributes*/
	Ipp_set_attr(&response.attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_CHARSET, IPPDT_CHARSET, 0,
		        IPPAV_UTF_8, safestrlen(IPPAV_UTF_8));
	Ipp_set_attr(&response.attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_NAT_LANG, IPPDT_NAT_LANG, 0,
		        IPPAV_EN_US, safestrlen(IPPAV_EN_US));

	Init_line_list(&printers);

	/*validate charset and language*/
	response.op_id_status = Ipa_validate_charset(NULL, ipp_request->attributes);
	if (response.op_id_status != SUCCESSFUL_OK) goto ipperr;
	response.op_id_status = Ipa_validate_language(NULL, ipp_request->attributes);
	if (response.op_id_status != SUCCESSFUL_OK) goto ipperr;

	printer = Find_str_value(auth_info, KWA_PRINTER);
	ppath = Find_str_value(auth_info, KWA_PPATH);
	if (!printer || !ppath) {
		response.op_id_status = CLIENT_ERROR_NOT_FOUND;
		goto ipperr;
	}

	DEBUGF(DNW2)("Ipp_op_create_printer_subscriptions: printer: %s path %s", printer, ppath);
	response.op_id_status = SUCCESSFUL_OK;

	/*unsupported attributes*/
	for (a = ipp_request->attributes; a && (a->group == OPERATION_ATTRIBUTES_GRP); a = a->next) {
		if ((!safestrcmp(a->name, IPPAN_CHARSET)) ||
		    (!safestrcmp(a->name, IPPAN_NAT_LANG)) ||
		    (!safestrcmp(a->name, IPPAN_PRINTER_URI)) ||
		    (!safestrcmp(a->name, IPPAN_RQ_USRNAME))) continue;
		Ipp_set_attr(&response.attributes, UNSUPPORTED_ATTRIBUTES_GRP, 0, a->name, IPPDT_UNSUPPORTED, 0, NULL, 0);
		response.op_id_status = SUCCESSFUL_OK_IGNORED_SUBSTITUED;
	}

	/*make printer list*/
	if (!safestrcmp(printer, IPPC_ALLPRINTERS)) {
		Get_all_printcap_entries();
		Merge_line_list(&printers, &All_line_list, "", 0, 1);
	} else {
		Add_line_list(&printers, printer, "", 1, 1);
	}

	for (prcount = 0; prcount < printers.count; prcount++) {
		printer = printers.list[prcount];
		if (Setup_printer(printer, error, errlen, 0))	{
			response.op_id_status = SERVER_ERROR_INTERNAL_ERROR;
			goto ipperr;
		}
		Ipa_set_perm(&Perm_check, 'S', auth_info);
		if (Perms_check(&Perm_line_list, &Perm_check, 0, 0) == P_REJECT)
		{
			response.op_id_status = CLIENT_ERROR_NOT_AUTHENTICATED;
			goto ipperr;
		}
	}

	DEBUGF(DNW2)("Ipp_op_create_printer_subscriptions: auths ok");

	Ipp_create_subscriptions(&response, ipp_request, printer, NULL, 0);

 ipperr:
	Ipp_send_response(conn, http_headers, body_rest_len, &response, auth_info, 0, NULL);

	Free_line_list(&printers);
	Ipp_free_operation(&response);

	return 0;
}

/*UNTESTED*/
static int Ipp_op_get_subscription_attributes(struct line_list *http_request, struct line_list *http_headers, const struct http_conn *conn,
              ssize_t *body_rest_len, struct ipp_operation *ipp_request, struct line_list *auth_info)
{

	struct ipp_operation response;
	char *printer = NULL;
	char *ppath = NULL;
	struct ipp_attr *a;
	char error[SMALLBUFFER];
	int errlen = sizeof(error);
	struct line_list printers, requested_attributes;
	int prcount, group, jobid, limit, i, mysubscriptions;
	struct int_data {
		int id;
		struct int_data *next;
		char *owner;
	} *subscriptions, *sx;

	Ipp_init_operation(&response);
	response.version = ipp_request->version;
	response.request_id = ipp_request->request_id;
	/*mandatory attributes*/
	Ipp_set_attr(&response.attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_CHARSET, IPPDT_CHARSET, 0,
		        IPPAV_UTF_8, safestrlen(IPPAV_UTF_8));
	Ipp_set_attr(&response.attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_NAT_LANG, IPPDT_NAT_LANG, 0,
		        IPPAV_EN_US, safestrlen(IPPAV_EN_US));

	Init_line_list(&printers);
	Init_line_list(&requested_attributes);
	subscriptions = NULL;

	/*validate charset and language*/
	response.op_id_status = Ipa_validate_charset(NULL, ipp_request->attributes);
	if (response.op_id_status != SUCCESSFUL_OK) goto ipperr;
	response.op_id_status = Ipa_validate_language(NULL, ipp_request->attributes);
	if (response.op_id_status != SUCCESSFUL_OK) goto ipperr;

	printer = Find_str_value(auth_info, KWA_PRINTER);
	ppath = Find_str_value(auth_info, KWA_PPATH);
	if (!printer || !ppath) {
		response.op_id_status = CLIENT_ERROR_NOT_FOUND;
		goto ipperr;
	}
	DEBUGF(DNW2)("Ipp_op_get_subscription_attributes: printer: %s path %s", printer, ppath);

	/*make printer list*/
	if (!safestrcmp(printer, IPPC_ALLPRINTERS)) {
		Get_all_printcap_entries();
		Merge_line_list(&printers, &All_line_list, "", 0, 1);
	} else {
		Add_line_list(&printers, printer, "", 1, 1);
	}

	for (prcount = 0; prcount < printers.count; prcount++) {
		printer = printers.list[prcount];
		if (Setup_printer(printer, error, errlen, 0))	{
			response.op_id_status = SERVER_ERROR_INTERNAL_ERROR;
			goto ipperr;
		}
		Ipa_set_perm(&Perm_check, 'S', auth_info); /*C?*/
		if (Perms_check(&Perm_line_list, &Perm_check, 0, 0) == P_REJECT)
		{
			response.op_id_status = CLIENT_ERROR_NOT_AUTHENTICATED;
			goto ipperr;
		}
	}

	DEBUGF(DNW2)("Ipp_op_get_subscription_attributes: auths ok");

	response.op_id_status = SUCCESSFUL_OK;

	/*add unsupported group 2*/
	Ipp_set_attr(&response.attributes, UNSUPPORTED_ATTRIBUTES_GRP, 0, NULL, 0, 0, NULL, 0);

	/*find/get the requested attributes list*/
	a = Ipp_get_attr(ipp_request->attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_REQUESTED_ATTRIBUTES, -1);
	do {
		if (!a) {
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_SUBSCRIPTION_ID, RQ_TRUE);
		}
		else if (!safestrncmp(a->value, IPPAV_ALL, a->value_len)) {
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_SUBSCRIPTION_ID, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_SEQUENCE_NUMBER, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_LEASE_EXPIRATION, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_PRINTER_UP_TIME, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_PRINTER_URI, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_JOB_ID, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_SUBSCRIBER_USERN, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_PULL_METHOD_SUPP, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_EVENTS_DEFAULT, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_EVENTS_SUPPORTED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_MAX_EVENTS_SUPP, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_LEASE_DURATION_D, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_LEASE_DURATION_S, RQ_TRUE);
		break;
		}
		else if (!safestrncmp(a->value, IPPAV_SUBSCRIPTION_TEMPLATE, a->value_len)) {
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_PULL_METHOD_SUPP, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_EVENTS_DEFAULT, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_EVENTS_SUPPORTED, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_MAX_EVENTS_SUPP, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_LEASE_DURATION_D, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_LEASE_DURATION_S, RQ_TRUE);
		}
		else if (!safestrncmp(a->value, IPPAV_SUBSCRIPTION_DESCRIPT, a->value_len)) {
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_SUBSCRIPTION_ID, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_SEQUENCE_NUMBER, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_LEASE_EXPIRATION, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_PRINTER_UP_TIME, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_PRINTER_URI, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_JOB_ID, RQ_TRUE);
			Set_casekey_str_value(&requested_attributes, IPPAN_NTF_SUBSCRIBER_USERN, RQ_TRUE);
		}
		else {
			char *buf = malloc_or_die(a->value_len + 1, __FILE__, __LINE__);
			memcpy(buf, a->value, a->value_len); buf[a->value_len] = '\0';
			Set_casekey_str_value(&requested_attributes, buf, RQ_TRUE);
			free(buf);
		}
		if (a) a = Ipp_get_attr(a->next, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_REQUESTED_ATTRIBUTES, -1);
	} while (a);

	limit = 0;
	mysubscriptions = 0;
	jobid = 0;
	if (ipp_request->op_id_status == IPPOP_GET_SUBSCRIPTION_ATTRIBUTES) {
		/*the response consists of attributes for maximum 1 job */
		/*fill subscriptions record by notify-subscription-id*/
		a = Ipp_get_attr(ipp_request->attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_NTF_SUBSCRIPTION_ID, -1);
		if (a && a->value_len == 4) {
			i = ntoh32(a->value);
			/*we support CUPS "all printers" printer-uri, but only for some simple subscription_id which do not rely on particular printer*/
			if (is_job_subscription(i)) {
				Ipa_ipp2jobid(&jobid, NULL, subscription_jobid(i));
			}
			if ((jobid || (i & EVENT_DATA)) && !safestrcmp(printer, IPPC_ALLPRINTERS)) {
				response.op_id_status = CLIENT_ERROR_NOT_FOUND;
				goto ipperr;
			}
			subscriptions = malloc_or_die(sizeof(struct int_data), __FILE__, __LINE__);
			if (subscriptions) {
				subscriptions->next = NULL;
				subscriptions->id = i;
				if (jobid) {
					subscriptions->owner = NULL; /*should be job owner for a job*/
				} else {
					subscriptions->owner = NULL;
				}
			} else {
				response.op_id_status = SERVER_ERROR_INTERNAL_ERROR;
				goto ipperr;
			}
		} else {
			response.op_id_status = CLIENT_ERROR_BAD_REQUEST;
			goto ipperr;
		}
	} else {
		/*limit requested ?*/
		a = Ipp_get_attr(ipp_request->attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_LIMIT, -1);
		if (a && a->value_len == 4) limit = ntoh32(a->value);
		/*my-subscriptions requested ?*/
		a = Ipp_get_attr(ipp_request->attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_MY_SUBSCRIPTIONS, -1);
		mysubscriptions = (a && a->value_len == 1 && ( *(char *)(a->value) & 0xff));
		/*optional job-id*/
		a = Ipp_get_attr(ipp_request->attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_NTF_JOB_ID, -1);
		if (a && a->value_len == 4) {
			Ipa_ipp2jobid(&jobid, NULL, ntoh32(a->value));
		}

		/*create subscription list here todo ....*/
		if (jobid) {
			/*must have existing printer*/
			if (!safestrcmp(printer, IPPC_ALLPRINTERS)) {
				response.op_id_status = CLIENT_ERROR_NOT_FOUND;
				goto ipperr;
			}
			if (subscribe_high_jobid(jobid)) {
				response.op_id_status = CLIENT_ERROR_TOO_MANY_SUBSCRIPTIONS;
				goto ipperr;
			}
			/*add all stateless job events ...*/
			int ejch, ejcr, ejcm;
			for (ejch = 0; ejch == 1; ejch++)
			for (ejcr = 0; ejcr == 1; ejcr++)
			for (ejcm = 0; ejcm == 1; ejcm++) {
				if (!ejch && !ejcr && !ejcm) continue;
				if (subscriptions) {
					if (!sx->next) sx->next = malloc_or_die(sizeof(struct int_data), __FILE__, __LINE__);
					if (sx->next) sx = sx->next;
				} else {
					subscriptions = malloc_or_die(sizeof(struct int_data), __FILE__, __LINE__);
					sx = subscriptions;
				}
				if (sx) {
					sx->next = NULL;
					sx->id = EVENT_JOB | jobid;
					sx->owner = NULL; /*should be job owner*/
					if (ejch) sx->id |= EVENT_JOB_STATECHANGE;
					if (ejcr) sx->id |= EVENT_JOB_CREATED;
					if (ejcm) sx->id |= EVENT_JOB_COMPLETED;
					if (limit) {
						limit-- ;
						if (!limit) goto event_limit;
					}
				}
			}
			/*add job special IDs with EVENT_DATA from job control file todo here ...
			 with respect to my-subscriptions
			 * */
		} else {
			/*add all stateless events ...*/
			int epch, epst, ejch, ejcr, ejcm;
			for (epch = 0; epch == 1; epch++)
			for (epst = 0; epst == 1; epst++)
			for (ejch = 0; ejch == 1; ejch++)
			for (ejcr = 0; ejcr == 1; ejcr++)
			for (ejcm = 0; ejcm == 1; ejcm++) {
				if (!epch && !epst && !ejch && !ejcr && !ejcm) continue;
				if (subscriptions) {
					if (!sx->next) sx->next = malloc_or_die(sizeof(struct int_data), __FILE__, __LINE__);
					sx = sx->next;
				} else {
					subscriptions = malloc_or_die(sizeof(struct int_data), __FILE__, __LINE__);
					sx = subscriptions;
				}
				if (sx) {
					sx->next = NULL;
					sx->id = EVENT_PRINTER;
					sx->owner = NULL;
					if (epch) sx->id |= EVENT_PR_STATECHANGE;
					if (epst) sx->id |= EVENT_PR_STOPPED;
					if (ejch) sx->id |= EVENT_JOB_STATECHANGE;
					if (ejcr) sx->id |= EVENT_JOB_CREATED;
					if (ejcm) sx->id |= EVENT_JOB_COMPLETED;
					if (limit) {
						limit-- ;
						if (!limit) goto event_limit;
					}
				}
			}
			/*add printer special IDs with EVENT_DATA from printer control file todo here ... - only for valid = not "all" printer
			 with respect to my-subscriptions
			 * */
		}
   event_limit:	;
	}

	for (sx = subscriptions, group = 0; sx; sx = sx->next, group++) {
		/*requested attribute values*/
		if (Find_casekey_str_value(&requested_attributes, IPPAN_NTF_SUBSCRIPTION_ID, Hash_value_sep)) {
			char buf[4];
			hton32(buf, sx->id);
			Ipp_set_attr(&response.attributes, SUBSCRIPTION_ATTRIBUTES_GRP, group, IPPAN_NTF_SUBSCRIPTION_ID,
					IPPDT_INTEGER, 0, buf, 4);
			if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_NTF_SUBSCRIPTION_ID, RQ_SUPP);
		}
		if (Find_casekey_str_value(&requested_attributes, IPPAN_NTF_SEQUENCE_NUMBER, Hash_value_sep)) {
			char buf[4];
			time_t t;
			time(&t);
			hton32(buf, t);
			Ipp_set_attr(&response.attributes, SUBSCRIPTION_ATTRIBUTES_GRP, group, IPPAN_NTF_SEQUENCE_NUMBER,
					IPPDT_INTEGER, 0, buf, 4);
			if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_NTF_SEQUENCE_NUMBER, RQ_SUPP);
		}
		if (Find_casekey_str_value(&requested_attributes, IPPAN_NTF_LEASE_EXPIRATION, Hash_value_sep)) {
			char buf[4];
			hton32(buf, 0);
			Ipp_set_attr(&response.attributes, SUBSCRIPTION_ATTRIBUTES_GRP, group, IPPAN_NTF_LEASE_EXPIRATION,
					IPPDT_INTEGER, 0, buf, 4);
			if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_NTF_LEASE_EXPIRATION, RQ_SUPP);
		}
		if (Find_casekey_str_value(&requested_attributes, IPPAN_NTF_PRINTER_UP_TIME, Hash_value_sep)) {
			if (!jobid) {
				char buf[4];
				time_t t;
				time(&t);
				hton32(buf, t);
				Ipp_set_attr(&response.attributes, SUBSCRIPTION_ATTRIBUTES_GRP, group, IPPAN_NTF_PRINTER_UP_TIME,
						IPPDT_INTEGER, 0, buf, 4);
			}
			if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_NTF_PRINTER_UP_TIME, RQ_SUPP);
		}
		if (Find_casekey_str_value(&requested_attributes, IPPAN_NTF_PRINTER_URI, Hash_value_sep)) {
			char *uri = NULL;
			Ipa_get_printer_uri(&uri, ipp_request->version, conn->port, ppath, printer);
			Ipp_set_attr(&response.attributes, SUBSCRIPTION_ATTRIBUTES_GRP, group, IPPAN_NTF_PRINTER_URI,
					IPPDT_URI, 0, uri, strlen(uri));
			if (uri) free(uri);
			if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_NTF_PRINTER_URI, RQ_SUPP);
		}
		if (Find_casekey_str_value(&requested_attributes, IPPAN_NTF_JOB_ID, Hash_value_sep)) {
			if (jobid) {
				char buf[4];
				hton32(buf, Ipa_jobid2ipp(printer, jobid));
				Ipp_set_attr(&response.attributes, SUBSCRIPTION_ATTRIBUTES_GRP, group, IPPAN_NTF_JOB_ID,
						IPPDT_INTEGER, 0, buf, 4);
			}
			if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_NTF_JOB_ID, RQ_SUPP);
		}
		if (Find_casekey_str_value(&requested_attributes, IPPAN_NTF_SUBSCRIBER_USERN, Hash_value_sep)) {
			char *owner = sx->owner ? sx->owner : Find_str_value(auth_info, KWA_USER);
			Ipp_set_attr(&response.attributes, SUBSCRIPTION_ATTRIBUTES_GRP, group, IPPAN_NTF_SUBSCRIBER_USERN,
					IPPDT_NAME_WITHOUT_LANG, 0, owner, safestrlen(owner));
			if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_NTF_PULL_METHOD_SUPP, RQ_SUPP);
		}
		if (Find_casekey_str_value(&requested_attributes, IPPAN_NTF_PULL_METHOD_SUPP, Hash_value_sep)) {
			Ipp_set_attr(&response.attributes, SUBSCRIPTION_ATTRIBUTES_GRP, group, IPPAN_NTF_PULL_METHOD_SUPP,
					IPPDT_KEYWORD, 0, IPPAV_IPPGET, strlen(IPPAV_IPPGET));
			if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_NTF_PULL_METHOD_SUPP, RQ_SUPP);
		}
		if (Find_casekey_str_value(&requested_attributes, IPPAN_NTF_EVENTS_DEFAULT, Hash_value_sep)) {
			Ipp_set_attr(&response.attributes, SUBSCRIPTION_ATTRIBUTES_GRP, group, IPPAN_NTF_EVENTS_DEFAULT,
					IPPDT_KEYWORD, 0, IPPAV_EVENT_NONE, strlen(IPPAV_EVENT_NONE));
			if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_NTF_EVENTS_DEFAULT, RQ_SUPP);
		}
		if (Find_casekey_str_value(&requested_attributes, IPPAN_NTF_EVENTS_SUPPORTED, Hash_value_sep)) {
			Ipp_set_attr(&response.attributes, SUBSCRIPTION_ATTRIBUTES_GRP, group, IPPAN_NTF_EVENTS_SUPPORTED,
					IPPDT_KEYWORD, 0, IPPAV_EVENT_PR_STATECHANGE, strlen(IPPAV_EVENT_PR_STATECHANGE));
			Ipp_set_attr(&response.attributes, SUBSCRIPTION_ATTRIBUTES_GRP, group, IPPAN_NTF_EVENTS_SUPPORTED,
					IPPDT_KEYWORD, 1, IPPAV_EVENT_PR_STOPPED, strlen(IPPAV_EVENT_PR_STOPPED));
			Ipp_set_attr(&response.attributes, SUBSCRIPTION_ATTRIBUTES_GRP, group, IPPAN_NTF_EVENTS_SUPPORTED,
					IPPDT_KEYWORD, 2, IPPAV_EVENT_JOB_STATECHANGE, strlen(IPPAV_EVENT_JOB_STATECHANGE));
			Ipp_set_attr(&response.attributes, SUBSCRIPTION_ATTRIBUTES_GRP, group, IPPAN_NTF_EVENTS_SUPPORTED,
					IPPDT_KEYWORD, 3, IPPAV_EVENT_JOB_CREATED, strlen(IPPAV_EVENT_JOB_CREATED));
			Ipp_set_attr(&response.attributes, SUBSCRIPTION_ATTRIBUTES_GRP, group, IPPAN_NTF_EVENTS_SUPPORTED,
					IPPDT_KEYWORD, 4, IPPAV_EVENT_JOB_COMPLETED, strlen(IPPAV_EVENT_JOB_COMPLETED));
			if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_NTF_EVENTS_SUPPORTED, RQ_SUPP);
		}
		if (Find_casekey_str_value(&requested_attributes, IPPAN_NTF_MAX_EVENTS_SUPP, Hash_value_sep)) {
			char buf[4];
			hton32(buf, NTF_MAX_EVENTS);
			Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_NTF_MAX_EVENTS_SUPP,
					IPPDT_INTEGER, 0, buf, 4);
			if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_NTF_PULL_METHOD_SUPP, RQ_SUPP);
		}
		if (Find_casekey_str_value(&requested_attributes, IPPAN_NTF_LEASE_DURATION_D, Hash_value_sep)) {
			char buf[4];
			hton32(buf, 0);
			Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_NTF_LEASE_DURATION_D,
					IPPDT_INTEGER, 0, buf, 4);
			if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_NTF_LEASE_DURATION_D, RQ_SUPP);
		}
		if (Find_casekey_str_value(&requested_attributes, IPPAN_NTF_LEASE_DURATION_S, Hash_value_sep)) {
			char buf[4];
			hton32(buf, 0);
			Ipp_set_attr(&response.attributes, PRINTER_ATTRIBUTES_GRP, group, IPPAN_NTF_LEASE_DURATION_S,
					IPPDT_INTEGER, 0, buf, 4);
			if (!group) Set_casekey_str_value(&requested_attributes, IPPAN_NTF_LEASE_DURATION_S, RQ_SUPP);
		}

		/*unsupported attributes*/
		if (!group) {
			for (i = requested_attributes.count - 1; i >= 0 ; i--) {
				char *c, s, *key;
				c = safestrpbrk(requested_attributes.list[i], Hash_value_sep);
				if (!c) continue;
				s = *c;	*c = '\0';
				key = safestrdup(requested_attributes.list[i], __FILE__, __LINE__);
				*c = s;
				if (!safestrcmp(Find_casekey_str_value(&requested_attributes, key, Hash_value_sep), RQ_TRUE)) {
					Ipp_set_attr(&response.attributes, UNSUPPORTED_ATTRIBUTES_GRP, 0, key, IPPDT_UNSUPPORTED, 0, NULL, 0);
					Set_casekey_str_value(&requested_attributes, key, NULL);
					response.op_id_status = SUCCESSFUL_OK_IGNORED_SUBSTITUED;
				}
				if (key) free(key);
			}
		}

	}


	while (subscriptions) {
		sx=subscriptions; subscriptions=subscriptions->next;
		if (sx->owner) free(sx->owner);
		free(sx);
	}

 ipperr:
	Ipp_send_response(conn, http_headers, body_rest_len, &response, auth_info, 0, NULL);

	Free_line_list(&requested_attributes);
	Free_line_list(&printers);
	Ipp_free_operation(&response);

	return 0;
}

/*UNTESTED*/
static int Ipp_op_renew_cancel_subscription(struct line_list *http_request, struct line_list *http_headers, const struct http_conn *conn,
              ssize_t *body_rest_len, struct ipp_operation *ipp_request, struct line_list *auth_info)
{

	struct ipp_operation response;
	char *printer = NULL;
	char *ppath = NULL;
	struct ipp_attr *a;
	char error[SMALLBUFFER];
	int errlen = sizeof(error);
	int subscription_id;
	int lease;
	struct line_list printers;
	int prcount;

	Ipp_init_operation(&response);
	response.version = ipp_request->version;
	response.request_id = ipp_request->request_id;
	/*mandatory attributes*/
	Ipp_set_attr(&response.attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_CHARSET, IPPDT_CHARSET, 0,
		        IPPAV_UTF_8, safestrlen(IPPAV_UTF_8));
	Ipp_set_attr(&response.attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_NAT_LANG, IPPDT_NAT_LANG, 0,
		        IPPAV_EN_US, safestrlen(IPPAV_EN_US));

	Init_line_list(&printers);

	/*validate charset and language*/
	response.op_id_status = Ipa_validate_charset(NULL, ipp_request->attributes);
	if (response.op_id_status != SUCCESSFUL_OK) goto ipperr;
	response.op_id_status = Ipa_validate_language(NULL, ipp_request->attributes);
	if (response.op_id_status != SUCCESSFUL_OK) goto ipperr;

	printer = Find_str_value(auth_info, KWA_PRINTER);
	ppath = Find_str_value(auth_info, KWA_PPATH);
	if (!printer || !ppath) {
		response.op_id_status = CLIENT_ERROR_NOT_FOUND;
		goto ipperr;
	}
	DEBUGF(DNW2)("Ipp_op_renew_cancel_subscription: printer: %s path %s", printer, ppath);
	/*make printer list*/
	if (!safestrcmp(printer, IPPC_ALLPRINTERS)) {
		Get_all_printcap_entries();
		Merge_line_list(&printers, &All_line_list, "", 0, 1);
	} else {
		Add_line_list(&printers, printer, "", 1, 1);
	}

	for (prcount = 0; prcount < printers.count; prcount++) {
		printer = printers.list[prcount];
		if (Setup_printer(printer, error, errlen, 0))	{
			response.op_id_status = SERVER_ERROR_INTERNAL_ERROR;
			goto ipperr;
		}
		Ipa_set_perm(&Perm_check, 'S', auth_info); /*C?*/
		if (Perms_check(&Perm_line_list, &Perm_check, 0, 0) == P_REJECT)
		{
			response.op_id_status = CLIENT_ERROR_NOT_AUTHENTICATED;
			goto ipperr;
		}
	}

	DEBUGF(DNW2)("Ipp_op_renew_cancel_subscription: auths ok");

	response.op_id_status = SUCCESSFUL_OK;

	/*required attribute*/
	a = Ipp_get_attr(ipp_request->attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_NTF_SUBSCRIPTION_ID, -1);
	if (a && a->value_len == 4) {
		subscription_id = ntoh32(a->value);
	} else {
		response.op_id_status = CLIENT_ERROR_BAD_REQUEST;
		goto ipperr;
	}

	/*unsupported attributes*/
	for (a = ipp_request->attributes; a && (a->group == OPERATION_ATTRIBUTES_GRP); a = a->next) {
		if ((!safestrcmp(a->name, IPPAN_CHARSET)) ||
		    (!safestrcmp(a->name, IPPAN_NAT_LANG)) ||
		    (!safestrcmp(a->name, IPPAN_PRINTER_URI)) ||
		    (!safestrcmp(a->name, IPPAN_RQ_USRNAME)) ||
		    (!safestrcmp(a->name, IPPAN_NTF_SUBSCRIPTION_ID)) ||
		    (!safestrcmp(a->name, IPPAN_NTF_LEASE_DURATION))) continue;
		Ipp_set_attr(&response.attributes, UNSUPPORTED_ATTRIBUTES_GRP, 0, a->name, IPPDT_UNSUPPORTED, 0, NULL, 0);
		response.op_id_status = SUCCESSFUL_OK_IGNORED_SUBSTITUED;
	}

	/**/
	if (!subscription_id) {
		response.op_id_status = CLIENT_ERROR_NOT_FOUND;
		goto ipperr;
	}
	/*job subscription test*/
	if (subscription_id & EVENT_JOB) {
		response.op_id_status = CLIENT_ERROR_NOT_POSSIBLE;
		goto ipperr;
	}
	/*data subscription test - not implemented yet*/
	if (subscription_id & EVENT_DATA) {
		response.op_id_status = SERVER_ERROR_INTERNAL_ERROR;
		goto ipperr;
	}

	if (ipp_request->op_id_status == IPPOP_RENEW_SUBSCRIPTION) {

		lease = 0;

		char buf[4];
		hton32(buf, lease);
		Ipp_set_attr(&response.attributes, SUBSCRIPTION_ATTRIBUTES_GRP, 0, IPPAN_NTF_LEASE_DURATION,
				IPPDT_INTEGER, 0, buf, 4);
	}


 ipperr:
	Ipp_send_response(conn, http_headers, body_rest_len, &response, auth_info, 0, NULL);

	Free_line_list(&printers);
	Ipp_free_operation(&response);

	return 0;
}


#define	PRINTER_EVENT	0
#define	JOB_EVENT	1
/*Ipp_get_notifications aux function*/
struct eventlist {
	int id;
	struct eventlist *next;
	char *event;
	char *printer_uri;
	time_t event_time;
	int etype;      /*0 = printer 1 = job event*/
	int seqno;
	int aux_id; 	/*job-id or printer-is-accepting-jobs*/
	int state;
	char *reason;
	char *data1, *data2;	/*printer-name, job-name for CUPS*/
};
/*add an event into list with timestamp order*/
static struct eventlist *add_event(struct eventlist **elist, int subscription_id, int ipp_ver, int port, const char *ppath, const char *printer,
		 const char *event, time_t etime, int evtype, int state, const char *reason, int aux_id)
{
	struct eventlist *e, *a, *b;


	e = malloc_or_die(sizeof(struct eventlist), __FILE__, __LINE__);
	if (!(*elist)) {
		(*elist) = e;
		e->next = NULL;
	} else {
		b = NULL;
		for (a = (*elist); a && (a->event_time < etime); b = a, a = a->next);
		if (!b) {
			e->next = (*elist);
			(*elist) = e;
		} else {
			e->next = b->next;
			b->next = e;
		}
	}
	e->id = subscription_id;
	Ipa_get_printer_uri(&(e->printer_uri), ipp_ver, port, ppath, printer);
	e->event = safestrdup(event, __FILE__, __LINE__);
	e->event_time = etime;
	e->etype = evtype;
	e->state = state;
	e->aux_id = aux_id;
	e->reason = reason ? safestrdup(reason, __FILE__, __LINE__) : NULL;
	e->data1 = NULL;
	e->data2 = NULL;

	int c = 0;
	for (b = (*elist); b; b=b->next, c++);
	DEBUGF(DNW4)("add_event: %s sum %d", event, c);


	return e;
}

static int Ipp_op_get_notifications(struct line_list *http_request, struct line_list *http_headers, const struct http_conn *conn,
             ssize_t *body_rest_len, struct ipp_operation *ipp_request, struct line_list *auth_info)
{

	struct ipp_operation response;
	char *printer = NULL;
	char *ppath = NULL;
	struct ipp_attr *a;
	char error[SMALLBUFFER];
	int errlen = sizeof(error);
	int permission;
	int printable, move, held, err, done, count, group, jobnum, jobstate;
	struct job job;
	char *jreason, *jobname;
	time_t tt;
	int i;
	struct line_list printers;
	int prcount;
	struct sid_data {
		int id;
		int seqno;
		int pr_data, seq_data;
		char *printer;		/*printer name for / job subscription*/
		struct sid_data *next;
	} *subscriptions, *sx;
	int printer_events, job_events;
	struct eventlist *events, *e;
	int pr_bits, get_interval;

	Init_job(&job);
	Ipp_init_operation(&response);
	response.version = ipp_request->version;
	response.request_id = ipp_request->request_id;
	Init_line_list(&printers);
	subscriptions = NULL;
	events = NULL;

	/*set response mandatory attributes*/
	Ipp_set_attr(&response.attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_CHARSET, IPPDT_CHARSET, 0,
		        IPPAV_UTF_8, safestrlen(IPPAV_UTF_8));
	Ipp_set_attr(&response.attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_NAT_LANG, IPPDT_NAT_LANG, 0,
		        IPPAV_EN_US, safestrlen(IPPAV_EN_US));
	/*validate request - charset and language*/
	response.op_id_status = Ipa_validate_charset(NULL, ipp_request->attributes);
	if (response.op_id_status != SUCCESSFUL_OK) goto ipperr;
	response.op_id_status = Ipa_validate_language(NULL, ipp_request->attributes);
	if (response.op_id_status != SUCCESSFUL_OK) goto ipperr;

	printer = Find_str_value(auth_info, KWA_PRINTER);
	ppath = Find_str_value(auth_info, KWA_PPATH);
	if (!printer || !ppath) {
		response.op_id_status = CLIENT_ERROR_NOT_FOUND;
		goto ipperr;
	}
	DEBUGF(DNW2)("Ipp_op_get_notifications: printer: %s path %s", printer, ppath);

	response.op_id_status = SUCCESSFUL_OK;

	for (a = ipp_request->attributes; a; a = a->next) {
		if ((!safestrcmp(a->name, IPPAN_CHARSET)) ||
		    (!safestrcmp(a->name, IPPAN_NAT_LANG)) ||
		    (!safestrcmp(a->name, IPPAN_RQ_USRNAME)) ||
		    (!safestrcmp(a->name, IPPAN_PRINTER_URI)) ||
		    (!safestrcmp(a->name, IPPAN_NTF_SUBSCRIPTION_IDS)) ||
		    (!safestrcmp(a->name, IPPAN_NTF_SEQUENCE_NUMBERS)) ||
		    (!safestrcmp(a->name, IPPAN_NTF_WAIT))) continue;
		Ipp_set_attr(&response.attributes, UNSUPPORTED_ATTRIBUTES_GRP, 0, a->name, IPPDT_UNSUPPORTED, 0, NULL, 0);
		response.op_id_status = SUCCESSFUL_OK_IGNORED_SUBSTITUED;
	}

	/*mandatory attributes - subscriptions etc here todo*/
	printer_events = 0;
	job_events = 0;
	a = Ipp_get_attr(ipp_request->attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_NTF_SUBSCRIPTION_IDS, -1);
	if (!a) {
		response.op_id_status = CLIENT_ERROR_BAD_REQUEST;
		goto ipperr;
	}
	time(&tt);
	for (; a; a = Ipp_get_attr(a->next, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_NTF_SUBSCRIPTION_IDS, -1)) {
		if (a->value_len == 4) i = ntoh32(a->value); else continue;
		DEBUGF(DNW3)("Ipa_op_get_notifications: subscription %x , job subscription %d, data subscription %d", i, is_job_subscription(i), is_data_subscription(i));
		if (!subscriptions) {
			sx = malloc_or_die(sizeof(struct sid_data), __FILE__, __LINE__);
			subscriptions = sx;
		} else {
			sx->next = malloc_or_die(sizeof(struct sid_data), __FILE__, __LINE__);
			sx = sx->next;
		}
		if (!sx) {
			response.op_id_status = SERVER_ERROR_INTERNAL_ERROR;
			goto ipperr;
		}
		sx->id = i;
		sx->seqno = 1;
		sx->pr_data = 0;
		sx->seq_data = 0;
		sx->printer = NULL;
		sx->next = NULL;
		if is_job_subscription(i) {
			job_events = 1;
			Ipa_ipp2jobid(NULL, &(sx->printer), subscription_jobid(i));
			if (sx->printer) {
				Add_line_list(&printers, sx->printer, "", 1, 1);
			} else {
				if (safestrcmp(printer, IPPC_ALLPRINTERS)) {
					Add_line_list(&printers, printer, "", 1, 1);
					sx->printer = safestrdup(printer, __FILE__, __LINE__);
				}
			}
		}
		else { /*printer subscription for printer and job events*/
			if (i & EVENTS_PRINTER) printer_events = 1;
			if (i & EVENTS_JOB) job_events = 1;
			if (!safestrcmp(printer, IPPC_ALLPRINTERS)) {
				Free_line_list(&printers);
				Get_all_printcap_entries();
				Merge_line_list(&printers, &All_line_list, "", 0, 1);
			} else {
				Add_line_list(&printers, printer, "", 1, 1);
			}
		}
	}
	/*no valid subscription*/
	if (!subscriptions) {
		response.op_id_status = CLIENT_ERROR_NOT_FOUND;
		goto ipperr;
	}

	/*number of bits to store printer event data*/
	pr_bits = 0;
	if (printer_events) {
		/*if (printers.count == 1) pr_bits = 3;
		else */ for (prcount = printers.count + 1; prcount && (pr_bits < 6); prcount >>= 1)  pr_bits++ ;
	}

	/*assign notify-sequence-numbers*/
	for (sx = subscriptions, a = Ipp_get_attr(ipp_request->attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_NTF_SEQUENCE_NUMBERS, -1);
	     sx && a; /*FOR condition !*/
	     a = Ipp_get_attr(a->next, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_NTF_SEQUENCE_NUMBERS, -1), sx = sx->next) {
		if (a->value_len == 4) {
			sx->seqno = ntoh32(a->value);
			if (is_printer_subscription(sx->id) && (sx->id & EVENTS_PRINTER)) {
				sx->pr_data = (sx->seqno - 1) & ((1 << pr_bits) - 1);
				sx->seqno = (sx->seqno-1) & ~((1 << pr_bits) - 1);
			}
		}

	}

	get_interval = 1;

	for (prcount = 0; prcount < printers.count; prcount++) {
		printer = printers.list[prcount];
		if (Setup_printer(printer, error, errlen, 0))	{
			response.op_id_status = SERVER_ERROR_INTERNAL_ERROR;
			goto ipperr;
		}

		if (printer_events) {
			Ipa_set_perm(&Perm_check, 'S', auth_info);
			permission = Perms_check(&Perm_line_list, &Perm_check, 0, 0);
			DEBUGF(DNW3)("Ipa_op_get_notifications: printer permission '%s'", perm_str(permission));
			if (permission == P_REJECT)
			{
				response.op_id_status = CLIENT_ERROR_NOT_AUTHENTICATED;
				goto ipperr;
			}
			int state; char *reason; int job_accept, job_count;
			Ipa_get_printer_state(&state, &reason, &job_accept, &job_count);
			for (sx = subscriptions; sx; sx = sx->next) {
				if (!is_printer_subscription(sx->id)) continue;
				/*debug* continue*/;
				if (tt >> pr_bits < sx->seqno >> pr_bits) continue; /*future event*/
				int addev = 1;
				if (prcount <= ((1 << pr_bits) - 2)) {
					switch (sx->pr_data) {
						case 0: addev = (!job_accept) || (state != IPPAV_PRS_IDLE); break;
						case 1: break;
						default: addev = (!job_accept) || (state != IPPAV_PRS_IDLE) || (sx->pr_data == prcount + 2); break;
					}
					if ((!job_accept) || (state != IPPAV_PRS_IDLE))
						switch (sx->seq_data) {
							case 0: sx->seq_data = prcount + 2; break;
							case 1: break;
							default: sx->seq_data = 1; break;
						}
				}
				if (addev) {
					if (sx->id & EVENT_PR_STATECHANGE) {
						e = add_event(&events, sx->id, ipp_request->version, conn->port, ppath, printer, IPPAV_EVENT_PR_STATECHANGE, tt,
								PRINTER_EVENT, state, reason, job_accept);
						Ipa_get_printername(&(e->data1), ppath, printer);
					}
					if ((sx->id & EVENT_PR_STOPPED) && (state == IPPAV_PRS_STOPPED)) {
						e = add_event(&events, sx->id, ipp_request->version, conn->port, ppath, printer, IPPAV_EVENT_PR_STOPPED, tt,
								PRINTER_EVENT, state, reason, job_accept);
						Ipa_get_printername(&(e->data1), ppath, printer);
					}
				}
			}
		}

		if (job_events) {
			Ipa_set_perm(&Perm_check, 'Q', auth_info);
			permission = Perms_check(&Perm_line_list, &Perm_check, 0, 0);
			DEBUGF(DNW3)("Ipa_op_get_notifications: job permission '%s'", perm_str(permission));
			if (permission == P_REJECT)
			{
				response.op_id_status = CLIENT_ERROR_NOT_AUTHENTICATED;
				goto ipperr;
			}

			Free_line_list(&Spool_control);
			Get_spool_control(Queue_control_file_DYN, &Spool_control);
			Scan_queue(&Spool_control, &Sort_order, &printable, &held, &move, 0, &err, &done, 0, 0);
			/*remove done jobs - depends on Sort_order global variable*/
			if (Remove_done_jobs()) {
				Scan_queue(&Spool_control, &Sort_order, &printable, &held, &move, 0, &err, &done, 0, 0);
			}

			for(count = 0; count < Sort_order.count; count++) {
				/*get job info*/
				Free_job(&job);
				Get_job_ticket_file(0, &job, Sort_order.list[count]);
				if (!job.info.count) {
					/*got lpd_status.c*/
					continue;
				}

				DEBUGFC(DNW4)Dump_job("Ipa_get_notifications - info", &job);

				jobnum = Find_decimal_value(&job.info, NUMBER);
				jobname = Find_str_value(&job.info, JOBNAME);

				for (sx = subscriptions; sx; sx = sx->next) {
					if (!(is_printer_subscription(sx->id) ||
						 (is_job_subscription(sx->id) && ((jobnum != subscription_jobid(sx->id)) ||
										  safestrcmp(sx->printer, printer)
										  ))
						)) continue;

					DEBUGF(DNW4)("Ipa_op_get_notifications: subscription '%x'", sx->id);

					Ipa_get_job_state(&jobstate, &jreason, &job, &Spool_control);
					time_t oldest_time, remove_time, error_time, hold_time, job_time, start_time, pending_time;
					int init_state;
					char *init_reason;
					/*do job state by time, not last state*/
					oldest_time = remove_time = Convert_to_time_t(Find_str_value(&job.info, REMOVE_TIME));
					init_state = jobstate;
					init_reason = jreason;
					if ((sx->id & EVENT_JOB_COMPLETED) && remove_time && (remove_time >= sx->seqno)) {
						e = add_event(&events, sx->id, ipp_request->version, conn->port, ppath, printer, IPPAV_EVENT_JOB_COMPLETED, remove_time,
								JOB_EVENT, jobstate, jreason, jobnum);
						Ipa_get_printername(&(e->data1), ppath, printer);
						e->data2 = Local_to_utf8(jobname);
					}
					if ((sx->id & EVENT_JOB_STATECHANGE) && remove_time && (remove_time >= sx->seqno)) {
						e = add_event(&events, sx->id, ipp_request->version, conn->port, ppath, printer, IPPAV_EVENT_JOB_STATECHANGE, remove_time,
								JOB_EVENT, jobstate, jreason, jobnum);
						Ipa_get_printername(&(e->data1), ppath, printer);
						e->data2 = Local_to_utf8(jobname);
					}
					error_time = Convert_to_time_t(Find_str_value(&job.info, ERROR_TIME));
					if (!remove_time /*= not in canceled/aborted state*/ &&
					    (sx->id & EVENT_JOB_STATECHANGE) && error_time && (error_time >= sx->seqno)) {
						e = add_event(&events, sx->id, ipp_request->version, conn->port, ppath, printer, IPPAV_EVENT_JOB_STATECHANGE, error_time,
								JOB_EVENT, IPPAV_JRS_STOPPED, IPPAV_JSR_NONE, jobnum);
						Ipa_get_printername(&(e->data1), ppath, printer);
						e->data2 = Local_to_utf8(jobname);
					}
					if ((error_time < oldest_time) || (error_time && !oldest_time)) {
						oldest_time = error_time; init_state = IPPAV_JRS_STOPPED; init_reason = (char *)IPPAV_JSR_NONE;
					}
					hold_time = Convert_to_time_t(Find_str_value(&job.info, HOLD_TIME));
					if ((sx->id & EVENT_JOB_STATECHANGE) && hold_time && (hold_time >= sx->seqno)) {
						e = add_event(&events, sx->id, ipp_request->version, conn->port, ppath, printer, IPPAV_EVENT_JOB_STATECHANGE, hold_time,
								JOB_EVENT, IPPAV_JRS_HELD, IPPAV_JSR_NONE, jobnum);
						Ipa_get_printername(&(e->data1), ppath, printer);
						e->data2 = Local_to_utf8(jobname);
					}
					if ((hold_time < oldest_time) || (hold_time && !oldest_time)) {
						oldest_time = hold_time; init_state = IPPAV_JRS_HELD; init_reason = (char *)IPPAV_JSR_NONE;
					}
					start_time = Convert_to_time_t(Find_str_value(&job.info, START_TIME));
					if ((sx->id & EVENT_JOB_STATECHANGE) && start_time && ((start_time < oldest_time) || !oldest_time) && (start_time >= sx->seqno)) {
						e = add_event(&events, sx->id, ipp_request->version, conn->port, ppath, printer, IPPAV_EVENT_JOB_STATECHANGE, start_time,
								JOB_EVENT, IPPAV_JRS_PROCESSING, IPPAV_JSR_NONE, jobnum);
						Ipa_get_printername(&(e->data1), ppath, printer);
						e->data2 = Local_to_utf8(jobname);
					}
					if ((start_time < oldest_time) || (start_time && !oldest_time)) {
						oldest_time = start_time; init_state = IPPAV_JRS_PROCESSING; init_reason = (char *)IPPAV_JSR_NONE;
					}
					/*we do not have RELEASE TIME, UN_ERROR TIME, RESETART TIME  so send the info of current state now (from any state to pending)*/
					if (!oldest_time && (jobstate == IPPAV_JRS_PENDING)) {
						oldest_time = pending_time = time((void *)0); init_state = IPPAV_JRS_PENDING; init_reason = (char *)IPPAV_JSR_NONE;

					} else pending_time = 0;
					if ((sx->id & EVENT_JOB_STATECHANGE) && pending_time && (pending_time >= sx->seqno)) {
						e = add_event(&events, sx->id, ipp_request->version, conn->port, ppath, printer, IPPAV_EVENT_JOB_STATECHANGE, pending_time,
								JOB_EVENT, IPPAV_JRS_PENDING, IPPAV_JSR_NONE, jobnum);
						Ipa_get_printername(&(e->data1), ppath, printer);
						e->data2 = Local_to_utf8(jobname);
					}
					job_time = Convert_to_time_t(Find_str_value(&job.info, JOB_TIME));
					if ((sx->id & EVENT_JOB_CREATED) && job_time &&(job_time >= sx->seqno)) {
						e = add_event(&events, sx->id, ipp_request->version, conn->port, ppath, printer, IPPAV_EVENT_JOB_CREATED, job_time,
								JOB_EVENT, init_state, init_reason, jobnum);
						Ipa_get_printername(&(e->data1), ppath, printer);
						e->data2 = Local_to_utf8(jobname);
					}
					if ((sx->id & EVENT_JOB_STATECHANGE) && job_time && ((job_time < oldest_time) || !oldest_time) && (job_time >= sx->seqno)) {
						e = add_event(&events, sx->id, ipp_request->version, conn->port, ppath, printer, IPPAV_EVENT_JOB_STATECHANGE, job_time,
								JOB_EVENT, init_state, init_reason, jobnum);
						Ipa_get_printername(&(e->data1), ppath, printer);
						e->data2 = Local_to_utf8(jobname);
					}

					/*
					time_t t = Convert_to_time_t(Find_str_value(&job.info, JOB_TIME));
					if ((sx->id & EVENT_JOB_CREATED) && t &&(t >= sx->seqno)) {
						e = add_event(&events, sx->id, ipp_request->version, conn->port, ppath, printer, IPPAV_EVENT_JOB_CREATED, t,
								JOB_EVENT, IPPAV_JRS_PENDING, IPPAV_JSR_NONE, jobnum);
						Ipa_get_printername(&(e->data1), ppath, printer);
						e->data2 = Local_to_utf8(jobname);
					}
					if ((sx->id & EVENT_JOB_STATECHANGE) && t && (t >= sx->seqno)) {
						e = add_event(&events, sx->id, ipp_request->version, conn->port, ppath, printer, IPPAV_EVENT_JOB_STATECHANGE, t,
								JOB_EVENT, IPPAV_JRS_PENDING, IPPAV_JSR_NONE, jobnum);
						Ipa_get_printername(&(e->data1), ppath, printer);
						e->data2 = Local_to_utf8(jobname);
					}
					t = Convert_to_time_t(Find_str_value(&job.info, START_TIME));
					if ((sx->id & EVENT_JOB_STATECHANGE) && t && (t >= sx->seqno)) {
						e = add_event(&events, sx->id, ipp_request->version, conn->port, ppath, printer, IPPAV_EVENT_JOB_STATECHANGE, t,
								JOB_EVENT, IPPAV_JRS_PROCESSING, IPPAV_JSR_NONE, jobnum);
						Ipa_get_printername(&(e->data1), ppath, printer);
						e->data2 = Local_to_utf8(jobname);
					}
					t = Convert_to_time_t(Find_str_value(&job.info, REMOVE_TIME));
					if ((sx->id & EVENT_JOB_COMPLETED) && t && (t >= sx->seqno)) {
						e = add_event(&events, sx->id, ipp_request->version, conn->port, ppath, printer, IPPAV_EVENT_JOB_COMPLETED, t,
								JOB_EVENT, jobstate, jreason, jobnum);
						Ipa_get_printername(&(e->data1), ppath, printer);
						e->data2 = Local_to_utf8(jobname);
					}
					if ((sx->id & EVENT_JOB_STATECHANGE) && t && (t >= sx->seqno)) {
						e = add_event(&events, sx->id, ipp_request->version, conn->port, ppath, printer, IPPAV_EVENT_JOB_STATECHANGE, t,
								JOB_EVENT, jobstate, jreason, jobnum);
						Ipa_get_printername(&(e->data1), ppath, printer);
						e->data2 = Local_to_utf8(jobname);
					}
					t = Convert_to_time_t(Find_str_value(&job.info, HOLD_TIME));
					if ((sx->id & EVENT_JOB_STATECHANGE) && t && (t >= sx->seqno)) {
						e = add_event(&events, sx->id, ipp_request->version, conn->port, ppath, printer, IPPAV_EVENT_JOB_STATECHANGE, t,
								JOB_EVENT, IPPAV_JRS_HELD, IPPAV_JSR_NONE, jobnum);
						Ipa_get_printername(&(e->data1), ppath, printer);
						e->data2 = Local_to_utf8(jobname);
					}
					t = Convert_to_time_t(Find_str_value(&job.info, ERROR_TIME));
					if (!Find_str_value(&job.info, REMOVE_TIME) &&
					    (sx->id & EVENT_JOB_STATECHANGE) && t && (t >= sx->seqno)) {
						e = add_event(&events, sx->id, ipp_request->version, conn->port, ppath, printer, IPPAV_EVENT_JOB_STATECHANGE, t,
								JOB_EVENT, IPPAV_JRS_STOPPED, IPPAV_JSR_NONE, jobnum);
						Ipa_get_printername(&(e->data1), ppath, printer);
						e->data2 = Local_to_utf8(jobname);
					}
					/ *we do not have RELEASE TIME, UN_ERROR TIME, RESETART TIME  so send the info of current state now (from any state to pending)* /
					if ((sx->id & EVENT_JOB_STATECHANGE) && (t >= sx->seqno) && (jobstate == IPPAV_JRS_PENDING)) {
						e = add_event(&events, sx->id, ipp_request->version, conn->port, ppath, printer, IPPAV_EVENT_JOB_STATECHANGE, tt,
								JOB_EVENT, IPPAV_JRS_PENDING, IPPAV_JSR_NONE, jobnum);
						Ipa_get_printername(&(e->data1), ppath, printer);
						e->data2 = Local_to_utf8(jobname);
					}
					*/
				}

			} /* count<Sort_order.count*/
		}
		get_interval++;
	}

	get_interval += Lpq_status_interval_DYN;
	if (get_interval < (1 << pr_bits)) get_interval = 1 << pr_bits;

	/*send events here*/
	char buf[4], tst[4];
	hton32(buf, get_interval);
	Ipp_set_attr(&response.attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_NTF_GET_INTERVAL,
			IPPDT_INTEGER, 0, buf, 4);
	hton32(tst, tt);
	Ipp_set_attr(&response.attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_PRINTER_UP_TIME,
			IPPDT_INTEGER, 0, buf, 4);
	for (e = events, group = 0; e; e = e->next, group++) {
		hton32(buf, e->id);
		Ipp_set_attr(&response.attributes, EVENT_NOTIFICATION_ATTRIBUTES_GRP, group, IPPAN_NTF_SUBSCRIPTION_ID,
				IPPDT_INTEGER, 0, buf, 4);
		Ipp_set_attr(&response.attributes, EVENT_NOTIFICATION_ATTRIBUTES_GRP, group, IPPAN_NTF_PRINTER_URI,
				IPPDT_URI, 0, e->printer_uri, safestrlen(e->printer_uri));
		Ipp_set_attr(&response.attributes, EVENT_NOTIFICATION_ATTRIBUTES_GRP, group, IPPAN_NTF_SUBSCRIBED_EVENT,
				IPPDT_KEYWORD, 0, e->event, safestrlen(e->event));
		hton32(buf, e->event_time);
		Ipp_set_attr(&response.attributes, EVENT_NOTIFICATION_ATTRIBUTES_GRP, group, IPPAN_PRINTER_UP_TIME,
				IPPDT_INTEGER, 0, buf, 4);

		if (is_printer_subscription(e->id) && (e->id & EVENTS_PRINTER)) {
		/*find sequence number*/
			for (sx = subscriptions; sx && sx->id != e->id; sx = sx->next);
			hton32(buf, sx ? (tt & ~((1 << pr_bits) - 1)) | sx->seq_data : tt);
			Ipp_set_attr(&response.attributes, EVENT_NOTIFICATION_ATTRIBUTES_GRP, group, IPPAN_NTF_SEQUENCE_NUMBER,
					IPPDT_INTEGER, 0, buf, 4);
		} else {
			Ipp_set_attr(&response.attributes, EVENT_NOTIFICATION_ATTRIBUTES_GRP, group, IPPAN_NTF_SEQUENCE_NUMBER,
					IPPDT_INTEGER, 0, tst, 4);		}
		Ipp_set_attr(&response.attributes, EVENT_NOTIFICATION_ATTRIBUTES_GRP, group, IPPAN_NTF_CHARSET,
				IPPDT_CHARSET, 0, IPPAV_UTF_8, safestrlen(IPPAV_UTF_8));
		Ipp_set_attr(&response.attributes, EVENT_NOTIFICATION_ATTRIBUTES_GRP, group, IPPAN_NTF_NAT_LANG,
				IPPDT_NAT_LANG, 0, IPPAV_EN_US, safestrlen(IPPAV_EN_US));
		Ipp_set_attr(&response.attributes, EVENT_NOTIFICATION_ATTRIBUTES_GRP, group, IPPAN_NTF_USER_DATA,
				IPPDT_OCTET_STRING, 0, 0, 0);
		Ipp_set_attr(&response.attributes, EVENT_NOTIFICATION_ATTRIBUTES_GRP, group, IPPAN_NTF_TEXT,
				IPPDT_TEXT_WITHOUT_LANG, 0, 0, 0);
		if (e->etype == PRINTER_EVENT) {
			hton32(buf, e->state);
			Ipp_set_attr(&response.attributes, EVENT_NOTIFICATION_ATTRIBUTES_GRP, group, IPPAN_PRINTER_STATE,
					IPPDT_ENUM, 0, buf, 4);
			Ipp_set_attr(&response.attributes, EVENT_NOTIFICATION_ATTRIBUTES_GRP, group, IPPAN_PRINTER_SREASONS,
					IPPDT_KEYWORD, 0, e->reason, safestrlen(e->reason));
			char b = e->aux_id ? 1 : 0;
			Ipp_set_attr(&response.attributes, EVENT_NOTIFICATION_ATTRIBUTES_GRP, group, IPPAN_PRINTER_JOB_ACCEPTING,
					IPPDT_BOOLEAN, 0, &b, 1);
			/*not required by rfc3996, but wanted by system-config-printer*/
			Ipp_set_attr(&response.attributes, EVENT_NOTIFICATION_ATTRIBUTES_GRP, group, IPPAN_PRINTER_NAME,
					IPPDT_NAME_WITHOUT_LANG, 0, e->data1, safestrlen(e->data1));
		} else { /*JOB_EVENT*/
			hton32(buf, Ipa_jobid2ipp(e->data1, e->aux_id));
			Ipp_set_attr(&response.attributes, EVENT_NOTIFICATION_ATTRIBUTES_GRP, group, IPPAN_JOB_ID,
					IPPDT_INTEGER, 0, buf, 4);
			hton32(buf, e->state);
			Ipp_set_attr(&response.attributes, EVENT_NOTIFICATION_ATTRIBUTES_GRP, group, IPPAN_JOB_STATE,
					IPPDT_ENUM, 0, buf, 4);
			Ipp_set_attr(&response.attributes, EVENT_NOTIFICATION_ATTRIBUTES_GRP, group, IPPAN_JOB_SREASONS,
					IPPDT_KEYWORD, 0, e->reason, safestrlen(e->reason));
			/*not required by rfc3996, but wanted by system-config-printer*/
			hton32(buf, Ipa_jobid2ipp(e->data1, e->aux_id));
			Ipp_set_attr(&response.attributes, EVENT_NOTIFICATION_ATTRIBUTES_GRP, group, IPPAN_NTF_JOB_ID,
					IPPDT_INTEGER, 0, buf, 4);
			Ipp_set_attr(&response.attributes, EVENT_NOTIFICATION_ATTRIBUTES_GRP, group, IPPAN_JOB_NAME,
					IPPDT_NAME_WITHOUT_LANG, 0, e->data2, safestrlen(e->data2));
			Ipp_set_attr(&response.attributes, EVENT_NOTIFICATION_ATTRIBUTES_GRP, group, IPPAN_PRINTER_NAME,
					IPPDT_NAME_WITHOUT_LANG, 0, e->data1, safestrlen(e->data1));
			}
	}


 ipperr:

	while (subscriptions) {
		sx=subscriptions; subscriptions=subscriptions->next;
		if (sx->printer) free(sx->printer);
		free(sx);
	}
	while (events) {
		e=events; events=events->next;
		if (e->event) free(e->event);
		if (e->printer_uri) free(e->printer_uri);
		if (e->reason) free(e->reason);
		if (e->data1) free(e->data1);
		if (e->data2) free(e->data2);
		free(e);
	}

	Free_job(&job);
	Free_line_list(&printers);
	Ipp_send_response(conn, http_headers, body_rest_len, &response, auth_info, 0, NULL);
	Ipp_free_operation(&response);

	return 0;
}


struct ipp_procs OperationsSupported[] = {
	{ IPPOP_PRINT_JOB, Ipp_op_print_validate_job, AUTHS_PRINTER },
	{ IPPOP_VALIDATE_JOB, Ipp_op_print_validate_job, AUTHS_PRINTER },
	{ IPPOP_CREATE_JOB, Ipp_op_print_validate_job, AUTHS_PRINTER },
	{ IPPOP_SEND_DOCUMENT, Ipp_op_send_document, AUTHS_JOB },
	{ IPPOP_CANCEL_JOB, Ipp_op_cancel_job, AUTHS_JOB },
	{ IPPOP_GET_JOB_ATTRIBUTES, Ipp_op_get_jobs, AUTHS_JOB },
	{ IPPOP_GET_JOBS, Ipp_op_get_jobs, AUTHS_ALL_PRINTERS },
	{ IPPOP_GET_PRINTER_ATTRIBUTES, Ipp_op_get_printer_attributes, AUTHS_PRINTER },
	{ IPPOP_HOLD_JOB, Ipp_op_set_job_attributes, AUTHS_JOB },
	{ IPPOP_RELEASE_JOB, Ipp_op_set_job_attributes, AUTHS_JOB },
	{ IPPOP_RESTART_JOB, Ipp_op_set_job_attributes, AUTHS_JOB },
	{ IPPOP_CHANGE_JOB_ATTRIBUTES, Ipp_op_set_job_attributes, AUTHS_JOB },
	{ IPPOP_CREATE_PRINTER_SUBSCRIPTIONS, Ipp_op_create_printer_subscriptions, AUTHS_SUBSCRIPTION_PRINTERS },
	{ IPPOP_GET_SUBSCRIPTION_ATTRIBUTES, Ipp_op_get_subscription_attributes, AUTHS_SUBSCRIPTION_ATTRIBUTES },
	{ IPPOP_GET_SUBSCRIPTIONS, Ipp_op_get_subscription_attributes, AUTHS_SUBSCRIPTION_PRINTERS },
	{ IPPOP_RENEW_SUBSCRIPTION, Ipp_op_renew_cancel_subscription, AUTHS_SUBSCRIPTION },
	{ IPPOP_CANCEL_SUBSCRIPTION, Ipp_op_renew_cancel_subscription, AUTHS_SUBSCRIPTION },
	{ IPPOP_GET_NOTIFICATIONS, Ipp_op_get_notifications, AUTHS_SUBSCRIPTION_PRINTERS },
	{ IPPOP_CUPS_GET_DEFAULT, Ipp_op_get_printer_attributes, AUTHS_NONE },
	{ IPPOP_CUPS_GET_PRINTERS, Ipp_op_get_printer_attributes, AUTHS_NONE },
	{ IPPOP_CUPS_GET_CLASSES, Ipp_op_empty_ok, AUTHS_NONE },
	{ IPPOP_CUPS_MOVE_JOB, Ipp_op_set_job_attributes, AUTHS_PRINTER_OR_JOB },
	{ 0, NULL, AUTHS_NONE }
};

