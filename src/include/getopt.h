/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 * $Id: getopt.h,v 1.65 2004/02/04 00:54:14 papowell Exp $
 ***************************************************************************/



#ifndef _GETOPT_H_
#define _GETOPT_H_ 1


extern int Optind, Opterr;
extern char *Optarg;
extern char *Name;			/* program name */

/* PROTOTYPES */
int Getopt (int argc, char *argv[], char *optstring);

#endif
