/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 * $Id: lprm.h,v 5.1 1999/09/12 21:33:03 papowell Exp papowell $
 ***************************************************************************/



#ifndef _LPRM_1_
#define _LPRM_1_


EXTERN int All_printers;    /* show all printers */
EXTERN int LP_mode;    /* show all printers */

/* PROTOTYPES */

void Get_parms( int argc, char **argv );

#endif
