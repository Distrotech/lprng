/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpr_makjob.c
 * PURPOSE: Set up the control file with values
 *
 **************************************************************************/

static char *const _id =
"$Id: lpr_makejob.c,v 3.4 1996/08/25 22:20:05 papowell Exp papowell $";

#include "lpr.h"
#include "printcap.h"


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
static int get_job_number();

void Make_job( struct control_file *cf )
{
	int job_number;			/* job number */
	char jstr[LINEBUFFER];	/* jobname */
	char nstr[LINEBUFFER];	/* information */
	struct keywords *keys;	/* keyword entry in the parameter list */
	char *str;				/* buffer where we allocate stuff */
	int i, j, c, n;			/* ACME Connectors and Variables, Inc. */
	struct data_file *data_file;	/* data file entry */

	/*
	 * first we get the job name
	 */

	cf->number = job_number = get_job_number();
	if( job_number > 999 ){
		plp_snprintf( jstr, sizeof(jstr), "cf%c%06d", Priority, job_number );
	} else {
		plp_snprintf( jstr, sizeof(jstr), "cf%c%03d", Priority, job_number );
	}
	safestrncat( jstr, Host );
	DEBUG3("Make_job: '%s'", jstr );

	cf->name = add_buffer( &cf->control_file, strlen( jstr )+1 );
	strcpy( cf->name, jstr );

	/* we put the option strings in the buffer */
	for( keys = lpr_parms; keys->keyword; ++keys ){
		DEBUG3("Make_job: key '%s', maxval 0x%x", keys->keyword,keys->maxval);
		if( keys->maxval ){
			switch( keys->type ){
			case INTEGER_K:
				if( (n = *(int *)keys->variable) ){
					plp_snprintf( jstr, sizeof(jstr), "%c%d", keys->flag, n );
				} else {
					continue;
				}
				break;
			case STRING_K:
				if( (str = *(char **)keys->variable) ){
					plp_snprintf( jstr, sizeof(jstr), "%c%s", keys->flag, str );
				} else {
					continue;
				}
				break;
			default:
				continue;
			}
			if( isdigit( keys->flag)  ){
				cf->digitoptions[ keys->flag -'0' ] = Add_job_line( cf, jstr );
			} else {
				cf->capoptions[ keys->flag - 'A' ] = Add_job_line( cf, jstr );
			}
			DEBUG6("Make_job:line [%d] key '%s', flag '%c' = '%s'",
				cf->info_lines.count, keys->keyword, keys->flag, jstr );
		}
	}

	/* record the number of control lines */

	cf->control_info = cf->info_lines.count;

	/* next, fix up the first character in the data file name
	 * FcfXNNNHost
     * 0123456...
	 * - fix up the format specifier
     */
	for( i = 0; i < cf->data_file.count; ++i ){
		data_file = (void *)cf->data_file.list;
		data_file = &data_file[i];

		if( i < 26 ){
			c = 'A'+i;
		} else {
			c = 'a'+i;
		}
		/* each data file entry is:
           FdfNNNhost\n    (namelen+1)
           UdfNNNhost\n    (namelen+1)
		   Nfile\n         (strlen(datafile->file)+2
		 */
		if( job_number > 999 ){
			plp_snprintf( jstr, sizeof(jstr), "%cdf%c%06d%s",
				*Format, c, job_number, Host );
		} else {
			plp_snprintf( jstr, sizeof(jstr), "%cdf%c%03d%s",
				*Format, c, job_number, Host );
		}
		if( data_file->Ninfo ){
			plp_snprintf(nstr, sizeof(nstr), "N%s", data_file->Ninfo );
		}
		/*
		 * put out:  fdfAnnnHOST
		 *           N...
		 * as many times as needed
		 */

		if( data_file->copies == 0 ){
			data_file->copies = 1;
		}
		for(j = 0; j < data_file->copies; ++j ){
			str = Add_job_line( cf, jstr );
			DEBUG6("Make_job:line [%d] '%s'", cf->info_lines.count, jstr );
			data_file->transfername = str+1;
			if( data_file->Ninfo ){
				Add_job_line( cf, nstr );
				DEBUG6("Make_job:line [%d] '%s'", cf->info_lines.count, nstr );
			}
		}
		/* put in the 'UdfXNNNHost' line */
		if( job_number > 999 ){
			plp_snprintf( jstr, sizeof(jstr), "Udf%c%06d%s",
				c, job_number, Host );
		} else {
			plp_snprintf( jstr, sizeof(jstr), "Udf%c%03d%s",
				c, job_number, Host );
		}
		Add_job_line( cf, jstr );
		DEBUG6("Make_job:line [%d] '%s'", cf->info_lines.count, jstr );
	}
	DEBUG6("Make_job: line count %d, control_info %d",
		cf->info_lines.count, cf->control_info );

	if( Debug > 6) dump_control_file( "Make_job", cf );
}

/**************************************************************************
 * static int get_job_number();
 * - get an integer value from 1 - 999 for the job number
 * - use the PID of the process
 **************************************************************************/

static int get_job_number()
{
	int job;

	job = getpid();
	if( DbgJob ) job = DbgJob;
	if( Long_number && !Backwards_compatible ){
		job = job % 1000000;
	} else {
		job = job % 1000;
	}
	return( job );
}
