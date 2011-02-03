/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 * $Id: debug.h,v 5.1 1999/09/12 21:32:56 papowell Exp papowell $
 ***************************************************************************/



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
#define DEBUG1      if(0) logDebug
#define DEBUGL1     (0)
#define DEBUG2      if(0) logDebug
#define DEBUGL2     (0)
#define DEBUG3      if(0) logDebug
#define DEBUGL3     (0)
#define DEBUG4      if(0) logDebug
#define DEBUGL4     (0)
#define DEBUG5      if(0) logDebug
#define DEBUGL5     (0)
#define DEBUG6      if(0) logDebug
#define DEBUGL6     (0)

#else

/* general purpose debug test */
#define DEBUGC(VAL,FLAG)     if( (Debug >= (VAL) ) || ((FLAG) & DbgFlag) ) logDebug
#define DEBUGL(VAL,FLAG)     ( (Debug >= (VAL) ) || ((FLAG) & DbgFlag) )
#define DEBUGF(FLAG)         if( (FLAG & DbgFlag) ) logDebug
#define DEBUGFC(FLAG)        if( (FLAG & DbgFlag) )
#define DEBUGFSET(FLAG)      ( (FLAG & DbgFlag) )

EXTERN int Debug;	/* debug flags */
EXTERN int DbgFlag;	/* force job number */

/* Debug variable level */
#define DEBUG1      DEBUGC(1,DRECV1|DCTRL1|DLPQ1|DLPRM1)
#define DEBUGL1     DEBUGL(1,DRECV1|DCTRL1|DLPQ1|DLPRM1)
#define DEBUG2      DEBUGC(2,DRECV2|DCTRL2|DLPQ2|DLPRM2)
#define DEBUGL2     DEBUGL(2,DRECV2|DCTRL2|DLPQ2|DLPRM2)
#define DEBUG3      DEBUGC(3,DRECV3|DCTRL3|DLPQ3|DLPRM3)
#define DEBUGL3     DEBUGL(3,DRECV3|DCTRL3|DLPQ3|DLPRM3)
#define DEBUG4      DEBUGC(4,DRECV4|DCTRL4|DLPQ4|DLPRM4)
#define DEBUGL4     DEBUGL(4,DRECV4|DCTRL4|DLPQ4|DLPRM4)
#define DEBUG5      DEBUGC(5,DRECV4|DCTRL4|DLPQ4|DLPRM4)
#define DEBUGL5     DEBUGL(5,DRECV4|DCTRL4|DLPQ4|DLPRM4)
#define DEBUG6      DEBUGC(6,0)
#define DEBUGL6     DEBUGL(6,0)

/* PROTOTYPES */

#endif

/* Flags for debugging */

#define DPRSHIFT 0
#define DLOGMASK ((0xF<<DPRSHIFT))
#define DLOG1  ((0x1<<DPRSHIFT))
#define DLOG2  ((0x2<<DPRSHIFT))
#define DLOG3  ((0x4<<DPRSHIFT))
#define DLOG4  ((0x8<<DPRSHIFT))

#define DNWSHIFT 4
#define DNWMASK  ((0xF<<DNWSHIFT))
#define DNW1     ((0x1<<DNWSHIFT))
#define DNW2     ((0x2<<DNWSHIFT))
#define DNW3     ((0x4<<DNWSHIFT))
#define DNW4     ((0x8<<DNWSHIFT))

#define DDBSHIFT 8
#define DDBMASK  ((0xF<<DDBSHIFT))
#define DDB1     ((0x1<<DDBSHIFT))
#define DDB2     ((0x2<<DDBSHIFT))
#define DDB3     ((0x4<<DDBSHIFT))
#define DDB4     ((0x8<<DDBSHIFT))

#define DRECVSHIFT 12
#define DRECVMASK  ((0xF<<DRECVSHIFT))
#define DRECV1     ((0x1<<DRECVSHIFT))
#define DRECV2     ((0x2<<DRECVSHIFT))
#define DRECV3     ((0x4<<DRECVSHIFT))
#define DRECV4     ((0x8<<DRECVSHIFT))

#define DCTRLSHIFT 16
#define DCTRLMASK  ((0xF<<DCTRLSHIFT))
#define DCTRL1     ((0x1<<DCTRLSHIFT))
#define DCTRL2     ((0x2<<DCTRLSHIFT))
#define DCTRL3     ((0x4<<DCTRLSHIFT))
#define DCTRL4     ((0x8<<DCTRLSHIFT))

#define DLPRMSHIFT 20
#define DLPRMMASK  ((0xF<<DLPRMSHIFT))
#define DLPRM1     ((0x1<<DLPRMSHIFT))
#define DLPRM2     ((0x2<<DLPRMSHIFT))
#define DLPRM3     ((0x4<<DLPRMSHIFT))
#define DLPRM4     ((0x8<<DLPRMSHIFT))

#define DLPQSHIFT 24
#define DLPQMASK  ((0xF<<DLPQSHIFT))
#define DLPQ1     ((0x1<<DLPQSHIFT))
#define DLPQ2     ((0x2<<DLPQSHIFT))
#define DLPQ3     ((0x4<<DLPQSHIFT))
#define DLPQ4     ((0x8<<DLPQSHIFT))

EXTERN int DbgTest;			/* Flags set to test various options */
EXTERN int DbgJob;	/* force job number */

#define IP_TEST 0x0001		/* test IP address */

void Parse_debug( char *arg, int interactive);

#endif
