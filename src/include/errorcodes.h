/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2002, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 * $Id: errorcodes.h,v 1.42 2002/12/07 00:30:39 papowell Exp $
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
#define JHOLD    37    /* hold this job */
#define JNOSPOOL 38    /* no spooling to this queue */
#define JNOPRINT 39    /* no printing from this queue  */
#define JSIGNAL  40    /* killed by unrecognized signal */
#define JFAILNORETRY 41 /* no retry on failure */
#define JSUSP    42		/* process suspended successfully */
#define JTIMEOUT 43		/* timeout */
#define JWRERR   44		/* write error */
#define JRDERR   45		/* read error  */
#define JCHILD   46		/* no children */
#define JNOWAIT  47		/* no wait status */

/* PROTOTYPES */

#endif
