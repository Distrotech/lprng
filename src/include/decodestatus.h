/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: decodestatus.h
 * PURPOSE: declare decode status routines
 * $Id: decodestatus.h,v 3.1 1996/08/31 21:11:58 papowell Exp papowell $
 **************************************************************************/

const char *Sigstr (int n);
const char *Decode_status (plp_status_t *status);
char *Server_status( int d );
