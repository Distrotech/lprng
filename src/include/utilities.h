/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: utilities.h
 * PURPOSE: declarations for utilities.c
 * utilities.h,v 3.2 1997/01/15 02:21:18 papowell Exp
 **************************************************************************/

#ifndef _UTILITIES_H
#define _UTILITIES_H

/*
 * Time_str: return "cleaned up" ctime() string...
 *
 * in YY/MO/DY/hr:mn:sc
 * Thu Aug 4 12:34:17 BST 1994 -> 12:34:17
 */

char *Time_str(int shortform, time_t t);

/***************************************************************************
 * Print the usage message list or any list of strings
 *  Use for copyright printing as well
 ***************************************************************************/

void Printlist( char **m, FILE *f );

/***************************************************************************
 * Utility functions: write a string to a fd (bombproof)
 *   write a char array to a fd (fairly bombproof)
 * Note that there is a race condition here that is unavoidable;
 * The problem is that there is no portable way to disable signals;
 *  post an alarm;  <enable signals and do a read simultaneously>
 * There is a solution that involves forking a subprocess, but this
 *  is so painful as to be not worth it.  Most of the timeouts
 *  in the LPR stuff are in the order of minutes, so this is not a problem.
 *
 * Note: we do the write first and then check for timeout.
 ***************************************************************************/

int Write_fd_str( int fd, char *msg );
int Write_fd_len( int fd, char *msg, int len );
int Write_fd_len_timeout( int timeout, int fd, char *msg, int len );
int Write_fd_str_timeout( int timeout, int fd, char *msg );
int Read_fd_len_timeout( int timeout, int fd, char *msg, int len );

/***************************************************************************
 * void trunc_str(char *s)
 *  - remove trailing spaces from the end of a string
 *  - a \ before last trailing space will escape it
 ***************************************************************************/

void trunc_str( char *s );

/**************************************************************************
 * static int Fix_job_number();
 * - fixes the job number range and value
 **************************************************************************/

void Fix_job_number( struct control_file *cfp );

/************************************************************************
 * Make_identifier - add an identifier field to the job
 *  the identifier has the format Aname@host%id
 *  Since the identifier is put in the cfp->identifier field,
 *  and you may want to use it as the job identifier,  we put the
 *  leading 'A' character on the name.
 * 
 ************************************************************************/

int Make_identifier( struct control_file *cfp );


/***************************************************************************
 * int *Crackline( char *line, struct token *token )
 *  cracks the input line into tokens,  separating them at whitespace
 *  Note: for most practical purposes,  you have either
 *  1 token:  keyword
 *  2 tokens: keyword value-to-<endofline>
 *  many tokens: keyword value1 value2 value3
 *    - single word tokens separated by whitespace
 ***************************************************************************/

int Crackline( char *line, struct token *token, int max );

#endif
