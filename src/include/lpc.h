/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2000, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 * $Id: lpc.h,v 5.7 2000/12/25 01:51:19 papowell Exp papowell $
 ***************************************************************************/



#ifndef _LPC_H_
#define _LPC_H_ 1

EXTERN int Auth;
extern char LPC_optstr[]; /* number of status lines */
EXTERN char *Server;

/* PROTOTYPES */
int main(int argc, char *argv[], char *envp[]);
void doaction( struct line_list *args );
void Show_formatted_info( void );
void Get_parms(int argc, char *argv[] );
void use_msg(void);
void usage(void);
int Start_worker( struct line_list *args, int fd );

#endif