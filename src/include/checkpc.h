/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 ***************************************************************************
 * MODULE: checkpc.h
 * PURPOSE: Include file for checkpc
 * Note: defines global variables that do not need initialization
 *
 * $Id: checkpc.h,v 3.0 1996/05/19 04:06:16 papowell Exp $
 **************************************************************************/

#ifndef _CHECKPC_H_
#define _CHECKPC_H_ 1

#ifndef EXTERN
#define EXTERN extern
#endif

#include "lpd.h"

void Test_port( int ruid, int euid, char *term );

#endif
