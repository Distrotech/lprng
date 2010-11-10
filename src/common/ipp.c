/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 *
 * ipp protocol service
 * copyright 2008 Vaclav Michalek, Jonit Laberatory of Optics, Olomouc
 *
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

#include "lp.h"
#include "child.h"
#include "linelist.h"
#include "utilities.h"
#include "ipp.h"
#include "ipp_proc.h"
#include "plp_snprintf.h"
#include "ipp_aux.h"
#include "fileopen.h"
#ifdef	SSL_ENABLE
#include "user_auth.h"
#include "ssl_auth.h"
#include "getqueue.h"
#endif

/*some status codes for internal functions*/
#define RS_OK		0
#define RS_EOF		10
#define RS_READ		1	/*read error*/
#define RS_SMALLBUF	3       /*small internal buffer error*/
#define	RS_SYNTAX_ERR	4       /*http request/header syntax error*/


/*keywords for request*/
#define KWH_METHOD 	"METHOD"
#define KWH_URI		"URI"
#define KWH_VER		"VERSION"

static int Http_read_request(const struct http_conn *conn, struct line_list *request);
static int Http_read_headers(const struct http_conn *conn, struct line_list *headers);
int Http_proto_version(const char *version_str);
static ssize_t Http_skip_message_body(const struct http_conn *conn, ssize_t rest_len, struct line_list *headers);
int Http_send_status(const struct http_conn *conn, const char *version, int code, const char *value);
int Http_send_date_header(const struct http_conn *conn);
int Http_send_header(const struct http_conn *conn, const char *name, const char *value);

static int Ipp_decode_request(struct line_list *http_request, struct line_list *http_headers, const struct http_conn *conn,
                              ssize_t *rest_len, struct ipp_operation *ipp_request);
static int Ipp_authenticate(struct line_list *auth_info, struct line_list *http_request, struct line_list *http_headers,
		            const char *from_addr, const struct http_conn *conn, ssize_t *rest_len, struct ipp_operation *ipp_request, int auth_scope);
#ifdef	SSL_ENABLE
static int Sec_proto(const char *s, const char *proto);
static int Ssl_switch(struct http_conn *conn);
static int Ssl_close(struct http_conn *conn);
#endif

struct http_io plain_io; /*defined below*/
#ifdef	SSL_ENABLE
struct http_io ssl_io; /*defined below*/
#endif


/*check configuration parametres releated to IPP, called from lpd.c*/
void Ipp_check_options(void)
{
	/*if we are serving IPP, we must hold job events (=jobs) at least 15 seconds - minimum evet life value (ippget-event-life) rfc3996
	  job save also needed for IPP Create-Job/Send-Document operations pair
*/
	if (Done_jobs_max_age_DYN < 15) {
		Set_decimal_value(&Config_line_list, "done_jobs_max_age", 15);
		Done_jobs_max_age_DYN = 15;
	}
#ifdef	SSL_ENABLE
	/*since we must advertize IPP/SSL, test SSL availability*/
	SSL_CTX *ctx = 0;
	char errmsg[LARGEBUFFER]; errmsg[0] = 0;
	char *file = Ssl_server_password_file_DYN;
	char password_value[64], *s;
	struct stat statb;
	int fd = -1, n;

	/*avoid DIE - see Ssl_server_password_file .... etc from SSL_initialize todo*/
	if( file ){
		if( ((fd = Checkread( file, &statb )) < 0) ||
		    ((n = ok_read(fd, password_value, sizeof(password_value)-1)) < 0 )
		  ) {
			ipp_ssl_available = 0;
			goto ssl_err;
		}
		password_value[n] = 0;
		if( (s = safestrchr(password_value,'\n')) ) *s = 0;
		n = strlen(password_value);
		if( n == 0 ){
			ipp_ssl_available = 0;
			goto ssl_err;
		}
	}
	ipp_ssl_available = !SSL_Initialize_ctx(&ctx, errmsg, sizeof(errmsg));
	if (ctx) Destroy_ctx(ctx);
ssl_err:;
	/*fprintf(stderr,"SSL available: %d\n", ipp_ssl_available);*/
	DEBUG2("Test SSL availability: SSL available: %d, error %s",
		ipp_ssl_available, ipp_ssl_available ? "none" : (errmsg[0] ? errmsg : "Ssl_server_password_file" ));
	if (fd >= 0) close(fd);
#endif

}

/*
 * Main service function
 * 1. read http headers into linelist
 * 2. test/dispatch http communication parametres
 * 3. if desired, switch to TLS encryption - todo
 * 4. read ipp request
 * 5. read files/dispatch the request
 *
 */
void Service_ipp(int talk, int port, const char *from_addr)
{

	int rqtimeout = (Send_job_rw_timeout_DYN>0)?Send_job_rw_timeout_DYN:
	                                            ((Connect_timeout_DYN>0)?Connect_timeout_DYN:10);
	int http_persistent = 0;              /*http 1.1 persistent connection*/
	struct http_conn conn = {
		rqtimeout, talk, port,
#ifdef SSL_ENABLE
		NULL,
		NULL,
		NULL,
#endif
		&plain_io};
	int status = 0;
	struct line_list http_request, http_headers, auth_info;
	int http_version = 11;                /*http protocol version*/
	char *h;
	struct ipp_operation ipp_request;     /*decoded IPP request*/
	ssize_t body_rest_len;                /*rest length of http message body*/
	struct ipp_procs *s;
	struct host_information *save_host, *save_remotehost;
	struct stat statb;
	int forbidden;


	Init_line_list(&http_request);
	Init_line_list(&http_headers);
	Ipp_init_operation(&ipp_request);
	Init_line_list(&auth_info);

	save_host = Perm_check.host;              /*save values over persistent connection*/
	save_remotehost = Perm_check.remotehost;
	/*check some variables value*/
	if (Ipp_compat_hrcount_DYN <= 0) Ipp_compat_hrcount_DYN = 100;

	Name = "SERVER-IPP";

	DEBUGF(DNW2)("Http connect from %s to port %d", from_addr, conn.port);

#ifdef	SSL_ENABLE
	/*connected directly to http secure port ?*/
	if (conn.port == ipp_ippsport) {
		Ssl_switch(&conn);
		if (!conn.ssl) goto conn_err;
	}
#endif
	/*test "X" service permission*/
	forbidden = (Perms_check( &Perm_line_list, &Perm_check, 0, 0 ) == P_REJECT);

	do {
		/*DEBUGF(DNW4)("Http request read start");*/
		status = Http_read_request(&conn, &http_request);
		if (status == RS_OK)
			status = Http_read_headers(&conn, &http_headers);
		DEBUGF(DNW4)("Http request read status: %d", status);

		http_version = Http_proto_version(Find_str_value(&http_request, KWH_VER));
		/*test for bad request*/
		if ((status == RS_EOF) || (status == RS_READ))
			break; /*more friendly to browsers than to send Bad request (due to persistent connection timeout) */
		if ((status != RS_OK) ||         /*socket read or syntax error*/
		    (http_version < 10) ||       /*unknown HTTP version*/
		    ((http_version >=11) && !Find_str_value(&http_headers, HTTPH_HOST))  /*missing mandatory Host: header for http 1.1 protocol*/
		   ) {
			Http_send_status(&conn, HTTP_VER_11, 400, HTTPM_BAD_REQUEST);
			Http_send_date_header(&conn);
			Http_send_header(&conn, HTTPH_SERVER, HTTPV_LPRNGS);
			Http_send_header(&conn, HTTPH_CONNECTION, HTTPV_CLOSE);
			Http_send_header(&conn, NULL, NULL);
			break;
		}
		/*send properly formated "forbidden" response if no connect permission*/
		if (forbidden) {
			Http_send_status(&conn, HTTP_VER_11, 403, HTTPM_FORBIDDEN);
			Http_send_date_header(&conn);
			Http_send_header(&conn, HTTPH_SERVER, HTTPV_LPRNGS);
			Http_send_header(&conn, HTTPH_CONNECTION, HTTPV_CLOSE);
			Http_send_header(&conn, NULL, NULL);
			break;
		}
		/*test for Transfer-Encoding: only the "chunked" is accepted*/
		if ((h = Find_str_value(&http_headers, HTTPH_TRANSFER_ENCODING)) &&
		    safestrcasecmp(h, HTTPV_CHUNKED) ) {
			Http_send_status(&conn, HTTP_VER_11, 501, HTTPM_TE_UNIMPLEMENTED);
			Http_send_date_header(&conn);
			Http_send_header(&conn, HTTPH_SERVER, HTTPV_LPRNGS);
			Http_send_header(&conn, HTTPH_CONNECTION, HTTPV_CLOSE);
			Http_send_header(&conn, NULL, NULL);
			break;
		}
		/*test the Expect: field - only "100-contine" is allowed*/
		if ((http_version >= 11) &&
		    (h = Find_str_value(&http_headers, HTTPH_EXPECT)) &&
		    safestrcasecmp(h, HTTPV_100_CONTINUE) ) {
			Http_send_status(&conn, HTTP_VER_11, 417, HTTPM_EXPECTATION_FAILED);
			Http_send_date_header(&conn);
			Http_send_header(&conn, HTTPH_SERVER, HTTPV_LPRNGS);
			Http_send_header(&conn, HTTPH_CONNECTION, HTTPV_CLOSE);
			Http_send_header(&conn, NULL, NULL);
			break;
		}
		/*persistent connection*/
		h = Find_str_value(&http_headers, HTTPH_CONNECTION);
		http_persistent = (h && safestrcmp(h, HTTPV_CLOSE)) ||
		                  ((http_version >= 11) && !h);

#ifndef	SSL_ENABLE
		DEBUGF(DNW4)("Http request version %.1f, persistent connection: %d", http_version*0.1, http_persistent);
#else
		DEBUGF(DNW4)("Http request version %.1f, persistent connection: %d ssl: %d", http_version*0.1, http_persistent, conn.ssl ? 1 : 0);

		/*switch to TLS if desired*/
		if ((http_version >= 11) && (!conn.ssl)	&&
		    (h = Find_str_value(&http_headers, HTTPH_UPGRADE)) && Sec_proto(h, HTTP_TLS_10) &&
		    (h = Find_str_value(&http_headers, HTTPH_CONNECTION)) && !safestrcasecmp(h, HTTPH_UPGRADE)) {

			int sz = safestrlen(HTTP_VER_11) + safestrlen(HTTP_TLS_10) + 3;
			char *u = malloc_or_die(sz, __FILE__, __LINE__);
			/*plp_snprintf(u, sz, "%s, %s", HTTP_TLS_10, HTTP_VER_11);*/
			plp_snprintf(u, sz, "%s", HTTP_TLS_10);

			Http_send_status(&conn, HTTP_VER_11, 101, HTTPM_SWITCHING_PROTOCOLS);
			Http_send_date_header(&conn);
			Http_send_header(&conn, HTTPH_SERVER, HTTPV_LPRNGS);
			Http_send_header(&conn, HTTPH_UPGRADE, u);
			Http_send_header(&conn, HTTPH_CONNECTION, HTTPH_UPGRADE);
			Http_send_header(&conn, NULL, NULL);
			if (u) free(u);
			Ssl_switch(&conn);
			if (!conn.ssl) break;
			DEBUGF(DNW4)("Http: Switched to SSL ok");
		}
#endif
		/*process by method*/
		h = Find_str_value(&http_request, KWH_METHOD);
		if (!safestrcmp(h, HTTP_POST)) {

			/*test Accept, Content-type, Accept-Encoding, Content-Encoding headers*/
			if (((h = Find_str_value(&http_headers, HTTPH_ACCEPT)) && (!strstr(h, HTTPV_APPLICATION_IPP)) && (!strstr(h, "*/*"))) ||
			    ((h = Find_str_value(&http_headers, HTTPH_CONTENT_TYPE)) && (safestrcmp(h, HTTPV_APPLICATION_IPP))) ||
			    ((h = Find_str_value(&http_headers, HTTPH_ACCEPT_ENCODING)) && (!strstr(h, HTTPV_IDENTITY))) ||
			    ((h = Find_str_value(&http_headers, HTTPH_CONTENT_ENCODING)) && (safestrcmp(h, HTTPV_IDENTITY))) )   {

				/*406 Not acceptable*/
				Http_send_status(&conn, HTTP_VER_11, 406, HTTPM_NOT_ACCEPTABLE);
				Http_send_date_header(&conn);
				Http_send_header(&conn, HTTPH_SERVER, HTTPV_LPRNGS);
				Http_send_header(&conn, HTTPH_ACCEPT, HTTPV_APPLICATION_IPP);
				Http_send_header(&conn, HTTPH_ACCEPT_ENCODING, HTTPV_IDENTITY);
				Http_send_header(&conn, HTTPH_CONTENT_LENGTH, "0");
				Http_send_header(&conn, NULL, NULL);
				/*body must be read after the 4xx response send - possible Expect*/
				Http_skip_message_body(&conn, -1, &http_headers);
				continue;
			}
			/*get the body if not yet sent*/
			if ((http_version >= 11) && (Find_str_value(&http_headers, HTTPH_EXPECT))) {
				Http_send_status(&conn, HTTP_VER_11, 100, HTTPM_CONTINUE);
				Http_send_header(&conn, NULL, NULL);
			}
			/*decode and dispatch ipp-request here*/
			body_rest_len = -1;
			status = Ipp_decode_request(&http_request, &http_headers, &conn, &body_rest_len, &ipp_request);
			if (status < 0) {
				Http_send_status(&conn, HTTP_VER_11, 400, HTTPM_BAD_REQUEST);
				Http_send_date_header(&conn);
				Http_send_header(&conn, HTTPH_SERVER, HTTPV_LPRNGS);
				Http_send_header(&conn, HTTPH_CONNECTION, HTTPV_CLOSE);
				Http_send_header(&conn, NULL, NULL);
				break;
			}

			/*operation dispatch*/
			for (s = OperationsSupported; (s->op_code) && (s->op_code != ipp_request.op_id_status); s++);
			if (s->op_proc) {
				/*www-authentication (based on printer, not uri and hence after ipp request decoding)*/
				if (!Ipp_authenticate(&auth_info, &http_request, &http_headers, from_addr, &conn,
						&body_rest_len, &ipp_request, s->auth_scope))
					continue;
				(s->op_proc)(&http_request, &http_headers, &conn, &body_rest_len, &ipp_request, &auth_info);
			} else {
				Ipp_op_unknown(&http_request, &http_headers, &conn, &body_rest_len, &ipp_request, &auth_info);
			}

		} else if ((!safestrcmp(h, HTTP_GET)) ||
		           (!safestrcmp(h, HTTP_HEAD)) ) {

			if (!safestrcmp(Find_str_value(&http_request, KWH_URI), "/")) {
				char *body = "This is an IPP server Lprng\r\n";
				char bstrlen[16]; sprintf(bstrlen, "%d", safestrlen(body));
				char *u;
#ifdef	SSL_ENABLE
				if (ipp_ssl_available || conn.ssl) {
					int sz = safestrlen(HTTP_VER_11) + safestrlen(HTTP_TLS_10) + 3; /*comma, spaces + zero*/
					u = malloc_or_die(sz, __FILE__, __LINE__);
					plp_snprintf(u, sz, "%s, %s", HTTP_TLS_10, HTTP_VER_11);
				} else
#endif
					u = safestrdup(HTTP_VER_11, __FILE__, __LINE__);

				Http_send_status(&conn, HTTP_VER_11, 200, HTTPM_OK);
				Http_send_date_header(&conn);
				Http_send_header(&conn, HTTPH_SERVER, HTTPV_LPRNGS);
				Http_send_header(&conn, HTTPH_UPGRADE, u);
				Http_send_header(&conn, HTTPH_CONTENT_TYPE, "text/plain");
				Http_send_header(&conn, HTTPH_CONTENT_LENGTH, bstrlen);
				Http_send_header(&conn, NULL, NULL);
				if (safestrcmp(h, HTTP_HEAD)) {
					(conn.iofunc->write_conn_len_timeout)(&conn, body, safestrlen(body));
				}
				if (u) free(u);
			} else if ((h = Iph_CUPS_ppd_uri(Find_str_value(&http_request, KWH_URI)))) {
				/*CUPS ppd uri*/
				DEBUGF(DNW4)("Http GET CUPS PPD: printer %s", h);

				int fd = -1;
				Ipa_ppd_fd(&fd, &statb, NULL, h);
				if (fd > 0) {
					char bstrlen[16]; size_t sz = statb.st_size;
					sprintf(bstrlen, "%zd", sz);
					Http_send_status(&conn, HTTP_VER_11, 200, HTTPM_OK);
					Http_send_date_header(&conn);
					Http_send_header(&conn, HTTPH_SERVER, HTTPV_LPRNGS);
					Http_send_header(&conn, HTTPH_CONTENT_LENGTH, bstrlen);
					Http_send_header(&conn, NULL, NULL);

					if (safestrcmp(h, HTTP_HEAD)) {
						Ipa_copy_fd(&conn, fd, statb.st_size);
					}
				} else {
					Http_send_status(&conn, HTTP_VER_11, 404, HTTPM_NOT_FOUND);
					Http_send_date_header(&conn);
					Http_send_header(&conn, HTTPH_SERVER, HTTPV_LPRNGS);
					Http_send_header(&conn, HTTPH_CONTENT_LENGTH, "0");
					Http_send_header(&conn, NULL, NULL);
				}

				free(h); h = NULL;
				if (fd >= 0) close(fd);
			} else {
				Http_send_status(&conn, HTTP_VER_11, 404, HTTPM_NOT_FOUND);
				Http_send_date_header(&conn);
				Http_send_header(&conn, HTTPH_SERVER, HTTPV_LPRNGS);
				Http_send_header(&conn, HTTPH_CONTENT_LENGTH, "0");
				Http_send_header(&conn, NULL, NULL);
			}

		} else if (!safestrcmp(h, HTTP_OPTIONS)) {

			if ((http_version >= 11) && (Find_str_value(&http_headers, HTTPH_EXPECT))) {
				Http_send_status(&conn, HTTP_VER_11, 100, HTTPM_CONTINUE);
				Http_send_header(&conn, NULL, NULL);
			}

			Http_skip_message_body(&conn, -1, &http_headers);

			int sz = safestrlen(HTTP_OPTIONS) + safestrlen(HTTP_POST) + safestrlen(HTTP_GET) + safestrlen(HTTP_HEAD) + 7; /*comma, spaces + zero*/
			char *s = malloc_or_die(sz, __FILE__, __LINE__);
			plp_snprintf(s, sz, "%s, %s, %s, %s", HTTP_OPTIONS, HTTP_POST, HTTP_GET, HTTP_HEAD);
			char *u;
#ifdef	SSL_ENABLE
			if (ipp_ssl_available || conn.ssl) {
				sz = safestrlen(HTTP_VER_11) + safestrlen(HTTP_TLS_10) + 3; /*3 = comma, spaces + zero*/
				u = malloc_or_die(sz, __FILE__, __LINE__);
				plp_snprintf(u, sz, "%s, %s", HTTP_TLS_10, HTTP_VER_11);
			} else
#endif
				u = safestrdup(HTTP_VER_11, __FILE__, __LINE__);

			Http_send_status(&conn, HTTP_VER_11, 200, HTTPM_OK);
			Http_send_date_header(&conn);
			Http_send_header(&conn, HTTPH_SERVER, HTTPV_LPRNGS);
			Http_send_header(&conn, HTTPH_UPGRADE, u);
			Http_send_header(&conn, HTTPH_ALLOW, s);
			Http_send_header(&conn, HTTPH_CONTENT_LENGTH, "0");   /*required for persistent connection*/
			Http_send_header(&conn, NULL, NULL);
			if (s) free (s);
			if (u) free (u);

		} else { /*invalid/unknown http method*/

			int sz = safestrlen(HTTP_OPTIONS) + safestrlen(HTTP_POST) + safestrlen(HTTP_GET) + safestrlen(HTTP_HEAD) + 7; /*comma, spaces + zero*/
			char *s = malloc_or_die(sz, __FILE__, __LINE__);
			plp_snprintf(s, sz, "%s, %s, %s, %s", HTTP_OPTIONS, HTTP_POST, HTTP_GET, HTTP_HEAD);

			Http_send_status(&conn, HTTP_VER_11, 405, HTTPM_BAD_METHOD);
			Http_send_date_header(&conn);
			Http_send_header(&conn, HTTPH_SERVER, HTTPV_LPRNGS);
			Http_send_header(&conn, HTTPH_ALLOW, s);
			Http_send_header(&conn, HTTPH_CONTENT_LENGTH, "0");
			Http_send_header(&conn, NULL, NULL);
			if (s) free (s);
			/*due possible Expect: must be body reading after 4xx response sending*/
			Http_skip_message_body(&conn, -1, &http_headers);

		} /*http method dispatch*/
		/*clear some data in persistent connection*/
		Perm_check.service = 'X';
		Perm_check.printer = NULL;
		Perm_check.ppath = NULL;
		Perm_check.remoteuser = NULL;
		Perm_check.user = NULL;
		Perm_check.authtype = NULL;
		Perm_check.authuser = NULL;
		Set_DYN(&Printer_DYN, NULL);
		Perm_check.lpc = 0;
		Perm_check.host = save_host;
		Perm_check.remotehost = save_remotehost;

	} while (http_persistent);

#ifdef	SSL_ENABLE
conn_err:
	if (conn.ssl_info) Ssl_close(&conn);
#endif

	Free_line_list(&auth_info);
	Ipp_free_operation(&ipp_request);
	Free_line_list(&http_request);
	Free_line_list(&http_headers);

	DEBUGF(DNW2)("Http close to %s", from_addr);

	cleanup(0);

}

/*extract version from protocol version string
   e.g. "HTTP/1.0" -> return 10
        "HTTP/1.1" -> return 11
   return -1 on error
 */
int Http_proto_version(const char *version_str)
{
        int len = safestrlen(version_str);
        if (len < 3) return -1;

        char *c = rindex(version_str, '/');
        c++;

	char *loc = setlocale(LC_NUMERIC, NULL);
	setlocale(LC_NUMERIC, "C");
	int rs = strtod(c, NULL)*10;
	setlocale(LC_NUMERIC, loc);
        rs = (rs <= 0) ? -1 : rs;

	return rs;
}

/*auxiliary functions for Http_read_request/headers
  Append character to string
*/
static inline int Append_char(char *dest, int *count, int maxsize, char c)
{
	if (*count == maxsize)
		return RS_SMALLBUF;
	dest[*count] = c;
	(*count)++;
	dest[*count] = '\0';
	return RS_OK;
}
/*"remove" white space in string*/
static inline void Remove_ending_LWS(char *str, int *size)
{
	while (*size > 0) {
	    switch (str[(*size)-1]) {
		case ' ':
		case '\t':
		case '\r':
		case '\n': str[(*size)-1] = '\0';
		           (*size)--;
			   break;
		default  : return;
	    }
	}

}
/*read one character from http connection*/
static inline int io_read_char(const struct http_conn *conn, char *c)
{
	int rs;

	rs = (conn->iofunc->read_conn_len_timeout)(conn, c, 1);
	/*DEBUGF(DNW2)("read char %d", rs);*/
	switch (rs) {
		case 0: return RS_EOF;
			break;
		case 1: return RS_OK;
			break;
		default:return RS_READ;
			break;
	}


}
/*read and decode http request line (example: GET / HTTP/1.0)*/
static int Http_read_request(const struct http_conn *conn, struct line_list *request)
{
	const char *HTKeys[] = {KWH_METHOD, KWH_URI, KWH_VER, 0};

	char hbuf[SMALLBUFFER];
	char c;
	int running = 1;
	int k = 0;
	int hcount;
	int maxc = sizeof(hbuf) - 1 ;
	int status = RS_OK;

	Free_line_list(request);

	c = '\0';
	hcount = 0; hbuf[0] = '\0';
	while (running) {
		if ( (status = io_read_char(conn, &c)) != RS_OK) {
			goto rq_error;
		}
		switch (c) {
		    case '\n':  if (hbuf[hcount] != '\r') { /*error HTTP line does not end with CRLF*/ ; }
			        running = 0; /*outer break*/
		    case '\r':
		    case ' ' :
		    case '\t':  if (hcount && HTKeys[k]) {
					Set_str_value(request, HTKeys[k], hbuf);
					k++;
				}
				hcount = 0;
				continue;
		}
		if (HTKeys[k] && ((status = Append_char(hbuf, &hcount, maxc, c)) != RS_OK)) {
			goto rq_error;
		}
	}

  rq_error:

	if (status == RS_OK) {
		DEBUGFC(DNW4)Dump_line_list("Http_read_request: read request line OK ", request);
	} else {
		DEBUGF(DNW2)("Http_read_request: read request error, status %d", status);
	}
	return status;
}

/*read and decode http headers*/
static int Http_read_headers(const struct http_conn *conn, struct line_list *headers)
{

	char hbuf[SMALLBUFFER];     	/*field-name buffer*/
	char vbuf[SMALLBUFFER];         /*field-value*/

	char c;
	int stage;
	int hcount, vcount;
	int maxc = sizeof(hbuf) - 1 ;
	int maxv = sizeof(vbuf) -1 ;
	int status = RS_OK;
	int cquote ;                    /*-1 = quoted string 0 = none, positive = comment */
	int bquote ;                    /*following character is quoted*/

	Free_line_list(headers);

	/*parse HTTP headers - delimited by CRLF except quoted escape string, comment and value continuation rfc2616*/
        /*direct implementation is simpler than state-machine ...*/
	stage = 1;    /*0 = read done, 1 = read field name, 2 = read value*/
	c = '\0';
	hcount = 0; hbuf[0] = '\0';
	vcount = 0; vbuf[0] = '\0';

	while (stage) {
		/*read field-name*/
		while (stage == 1) {
			if ( (status = io_read_char(conn, &c)) != RS_OK) {
			        goto rq_error;
			}
			switch (c) {
			    case ':' :	if (hcount) {
						stage = 2;
						continue;
					} else {
						status = RS_SYNTAX_ERR;
						goto rq_error;
					}
					break;
			    case '\t':
			    case ' ' :  if (!hcount) {  /*value continuation*/
						if ((status = Append_char(vbuf, &vcount, maxv, c)) != RS_OK)
							goto rq_error;
						stage = 2;
						continue;
					}
					break;
			    case '\r':  continue;	/*just ignore ... */
			    case '\n':  if (vcount) {
						Remove_ending_LWS(vbuf, &vcount);
						Set_str_value(headers, hbuf, vbuf);
					}
					stage = 0; /*end of headers*/
				        continue;
			    default  :  if (vcount) {
						Remove_ending_LWS(vbuf, &vcount);
						Set_str_value(headers, hbuf, vbuf);
						hcount = 0; hbuf[0] = '\0';
						vcount = 0; vbuf[0] = '\0';
					}
					break;
			}
			if ((status = Append_char(hbuf, &hcount, maxc, c)) != RS_OK) {
				goto rq_error;
			}
		}

		/*read field-value*/
		bquote = 0; cquote = 0;
		while (stage == 2) {
			if ( (status = io_read_char(conn, &c)) != RS_OK) {
			        goto rq_error;
			}
			if (vbuf[vcount] != '\\') bquote = 0;
			if (!bquote) {
				switch (c) {
					case '\\' : if (cquote)
							    bquote = (vbuf[vcount] == '\\') ? !bquote : 1;
						    break;
					case '"' :  switch (cquote) {
							    case  0: cquote = -1; break;
							    case -1: cquote = 0;  break;
						    }
						    break;
					case '(' :  if (cquote >= 0) cquote++;  break;
					case ')' :  if (cquote > 0) cquote--; break;
					case '\n':  if (cquote) break;
						    stage = 1;  /*continue to next header field-name*/
						    continue;
					case '\r':
					case '\t':  if (!cquote) c = ' ';
					case ' ' :  if (cquote) break;
						    if (!vcount) continue;  /*remove leading space*/
						    break;
				}
			}
			if ((status = Append_char(vbuf, &vcount, maxv, c)) != RS_OK) {
				goto rq_error;
			}
		}
	}

  rq_error:

	if (status == RS_OK) {
		DEBUGFC(DNW4)Dump_line_list("Http_read_headers: read request headers OK ", headers);
	} else {
		DEBUGF(DNW2)("Http_read_headers: read headers error, status %d", status);
	}

	return status;
}

/* read and skip optional HTTP message body
 * used for methods with rfc-possible, but not needed data (like OPTIONS)
 * rest_len: size or unread chunk length to skip, if first read, use -1
 * return >=0 OK
 *         -1 error: missing Content-length or Transfer-Encoding
 */
static ssize_t Http_skip_message_body(const struct http_conn *conn, ssize_t rest_len, struct line_list *headers)
{
	char c[SMALLBUFFER];
	ssize_t body_len, rs;
	ssize_t rd = 0;

	body_len = Http_content_length(headers);
	if ((body_len <= 0) && (body_len != -99)) return body_len ;
	/*DEBUGF(DNW4)("Http_skip_messsage_body: to skip: %ld bytes", body_len); 				*/
	do {
		rs = Http_read_body(conn, body_len, &rest_len, c, sizeof(c), headers);
		if (rs < 0) return -1;
		if (rs == 0) break; /*EOF*/
		rd += rs;

	} while (rest_len > 0);

	if (rd) DEBUGF(DNW4)("Http_skip_messsage_body: skipped %ld bytes", (long)rd);
	return rd;
}

/*return message body size, -99 if chunked, 0 if unspecified, -1 on error*/
ssize_t Http_content_length(struct line_list *headers)
{

	ssize_t rs = 0;
	int i;
	char  *h, *e;
	struct line_list encodings;

	h = Find_str_value(headers, HTTPH_TRANSFER_ENCODING);
	if (h) {
		Init_line_list(&encodings);
		Split(&encodings, h, " \r\n\t,", 0, NULL, 1, 1, 0, NULL);
		for (i = 0; i < encodings.count; i++) {
			if (!safestrcasecmp(encodings.list[i], HTTPV_CHUNKED)) {
				rs = -99;
				break;
			}
		}
		Free_line_list(&encodings);
		if (rs != 0) return rs;
	}

	h = Find_str_value(headers, HTTPH_CONTENT_LENGTH);
	if (!h) return 0;  /*Content-Length header not present, probably no message-body present*/
	rs = strtoll(h, &e, 10);
	if (*e != '\0' ) return -1;

	return rs;
}
/* read max count bytes from fd and return the actual read size or -1 if error
 * content_len: message size from http header or -99 for chunked transfer
 * rest_len: number of rest bytes from message or previous chunk; the value is updated; first call with -1, returned 0 when whole message read
 * headers: if not null, merged with trailer entity headers (chunked encoding)
 */
inline ssize_t Http_read_body(const struct http_conn *conn, ssize_t content_len, ssize_t *rest_len, char *buf, ssize_t count, struct line_list *headers)
{

	char c = '\0', d, *e;
	char chunksz[129];   /*string to hold chunk size*/
	ssize_t rd = 0;
	int rs;
	unsigned int i;

	if (!count || !(*rest_len)) return 0;

	if (content_len >= 0) { /*no encoding*/
		if ((*rest_len < 0) || (*rest_len > content_len)) *rest_len = content_len;
		if (*rest_len < count) count = *rest_len;
		rd = (conn->iofunc->read_conn_len_timeout)(conn, buf, count);
		if (rd < 0) return -1;
		if (rd == 0) return 0; /*EOF*/
		*rest_len -= rd;
		return rd;
	}

	if (content_len == -99) { /*chunked encoding*/
		while (1) {
			if (!count) return rd;
			/*read rest of previous chunk*/
			/*DEBUGF(DNW4)("Http_read_body: rest_len %ld", *rest_len);*/
			if (*rest_len > 0) {
				rs = (conn->iofunc->read_conn_len_timeout)(conn, buf + rd, *rest_len < count ? *rest_len : count);
				if (rs < 0) return -1;
				if (rs == 0) return 0; /*EOF*/
				count -= rs;
				*rest_len -= rs;
				rd += rs;
				if (*rest_len) {  /* size of previous chunk is greater than count */
					return rd;
				}
				/*whole chunk read, so read chunk-ending CRLF*/
				if (((conn->iofunc->read_conn_len_timeout)(conn, &c, 1) != 1)  ||  (c != '\r') ||
				    ((conn->iofunc->read_conn_len_timeout)(conn, &c, 1) != 1)  ||  (c != '\n')) {
					/*return -1;*/
				}
			}
			/*read next chunk size*/
			i = 0;
			chunksz[0] = '\0';
			while ( (i + 1 < sizeof(chunksz)) &&
			        ((rs = (conn->iofunc->read_conn_len_timeout)(conn, &c, 1)) == 1) &&
				isxdigit(c) ) {
				chunksz[i] = c; chunksz[i+1] = '\0';
				i++;
			}
			if (rs < 0) return -1;
			if (rs == 0) return 0; /*EOF*/
			*rest_len = strtoll(chunksz, &e, 16);
			if (*e != '\0' ) return -1;
			DEBUGF(DNW4)("Http_read_body: chunk size %ld bytes", (long)*rest_len);
			/*now we have chunk size in *rest_len and some next char in c*/
			/*skip characters until CRLF (for simplicity, we do not deal with chunk-extensions) */
			while (c!='\r') if ((rs = (conn->iofunc->read_conn_len_timeout)(conn, &c, 1)) <= 0) return -1;  /* rs==0 -> EOF */
			if (((rs = (conn->iofunc->read_conn_len_timeout)(conn, &c, 1)) <= 0) || (c != '\n')) return -1;
			/*test last chunk*/
			if (!(*rest_len)) { /*last-chunk*/
				/*skip/save trailer + CRLF*/
				struct line_list trailer;
				Init_line_list(&trailer);
				Http_read_headers(conn, &trailer);
				if (headers) {
					/*clear rcf-invalid headers in trailer*/
					Set_str_value(&trailer, HTTPH_TRANSFER_ENCODING, "");
					Set_str_value(&trailer, HTTPH_CONTENT_LENGTH, "");
					Set_str_value(&trailer, HTTPH_TRAILER, "");
					Merge_line_list(headers, &trailer, 0, 1, 1);
					/*DEBUGFC(DNW4)Dump_line_list("Http_read_body: headers after adding trailer:", headers);*/
				}
				Free_line_list(&trailer);
				return rd;
			}
		}
	}

	/*invalid content_len*/
	return -1;
}

/*send http response line*/
int Http_send_status(const struct http_conn *conn, const char *version, int code, const char *value)
{
	char *s;
	int rs, len;

	len = safestrlen(version) + safestrlen(value) + 10;
	s = malloc_or_die(len, __FILE__, __LINE__);
	plp_unsafe_snprintf(s, len, "%s %3.3d %s%s", version, code, value, HTTP_CRLF); s[len-1] = '\0';
	rs = (conn->iofunc->write_conn_len_timeout)(conn, s, safestrlen(s));

	/*make pretty debug print string*/
	if (safestrlen(s) >= safestrlen(HTTP_CRLF)) s[safestrlen(s) - safestrlen(HTTP_CRLF)] = '\0';
	DEBUGF(DNW4)("Http_send_status: status %d, status line '%s'", rs, s);

	if (s) free(s);
	return rs;

}

int Http_send_date_header(const struct http_conn *conn)
{
	char *s, *r;
	int rs, len;
	time_t t;

        len = safestrlen(HTTPH_DATE) + 35;
	s = malloc_or_die(len, __FILE__, __LINE__);
	time(&t);
	strftime(s, len, "%a, %d %b %Y %H:%M:%S GMT", gmtime(&t)); s[len-1] = '\0';
	rs = Http_send_header(conn, HTTPH_DATE, s);
	if (s) free(s);

	return rs;
}

int Http_send_header(const struct http_conn *conn, const char *name, const char *value)
{
	char *s;
	int rs, len;

	len = safestrlen(name) + safestrlen(value) + 10;
	s = malloc_or_die(len, __FILE__, __LINE__);
	if (name) {
		plp_unsafe_snprintf(s, len, "%s: %s%s", name, value, HTTP_CRLF); s[len-1] = '\0';
	} else {
		strcpy(s, HTTP_CRLF);
	}
	rs = (conn->iofunc->write_conn_len_timeout)(conn, s, safestrlen(s));

	if (DEBUGFSET(DNW4)) {
		/*make pretty debug print string*/
		if (safestrlen(s) >= safestrlen(HTTP_CRLF)) s[safestrlen(s) - safestrlen(HTTP_CRLF)] = '\0';
		LOGDEBUG("Http_send_header: status %d, header line %s length(%d)", rs, safestrlen(s) ? s: "<End of headers>", safestrlen(s));
	}

	if (s) free(s);
	return rs;

}

/*application/ipp related routines*/
/*init structure ipp_operation*/
int Ipp_init_operation(struct ipp_operation *operation)
{
	operation->version = 0;
	operation->op_id_status = 0;
	operation->request_id = 0;
	operation->attributes = NULL;
	return 0;
}

/*dispose ipp_operation structure*/
int Ipp_free_operation(struct ipp_operation *operation)
{
	operation->version = 0;
	operation->op_id_status = 0;
	operation->request_id = 0;
	while (operation->attributes) {
	    if (operation->attributes->name) free(operation->attributes->name);
	    if (operation->attributes->value) free(operation->attributes->value);
	    void *v = operation->attributes;
	    operation->attributes = operation->attributes->next;
	    free(v);
	}
	operation->attributes = NULL;
	return 0;
}

/*find ipp atrribute within sorted linked list
 * use -1 for arbitrary group, arbitrary group index and arbitrary value index
 * */
struct ipp_attr *Ipp_get_attr(const struct ipp_attr *attributes, int group, int group_index, const char *name, int index)
{
	struct ipp_attr *a;

	a = (struct ipp_attr *)attributes;
	/*find group+group index*/
	/*DEBUGF(DNW4)("Ipp_get_attr: find name  %s group %d  group_index %d", name ? name: "NULL" , group, group_index);*/
	while ((a) &&
		(((a->group != group) && (group != -1)) ||
		 ((a->group_num != group_index) && (group != -1) && (group_index != -1))) ) {
		a = a->next;
		/*DEBUGF(DNW4)("Ipp_get_attr: attributes %s group %d  a %s group %d",
				attributes ? attributes->name : "NULL", attributes ? attributes->group : -1, a ? a->name : "NULL", a ? a->group : -1);*/
	}
	/*find first attribute in group only*/
	if (!name && a &&
	    ((group == -1) || (a->group == group)) &&
	    ((group_index == -1) || (a->group_num == group_index))) {
		/*DEBUGF(DNW4)("Ipp_get_attr: return name %s", a ? a->name : "NULL");*/
		return a;
	}
	/*find name within the group and group index*/
	/*attribute with NULL name will be used to indicate empty mandatory attribute group*/
	while ((a) && ((group == -1) ||
		       ((a->group == group) && ((a->group_num == group_index) || (group_index == -1))))  &&
	       (safestrcmp(a->name, name) || !(a->name && name)) )   {
		a = a->next;
		/*DEBUGF(DNW4)("Ipp_get_attr: a name %s group %d", a ? a->name : "NULL", a ? a->group : -1);*/
	}
	/*DEBUGF(DNW4)("Ipp_get_attr: a name found ?  %s group %d", a ? a->name : "NULL", a ? a->group : -1);*/
	/*find value index in given group, group index, name*/
	while ((a) && ((group == -1) ||
		       ((a->group == group) && ((a->group_num == group_index) || (group_index == -1)))) &&
	       (!safestrcmp(a->name, name) || (a->name && name)) && (a->value_index != index) && (index != -1) ) {
		a = a->next;
	}
	if ((a) &&
	    ((group == -1) || (a->group == group)) &&
	    ((group_index == -1) || (a->group_num == group_index)) &&
	    (!safestrcmp(a->name, name) || (a->name && name)) &&
	    ((index == -1) || (a->value_index == index)) ) {
		/*DEBUGF(DNW4)("Ipp_get_attr: return name %s", a ? a->name : "NULL");*/
		return a;
	}
	/*DEBUGF(DNW4)("Ipp_get_attr: return name NULL");*/
	return NULL;
}

/*attributes are stored in linked list, sorted by IPP group, group number and name*/
struct ipp_attr *Ipp_set_attr(struct ipp_attr **attributes, int group, int group_index, const char *name, int type, int index, const void *value, int value_len)
{
	struct ipp_attr *a = NULL;
	struct ipp_attr *b = NULL;

	if (!attributes) return NULL;

	a = *attributes;
	b = a;
	/*find group+group index*/
	while ((a) &&
		(((a->group != group) && (group != -1)) ||
		 ((a->group_num != group_index) && (group != -1) && (group_index != -1))
		) ) {
		b = a;
		a = a->next;
	}
	/*find name within the group and group index*/
	/*attribute with NULL name will be used to indicate empty mandatory attribute group*/
	while ((a) && ((group == -1) ||
		       ((a->group == group) && ((a->group_num == group_index) || (group_index == -1))))  &&
	       (safestrcmp(a->name, name) || !(a->name && name)) )   {
		b = a;
		a = a->next;
	}
	/*find value index in given group, group index, name (multiple-value attributes)*/
	while ((a) && ((group == -1) ||
		       ((a->group == group) && ((a->group_num == group_index) || (group_index == -1)))) &&
	       (!safestrcmp(a->name, name) || (a->name && name)) && (a->value_index != index) && (index != -1) ) {
		b = a;
		a = a->next;
	}
	if ((a) &&
	    ((group == -1) || (a->group == group)) &&
	    ((group_index == -1) || (a->group_num == group_index)) &&
	    (!safestrcmp(a->name, name) || (a->name && name)) &&
	    ((index == -1) || (a->value_index == index)) ) {
		/*set/delete value here*/;
		if (!value || (value_len == 0)) {
			(*attributes)->next = a->next;
			if (a->name) free (a->name);
			if (a->value) free (a->value);
			free(a);
		} else {
			a->type = type;
			if (a->value) {
				a->value = realloc_or_die(a->value, value_len, __FILE__, __LINE__);
			} else {
				a->value = malloc_or_die(value_len, __FILE__, __LINE__);
			}
			memcpy(a->value, value, value_len);
			a->value_len = value_len;
		}
	} else {
		/*put new value after attributes*/;
		a = malloc_or_die(sizeof(struct ipp_attr), __FILE__, __LINE__);
		if (name) {
			a->name = malloc_or_die(strlen(name) + 1, __FILE__, __LINE__);
			strcpy(a->name, name);
		} else {
			a->name = NULL;
		}
		a->group = group;
		a->group_num = group_index;
		a->type = type;
		a->value_index = index;
		if (value && (value_len > 0)) {
			a->value = malloc_or_die(value_len, __FILE__, __LINE__);
			memcpy(a->value, value, value_len);
			a->value_len = value_len;
		} else {
			a->value = NULL;
			a->value_len = 0;
		}
		if (*attributes) {
			a->next = b->next;
			b->next = a;
		} else	{
			a->next = NULL;
			*attributes = a;
		}
	}

	return a;
}

/*read and decode IPP request, return list of attributes in ipp_request
  return 0 = OK, -1 = error
*/
static int Ipp_decode_request(struct line_list *http_request, struct line_list *http_headers, const struct http_conn *conn,
                              ssize_t *rest_len, struct ipp_operation *ipp_request)
{
	ssize_t ct_len, rs;
	char buf[SMALLBUFFER];
	char tag, delim;
	int nlen, group_num;
	struct ipp_attr *aattr, *ap;
	struct line_list group_nums;
	char groupx[2]; memset(groupx, 0, sizeof(groupx));


	Init_line_list(&group_nums);
	Ipp_free_operation(ipp_request);

	if ((ct_len = Http_content_length(http_headers)) == -1) {
		rs = -1; goto ret;
	}
	rs = Http_read_body(conn, ct_len, rest_len, buf, 9, http_headers);
	if (rs != 9 ) {
		rs = -1; goto ret;
	}
	rs = 0;

	ipp_request->version = buf[0] * 10 + buf[1];
	ipp_request->op_id_status = ntoh16(&(buf[2])) ;
	ipp_request->request_id = ntoh32(&(buf[4])) ;
	DEBUGF(DNW4)("Ipp_decode_request: IPP version %1.1f, operation %.2X, request-id %ld",
	             ipp_request->version * 0.1, ipp_request->op_id_status, ipp_request->request_id);
	tag = buf[8];
	delim = 0;
	group_num = 0;
	aattr = NULL; /*current attribute*/
	ap = NULL;    /*previous attribute*/

	while (tag != END_OF_ATTRIBUTES_TAG) {
		if (tag <= 0x0f) {
			groupx[0] = 'A' + tag;  /*tag = group "type"*/
			group_num = Find_decimal_value(&group_nums, groupx); /*distinguish among more groups of the same type, e.g. more subscription groups*/
			Set_flag_value(&group_nums, groupx, group_num + 1);
			delim = tag;
			if (Http_read_body(conn, ct_len, rest_len, &tag, 1, http_headers) != 1) {rs = -1; goto ret;} /*read next tag*/
			if (tag <= 0x0f) continue;
		}
		/*read name len and name*/
		if (Http_read_body(conn, ct_len, rest_len, buf, 2, http_headers) != 2) {rs = -1; goto ret;}
		nlen = ntoh16(buf);
		ap = aattr; /*previous attribute*/
		aattr = malloc_or_die(sizeof(struct ipp_attr), __FILE__, __LINE__);
		if (!ap)
			ipp_request->attributes = aattr;
		else
			ap->next = aattr;
		aattr->next = NULL;

		aattr->group = delim;   aattr->group_num = group_num;
		aattr->name = NULL;	aattr->type = tag;
		aattr->value_index = 0;
		aattr->value_len = 0;	aattr->value = NULL;
		if (nlen) {
			aattr->name = malloc_or_die(nlen + 1, __FILE__, __LINE__);
			if (Http_read_body(conn, ct_len, rest_len, aattr->name, nlen, http_headers) != nlen) {rs = -1; goto ret;}
			aattr->name[nlen] = '\0';

		} else { /*set of values*/
			if (ap) {
				aattr->name = safestrdup(ap->name, __FILE__, __LINE__);
				aattr->value_index = ap->value_index + 1;
			}
		}
		/*read value len and value*/
		if (Http_read_body(conn, ct_len, rest_len, buf, 2, http_headers) != 2) {rs = -1; goto ret;}
		nlen = ntoh16(buf);
		if (nlen) {
			aattr->value = malloc_or_die(nlen, __FILE__, __LINE__);
			if (Http_read_body(conn, ct_len, rest_len, aattr->value, nlen, http_headers) != nlen) {rs = -1; goto ret; }
			aattr->value_len = nlen;
		}
		/*debug print here*/
		if (DEBUGFSET(DNW4)) {
			switch(aattr->type) {
				case IPPDT_BOOLEAN:
					 nlen = (int)sizeof(buf) < 6 ? (int)sizeof(buf) : 6;
					 plp_snprintf(buf, sizeof(buf), "%s", ((char)(*(aattr->value)) ? "true" : "false") );
					 break;
				case IPPDT_INTEGER:
				case IPPDT_ENUM:
					 nlen = (int)sizeof(buf) < 12 ? (int)sizeof(buf) : 12;
					 plp_snprintf(buf, sizeof(buf), "%d", ntoh32(aattr->value) & 0xffffffff);
					 break;
				default: nlen = (int)sizeof(buf) < aattr->value_len ? (int)sizeof(buf) : aattr->value_len;
					 strncpy(buf, aattr->value ? (aattr->value) : "(NULL)", nlen);
			}
			if (nlen < (int)sizeof(buf)) buf[nlen] = '\0'; else buf[sizeof(buf) - 1] = '\0';
			LOGDEBUG("Ipp_decode_request: name %s type 0x%.2X group %d/%d index %d len %d value %s",
		                  aattr->name, aattr->type, aattr->group, aattr->group_num, aattr->value_index, aattr->value_len, buf );
		}
		/*read next tag*/
		if (Http_read_body(conn, ct_len, rest_len, &tag, 1, http_headers) != 1) {rs = -1; goto ret; }
	}

ret:
	Free_line_list(&group_nums);
	return rs;
}

/*returns true if method contained in line list*/
static inline int printer_auth_method(const struct line_list *auths, const char *method)
{
	int i;
	for (i = 0; i < auths->count; i++) {
		if (!safestrcmp(auths->list[i], method)) return 1;
	}
	return 0;
}

/*decode printer name from ipp_request and perform www authentication; send response if fail
  return 0 = fail
         1 = authentication OK
*/
static int Ipp_authenticate(struct line_list *auth_info, struct line_list *http_request, struct line_list *http_headers,
		            const char *from_addr, const struct http_conn *conn, ssize_t *rest_len, struct ipp_operation *ipp_request, int auth_scope)
{
	char *printer = NULL;
	char *ppath = NULL;
	int rs ;
	struct line_list pc_entry, pc_alias, auths, auth_parts;
	char *ah, *ath;
	char buf[LINEBUFFER], brealm[LINEBUFFER];
	struct ipp_attr *a;
	size_t sz;
	char *uagent;

	Init_line_list(&pc_entry);
	Init_line_list(&pc_alias);
	Init_line_list(&auths);
	Free_line_list(auth_info);
	Init_line_list(&auth_parts);


	if (auth_scope == AUTHS_NONE) {
		rs = 1;
		goto endproc;
	}

	if ( (Ipa_get_printer(&printer, &ppath, ipp_request->attributes, auth_scope) != SUCCESSFUL_OK))
	{
		/*invalid printer name/URI*/
		Http_send_status(conn, HTTP_VER_11, 404, HTTPM_NOT_FOUND);
		Http_send_date_header(conn);
		Http_send_header(conn, HTTPH_SERVER, HTTPV_LPRNGS);
		Http_send_header(conn, HTTPH_CONTENT_LENGTH, "0");
		Http_send_header(conn, NULL, NULL);
		Http_skip_message_body(conn, *rest_len, http_headers);
		rs = 0;
		goto endproc;
	}

	Set_str_value(auth_info, KWA_PPATH, ppath);
	Set_str_value(auth_info, KWA_PRINTER, printer);

	/*special & compatible uri*/
	if (!safestrcmp(printer, IPPC_ALLPRINTERS) /*&&
	    !safestrcmp(ppath, IPPC_ALLJOBS_CUPS_PATH)*/) {
		Ipa_get_all_printcap_auth(&auths, ppath, printer, &Perm_check);
	} else {
		Select_pc_info(printer, &pc_entry, &pc_alias, &PC_names_line_list, &PC_order_line_list, &PC_info_line_list, 0, 0);
		Ipa_get_printcap_auth(&auths, &pc_entry, ppath, printer, &Perm_check);
	}
	/*construct basic auth realm name*/
	if (printer_auth_method(&auths, IPPAV_BASIC)) {
		/*basic realm will include printer name*/
		plp_snprintf(brealm, sizeof(brealm), "%s=\"%s%s\"", HTTPV_REALM, IPPC_BASIC_REALM, printer); brealm[sizeof(brealm)-1] = '\0';
	}


	ah = Find_str_value(http_headers, HTTPH_AUTHORIZATION);

	/*negotiate method: todo*/

	/*basic authentication if present (=sent in http header) and allowed for the printer*/
	if (ah) Split(&auth_parts, ah, Whitespace, 0, NULL, 0, 0, 0, NULL);
	if (ah && printer_auth_method(&auths, IPPAV_BASIC) &&
	   (auth_parts.count > 1) && !safestrcmp(auth_parts.list[0], HTTPV_BASIC)
			) {
		char *dc = NULL;
		char *pwdp = NULL;
		Base64_decode(&dc, auth_parts.list[1]);
		pwdp = strchr(dc, ':');
		if (pwdp) {
			pwdp[0] = '\0';
			pwdp++;
		}
		DEBUGF(DNW4)("Ipa_authenticate: basic, %s, user %s pass %s", auth_parts.list[1], dc, pwdp);
		rs = Ipa_check_password(dc, pwdp);
		DEBUGF(DNW4)("Ipa_authenticate: user %s, basic authentication %s", dc, rs ? "succeeded" : "failed");
		if (rs) {
			Set_str_value(auth_info, KWA_USER, dc);
			Set_str_value(auth_info, KWA_AUTHTYPE, PCV_BASIC);
			/*save realm - password is now ok, but the operation may not be permited - allow type another username/password see Ipp_send_response*/
			Set_str_value(auth_info, KWA_ALTBASIC, brealm);
#ifdef	SSL_ENABLE
			if (conn->ssl_info) {
				Set_str_value(auth_info, KWA_AUTHFROM, Find_str_value(conn->ssl_info, AUTHFROM));
				Set_str_value(auth_info, KWA_AUTHCA, Find_str_value(conn->ssl_info, AUTHCA));
			}
#endif
		}
		if (dc) free(dc);
		if (rs) goto endproc;
	}

	/* requesting-user-name;
	 * allow also this type when Basic-Authentication  password is wrong*/
	if (printer_auth_method(&auths, IPPAV_RQ_USRNAME))
	{
		a = Ipp_get_attr(ipp_request->attributes, OPERATION_ATTRIBUTES_GRP, 0, IPPAN_RQ_USRNAME, -1);
		if (a) {
			sz = a->value_len;
			if (sz>sizeof(buf)-1) sz = sizeof(buf)-1;
			memcpy(buf, a->value, sz);
			buf[sz] = '\0';
			/*reduce "username@MA-CA-DD-RE-SS" sent by Novell iPrint win clinet into "username" only */
			if ((uagent = Find_str_value(http_headers, HTTPH_USER_AGENT)) && !safestrncmp(uagent, HTTPV_UA_NOVELL, safestrlen(HTTPV_UA_NOVELL))) {
				char *at = strrchr(buf, '@');
				if (at) *at = '\0';
			}
			Set_str_value(auth_info, KWA_USER, buf);
			Set_str_value(auth_info, KWA_AUTHTYPE, PCV_USRNAME);

		} else {
			/*client did not send requested-user-name - use anonymous*/
			Set_str_value(auth_info, KWA_USER, IPPC_USER_ANONYMOUS);
			Set_str_value(auth_info, KWA_AUTHTYPE, PCV_USRNAME);
		}
#ifdef	SSL_ENABLE
		if (conn->ssl_info) {
			Set_str_value(auth_info, KWA_AUTHFROM, Find_str_value(conn->ssl_info, AUTHFROM));
			Set_str_value(auth_info, KWA_AUTHCA, Find_str_value(conn->ssl_info, AUTHCA));
		}
#endif
		/*if printer allows also stronger authentication, save this for later use - see Ipp_send_response*/
		if (printer_auth_method(&auths, IPPAV_BASIC)) {
			Set_str_value(auth_info, KWA_ALTBASIC, brealm);
		}

		rs = 1;
		goto endproc;
	}

 unauth:
	Http_send_status(conn, HTTP_VER_11, 401, HTTPM_UNAUTHORIZED);
	Http_send_date_header(conn);
	Http_send_header(conn, HTTPH_SERVER, HTTPV_LPRNGS);
	Http_send_header(conn, HTTPH_CONTENT_LENGTH, "0");
	if (printer_auth_method(&auths, IPPAV_BASIC)) {
		/*basic realm will include printer name*/
		plp_snprintf(buf, sizeof(buf), "%s=\"%s%s\"", HTTPV_REALM, IPPC_BASIC_REALM, printer); buf[sizeof(buf)-1] = '\0';
		plp_snprintf(buf, sizeof(buf), "%s %s", HTTPV_BASIC, brealm); buf[sizeof(buf)-1] = '\0';
		Http_send_header(conn, HTTPH_WWW_AUTHENTICATE, buf);
	}
	Http_send_header(conn, NULL, NULL);
	Http_skip_message_body(conn, *rest_len, http_headers);
	rs = 0;

 endproc:

	Free_line_list(&auth_parts);
	if (printer) free(printer);
	if (ppath) free (ppath);
	Free_line_list(&pc_alias);
	Free_line_list(&pc_entry);
	Free_line_list(&auths);

	return rs;
}

/*write response to connection with possibe following data*/
int Ipp_send_response(const struct http_conn *conn, struct line_list *headers, ssize_t *body_rest_len,
		      struct ipp_operation *resp_op, struct line_list *alt_info, int data_len, char *data)
{
	ssize_t len;
	struct ipp_attr *attr;
	int grp, grp_idx, l;
	char *aname, buf[LINEBUFFER], *brealm = NULL;


	/*read rest of the response - CUPS wants it before answer*/

	Http_skip_message_body(conn, *body_rest_len, headers);
	body_rest_len = 0;


	/*compute response length*/
	len = 9; /*version 2 bytes, status 2 bytes, request-id 4 bytes, ... end-of attributes tag 1 byte*/
	attr = resp_op->attributes;
	grp = -1;
	grp_idx = -1;
	aname = NULL;
	for ( ; attr; attr = attr->next) {
		if ((attr->group != grp) || (attr->group_num != grp_idx)) {
			grp = attr->group;
			grp_idx = attr->group_num;
			len ++;
			aname = NULL;
		}
		if (!(attr->name)) continue; /*empty group*/
		len++;  /*value tag*/
		len += 2; /*name length*/
		if (!aname || safestrcmp(attr->name, aname)) {  /*first (not additional) value*/
			len += strlen(attr->name);
			aname = attr->name;
		}
		len += 2 + attr->value_len;
	}

	len += data_len;

	/* if current IPP authentication failed, try stronger authentication or retyping another user/password:
	 * if IPP response code is CLIENT_NOT_AUTHENTICATED and possible authentication is basic, respond
	 * with UNAUTHORIZED status
	 * */
	if (resp_op->op_id_status == CLIENT_ERROR_NOT_AUTHENTICATED) {
		brealm = Find_str_value(alt_info, KWA_ALTBASIC);
	}

	if (brealm)
		Http_send_status(conn, HTTP_VER_11, 401, HTTPM_UNAUTHORIZED);
	else
		Http_send_status(conn, HTTP_VER_11, 200, HTTPM_OK);
	Http_send_date_header(conn);
	Http_send_header(conn, HTTPH_SERVER, HTTPV_LPRNGS);
	Http_send_header(conn, HTTPH_CACHE_CONTROL, HTTPV_NO_CACHE);
	Http_send_header(conn, HTTPH_PRAGMA, HTTPV_NO_CACHE);
	Http_send_header(conn, HTTPH_CONTENT_TYPE, HTTPV_APPLICATION_IPP);
	sprintf(buf, "%zd", len);
	Http_send_header(conn, HTTPH_CONTENT_LENGTH, buf);
	if (brealm) {
		plp_snprintf(buf, sizeof(buf), "%s %s", HTTPV_BASIC, brealm); buf[sizeof(buf)-1] = '\0';
		Http_send_header(conn, HTTPH_WWW_AUTHENTICATE, buf);
	}
	Http_send_header(conn, NULL, NULL);

	if ((!resp_op->version) || (resp_op->version >= 11)) {
		buf[0] = 1;
		buf[1] = 1;
	} else {
		buf[0] = 1; /*IPP version maior*/
		buf[1] = 0; /*version minor*/
	}
	hton16(&(buf[2]), resp_op->op_id_status);
	hton32(&(buf[4]), resp_op->request_id);
	(conn->iofunc->write_conn_len_timeout)(conn, buf, 8);

	DEBUGF(DNW4)("Ipp_send_response: IPP version %1.1f, status %.2X, request-id %ld",
	              resp_op->version * 0.1, resp_op->op_id_status, resp_op->request_id);

	attr = resp_op->attributes;
	grp = -1;
	grp_idx = -1;
	aname = NULL;
	for ( ; attr; attr = attr->next) {
		if ((attr->group != grp) || (attr->group_num != grp_idx)) {
			grp = attr->group;
			grp_idx = attr->group_num;
			buf[0] = attr->group & 0xff;
			(conn->iofunc->write_conn_len_timeout)(conn, &buf[0], 1);
			DEBUGF(DNW4)("Ipp_send_response: group 0x%.2X", buf[0]);
			aname = NULL;
		}
		if (!(attr->name)) continue; /*empty group*/
		buf[0] = attr->type & 0xff; /*value tag*/
		if (!aname || safestrcmp(attr->name, aname)) {  /*first (not additional) value*/
			hton16(&(buf[1]), strlen(attr->name));
			(conn->iofunc->write_conn_len_timeout)(conn, buf, 3);
			(conn->iofunc->write_conn_len_timeout)(conn, attr->name, strlen(attr->name));
			aname = attr->name;
		} else {
			hton16(&(buf[1]), 0);                  /*set of values*/
			(conn->iofunc->write_conn_len_timeout)(conn, buf, 3);
		}
		hton16(&(buf[0]), attr->value_len);
		(conn->iofunc->write_conn_len_timeout)(conn, buf, 2);
		(conn->iofunc->write_conn_len_timeout)(conn, attr->value, attr->value_len);
		/*debug print*/
		if (DEBUGFSET(DNW4)) {
			switch(attr->type) {
				case IPPDT_BOOLEAN:
					 l = (int)sizeof(buf) < 6 ? (int)sizeof(buf) : 6;
					 plp_snprintf(buf, sizeof(buf), "%s", ((char)(*(attr->value)) ? "true" : "false") );
					 break;
				case IPPDT_INTEGER:
				case IPPDT_ENUM:
					 l = (int)sizeof(buf) < 12 ? (int)sizeof(buf) : 12;
					 plp_snprintf(buf, sizeof(buf), "%d", ntoh32(attr->value) & 0xffffffff);
					 break;
				default: l = (int)sizeof(buf) < attr->value_len ? (int)sizeof(buf) : attr->value_len;
					 strncpy(buf, attr->value ? (attr->value) : "(NULL)", l);
			}
			if (l < (int)sizeof(buf)) buf[l] = '\0'; else buf[sizeof(buf) - 1] = '\0';
			LOGDEBUG("Ipp_send_response: name %s, type 0x%.2X, index %d, len %d value %s",
		                  attr->name, attr->type, attr->value_index, attr->value_len, buf );
		}

	}

	buf[0] = END_OF_ATTRIBUTES_TAG;
	(conn->iofunc->write_conn_len_timeout)(conn, buf, 1);
	DEBUGF(DNW4)("Ipp_send_response: End of attributes");

	if (data) (conn->iofunc->write_conn_len_timeout)(conn, data, data_len);

	return len;

}

/*IO routines*/
static int plain_read_conn_len_timeout(const struct http_conn *conn, char *msg, ssize_t len)
{
	return Read_fd_len_timeout(conn->timeout, conn->fd, msg, len);
	/*int rs= Read_fd_len_timeout(conn->timeout, conn->fd, msg, len);
	if (len==1) fprintf(stderr, "%c", msg[0]);
	return rs;*/
}
static int plain_write_conn_len_timeout(const struct http_conn *conn, const char *msg, int len)
{
	return Write_fd_len_timeout(conn->timeout, conn->fd, msg, len);
}

struct http_io plain_io = {
	plain_read_conn_len_timeout,
	plain_write_conn_len_timeout,
};

#ifdef	SSL_ENABLE
static int ssl_read_conn_len_timeout(const struct http_conn *conn, char *msg, ssize_t len)
{
	char errmsg[LARGEBUFFER];
	int l = len;
	int rs;
	rs = Read_SSL_connection(conn->timeout, conn->ssl, msg, &l, errmsg, sizeof(errmsg));
	switch (rs) {
		case 0: return l;
			break;
		case 1: return 0;
			break;
		default:return -1;
			break;
	}
}
static int ssl_write_conn_len_timeout(const struct http_conn *conn, const char *msg, int len)
{
	char errmsg[LARGEBUFFER];
	int rs = Write_SSL_connection(conn->timeout, conn->ssl, (char *)msg, len, errmsg, len);
	if (!rs)
		return 0;
	else
		return -1;
}

struct http_io ssl_io = {
	ssl_read_conn_len_timeout,
	ssl_write_conn_len_timeout,
};

/*test protocol in http header value*/
const char *Headerproto = " \t,";
static int Sec_proto(const char *s, const char *proto)
{
	struct line_list l;
	int rs = 0;
	Init_line_list(&l);
	Split(&l, s, Headerproto, 0, 0, 0, 0, 0, 0);
	for (rs = l.count; (rs > 0) && safestrcmp(l.list[rs - 1], proto); rs--);
	Free_line_list(&l);
	return rs;
}

static int Ssl_switch(struct http_conn *conn)
{

	char errmsg[LARGEBUFFER];
	int rs = 0;

	if (conn->ctx) return 1;

	conn->ssl_info = malloc_or_die(sizeof(struct line_list), __FILE__, __LINE__);
	Init_line_list(conn->ssl_info);
	if (SSL_Initialize_ctx(&(conn->ctx), errmsg, sizeof(errmsg))) {
		rs = 1;
		DEBUGF(DNW4)("Ssl_switch: SSL_initialize_ctx err %s", errmsg);
		goto tend;
	}

	/*accept only TLS 1.0*/
	/*if (conn->port == ipp_ippport)*/
		SSL_CTX_set_options(conn->ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);

	if (Accept_SSL_connection(conn->fd, conn->timeout, conn->ctx, &(conn->ssl),
			conn->ssl_info, errmsg, sizeof(errmsg))) {
		DEBUGF(DNW4)("Ssl_switch: Accept_SSL_connection err %s", errmsg);
		rs = 2;
		goto tend;

	}

	if (!Find_str_value(conn->ssl_info, AUTHCA)) {
		if (conn->port == ipp_ippsport)
			Set_str_value(conn->ssl_info, AUTHCA, SSL_AUTHCA_DIRECT);
		else
			Set_str_value(conn->ssl_info, AUTHCA, SSL_AUTHCA_UPGRADE);
	}

	DEBUGFC(DNW4)Dump_line_list("Ssl_switch: info", conn->ssl_info);
	conn->iofunc = &ssl_io;
tend:
	return rs;
}

static int Ssl_close(struct http_conn *conn)
{

	if (conn->ssl) {
		Close_SSL_connection(conn->fd, conn->ssl);
		SSL_free(conn->ssl);
	}
	if (conn->ctx) Destroy_ctx(conn->ctx);
	if (conn->ssl_info) {
		Free_line_list(conn->ssl_info);
		free(conn->ssl_info);
	}
	return 0;
}

#endif

