/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: debug.h
 * PURPOSE: define parsing and other information for debug option
 *  handling.
 *
 * $Id: debug.h,v 3.0 1996/05/19 04:06:18 papowell Exp $
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

/* general purpose debug test */
#define DEBUGC(VAL,FLAG) if( (Debug > VAL ) || (FLAG & DbgFlag) ) logDebug
#define DEBUGV(VAL,VAR) if( (VAR > VAL) ) logDebug
#define DEBUGM(VAL,VAR,FLAG) if( (VAR > VAL) || (FLAG & DbgFlag) ) logDebug
#define DEBUGF(FLAG) if( (FLAG & DbgFlag) ) logDebug

/* to remove all debugging, redefine this as follows
 * note that a good optimizing compiler should not produce code
 *	for the logDebug call.  It may produce lots of warnings, but no code...
 */
#ifdef NODEBUG
#undef DEBUGC
#undef DEBUGV
#undef DEBUGM
#undef DEBUGF
#define DEBUGC(VAL,FLAG) if( 0 ) logDebug
#define DEBUGV(VAL,VAR) if( 0 ) logDebug
#define DEBUGM(VAL,VAR,FLAG) if( 0 ) logDebug
#define DEBUGF(FLAG) if( 0 ) logDebug
#endif

/* Debug variable level */
#define DEBUG0      DEBUGV(0,Debug)
#define DEBUG1      DEBUGV(1,Debug)
#define DEBUG2      DEBUGV(2,Debug)
#define DEBUG3      DEBUGV(3,Debug)
#define DEBUG4      DEBUGV(4,Debug)
#define DEBUG5      DEBUGV(5,Debug)
#define DEBUG6      DEBUGV(6,Debug)
#define DEBUG7      DEBUGV(7,Debug)
#define DEBUG8      DEBUGV(8,Debug)
#define DEBUG9      DEBUGV(9,Debug)

#define DEBUG0F(FLAG)      DEBUGC(0,FLAG)
#define DEBUG1F(FLAG)      DEBUGC(1,FLAG)
#define DEBUG2F(FLAG)      DEBUGC(2,FLAG)
#define DEBUG3F(FLAG)      DEBUGC(3,FLAG)
#define DEBUG4F(FLAG)      DEBUGC(4,FLAG)
#define DEBUG5F(FLAG)      DEBUGC(5,FLAG)
#define DEBUG6F(FLAG)      DEBUGC(6,FLAG)
#define DEBUG7F(FLAG)      DEBUGC(7,FLAG)
#define DEBUG8F(FLAG)      DEBUGC(8,FLAG)
#define DEBUG9F(FLAG)      DEBUGC(9,FLAG)

#define DBGREM0     DEBUGV(0,DbgRem)
#define DBGREM1     DEBUGV(1,DbgRem)
#define DBGREM2     DEBUGV(2,DbgRem)
#define DBGREM3     DEBUGV(3,DbgRem)
#define DBGREM4     DEBUGV(4,DbgRem)
#define DBGREM5     DEBUGV(5,DbgRem)
#define DBGREM6     DEBUGV(6,DbgRem)
#define DBGREM7     DEBUGV(7,DbgRem)
#define DBGREM8     DEBUGV(8,DbgRem)
#define DBGREM9     DEBUGV(9,DbgRem)

#define DBGREM0F(FLAG)      DEBUGM(0,DbgRem,FLAG)
#define DBGREM1F(FLAG)      DEBUGM(1,DbgRem,FLAG)
#define DBGREM2F(FLAG)      DEBUGM(2,DbgRem,FLAG)
#define DBGREM3F(FLAG)      DEBUGM(3,DbgRem,FLAG)
#define DBGREM4F(FLAG)      DEBUGM(4,DbgRem,FLAG)
#define DBGREM5F(FLAG)      DEBUGM(5,DbgRem,FLAG)
#define DBGREM6F(FLAG)      DEBUGM(6,DbgRem,FLAG)
#define DBGREM7F(FLAG)      DEBUGM(7,DbgRem,FLAG)
#define DBGREM8F(FLAG)      DEBUGM(8,DbgRem,FLAG)
#define DBGREM9F(FLAG)      DEBUGM(9,DbgRem,FLAG)


/* Flags for debugging */

#define DPRINT1 ((1<<8))
#define DPRINT2 ((3<<8))
#define DPRINT3 ((7<<8))
#define DPRINT4 ((0xF<<8))
#define DPRINT5 ((0x1F<<8))
#define DPRINT6 ((0x3F<<8))
#define DNW1 ((1<<16))
#define DNW2 ((3<<16))
#define DNW3 ((7<<16))
#define DNW4 ((0xF<<16))
#define DNW5 ((0x1F<<16))
#define DNW6 ((0x3F<<16))
#define DDB1 ((1<<20))
#define DDB2 ((3<<20))
#define DDB3 ((7<<20))
#define DDB4 ((0xF<<20))
#define DDB5 ((0x1F<<20))
#define DDB6 ((0x3F<<20))

EXTERN int Debug;	/* debug flags */
EXTERN int DbgRem;	/* Remote Connection debug flags */
EXTERN int DbgTest;			/* Flags set to test various options */
EXTERN int DbgJob;	/* force job number */
EXTERN int DbgFlag;	/* force job number */
EXTERN char *New_log_file;	/* new log file for spooler */

#define IP_TEST 0x0001		/* test IP address */

void Parse_debug( char *arg, struct keywords *keys, int interactive);
void Get_debug_parm(int argc, char *argv[], char *optstr,
	struct keywords *list);
void Get_parms(int argc,char *argv[]);

EXTERN struct keywords debug_vars[]		/* debugging variables */

#ifdef DEFINE
 = {
    { "debug",INTEGER_K,(void *)&Debug },
    { "test",INTEGER_K,(void *)&DbgTest },
    { "remote",INTEGER_K,(void *)&DbgRem },
    { "job",INTEGER_K,(void *)&DbgJob },
    { "print",FLAG_K,(void *)&DbgFlag,DPRINT1 },
    { "print+",FLAG_K,(void *)&DbgFlag,DPRINT2 },
    { "print+3",FLAG_K,(void *)&DbgFlag,DPRINT3 },
    { "print+4",FLAG_K,(void *)&DbgFlag,DPRINT4 },
    { "print+5",FLAG_K,(void *)&DbgFlag,DPRINT5 },
    { "print+6",FLAG_K,(void *)&DbgFlag,DPRINT6 },
    { "network",FLAG_K,(void *)&DbgFlag,DNW1 },
    { "network+",FLAG_K,(void *)&DbgFlag,DNW2 },
    { "network+3",FLAG_K,(void *)&DbgFlag,DNW3 },
    { "network+4",FLAG_K,(void *)&DbgFlag,DNW4 },
    { "network+5",FLAG_K,(void *)&DbgFlag,DNW5 },
    { "network+6",FLAG_K,(void *)&DbgFlag,DNW6 },
    { "database",FLAG_K,(void *)&DbgFlag,DDB1 },
    { "database+",FLAG_K,(void *)&DbgFlag,DDB2 },
    { "database+3",FLAG_K,(void *)&DbgFlag,DDB3 },
    { "database+4",FLAG_K,(void *)&DbgFlag,DDB4 },
    { "database+5",FLAG_K,(void *)&DbgFlag,DDB5 },
    { "database+6",FLAG_K,(void *)&DbgFlag,DDB6 },
    { (char *)0 }
}
#endif
;

#endif
