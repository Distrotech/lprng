/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: setupprinter.h
 * PURPOSE: declare setupprinter.c fucntions
 * setupprinter.h,v 3.1 1996/12/28 21:40:36 papowell Exp
 **************************************************************************/

int Setup_printer( char *name,
    char *error, int errlen,
    struct keywords *debug_list, int info_only,
    struct stat *control_statb, struct printcap_entry **pc_entry );
void Fix_update( struct keywords *debug_list, int info_only );

