/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: checkpc_globals.c
 * PURPOSE: define the LPR Lite program globals
 **************************************************************************/

static char *const _id =
"$Id: checkpc_gbl.c,v 3.0 1996/05/19 04:05:33 papowell Exp $";

#define EXTERN
#define DEFINE

#include "lp.h"
#include "lp_config.h"
#include "printcap.h"
#include "checkpc.h"
#include "timeout.h"
#include "permission.h"
#include "setuid.h"
#include "pr_support.h"
#include "jobcontrol.h"
