/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: getprinter.h
 * PURPOSE: get printer name from printcap database
 * $Id: getprinter.h,v 3.0 1996/05/19 04:06:20 papowell Exp $
 **************************************************************************/


/*****************************************************************
 * Get_printer()
 * - get printer and remote host names
 * Fix_remote_name()
 * - check printer name for printer@remote; if not, get printcap entry
 *****************************************************************/
void Get_printer();
void Fix_remote_name( int cyclecheck );
