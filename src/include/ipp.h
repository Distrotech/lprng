/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * IPP internet printing protocol
 * copyright 2008 Vaclav Michalek
 *
 * See LICENSE for conditions of use.
 ***************************************************************************/


#ifndef _IPP_H_
#define _IPP_H_

#ifdef	SSL_ENABLE
#include <openssl/ssl.h>
#endif

/* CONST MACROS*/
/*HTTP*/
/*EXTERN const char * HTTP_CRLF			DEFINE ( = "\r\n");*/
EXTERN const char * HTTP_CRLF			DEFINE ( = "\r\n");
EXTERN const char * HTTP_VER_11			DEFINE ( = "HTTP/1.1");
EXTERN const char * HTTP_GET			DEFINE ( = "GET");
EXTERN const char * HTTP_HEAD			DEFINE ( = "HEAD");
EXTERN const char * HTTP_OPTIONS		DEFINE ( = "OPTIONS");
EXTERN const char * HTTP_POST			DEFINE ( = "POST");
#ifdef	SSL_ENABLE
EXTERN const char * HTTP_TLS_10			DEFINE ( = "TLS/1.0");
#endif

EXTERN const char * HTTPM_OK			DEFINE ( = "OK");
EXTERN const char * HTTPM_BAD_REQUEST		DEFINE ( = "Bad request");
EXTERN const char * HTTPM_BAD_METHOD		DEFINE ( = "Method not allowed");
EXTERN const char * HTTPM_TE_UNIMPLEMENTED	DEFINE ( = "Transfer encoding unimplemented");
EXTERN const char * HTTPM_NOT_ACCEPTABLE	DEFINE ( = "Not acceptable");
EXTERN const char * HTTPM_EXPECTATION_FAILED	DEFINE ( = "Expectation failed");
EXTERN const char * HTTPM_CONTINUE		DEFINE ( = "Continue");
EXTERN const char * HTTPM_NOT_FOUND		DEFINE ( = "Not found");
EXTERN const char * HTTPM_UNAUTHORIZED		DEFINE ( = "Unauthorized");
EXTERN const char * HTTPM_FORBIDDEN		DEFINE ( = "Forbidden");
#ifdef	SSL_ENABLE
EXTERN const char * HTTPM_SWITCHING_PROTOCOLS	DEFINE ( = "Switching protocols");
#endif

EXTERN const char * HTTPH_ACCEPT		DEFINE ( = "Accept");
EXTERN const char * HTTPV_APPLICATION_IPP	DEFINE ( = "application/ipp");
EXTERN const char * HTTPH_ACCEPT_ENCODING	DEFINE ( = "Accept-Encoding");
EXTERN const char * HTTPH_AUTHORIZATION		DEFINE ( = "Authorization");
EXTERN const char * HTTPV_BASIC			DEFINE ( = "Basic");
EXTERN const char * HTTPV_REALM			DEFINE ( = "realm");
EXTERN const char * HTTPH_EXPECT		DEFINE ( = "Expect");
EXTERN const char * HTTPV_100_CONTINUE		DEFINE ( = "100-continue");
EXTERN const char * HTTPV_IDENTITY		DEFINE ( = "identity");
EXTERN const char * HTTPH_ALLOW			DEFINE ( = "Allow");
EXTERN const char * HTTPH_CACHE_CONTROL		DEFINE ( = "Cache-Control");
EXTERN const char * HTTPV_NO_CACHE		DEFINE ( = "no-cache");
EXTERN const char * HTTPH_CONNECTION		DEFINE ( = "Connection");
EXTERN const char * HTTPV_CLOSE			DEFINE ( = "close");
EXTERN const char * HTTPH_CONTENT_ENCODING	DEFINE ( = "Content-Encoding");
EXTERN const char * HTTPH_CONTENT_LENGTH	DEFINE ( = "Content-Length");
EXTERN const char * HTTPH_CONTENT_TYPE		DEFINE ( = "Content-Type");
EXTERN const char * HTTPH_DATE			DEFINE ( = "Date");
EXTERN const char * HTTPH_HOST			DEFINE ( = "Host");
EXTERN const char * HTTPH_PRAGMA		DEFINE ( = "Pragma");
EXTERN const char * HTTPH_SERVER		DEFINE ( = "Server");
#define 	    HTTPV_LPRNGS		  	    Version
EXTERN const char * HTTPH_TRANSFER_ENCODING	DEFINE ( = "Transfer-Encoding");
EXTERN const char * HTTPV_CHUNKED		DEFINE ( = "chunked");
EXTERN const char * HTTPH_TRAILER		DEFINE ( = "Trailer");
EXTERN const char * HTTPH_UPGRADE		DEFINE ( = "Upgrade");
EXTERN const char * HTTPH_USER_AGENT		DEFINE ( = "User-Agent");
EXTERN const char * HTTPH_WWW_AUTHENTICATE	DEFINE ( = "WWW-Authenticate");

EXTERN const char * HTTPV_UA_MS			DEFINE ( = "Internet Print Provider");
EXTERN const char * HTTPV_UA_NOVELL		DEFINE ( = "Novell iPrint Client");

/*IPP encoding values, groups and types*/
#define	END_OF_ATTRIBUTES_TAG			0x03
#define OPERATION_ATTRIBUTES_GRP		0x01
#define JOB_ATTRIBUTES_GRP			0x02
#define PRINTER_ATTRIBUTES_GRP			0x04
#define UNSUPPORTED_ATTRIBUTES_GRP		0x05
#define SUBSCRIPTION_ATTRIBUTES_GRP		0x06
#define EVENT_NOTIFICATION_ATTRIBUTES_GRP	0x07
#define IPPDT_UNSUPPORTED			0x10
#define IPPDT_UNKNOWN				0x12
#define IPPDT_NO_VALUE				0x13
#define IPPDT_NOT_SETTABLE			0x15
#define IPPDT_DELETE				0x16
#define IPPDT_INTEGER				0x21
#define IPPDT_BOOLEAN				0x22
#define IPPDT_ENUM				0x23
#define IPPDT_OCTET_STRING			0x30
#define IPPDT_RANGE_INT				0x33
#define IPPDT_TEXT_WITHOUT_LANG			0x41
#define IPPDT_NAME_WITHOUT_LANG			0x42
#define IPPDT_KEYWORD				0x44
#define IPPDT_URI				0x45
#define IPPDT_CHARSET				0x47
#define IPPDT_NAT_LANG				0x48
#define IPPDT_MIME_MEDIA_TYPE			0x49

/*IPP status values*/
#define	SUCCESSFUL_OK				0x0000
#define SUCCESSFUL_OK_IGNORED_SUBSTITUED	0x0001
#define	SUCCESSFUL_OK_TOO_MANY_EVENTS		0x0005
#define	CLIENT_ERROR_BAD_REQUEST		0x0400
#define	CLIENT_ERROR_NOT_AUTHENTICATED		0x0402
#define	CLIENT_ERROR_NOT_POSSIBLE		0x0404
#define CLIENT_ERROR_NOT_FOUND			0x0406
#define CLIENT_ERROR_REQUEST_ENTITY_TOO_LARGE	0x0408
#define CLIENT_ERROR_REQUEST_VALUE_TOO_LONG	0x0409
#define CLIENT_ERROR_ATTRS_OR_VALS_NOT_SUPPORTED	0x040b
#define CLIENT_ERROR_CHARSET_NOT_SUPPORTED	0x040d
#define CLIENT_ERROR_COMPRESSION_NOT_SUPPORTED	0x040f
#define CLIENT_ERROR_ATTRIBUTES_NOT_SETTABLE	0x0413
#define CLIENT_ERROR_TOO_MANY_SUBSCRIPTIONS	0x0415
#define SERVER_ERROR_INTERNAL_ERROR		0x0500
#define SERVER_ERROR_OPERATION_NOT_SUPPORTED	0x0501
#define SERVER_ERROR_TEMPORARY_ERROR		0x0505
#define SERVER_ERROR_NOT_ACCEPTING_JOBS		0x0506
#define SERVER_ERROR_BUSY			0x0507

/*IPP attribute names*/
EXTERN const char * IPPAN_CHARSET		DEFINE ( = "attributes-charset");
EXTERN const char * IPPAN_NAT_LANG		DEFINE ( = "attributes-natural-language");
EXTERN const char * IPPAN_AUTH_INFO_REQUIRED	DEFINE ( = "auth-info-required");
EXTERN const char * IPPAN_CHARSET_CONFIGURED	DEFINE ( = "charset-configured");
EXTERN const char * IPPAN_CHARSET_SUPPORTED	DEFINE ( = "charset-supported");
EXTERN const char * IPPAN_COMPRESSION		DEFINE ( = "compression");
EXTERN const char * IPPAN_COMPRESSION_SUPPORTED	DEFINE ( = "compression-supported");
EXTERN const char * IPPAN_COPIES		DEFINE ( = "copies");
EXTERN const char * IPPAN_COPIES_DEFAULT	DEFINE ( = "copies-default");
EXTERN const char * IPPAN_COPIES_SUPPORTED	DEFINE ( = "copies-supported");
EXTERN const char * IPPAN_DETAILED_STATUS	DEFINE ( = "detailed-status-message");
EXTERN const char * IPPAN_DOC_FORMAT		DEFINE ( = "document-format");
EXTERN const char * IPPAN_DOC_FORMAT_DEFAULT	DEFINE ( = "document-format-default");
EXTERN const char * IPPAN_DOC_FORMAT_SUPPORTED	DEFINE ( = "document-format-supported");
EXTERN const char * IPPAN_DOC_NAME		DEFINE ( = "document-name");
EXTERN const char * IPPAN_FIRST_JOB_ID		DEFINE ( = "first-job-id");
EXTERN const char * IPPAN_NL_SUPPORTED		DEFINE ( = "generated-natural-language-supported");
EXTERN const char * IPPAN_IPP_VER_SUPPORTED	DEFINE ( = "ipp-versions-supported");
EXTERN const char * IPPAN_IPPGET_EVENT_LIFE	DEFINE ( = "ippget-event-life");
EXTERN const char * IPPAN_JOB_DETAILED_STATUS	DEFINE ( = "job-detailed-status-messages");
EXTERN const char * IPPAN_JOB_HOLD_UNTIL	DEFINE ( = "job-hold-until");
EXTERN const char * IPPAN_JOB_HOLD_UNTIL_D	DEFINE ( = "job-hold-until-default");
EXTERN const char * IPPAN_JOB_HOLD_UNTIL_SUPP	DEFINE ( = "job-hold-until-supported");
EXTERN const char * IPPAN_JOB_ID		DEFINE ( = "job-id");
EXTERN const char * IPPAN_JOB_KOCTETS		DEFINE ( = "job-k-octets");
EXTERN const char * IPPAN_JOB_KOCTETS_PROCESSED DEFINE ( = "job-k-octets-processed");
EXTERN const char * IPPAN_JOB_NAME		DEFINE ( = "job-name");
EXTERN const char * IPPAN_ORIG_USERNAME		DEFINE ( = "job-originating-user-name");
EXTERN const char * IPPAN_JOB_PRINTER_UP_TIME	DEFINE ( = "job-printer-up-time");
EXTERN const char * IPPAN_JOB_PRINTER_URI	DEFINE ( = "job-printer-uri");
EXTERN const char * IPPAN_JOB_PRI		DEFINE ( = "job-priority");
EXTERN const char * IPPAN_JOB_PRI_DEFAULT	DEFINE ( = "job-priority-default");
EXTERN const char * IPPAN_JOB_PRI_SUPPORTED	DEFINE ( = "job-priority-supported");
EXTERN const char * IPPAN_JOB_SETTABLE_ATTRS_S	DEFINE ( = "job-settable-attributes-supported");
EXTERN const char * IPPAN_JOB_STATE		DEFINE ( = "job-state");
EXTERN const char * IPPAN_JOB_SREASONS		DEFINE ( = "job-state-reasons");
EXTERN const char * IPPAN_JOB_URI		DEFINE ( = "job-uri");
EXTERN const char * IPPAN_LAST_DOCUMENT		DEFINE ( = "last-document");
EXTERN const char * IPPAN_LIMIT			DEFINE ( = "limit");
EXTERN const char * IPPAN_MULT_DOC_HNDL		DEFINE ( = "multiple-document-handling");
EXTERN const char * IPPAN_MULT_DOC_HNDL_DEFAULT	DEFINE ( = "multiple-document-handling-default");
EXTERN const char * IPPAN_MULT_DOC_HNDL_SUPP	DEFINE ( = "multiple-document-handling-supported");
EXTERN const char * IPPAN_MULT_DOC_JOBS_SUPP	DEFINE ( = "multiple-document-jobs-supported");
EXTERN const char * IPPAN_MULT_OP_TIMEOUT	DEFINE ( = "multiple-operation-timeout");
EXTERN const char * IPPAN_MY_JOBS		DEFINE ( = "my-jobs");
EXTERN const char * IPPAN_MY_SUBSCRIPTIONS	DEFINE ( = "my-subscriptions");
EXTERN const char * IPPAN_NL_CONFIGURED		DEFINE ( = "natural-language-configured");
EXTERN const char * IPPAN_NTF_CHARSET		DEFINE ( = "notify-charset");
EXTERN const char * IPPAN_NTF_EVENTS		DEFINE ( = "notify-events");
EXTERN const char * IPPAN_NTF_EVENTS_DEFAULT	DEFINE ( = "notify-events-default");
EXTERN const char * IPPAN_NTF_EVENTS_SUPPORTED	DEFINE ( = "notify-events-supported");
EXTERN const char * IPPAN_NTF_GET_INTERVAL	DEFINE ( = "notify-get-interval");
EXTERN const char * IPPAN_NTF_JOB_ID		DEFINE ( = "notify-job-id");
EXTERN const char * IPPAN_NTF_LEASE_DURATION	DEFINE ( = "notify-lease-duration");
EXTERN const char * IPPAN_NTF_LEASE_DURATION_D	DEFINE ( = "notify-lease-duration-default");
EXTERN const char * IPPAN_NTF_LEASE_DURATION_S	DEFINE ( = "notify-lease-duration-supported");
EXTERN const char * IPPAN_NTF_LEASE_EXPIRATION	DEFINE ( = "notify-lease-expiration-time");
EXTERN const char * IPPAN_NTF_MAX_EVENTS_SUPP	DEFINE ( = "notify-max-events-supported");
EXTERN const char * IPPAN_NTF_NAT_LANG		DEFINE ( = "notify-natural-language");
EXTERN const char * IPPAN_NTF_PRINTER_UP_TIME	DEFINE ( = "notify-printer-up-time");
EXTERN const char * IPPAN_NTF_PRINTER_URI	DEFINE ( = "notify-printer-uri");
EXTERN const char * IPPAN_NTF_PULL_METHOD	DEFINE ( = "notify-pull-method");
EXTERN const char * IPPAN_NTF_PULL_METHOD_SUPP	DEFINE ( = "notify-pull-method-supported");
EXTERN const char * IPPAN_NTF_STATUS_CODE	DEFINE ( = "notify-status-code");
EXTERN const char * IPPAN_NTF_SEQUENCE_NUMBER	DEFINE ( = "notify-sequence-number");
EXTERN const char * IPPAN_NTF_SEQUENCE_NUMBERS	DEFINE ( = "notify-sequence-numbers");
EXTERN const char * IPPAN_NTF_SUBSCRIBED_EVENT	DEFINE ( = "notify-subscribed-event");
EXTERN const char * IPPAN_NTF_SUBSCRIBER_USERN	DEFINE ( = "notify-subscriber-user-name");
EXTERN const char * IPPAN_NTF_SUBSCRIPTION_ID	DEFINE ( = "notify-subscription-id");
EXTERN const char * IPPAN_NTF_SUBSCRIPTION_IDS	DEFINE ( = "notify-subscription-ids");
EXTERN const char * IPPAN_NTF_TEXT		DEFINE ( = "notify-text");
EXTERN const char * IPPAN_NTF_USER_DATA		DEFINE ( = "notify-user-data");
EXTERN const char * IPPAN_NTF_WAIT		DEFINE ( = "notify-wait");
EXTERN const char * IPPAN_OPERATIONS_SUPPORTED	DEFINE ( = "operations-supported");
EXTERN const char * IPPAN_PDL_OVRD_SUPPORTED 	DEFINE ( = "pdl-override-supported");
EXTERN const char * IPPAN_PRINTER_INFO		DEFINE ( = "printer-info");
EXTERN const char * IPPAN_PRINTER_JOB_ACCEPTING	DEFINE ( = "printer-is-accepting-jobs");
EXTERN const char * IPPAN_PRINTER_LOCATION	DEFINE ( = "printer-location");
EXTERN const char * IPPAN_PRINTER_MAKE_MODEL	DEFINE ( = "printer-make-and-model");
EXTERN const char * IPPAN_PRINTER_NAME		DEFINE ( = "printer-name");
EXTERN const char * IPPAN_PRINTER_STATE		DEFINE ( = "printer-state");
EXTERN const char * IPPAN_PRINTER_SREASONS	DEFINE ( = "printer-state-reasons");
EXTERN const char * IPPAN_PRINTER_TYPE		DEFINE ( = "printer-type");
EXTERN const char * IPPAN_PRINTER_UP_TIME	DEFINE ( = "printer-up-time");
EXTERN const char * IPPAN_PRINTER_URI		DEFINE ( = "printer-uri");
EXTERN const char * IPPAN_PRINTER_URI_SUPPORTED	DEFINE ( = "printer-uri-supported");
EXTERN const char * IPPAN_PURGE_JOB		DEFINE ( = "purge-job");
EXTERN const char * IPPAN_QUEUED_JOBS		DEFINE ( = "queued-job-count");
EXTERN const char * IPPAN_REQUESTED_ATTRIBUTES	DEFINE ( = "requested-attributes");
EXTERN const char * IPPAN_RQ_USRNAME		DEFINE ( = "requesting-user-name");
EXTERN const char * IPPAN_TIME_AT_CREATION	DEFINE ( = "time-at-creation");
EXTERN const char * IPPAN_TIME_AT_PROCESSING	DEFINE ( = "time-at-processing");
EXTERN const char * IPPAN_TIME_AT_COMPLETED	DEFINE ( = "time-at-completed");
EXTERN const char * IPPAN_URI_AUTH_SUPPORTED	DEFINE ( = "uri-authentication-supported");
EXTERN const char * IPPAN_URI_SEC_SUPPORTED	DEFINE ( = "uri-security-supported");
EXTERN const char * IPPAN_USER_RIGHTS		DEFINE ( = "user-rights");
EXTERN const char * IPPAN_WHICH_JOBS		DEFINE ( = "which-jobs");


/*IPP values*/
EXTERN const char * IPPAV_UTF_8			DEFINE ( = "utf-8");
EXTERN const char * IPPAV_EN_US			DEFINE ( = "en-us");
EXTERN const char * IPPAV_ALL			DEFINE ( = "all");
EXTERN const char * IPPAV_JOB_TEMPLATE		DEFINE ( = "job-template");
EXTERN const char * IPPAV_PRINTER_DESCRIPTION	DEFINE ( = "printer-description");
EXTERN const char * IPPAV_JOB_DESCRIPTION	DEFINE ( = "job-description");
EXTERN const char * IPPAV_SUBSCRIPTION_TEMPLATE	DEFINE ( = "subscription-template");
EXTERN const char * IPPAV_SUBSCRIPTION_DESCRIPT	DEFINE ( = "subscription-description");
EXTERN const char * IPPAV_RQ_USRNAME		DEFINE ( = "requesting-user-name");
EXTERN const char * IPPAV_BASIC			DEFINE ( = "basic");
#ifdef	SSL_ENABLE
EXTERN const char * IPPAV_SEC_TLS		DEFINE ( = "tls");
#endif
EXTERN const char * IPPAV_SEC_NONE		DEFINE ( = "none");
EXTERN const char * IPPAV_VER_11		DEFINE ( = "1.1");
EXTERN const char * IPPAV_COMPLETED		DEFINE ( = "completed");        /*used for get-jobs with which-jobs*/
EXTERN const char * IPPAV_NOT_COMPLETED		DEFINE ( = "not-completed");    /*which-jobs*/
/*printer states*/
#define		IPPAV_PRS_IDLE			3
#define		IPPAV_PRS_PROCESSING		4
#define		IPPAV_PRS_STOPPED		5
EXTERN const char * IPPAV_PRSR_NONE		DEFINE ( = "none");
EXTERN const char * IPPAV_PRSR_PAUSED		DEFINE ( = "paused");
EXTERN const char * IPPAV_APP_OCTET_STREAM	DEFINE ( = "application/octet-stream");
EXTERN const char * IPPAV_APP_CUPS_RAW		DEFINE ( = "application/vnd.cups-raw");
EXTERN const char * IPPAV_NOT_ATTEMPTED		DEFINE ( = "not-attempted");
EXTERN const char * IPPAV_COMPR_NONE		DEFINE ( = "none");
EXTERN const char * IPPAV_AUTH_NONE		DEFINE ( = "none");		/*used for auth-info-required*/
/*job states*/
#define 	IPPAV_JRS_PENDING		3
#define 	IPPAV_JRS_HELD			4
#define 	IPPAV_JRS_PROCESSING		5
#define 	IPPAV_JRS_STOPPED		6
#define 	IPPAV_JRS_CANCELED		7
#define 	IPPAV_JRS_ABORTED		8
#define 	IPPAV_JRS_COMPLETED		9

EXTERN const char * IPPAV_JSR_NONE		DEFINE ( = "none");
EXTERN const char * IPPAV_SDC_COLLATED		DEFINE ( = "multiple-documents-collated-copies");

/*subscriptions values*/
EXTERN const char * IPPAV_IPPGET		DEFINE ( = "ippget");
EXTERN const char * IPPAV_EVENT_NONE		DEFINE ( = "none");
EXTERN const char * IPPAV_EVENT_PR_STATECHANGE	DEFINE ( = "printer-state-changed");
EXTERN const char * IPPAV_EVENT_PR_STOPPED	DEFINE ( = "printer-stopped");
EXTERN const char * IPPAV_EVENT_JOB_STATECHANGE	DEFINE ( = "job-state-changed");
EXTERN const char * IPPAV_EVENT_JOB_CREATED	DEFINE ( = "job-created");
EXTERN const char * IPPAV_EVENT_JOB_COMPLETED	DEFINE ( = "job-completed");

/*some implementation values*/
#define		IPPC_URI_LEN			1023
#define		IPPC_SCHEME_HTTP		"http"
#define		IPPC_SCHEME_HTTPS		"https"
#define		IPPC_SCHEME_IPP			"ipp"
#define		IPPC_PRINTERS_PATH		"printers"
#define		IPPC_PRINTERS_PATH_RAW		"raw"
#define		IPPC_JOBS_PATH			"jobs"			/*standard path scheme://host:port/printers|raw/printername/JOBS/jobnum*/
#define		IPPC_BASIC_REALM		"Lprng_"		/*prefix for realm name of basic authentication*/
#define		IPPC_USER_ANONYMOUS		"anonymous"		/*default user name if not known from authentication*/
#define		IPPC_PAM_SERVICE		"lprng"			/*service name for PAM WWW Basic authentication*/
#define		IPPC_ALLJOBS_CUPS_PATH		"jobs"			/*CUPS ipp://lofalhost/JOBS*/
#define		IPPC_ALLPRINTERS		"all"			/*internal all-printers sign*/
#define		IPPC_NTF_ALLPRINTERS_CUPS_URI	"/"			/*all-printers notification uri*/
#define		IPPC_CUPS_PPD_PATH		"printers"              /*path of CUPS http GET /printers/printer.ppd request*/
#define		IPPC_CUPS_PPD_EXT		".ppd"			/*extension of CUPS http GET /printers/printer.ppd */


/*ipp-specific printcap entries*/
#define		PC_AUTH				"iauth"                 /*IPP authentication printcap option*/
#define		PCV_USRNAME			"usrname"               /*printcap */
#define		PCV_BASIC			"basic"
#define		PC_IPPD				"ppd"			/*ppd file for IPP printcap option*/
#define		PC_IMC				"imct"			/*copies handling*/
#define		PCV_IMC_IF			"if"			/*copies handling by lpd*/

EXTERN const char * ILPC_HOLD			DEFINE ( = "hold");     /*hold action name for IPP job control  permission handling */
EXTERN const char * ILPC_RELEASE		DEFINE ( = "release");  /*release action name for IPP job control  permission handling */
EXTERN const char * ILPC_RESTART		DEFINE ( = "redo");     /*redo action name for IPP job control  permission handling */
EXTERN const char * ILPC_CHATTR_JOB_PRI		DEFINE ( = "chattr-job-priority");     /*change job priority via IPP */
EXTERN const char * ILPC_MOVE			DEFINE ( = "move");     /*move job action name for IPP job control  permission handling */

EXTERN const char * IPPAV_NO_HOLD		DEFINE ( = "no-hold"); 	/*job-hold-until*/
EXTERN const char * IPPAV_INDEFINITE		DEFINE ( = "indefinite");


/*keys for auth_info line list see Ipa_authenticate*/
#define 	KWA_PPATH			"PPATH"
#define		KWA_PRINTER			"PRINTER"
#define		KWA_USER			"USERNAME"
#define		KWA_AUTHTYPE			"AUTHTYPE"
#define		KWA_AUTHFROM			"AUTHFROM"
#define		KWA_AUTHCA			"AUTHCA"
#define		KWA_ALTBASIC			"ALTBASIC"
#ifdef	SSL_ENABLE
#define		SSL_AUTHCA_DIRECT		"[SSL-D]"		/*AUTHCA permission field by direct HTTPS with no client certificate*/
#define		SSL_AUTHCA_UPGRADE		"[SSL-U]"		/*AUTHCA permission field by HTTP upgraded to SSL, no client certificate*/
#endif


/*auth scope*/
#define AUTHS_NONE			0
#define AUTHS_PRINTER			1	/*required printer-uri*/
#define AUTHS_JOB			2	/*required job-uri or printer-uri & job-id*/
#define AUTHS_ALL_PRINTERS		3	/*cups get-all jobs compatibilty*/
#define	AUTHS_SUBSCRIPTION		4	/*required printer-uri & notify-subscription-id*/
#define	AUTHS_SUBSCRIPTION_PRINTERS	5	/*cups all-printers subscriptions compatibility*/
#define	AUTHS_SUBSCRIPTION_ATTRIBUTES	6	/*cups all-printers subscriptions compatibility with possible notify-job-id*/
#define	AUTHS_PRINTER_OR_JOB		7	/*required printer or job*/
#define	AUTHS_DEST_PRINTER		8	/*used to decode destination printer fo CUPS-Move-Job operation*/



/* DATATYPES */

/*attribute-value*/

struct ipp_attr {
	int group;
	int group_num;  /*group index for Get-Job response*/
	char *name;
	int type;
	int value_index;
	int value_len;
	char *value;
	struct ipp_attr *next;
};

struct ipp_operation {
	int version;
	int op_id_status;
	long int request_id;
	struct ipp_attr *attributes;
};


/*http connection I/O functions, structures etc*/
struct http_conn;
typedef int (*proc_read_conn_len_timeout)(const struct http_conn *conn, char *msg, ssize_t len);
typedef int (*proc_write_conn_len_timeout)(const struct http_conn *conn, const char *msg, int len);

struct http_io {
	proc_read_conn_len_timeout read_conn_len_timeout;
	proc_write_conn_len_timeout write_conn_len_timeout;
};

struct http_conn {
	int timeout;
	int fd;		/*socket fd*/
	int port;
#ifdef	SSL_ENABLE
	SSL_CTX *ctx;
	SSL *ssl;
	struct line_list *ssl_info;
#endif
	struct http_io *iofunc;
};

/* GLOBAL VARIABLES*/

EXTERN long int ipp_ippport  DEFINE(= -1);   /* TCP port number IPP service is bound to */
#ifdef	SSL_ENABLE
EXTERN long int ipp_ippsport DEFINE(= -1);   /* TCP port number IPPS(https) service is bound to */
EXTERN int ipp_ssl_available DEFINE(= 0);    /* SSL available */
#endif


/* PROTOTYPES */

/*macros to convert IPP integer(char array) to machine integer and back (endianity)*/
#define	ntoh16(X)	(*((char*)(X)) << 8 | *(((char*)(X))+1))
#define	ntoh32(X)	((((char*)(X))[0] & 0xff) << 24 | (((char*)(X))[1] & 0xff) << 16 | (((char*)(X))[2] & 0xff) << 8 | (((char*)(X))[3] & 0xff))

#define	hton16(b,X)	((*((char*)(b)) = ((X) >> 8) & 0xff), (*(((char*)(b))+1) = (X) & 0xff) )
#define	hton32(b,X)	((*((char*)(b)) = ((X) >> 24) & 0xff), (*(((char*)(b))+1) = ((X) >> 16) & 0xff), (*(((char*)(b))+2) =((X) >> 8) & 0xff), (*(((char*)(b))+3) = (X) & 0xff) )


/*Ipp options chcek*/
void Ipp_check_options(void);

/*lprng IPP service*/
void Service_ipp (int talk, int port, const char *from_addr);

/*http functions*/
ssize_t Http_content_length(struct line_list *headers);
inline ssize_t Http_read_body(const struct http_conn *conn, ssize_t content_len, ssize_t *rest_len, char *buf, ssize_t count, struct line_list *headers);

/*functions for ipp_operation/attributes list*/
int Ipp_init_operation(struct ipp_operation *operation);
int Ipp_free_operation(struct ipp_operation *operation);
struct ipp_attr *Ipp_get_attr(const struct ipp_attr *attributes, int group, int group_index, const char *name, int index);
struct ipp_attr *Ipp_set_attr(struct ipp_attr **attributes, int group, int group_index, const char *name, int type, int index, const void *value, int value_len);

/*send ipp response*/
int Ipp_send_response(const struct http_conn *conn, struct line_list *headers, ssize_t *body_rest_len,
                      struct ipp_operation *resp_op, struct line_list *alt_info, int data_len, char *data);


#endif
