/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: decodestatus.h
 * PURPOSE: declare decode status routines
 * decodestatus.h,v 3.1 1996/12/28 21:40:25 papowell Exp
 **************************************************************************/

const char *Sigstr (int n);
const char *Decode_status (plp_status_t *status);
char *Server_status( int d );
