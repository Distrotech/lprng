/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: getprinter.h
 * PURPOSE: get printer name from printcap database
 * getprinter.h,v 3.3 1997/02/15 15:01:30 papowell Exp
 **************************************************************************/


/*****************************************************************
 * Get_printer()
 * - get printer and remote host names
 * Fix_remote_name()
 * - check printer name for printer@remote; if not, get printcap entry
 *****************************************************************/
void Get_printer( struct printcap_entry **pce );
void Fix_remote_name( struct printcap_entry **pce );
