/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: spoolcontrol.c
 * PURPOSE: read and write the spool queue control file
 **************************************************************************/

#include "lp.h"
#include "globmatch.h"
#include "patselect.h"
/**** ENDINCLUDE ****/

/***************************************************************************
 * int Patselect( struct token *token, struct control_file *cfp );
 *    check to see that the token value matches one of the following
 *    in the control file:
 *  token is INTEGER: then matches the job number
 *  token is string: then matches either the user name or host name
 *    then try glob matching job ID
 *
 ***************************************************************************/

int Patselect( struct token *token, struct control_file *cfp,
	struct destination **destination )
{
	char *s, *end;
	int val, len, j;
	struct destination *destination_list, *d;

	s = token->start;
	len = token->length;

	DEBUG3("patselect: '%s'", s );

	/* handle wildcard match */
	if( strcasecmp( s, "all" ) == 0 ){
		return( 1 );
	}
	end = s;
	val = strtol( s, &end, 10 );
	if( (end - s) == len ){
		/* we check job number */
		DEBUG3("patselect: job number check '%d' to job %d",
			val, cfp->number );
		return( val == cfp->number );
	} else {
		/* now we check to see if we have a name match */
		if( cfp->LOGNAME && strcasecmp( s, cfp->LOGNAME+1 ) == 0 ){
			DEBUG3("patselect: job logname '%s' match", cfp->LOGNAME );
			return(1);
		}
		if( strcasecmp( s, cfp->identifier+1 ) == 0 ){
			DEBUG3("patselect: job identifier '%s' match", cfp->identifier+1 );
			return(1);
		}
		if( destination && (j = cfp->destination_list.count) > 0 ){
			destination_list = (void *)cfp->destination_list.list;
			if( *destination ){
				/* get next one */
				d = *destination;
				++d;
			} else {
				d = destination_list;
			}
			*destination = 0;
			for( ;d < &destination_list[j]; ++d ){
				if( strcasecmp( s, d->identifier+1 ) == 0 ){
					DEBUG3("patselect: job identifier '%s' match", d->identifier+1 );
					*destination = d;
					return(1);
				} else if( strchr( s, '*' ) && Globmatch( s, d->identifier+1) == 0 ){
					DEBUG3("patselect: job identifier '%s' globmatch",d->identifier+1);
					*destination = d;
					return(1);
				}
			}
		}
		if( strchr( s, '*' ) && Globmatch( s, cfp->identifier+1) == 0 ){
			DEBUG3("patselect: job identifier '%s' globmatch",cfp->identifier+1);
			return(1);
		}
	}
	return(0);
}
