/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: debug.h
 * PURPOSE: define parsing and other information for debug option
 *  handling.
 *
 * debug.h,v 3.4 1997/10/04 16:14:09 papowell Exp
 **************************************************************************/

/****************************************
 * DEBUG Flags
 * Debug information is given by a line of the form:
 * -D level,key=level,key=level
 *
 * Parsing is done by Parse_debug( char *arg, struct keys *keys)
 * The struct keys contains a keyword and variable name to set
 * You are passed an array of keys, last of which has a null entry
 ****************************************/

#ifndef _DEBUG_H_
#define _DEBUG_H_ 1

/* to remove all debugging, redefine this as follows
 * note that a good optimizing compiler should not produce code
 *	for the logDebug call.  It may produce lots of warnings, but no code...
 */

#ifdef NODEBUG

#define DEBUGFSET(FLAG)       ( 0 )
#define DEBUGF(FLAG)          if( 0 ) logDebug
#define DEBUGFC(FLAG)        if( 0 )
#define DEBUG0      if(0) logDebug
#define DEBUGL0     (0)
#define DEBUG1      if(0) logDebug
#define DEBUGL1     (0)
#define DEBUG2      if(0) logDebug
#define DEBUGL2     (0)
#define DEBUG3      if(0) logDebug
#define DEBUGL3     (0)
#define DEBUG4      if(0) logDebug
#define DEBUGL4     (0)

#else

/* general purpose debug test */
#define DEBUGC(VAL,FLAG)     if( (Debug > (VAL) ) || ((FLAG) & DbgFlag) ) logDebug
#define DEBUGL(VAL,FLAG)     ( (Debug > (VAL) ) || ((FLAG) & DbgFlag) )
#define DEBUGF(FLAG)         if( (FLAG & DbgFlag) ) logDebug
#define DEBUGFC(FLAG)        if( (FLAG & DbgFlag) )
#define DEBUGFSET(FLAG)      ( (FLAG & DbgFlag) )

EXTERN int Debug;	/* debug flags */
EXTERN int DbgFlag;	/* force job number */
EXTERN int DbgAuth;	/* debug authenticated transfer */

/* Debug variable level */
#define DEBUG0      DEBUGC(0,DPRINT1|DNW1|DDB1|DRECV1|DAUTH1)
#define DEBUGL0     DEBUGL(0,DPRINT1|DNW1|DDB1|DRECV1|DAUTH1)
#define DEBUG1      DEBUGC(1,DPRINT2|DNW2|DDB2|DRECV2|DAUTH2)
#define DEBUGL1     DEBUGL(1,DPRINT2|DNW2|DDB2|DRECV2|DAUTH2)
#define DEBUG2      DEBUGC(2,DPRINT3|DNW3|DDB3|DRECV3|DAUTH3)
#define DEBUGL2     DEBUGL(2,DPRINT3|DNW3|DDB3|DRECV3|DAUTH3)
#define DEBUG3      DEBUGC(3,DPRINT4|DNW4|DDB4|DRECV4|DAUTH4)
#define DEBUGL3     DEBUGL(3,DPRINT4|DNW4|DDB4|DRECV4|DAUTH4)
#define DEBUG4      DEBUGC(4,DPRINT4|DNW4|DDB4|DRECV4|DAUTH4)
#define DEBUGL4     DEBUGL(4,DPRINT4|DNW4|DDB4|DRECV4|DAUTH4)

#endif

/* Flags for debugging */

#define DPRSHIFT 0
#define DPRINTMASK ((0xF<<DPRSHIFT))
#define DPRINT1  ((0xF<<DPRSHIFT))
#define DBPRINT1 ((0x8<<DPRSHIFT))
#define DPRINT2  ((0x7<<DPRSHIFT))
#define DBPRINT2 ((0x4<<DPRSHIFT))
#define DPRINT3  ((0x3<<DPRSHIFT))
#define DBPRINT3 ((0x2<<DPRSHIFT))
#define DPRINT4  ((0x1<<DPRSHIFT))
#define DBPRINT4 ((0x1<<DPRSHIFT))

#define DNWSHIFT 4
#define DNWMASK  ((0xF<<DNWSHIFT))
#define DNW1     ((0xF<<DNWSHIFT))
#define DBNW1    ((0x8<<DNWSHIFT))
#define DNW2     ((0x7<<DNWSHIFT))
#define DBNW2    ((0x4<<DNWSHIFT))
#define DNW3     ((0x3<<DNWSHIFT))
#define DBNW3    ((0x2<<DNWSHIFT))
#define DNW4     ((0x1<<DNWSHIFT))
#define DBNW4    ((0x1<<DNWSHIFT))

#define DDBSHIFT 8
#define DDBMASK  ((0xF<<DDBSHIFT))
#define DDB1     ((0xF<<DDBSHIFT))
#define DBB1     ((0x8<<DDBSHIFT))
#define DDB2     ((0x7<<DDBSHIFT))
#define DBB2     ((0x4<<DDBSHIFT))
#define DDB3     ((0x3<<DDBSHIFT))
#define DBB3     ((0x2<<DDBSHIFT))
#define DDB4     ((0x1<<DDBSHIFT))
#define DBB4     ((0x1<<DDBSHIFT))

#define DRECVSHIFT 12
#define DRECVMASK  ((0xF<<DRECVSHIFT))
#define DRECV1     ((0xF<<DRECVSHIFT))
#define DBRECV1    ((0x8<<DRECVSHIFT))
#define DRECV2     ((0x7<<DRECVSHIFT))
#define DBRECV2    ((0x4<<DRECVSHIFT))
#define DRECV3     ((0x3<<DRECVSHIFT))
#define DBRECV3    ((0x2<<DRECVSHIFT))
#define DRECV4     ((0x1<<DRECVSHIFT))
#define DBRECV4    ((0x1<<DRECVSHIFT))

#define DAUTHSHIFT 16
#define DAUTHMASK  ((0xF<<DAUTHSHIFT))
#define DAUTH1     ((0xF<<DAUTHSHIFT))
#define DBAUTH1    ((0x8<<DAUTHSHIFT))
#define DAUTH2     ((0x7<<DAUTHSHIFT))
#define DBAUTH2    ((0x4<<DAUTHSHIFT))
#define DAUTH3     ((0x3<<DAUTHSHIFT))
#define DBAUTH3    ((0x2<<DAUTHSHIFT))
#define DAUTH4     ((0x1<<DAUTHSHIFT))
#define DBAUTH4    ((0x1<<DAUTHSHIFT))

#define DMEMSHIFT 20
#define DMEMMASK  ((0xF<<DMEMSHIFT))
#define DMEM1     ((0xF<<DMEMSHIFT))
#define DBMEM1    ((0x8<<DMEMSHIFT))
#define DMEM2     ((0x7<<DMEMSHIFT))
#define DBMEM2    ((0x4<<DMEMSHIFT))
#define DMEM3     ((0x3<<DMEMSHIFT))
#define DBMEM3    ((0x2<<DMEMSHIFT))
#define DMEM4     ((0x1<<DMEMSHIFT))
#define DBMEM4    ((0x1<<DMEMSHIFT))

EXTERN int DbgTest;			/* Flags set to test various options */
EXTERN int DbgJob;	/* force job number */
EXTERN char *New_log_file;	/* new log file for spooler */

#define IP_TEST 0x0001		/* test IP address */

extern struct keywords debug_vars[];		/* debugging variables */
void Parse_debug( char *arg, struct keywords *keys, int interactive);
void Get_debug_parm(int argc, char *argv[], char *optstr,
	struct keywords *list);
void Get_parms(int argc,char *argv[]);

#endif
