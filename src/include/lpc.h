/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2002, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 * $Id: lpc.h,v 1.30 2002/05/06 01:06:43 papowell Exp $
 ***************************************************************************/



#ifndef _LPC_H_
#define _LPC_H_ 1

EXTERN int Auth;
extern char LPC_optstr[]; /* number of status lines */
EXTERN char *Server;
EXTERN int All_pc_printers; /* use the printers in the printcap */

/* PROTOTYPES */
int main(int argc, char *argv[], char *envp[]);
void doaction( struct line_list *args );
void Get_parms(int argc, char *argv[] );
void use_msg(void);
void usage(void);

#endif
