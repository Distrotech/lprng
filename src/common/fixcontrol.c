/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: fixcontrol.c
 * PURPOSE: fix order of lines in control file
 **************************************************************************/

static char *const _id =
"$Id: fixcontrol.c,v 3.2 1996/08/25 22:20:05 papowell Exp papowell $";
/********************************************************************
 * int Fix_control( struct control_file *cf, char *order )
 *   fix the order of lines in the control file so that they
 *   are in the order of the letters in the order string.
 * Lines are checked for metacharacters and other trashy stuff
 *   that might have crept in by user efforts
 *
 * cf - control file area in memory
 * order - order of options
 *
 *  order string: Letter - relative position in file
 *                * matches any character not in string
 *                  can have only one wildcard in string
 *   Berkeley-            HPJCLIMWT1234
 *   PLP-                 HPJCLIMWT1234*
 *
 * RETURNS: 0 if fixed correctly
 *          non-zero if there is something wrong with this file and it should
 *          be rejected out of hand
 ********************************************************************/

#include "lp.h"
#include "printcap.h"

/********************************************************************
 * BSD and LPRng order
 * We use these values to determine the order of jobs in the file
 * The order of the characters determines the order of the options
 *  in the control file.  A * puts all unspecified options there
 ********************************************************************/

static char Bsd_order[]
  = "HPJCLIMWT1234"
;

static char LPRng_order[]
  = "HPJCLIMWT1234*"
;


static char *wildcard;
static char *order;

int ordercomp( const void *left, const void *right )
{
	char *lpos, *rpos;

	lpos = strchr( order, **((char **)left) );
	if( lpos == 0 ) lpos = wildcard;
	rpos = strchr( order, **((char **)right) );
	if( rpos == 0 ) rpos = wildcard;
	DEBUG8("ordercomp '%s' to '%s', l %d, r %d -> %d",
		*(char **)left,*(char **)right,lpos,rpos,lpos-rpos);
	return( lpos - rpos );
}

int Fix_control( struct control_file *cf, char **bad )
{
	int i, c;			/* ACME Integers and Counters, Inc */
	char *line;			/* line in file */
	char *s;			/* ACME Pointers and Chalkboards, Inc */
	char **lines;		/* lines in file */

	/* if no order supplied, don't check */

	order = LPRng_order;
    if( Backwards_compatible ){
        order = Bsd_order;
	}
	wildcard = strchr( order, '*' );

	*bad = 0;

	DEBUG6("Fix_control: Use_queuename %d, Queuename '%s', Printer %s",
		Use_queuename, cf->QUEUENAME, Printer );
	DEBUG6("Fix_control: order '%s', line_count %d, control_info %d",
		order, cf->info_lines.count, cf->control_info );
	if( Debug > 6) dump_control_file( "Fix_control: before fixing", cf );

	/* check to see if we need to insert the Q entry */
	/* if we do, we insert this at the head of the list */

	if( Use_queuename && cf->QUEUENAME == 0 && Printer ){
		char buffer[M_QUEUENAME];
		plp_snprintf(buffer, sizeof(buffer)-1, "Q%s", Printer );
		cf->QUEUENAME = Prefix_job_line( cf, buffer );
		DEBUG6("Fix_control: adding QUEUENAME '%s'", cf->QUEUENAME );
	}
	if( Destination ){
		lines = cf->destination_lines.list+Destination->arg_start;
		for( i = 0; i < Destination->arg_count; ++i ){
			line = lines[i];
			if( line == 0 || *line == 0 ) continue;
			c = line[0];
			if( isupper(c) ){
				if( (s = cf->capoptions[c-'A']) ){
					*s = 0;
				}
				cf->capoptions[c-'A'] = Prefix_job_line( cf, line );
				DEBUG6("Fix_control: adding '%s'", cf->capoptions[c-'A']);
			}
		}
	} else if( Use_identifier && cf->IDENTIFIER == 0 ){
		if( cf->identifier == 0 ){
			Make_identifier( cf );
		}
		cf->IDENTIFIER = Prefix_job_line( cf, cf->identifier-1 );
		DEBUG6("Fix_control: adding IDENTIFIER '%s'", cf->IDENTIFIER );
	}


	/*
	 * we check to see if there is a metacharacter embedded on
	 * any line of the file.
	 */

	lines = (void *)cf->info_lines.list;
	for( i = 0; i < cf->info_lines.count; ++i ){
		/* get line and first character on line */
		line = lines[i];
		if( line == 0 || (c = *line) == 0 ) continue;
		/* remove any non-listed options */
		if( wildcard == 0 && isupper(c) && !strchr(order, c) ){
			DEBUG3("Fix_control: removing line '%s'", c, line );
				*line = 0;
				cf->capoptions[c-'A'] = 0;
			continue;
		}

		if( (s = Find_meta( line+1 )) ){
			log( LOG_INFO,
				"Fix_control: '%s' has metacharacter 0x%02x '%c' in line '%s'",
					cf->name, *s, isprint(*s)?*s:'?', line );
			Clean_meta( s );
		}
	}

	/*
	 * we check to see if order is correct - we need to check to
	 * see if allowed options in file first.
	 */

	if( wildcard == 0 ){
		wildcard = order + strlen( order );
	}

	if( Debug > 6) dump_control_file( "Fix_control: before sorting", cf );
	qsort( lines, cf->control_info, sizeof( char *), ordercomp );
	if( Debug > 6) dump_control_file( "Fix_control: after sorting", cf );

	return( 0 );
}
