/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 * $Id: errorcodes.h,v 5.1 1999/09/12 21:32:56 papowell Exp papowell $
 ***************************************************************************/



#ifndef _ERRORCODES_H_
#define _ERRORCODES_H_ 1
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
#define JIGNORE  36    /* ignore this job - not used! */
#define JHOLD    37    /* hold this job */
#define JNOSPOOL 38    /* no spooling to this queue */
#define JNOPRINT 39    /* no printing from this queue  */
#define JSIGNAL  40    /* killed by unrecognized signal */
#define JFAILNORETRY  41 /* no retry on failure */

/* PROTOTYPES */

#endif
