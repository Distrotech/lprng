/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: errorcode.h
 * PURPOSE: filter error return codes
 * "$Id: errorcodes.h,v 3.0 1996/05/19 04:06:18 papowell Exp $"
 **************************************************************************/

/*
 * filter return codes and job status codes
 * - exit status of the filter process 
 * If a printer filter fails, then we assume JABORT status and
 * will record information about failure
 */

#define JSUCC    0     /* done */
/* from 1 - 31 are signal terminations */
#define JFAIL    32    /* failed - retry later */
#define JABORT   33    /* aborted - do not try again, but keep job */
#define JREMOVE  34    /* failed - remove job */
#define JACTIVE  35    /* active server - try later */
#define JIGNORE  36    /* ignore this job */
#define JHOLD    37    /* hold this job */
