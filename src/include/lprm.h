/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lprm.h
 * PURPOSE: Include file for LPRM
 * Note: defines global variables that do not need initialization
 *
 * $Id: lprm.h,v 3.0 1996/05/19 04:06:30 papowell Exp $
 **************************************************************************/

#ifndef _LPRM_H_
#define _LPRM_H_ 1

#ifndef EXTERN
#define EXTERN extern
#endif

#include "lp.h"

/***************************************************************************
 * LPRM configuration file keywords
 ***************************************************************************/

extern struct keywords lpd_config[];

EXTERN struct keywords *lprm_config[]
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

EXTERN char LPRM_optstr[] 	/* LPQ options */
#ifdef DEFINE
 = "aD:P:V"
#endif
;

EXTERN int All_printers;	/* show all printers */

#endif
