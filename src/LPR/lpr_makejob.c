/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpr_makjob.c
 * PURPOSE: Set up the control file with values
 *
 **************************************************************************/

static char *const _id =
"$Id: lpr_makejob.c,v 3.6 1997/01/30 21:15:20 papowell Exp $";

#include "lp.h"
#include "dump.h"
#include "malloclist.h"
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

void Make_job( struct control_file *cfp )
{
	char nstr[LINEBUFFER];	/* information */
	struct keywords *keys;	/* keyword entry in the parameter list */
	char *str;				/* buffer where we allocate stuff */
	int i, j, c, n;			/* ACME Connectors and Variables, Inc. */
	struct data_file *data_file;	/* data file entry */

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
				cfp->digitoptions[ keys->flag -'0' ] = Add_job_line( cfp, nstr );
			} else {
				cfp->capoptions[ keys->flag - 'A' ] = Add_job_line( cfp, nstr );
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
	for( i = 0; i < cfp->data_file_list.count; ++i ){
		data_file = (void *)cfp->data_file_list.list;
		data_file = &data_file[i];

		if( i < 26 ){
			c = 'A'+i;
		} else {
			c = 'a'+ i - 26;
		}
		/* each data file entry is:
           FdfNNNhost\n    (namelen+1)
           UdfNNNhost\n    (namelen+1)
		   Nfile\n         (strlen(datafile->file)+2
		 */
		plp_snprintf( data_file->transfername, sizeof(data_file->transfername),
			"%cdf%c%0*d%s", *Format, c, cfp->number_len, cfp->number, FQDNHost );
		/* put in the 'UdfXNNNHost' line */
		safestrncpy( data_file->Uinfo, data_file->transfername );
		data_file->Uinfo[0] = 'U';
		data_file->format = *Format;

		/*
		 * put out:  fdfAnnnHOST
		 *           N...
		 * as many times as needed
		 */

		if( data_file->copies == 0 ){
			data_file->copies = 1;
		}
		for(j = 0; j < data_file->copies; ++j ){
			str = Add_job_line( cfp, data_file->transfername );
			DEBUG3("Make_job:line [%d] '%s'", cfp->control_file_lines.count, str );
			if( data_file->Ninfo[0] ){
				str = Add_job_line( cfp, data_file->Ninfo );
				DEBUG3("Make_job:line [%d] '%s'", cfp->control_file_lines.count, str );
			}
		}
		str = Add_job_line( cfp, data_file->Uinfo );
		DEBUG3("Make_job:line [%d] '%s'", cfp->control_file_lines.count, str );
	}
	DEBUG3("Make_job: line count %d, control_info %d",
		cfp->control_file_lines.count, cfp->control_info );

	if(DEBUGL3) dump_control_file( "Make_job - result", cfp );
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

