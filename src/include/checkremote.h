/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: checkremote.h
 * PURPOSE: includes for checkremote.c
 **************************************************************************/

#if 0
"$Id: checkremote.h,v 3.1 1996/12/28 21:40:24 papowell Exp $";
#endif

/***************************************************************************
Checkremotehost()
    check to see if we have a remote host specified by the LP_device name

 ***************************************************************************/

void Check_remotehost( int checkloop );
