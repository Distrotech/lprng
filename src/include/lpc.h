/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpc.h
 * PURPOSE: Include file for LPC
 * Note: defines global variables that need initialization
 *
 * $Id: lpc.h,v 3.0 1996/05/19 04:06:25 papowell Exp $
 **************************************************************************/

#ifndef _LPC_H_
#define _LPC_H_ 1

#ifndef EXTERN
#define EXTERN extern
#endif

#include "lp.h"

/***************************************************************************
 * Information from host environment and defaults
 ***************************************************************************/

EXTERN struct keywords lpc_parms[]
#ifdef DEFINE
 = {
{ 0 }
}
#endif
;

/***************************************************************************
 * LPC configuration file keywords
 ***************************************************************************/

extern struct keywords lpd_config[];

EXTERN struct keywords *lpc_config[]
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

EXTERN char LPC_optstr[] 	/* LPC options */
#ifdef DEFINE
 = "D:P:V"
#endif
;

void use_msg();

#endif
