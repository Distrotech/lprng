/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: printjob.h
 * PURPOSE: job printing support routines
 * "printjob.h,v 3.3 1997/12/17 19:34:56 papowell Exp"
 **************************************************************************/

int Print_job( struct control_file *cfp, struct printcap_entry *printcap_entry,
	int transfer_timeout );
void Setup_accounting( struct control_file *cfp, struct printcap_entry *printcap_entry );
int Do_accounting( int end, char *command, struct control_file *cfp,
	int timeout, struct printcap_entry *printcap_entry, int filter_out );
int Print_banner( char *banner_printer,
	struct control_file *cfp, int timeout, int input,
	int ff_len, char *ff_str, struct printcap_entry *printcap_entry );
