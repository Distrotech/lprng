/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2002, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 * $Id: lpd_secure.h,v 1.27 2002/04/01 17:54:58 papowell Exp $
 ***************************************************************************/



#ifndef _LPD_SECURE_H_
#define _LPD_SECURE_H_ 1

/* PROTOTYPES */
int Receive_secure( int *sock, char *input );
int Do_secure_work( int use_line_order, char *jobsize, int from_server,
	char *tempfile, struct line_list *header_info );
struct security *Fix_receive_auth( char *name, struct line_list *info );
int Pgp_receive( int *sock, char *user, char *jobsize, int from_server,
	char *authtype, struct line_list *info,
	char *error, int errlen, struct line_list *header_info, char *tempfile );
int Check_secure_perms( struct line_list *options, int from_server,
	char *error, int errlen );

#endif
