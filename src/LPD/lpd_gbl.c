/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpd_globals.c
 * PURPOSE: define the LPR Lite program globals
 **************************************************************************/

static char *const _id =
"$Id: lpd_gbl.c,v 3.0 1996/05/19 04:05:38 papowell Exp $";

#define EXTERN
#define DEFINE

#include "lpd.h"
#include "lp_config.h"
#include "printcap.h"
#include "timeout.h"
#include "permission.h"
#include "setuid.h"
#include "pr_support.h"
#include "jobcontrol.h"
