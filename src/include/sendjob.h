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
 * $Id: sendjob.h,v 3.1 1996/12/28 21:40:34 papowell Exp $
 **************************************************************************/

int Send_job( char *printer, char *host, struct control_file *cf,
	struct dpathname *dpath,
	int max_try, int connect_timeout, int connect_interval,
	int transfer_timeout, struct printcap_entry *printcap_entry );
