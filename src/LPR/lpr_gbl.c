/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpr_globals.c
 * PURPOSE: define the LPR Lite program globals
 **************************************************************************/

static char *const _id =
"$Id: lpr_gbl.c,v 3.1 1996/06/30 17:12:44 papowell Exp $";

#define EXTERN
#define DEFINE

#include "lp.h"
#include "lp_config.h"
#include "printcap.h"
#include "timeout.h"
#include "setuid.h"
#include "pr_support.h"
#include "lpr.h"

