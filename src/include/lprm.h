/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2000, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 * $Id: lprm.h,v 5.6 2000/12/25 01:51:21 papowell Exp papowell $
 ***************************************************************************/



#ifndef _LPRM_1_
#define _LPRM_1_


EXTERN int Auth;       /* use authentication */
EXTERN int All_printers;    /* show all printers */
EXTERN int LP_mode;    /* show all printers */

/* PROTOTYPES */
int main(int argc, char *argv[], char *envp[]);
void Do_removal(char **argv);
void Get_parms(int argc, char *argv[] );
void usage(void);
int Start_worker( struct line_list *l, int fd );

#endif
