/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: sendjob.h
 * PURPOSE: declare Send_job fucntions
 * sendjob.h,v 3.4 1998/01/08 09:51:29 papowell Exp
 **************************************************************************/

int Send_job( char *printer, char *host, struct control_file *cf,
	struct dpathname *dpath,
	int connect_timeout, int connect_interval, int max_connect_interval,
	int transfer_timeout, struct printcap_entry *printcap_entry );
