/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: fixcontrol.c
 * PURPOSE: fix order of lines in control file
 **************************************************************************/

static char *const _id =
"$Id: fixcontrol.c,v 3.14 1997/12/24 20:10:12 papowell Exp $";
/********************************************************************
 * int Fix_control( struct control_file *cfp, char *order )
 *   fix the order of lines in the control file so that they
 *   are in the order of the letters in the order string.
 * Lines are checked for metacharacters and other trashy stuff
 *   that might have crept in by user efforts
 *
 * cfp - control file area in memory
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
#include "cleantext.h"
#include "dump.h"
#include "errorcodes.h"
#include "fileopen.h"
#include "fixcontrol.h"
#include "getqueue.h"
#include "malloclist.h"
#include "merge.h"
#include "pr_support.h"
#include "setup_filter.h"
#include "jobcontrol.h"
#include "decodestatus.h"
#include "killchild.h"
#include "setstatus.h"

/**** ENDINCLUDE ****/

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
	int cmp;

	/* blank lines always come first */
	lpos = *((char **)left);
	if( lpos && *lpos == 0 ) lpos = 0;
	rpos = *((char **)right);
	if( rpos && *rpos == 0 ) rpos = 0;
	if( lpos == rpos ){
		cmp = 0;
	} else if( lpos == 0 ){
		cmp = -1;
	} else if( rpos == 0 ){
		cmp = 1;
	} else {
		lpos = strchr( order, *lpos );
		if( lpos == 0 ) lpos = wildcard;
		rpos = strchr( order, *rpos );
		if( rpos == 0 ) rpos = wildcard;
		cmp = lpos-rpos;
	}
	DEBUG4("ordercomp '%s' to '%s' -> %d",
		*((char **)left), *((char **)right), cmp );
	return( cmp );
}

int Fix_control( struct control_file *cfp, struct printcap_entry *printcap_entry )
{
	int i, cc, len;			/* ACME Integers and Counters, Inc */
	char *line;			/* line in file */
	char *s;			/* ACME Pointers and Chalkboards, Inc */
	char **lines;		/* lines in file */
	struct data_file *df = (void *)cfp->data_file_list.list;
	int priority = cfp->priority;

	/* fix up priorty if overridden by routing */
	if( Destination && Destination->priority ){
		priority = Destination->priority;
	}

	/* if no order supplied, don't check */
	order = LPRng_order;
    if( Backwards_compatible ){
        order = Bsd_order;
	}
	Fix_job_number( cfp );
	wildcard = strchr( order, '*' );

	DEBUG3("Fix_control: copynumber %d, Long_number %d, num_len %d",
		cfp->copynumber, Long_number, cfp->number_len );

	if(DEBUGL3) dump_control_file( "Fix_control: before fixing", cfp );
	if( (Backwards_compatible || Use_shorthost)
		&& (s = strchr( cfp->filehostname, '.' )) ){
		*s = 0;
	}

	cfp->number += cfp->copynumber;
	plp_snprintf( cfp->transfername, sizeof(cfp->transfername),
		"cf%c%0*d%s", priority, cfp->number_len,
		cfp->number, cfp->filehostname );
	/* we have to modify the transfer names
	 * transfername has the format xdfBnnnnnhostname 
	 *                             01234
	 * We start rewriting at the 4th spot
	 */
	for( i = 0; i < cfp->data_file_list.count; ++i ){
		plp_snprintf( df[i].transfername+4,
			sizeof( df[i].transfername)-4,
			"%0*d%s", cfp->number_len,
			cfp->number, cfp->filehostname );
		if( df[i].Uinfo[0] ){
			plp_snprintf( df[i].Uinfo+4,
				sizeof( df[i].Uinfo)-4,
				"%0*d%s", cfp->number_len,
				cfp->number, cfp->filehostname );
		}
	}

	/* if(DEBUGL3) dump_control_file( "Fix_control: data files fixed", cfp );*/
	DEBUG3("Fix_control: Use_queuename %d, Queuename '%s', Printer %s",
		Use_queuename, cfp->QUEUENAME, Printer );
	DEBUG3("Fix_control: Use_identifier %d, IDENTIFIER (0x%x) '%s', cfp->identifier '%s'",
		Use_identifier, cfp->IDENTIFIER, cfp->IDENTIFIER, cfp->identifier );
	DEBUG3("Fix_control: order '%s', line_count %d, control_info %d",
		order, cfp->control_file_lines.count, cfp->control_info );

	/* check to see if we need to insert the Q entry */
	/* if we do, we insert this at the head of the list */

	if( (Is_server && Forward_auth == 0)
		|| ( !Is_server && Use_auth == 0 && Use_auth_flag == 0) ){
		/* clobber the authentication information */
		cfp->auth_id[0] = 0;
	}
	if( Use_identifier && cfp->IDENTIFIER == 0 ){
		if( cfp->identifier[0] == 0 ){
			Make_identifier( cfp );
			DEBUG3("Fix_control: identifier '%s'", cfp->identifier );
		}
		cfp->IDENTIFIER = Insert_job_line( cfp, cfp->identifier, 1, 0 );
		DEBUG3("Fix_control: adding IDENTIFIER '%s'", cfp->IDENTIFIER );
	}
	if( Use_date &&
		(cfp->DATE == 0 || cfp->DATE[1] == 0) ){
		char buffer[M_DATE];
		plp_snprintf(buffer, sizeof(buffer)-1, "D%s",
				Time_str( 0, cfp->statb.st_ctime ) );
		cfp->DATE = Insert_job_line( cfp, buffer, 0, 0 );
		DEBUG3("Fix_control: adding DATE '%s'", cfp->DATE );
	}
	if( (Use_queuename || Force_queuename) &&
		(cfp->QUEUENAME == 0 || cfp->QUEUENAME[1] == 0) ){
		char buffer[M_QUEUENAME];
		s = Force_queuename;
		if( s == 0 || *s == 0 ) s = Queue_name;
		if( s == 0 || *s == 0 ) s = Printer;
		plp_snprintf(buffer, sizeof(buffer)-1, "Q%s", s );
		cfp->QUEUENAME = Insert_job_line( cfp, buffer, 0, 0 );
		DEBUG3("Fix_control: adding QUEUENAME '%s'", cfp->QUEUENAME );
	}

	/* fix up the control file lines overrided by routing */
	if( Destination ){
		DEBUG3("Fix_control: fixing destination information" );
		lines = cfp->hold_file_lines.list+Destination->arg_start;
		for( i = 0; i < Destination->arg_count; ++i ){
			line = lines[i];
			if( line == 0 || *line == 0 ) continue;
			DEBUG3("Fix_control: route info '%s'", line );
			if( strncmp( line, "route", 5 ) == 0 ){
				line += 5;
			}
			while( isspace( *line ) ) ++line;
			cc = line[0];
			if( isupper(cc) ){
				if( (s = cfp->capoptions[cc-'A']) ){
					*s = 0;
				}
				cfp->capoptions[cc-'A'] = Insert_job_line( cfp, line, 0, 0 );
				DEBUG3("Fix_control: adding '%s'", cfp->capoptions[cc-'A']);
			}
		}
	}

	/*
	 * we check to see if there is a metacharacter embedded on
	 * any line of the file.
	 */

	lines = (void *)cfp->control_file_lines.list;
	for( i = 0; i < cfp->control_file_lines.count; ++i ){
		/* get line and first character on line */
		line = lines[i];
		if( line == 0 || (cc = *line) == 0 ) continue;
		/* remove any non-listed options */
		if( wildcard == 0 && isupper(cc) && !strchr(order, cc) ){
			DEBUG2("Fix_control: removing line '%s'", line );
				*line = 0;
				cfp->capoptions[cc-'A'] = 0;
			continue;
		}
		if( Xlate_format && islower(cc) ){
			char *t = Xlate_format;
			while( (t = strchr( t, cc )) ){
				int len = t - Xlate_format;
				int newfmt = t[1];
				if( newfmt && (len & 1) == 0 && islower(newfmt) ){
					*line = newfmt;
					DEBUG3("Send_files: translate format '%s'", line );
					break;
				}
				t = t+1;
			}
		}
		Clean_meta( line+1 );
	}

	/*
	 * we check to see if order is correct - we need to check to
	 * see if allowed options in file first.
	 */

	if( wildcard == 0 ){
		wildcard = order + strlen( order );
	}

	if(DEBUGL3) dump_control_file( "Fix_control: before sorting", cfp );
	if( Mergesort( lines, cfp->control_info, sizeof( char *), ordercomp )){
		fatal( LOG_ERR, "Fix_control: Mergesort failed" );
	}
	if(DEBUGL3) dump_control_file( "Fix_control: after sorting", cfp );

	if( (Is_server || Lpr_bounce) && Control_filter && *Control_filter ){
		int tempfd, tempcf;
		int df_count = cfp->data_file_list.count;
		static struct data_file *dftemp;

		DEBUG3("Fix_control: control filter '%s'", Control_filter );
		if( dftemp ){
			free(dftemp);
			dftemp = 0;
		}
		if( !Is_server && Lpr_bounce ){
			malloc_or_die( dftemp, sizeof(dftemp[0])*df_count );
			memcpy( dftemp, df, sizeof(dftemp[0])*df_count );
		}
		tempfd = Make_temp_fd( 0, 0 );
		tempcf = Make_temp_fd( 0, 0 );
		/* at this point you have a filter, which is taking input
			from XF_fd_info.input; pass input file through it */
		lines = cfp->control_file_lines.list;
		for( i = 0; i < cfp->control_file_lines.count; ++i ){
			if( lines[i] && lines[i][0] ){
				if( Write_fd_str( tempcf, lines[i] ) < 0
				  || Write_fd_str( tempcf, "\n" ) < 0 ){
					Errorcode = JABORT;
					logerr_die( LOG_INFO,
						"Fix_control: error writing to temp control file" );
				}
			}
		}
		if( lseek( tempcf, 0, SEEK_SET ) < 0 ){
			Errorcode = JFAIL;
			logerr_die( LOG_INFO, "Fix_control: lseek failed" );
		}
		if( Make_filter( 'f', cfp, &XF_fd_info, Control_filter,
			0, /* no extra */
			0,	/* RW pipe */
			tempfd, /* dup to fd 1 */
			printcap_entry, /* printcap information */
			0, 0, Logger_destination != 0, tempcf ) ){
			Errorcode = JABORT;
			fatal( LOG_INFO, "Fix_control: failed '%s'", cfp->error );
		}
		i = Close_filter( 0, &XF_fd_info, 0, "control filter" );
		DEBUG3("Fix_control: control_filter exit %d, %s", i, Server_status(i) );
		if( i == JHOLD ){
			cfp->hold_info.hold_time = time( (void *)0 );
			Set_job_control( cfp, (void *)0 );
			setstatus(cfp,
				"Fix_control: control filter returned %s", Server_status( i ));
			Errorcode = JHOLD;
			cleanup(0);
		}
		if( i == JREMOVE ){
			Errorcode = JREMOVE;
			setstatus(cfp,
				"Fix_control: control filter returned %s", Server_status( i ));
			cleanup(0);
		}
		if( i != JSUCC ){
			Errorcode = JABORT;
			setstatus(cfp,
				"Fix_control: bounce queue filter returned %s", Server_status( i ));
			cleanup(0);
		}
		close( tempcf );
		if( fstat( tempfd, &cfp->statb ) < 0 ){
			Errorcode = JFAIL;
			logerr_die( LOG_INFO, "Fix_control: fstat failed" );
		}
		if( lseek( tempfd, 0, SEEK_SET ) < 0 ){
			Errorcode = JFAIL;
			logerr_die( LOG_INFO, "Fix_control: lseek failed" );
		}

		len = cfp->statb.st_size;
		DEBUG3("Fix_control: allocate control file buffer len %d", len );
		cfp->cf_info = add_buffer( &cfp->control_file_image, len+1 );
		for( i = 1, s = cfp->cf_info;
			len > 0 && (i = read( tempfd, s, len )) > 0;
			len -= i, s += i );
		*s++ = 0;
		if( i < 0 ){
			Errorcode = JFAIL;
			logerr_die( LOG_INFO, "cannot read temp_control_file" );
		}
		close( tempfd );
		DEBUG0("Fix_control: filtered control file '%s'",cfp->cf_info );
		if( Parse_cf( Is_server?SDpathname:0, cfp, Is_server ) ){
			Errorcode = JFAIL;
			logerr_die( LOG_INFO,
			"Fix_control: control filter generated by format '%s'",
			(void *)cfp->control_file_copy.list );
		}
		/* now we update the control file information if necessary */
		if( !Is_server && Lpr_bounce ){
			if( df_count != cfp->data_file_list.count ){
				Errorcode = JFAIL;
				logerr_die( LOG_INFO,
				"Fix_control: bad control filter generated by format '%s'",
				(void *)cfp->control_file_copy.list );
			}
			df = (void *)cfp->data_file_list.list;
			for( i = 0; i < df_count; ++i ){
				safestrncpy( df[i].openname, dftemp[i].openname );
				df[i].statb = dftemp[i].statb;
			}
		}
	}

	/* copy the lines into one big buffer */
	lines = cfp->control_file_lines.list;
	len = 0;
	for( i = 0; i < cfp->control_file_lines.count; ++i ){
		if( lines[i] && lines[i][0] ) len += strlen( lines[i] ) + 1;
	}
	/* add one for terminating 0 */
	++len;

	if( len >= cfp->control_file_copy.max ){
		extend_malloc_list( &cfp->control_file_copy, 1, len+100 );
		cfp->control_file_copy.count = len;
	}

	s = (void *)cfp->control_file_copy.list;
	for( i = 0; i < cfp->control_file_lines.count; ++i ){
		if( lines[i] && lines[i][0] ){
			strcpy( s, lines[i] );
			strcat( s, "\n" );
			s += strlen(s);
		}
	}
	*s = 0;
	s = (void *)cfp->control_file_copy.list;

	DEBUG0("Fix_control: new control file '%s'", s );

	return( 0 );
}
