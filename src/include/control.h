/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: control.h
 * PURPOSE: LPC control information
 * $Id: control.h,v 3.1 1996/12/28 21:40:24 papowell Exp $
 **************************************************************************/

#define  START		1
#define  STOP		2
#define  ENABLE		3
#define  DISABLE	4
#define  ABORT		6
#define  KILL		7
#define  HOLD		8
#define  RELEASE	9
#define  TOPQ		10
#define  LPQ		11
#define  LPRM		12
#define  STATUS		13
#define  REDIRECT	14
#define  LPD		15
#define  PRINTCAP	16
#define  UP			17
#define  DOWN		18
#define  REREAD		19
#define  MOVE		20
#define  DEBUG		21
#define  HOLDALL	22
#define  NOHOLDALL	23
#define  CLAss 		24

int Get_controlword( char *s );
