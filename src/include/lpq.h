/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpq.h
 * PURPOSE: Include file for LPQ
 * Note: defines global variables that need initialization
 *
 * $Id: lpq.h,v 3.0 1996/05/19 04:06:28 papowell Exp $
 **************************************************************************/

#ifndef _LPQ_H_
#define _LPQ_H_ 1

#ifndef EXTERN
#define EXTERN extern
#endif

#include "lp.h"

/***************************************************************************
 * Information from host environment and defaults
 ***************************************************************************/

EXTERN int All_printers;	/* show all printers */
EXTERN int Clear_scr;		/* clear screen */
EXTERN int Interval;		/* display interval */
EXTERN int Longformat;		/* Long format */

EXTERN struct keywords lpq_parms[]
#ifdef DEFINE
 = {
{ "All_printers",  INTEGER_K , &All_printers },
{ "Clear_scr",  INTEGER_K , &Clear_scr },
{ "Interval",  INTEGER_K , &Interval },
{ "Longformat",  INTEGER_K , &Longformat },
{ 0 }
}
#endif
;

/***************************************************************************
 * LPQ configuration file keywords
 ***************************************************************************/

extern struct keywords lpd_config[];

EXTERN struct keywords *lpq_config[]
#ifdef DEFINE
  = {
    lpd_config,
    0
}
#endif
;

/**************************************************************************
 * Command line options
 **************************************************************************/

EXTERN char LPQ_optstr[] 	/* LPQ options */
#ifdef DEFINE
 = "D:P:Vaclst:"
#endif
;

#endif
