/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpbanner_globals.c
 * PURPOSE: global variables for lpbanner program
 **************************************************************************/

static char *const _id =
"$Id: lpbanner_gbl.c,v 3.0 1996/05/19 04:05:35 papowell Exp $";

#define EXTERN
#define DEFINE

#include "lpbanner.h"
#include "errorcodes.h"

int errorcode = JABORT;
