/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpr_makjob.c
 * PURPOSE: Set up the control file with values
 *
 **************************************************************************/

static char *const _id =
"lpr_makejob.c,v 3.10 1998/03/24 02:43:22 papowell Exp";

#include "lp.h"
#include "dump.h"
#include "malloclist.h"
#include "fixcontrol.h"
/**** ENDINCLUDE ****/


/***************************************************************************
 * Commentary:
 * The struct control_file{}  data structure contains fields that point to
 * complete lines in the control file, i.e.- 'Jjobname', 'Hhostname'
 * We set up this information in a data structure.
 * Note that this is specific to the LPR program
 *
 * Make_job()
 * 1. Get the control file number and name information
 * 2. scan the set of variables,  and determine how much space is needed.
 * 3. scan the data files,  and determine how much space is needed
 * 4. allocate the space.
 * 5. Copy variables to the allocated space,  setting up pointers in the
 *    control_file data structure.
 **************************************************************************/
static int get_job_number(struct control_file *cfp );

int Make_job( struct control_file *cfp )
{
	char nstr[LARGEBUFFER];	/* information */
	struct keywords *keys;	/* keyword entry in the parameter list */
	char *str;				/* buffer where we allocate stuff */
	int n;
	int job_size;

	/*
	 * first we get the job name
	 */

	get_job_number(cfp);
	cfp->priority = Priority;
	safestrncpy( cfp->filehostname, FQDNHost );
	plp_snprintf( cfp->original, sizeof(cfp->original),
		"cf%c%0*d%s", cfp->priority, cfp->number_len, cfp->number, FQDNHost );
	safestrncat( cfp->transfername, cfp->original );
	DEBUG2("Make_job: '%s'", cfp->original );

	/* we put the option strings in the buffer */
	for( keys = Lpr_parms; keys->keyword; ++keys ){
		DEBUG2("Make_job: key '%s', maxval 0x%x", keys->keyword,keys->maxval);
		if( keys->maxval ){
			switch( keys->type ){
			case INTEGER_K:
				if( (n = *(int *)keys->variable) ){
					plp_snprintf( nstr, sizeof(nstr), "%c%d", keys->flag, n );
				} else {
					continue;
				}
				break;
			case STRING_K:
				if( (str = *(char **)keys->variable) ){
					plp_snprintf( nstr, sizeof(nstr), "%c%s", keys->flag, str );
				} else {
					continue;
				}
				break;
			default:
				continue;
			}
			if( isdigit( keys->flag)  ){
				cfp->digitoptions[ keys->flag -'0' ] = Add_job_line( cfp, nstr, 0,__FILE__,__LINE__  );
			} else {
				cfp->capoptions[ keys->flag - 'A' ] = Add_job_line( cfp, nstr, 0,__FILE__,__LINE__  );
			}
			DEBUG3("Make_job: line [%d] key '%s', flag '%c' = '%s'",
				cfp->control_file_lines.count, keys->keyword, keys->flag, nstr );
		}
	}

	/* record the number of control lines */

	cfp->control_info = cfp->control_file_lines.count;

	/* next, fix up the first character in the data file name
	 * FcfXNNNHost
     * 0123456...
	 * - fix up the format specifier
     */

	/*
	 * copy from standard in?
	 */
	if (Filecount == 0) {
		job_size = Copy_stdin( Cfp_static );
	} else {
		/*
		 * check to see that the input files are printable
		 */
		job_size = Check_files( Cfp_static, Files, Filecount );
	}
	if(DEBUGL3) dump_control_file( "Make_job - result", cfp );
	return( job_size );
}

/**************************************************************************
 * static int get_job_number();
 * - get an integer value from 1 - 999 for the job number
 * - use the PID of the process
 **************************************************************************/

static int get_job_number( struct control_file *cfp)
{
	if( DbgJob ){
		cfp->number = DbgJob;
	} else {
		cfp->number = getpid();
	}
	if( Spread_jobs ) cfp->number *= Spread_jobs;
	Fix_job_number( cfp );
	return( cfp->number );
}

struct keywords Lpr_parms[]
 = {
{ "Accntname",  STRING_K , &Accntname, M_ACCNTNAME, 'R' },
{ "Binary",  INTEGER_K , &Binary },
{ "Bnrname",  STRING_K , &Bnrname, M_BNRNAME, 'L' },
{ "Classname",  STRING_K , &Classname, M_CLASSNAME, 'C' },
{ "Copies",  INTEGER_K , &Copies },
{ "Format",  STRING_K , &Format },
{ "Font1",  STRING_K , &Font1, M_FONT, '1' },
{ "Font2",  STRING_K , &Font2, M_FONT, '2' },
{ "Font3",  STRING_K , &Font3, M_FONT, '3' },
{ "Font4",  STRING_K , &Font4, M_FONT, '4' },
{ "FQDNHost",  STRING_K , &FQDNHost },
{ "Hostname",  STRING_K , &FQDNHost, M_FROMHOST, 'H' },
{ "Indent",  INTEGER_K , &Indent, M_INDENT, 'I' },
{ "Jobname",  STRING_K , &Jobname, M_JOBNAME, 'J' },
{ "Logname",  STRING_K , &Logname, M_BNRNAME, 'P' },
{ "Mailname",  STRING_K , &Mailname, M_MAILNAME, 'M' },
{ "No_header",  INTEGER_K , &No_header },
{ "Option_order",  STRING_K , &Option_order },
{ "Printer",  STRING_K , &Printer },
{ "Priority",  INTEGER_K , &Priority },
{ "Prtitle",  STRING_K , &Prtitle, M_PRTITLE, 'T' },
{ "Pwidth",  INTEGER_K , &Pwidth, M_PWIDTH, 'W' },
{ "Queue_name",  STRING_K , &Queue_name },
{ "RemoteHost",  STRING_K , &RemoteHost },
{ "Removefiles",  INTEGER_K , &Removefiles },
{ "ShortHost",  STRING_K , &ShortHost },
{ "Setup_mailaddress",  INTEGER_K , &Setup_mailaddress },
{ "Secure", INTEGER_K , &Secure },
{ "Username",  STRING_K , &Username },
{ "Use_shorthost",  INTEGER_K , &Use_shorthost },
{ "Zopts",  STRING_K , &Zopts, M_ZOPTS, 'Z' },
{ "Filecount",  INTEGER_K , &Filecount },
{ "Files",  LIST_K , &Files },
{ 0 }
} ;

