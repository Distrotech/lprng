/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: sendjob.h
 * PURPOSE: declare Send_job fucntions
 * $Id: sendjob.h,v 3.0 1996/05/19 04:06:34 papowell Exp $
 **************************************************************************/

int Send_job( char *printer, char *host, struct control_file *cf,
	struct dpathname *dpath,
	int max_try, int connect_timeout, int connect_interval,
	int transfer_timeout, struct pc_used *pc_used );
