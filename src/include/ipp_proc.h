/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * IPP internet printing protocol
 * copyright 2008 Vaclav Michalek
 *
 * See LICENSE for conditions of use.
 ***************************************************************************/


#ifndef _IPP_PROC_H_
#define _IPP_PROC_H_

#include "linelist.h"
#include "ipp.h"

/*IPP operations*/

#define IPPOP_PRINT_JOB			0x0002
#define IPPOP_VALIDATE_JOB		0x0004
#define IPPOP_CREATE_JOB		0x0005
#define IPPOP_SEND_DOCUMENT		0x0006
#define IPPOP_CANCEL_JOB		0x0008
#define IPPOP_GET_JOB_ATTRIBUTES	0x0009
#define IPPOP_GET_JOBS			0x000a
#define IPPOP_GET_PRINTER_ATTRIBUTES	0x000b
#define IPPOP_HOLD_JOB			0x000c
#define IPPOP_RELEASE_JOB		0x000d
#define IPPOP_RESTART_JOB		0x000e
#define IPPOP_CHANGE_JOB_ATTRIBUTES	0x0014

#define IPPOP_CUPS_GET_DEFAULT		0x4001
#define IPPOP_CUPS_GET_PRINTERS		0x4002
#define IPPOP_CUPS_GET_CLASSES		0x4005
#define IPPOP_CUPS_GET_DEVICES		0x400B
#define IPPOP_CUPS_MOVE_JOB		0x400D

#define IPPOP_CREATE_PRINTER_SUBSCRIPTIONS 	0x0016
#define IPPOP_GET_SUBSCRIPTION_ATTRIBUTES	0x0018
#define IPPOP_GET_SUBSCRIPTIONS			0x0019
#define IPPOP_RENEW_SUBSCRIPTION		0x001a
#define IPPOP_CANCEL_SUBSCRIPTION		0x001b
#define IPPOP_GET_NOTIFICATIONS			0x001c


/* PROTOTYPES */


int Ipp_op_unknown(struct line_list *http_request, struct line_list *http_headers, const struct http_conn *conn,
                   ssize_t *body_rest_len, struct ipp_operation *ipp_request,
		   struct line_list *auth_info);


typedef int (*OP_PROC)(struct line_list *http_request, struct line_list *http_headers, const struct http_conn *conn,
                       ssize_t *body_rest_len, struct ipp_operation *ipp_request,
		       struct line_list *auth_info);
struct ipp_procs {
	int op_code;
	OP_PROC op_proc;
	int auth_scope;
};

extern struct ipp_procs OperationsSupported[];

#endif
