/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: sendjob.c
 * PURPOSE: Send a print job to the remote host
 *
 **************************************************************************/

static char *const _id =
"readstatus.c,v 3.9 1998/03/24 02:43:22 papowell Exp";

#include "lp.h"
#include "readstatus.h"
#include "linksupport.h"
#include "setstatus.h"
#include "malloclist.h"
#include "killchild.h"
#include "cleantext.h"
/**** ENDINCLUDE ****/

/***************************************************************************
 *int Read_status_info( int ack, int fd, int timeout );
 * ack = ack character from remote site
 * sock  = fd to read status from
 * char *host = host we are reading from
 * int output = output fd
 *  We read the input in blocks,  split up into lines,
 *  and then pass the lines to a lower level routine for processing.
 *  We run the status through the plp_snprintf() routine,  which will
 *   rip out any unprintable characters.  This will prevent magic escape
 *   string attacks by users putting codes in job names, etc.
 ***************************************************************************/

/* Note: at least two positions available at end of line */

static int Save_line( char *line, int output );

int Read_status_info( char *printer, int ack_needed, int sock,
	char *host, int output, int timeout )
{
	int i, cnt, next, header_len, len;
	int status;
	char statusline[LARGEBUFFER];
	char line[LINEBUFFER];
	char *s, *end, *colon;

	DEBUG0("Read_status_info: printer '%s'", printer );
	statusline[0] = 0;
	cnt = next = 0;
	header_len = 1;
	status = 0;
	colon = 0;

	if( ack_needed ){
		i = 0;
		status = Link_ack( host, &sock, timeout, 0, &i );
		if( status == 0 ) return( 0 );
		if( isprint(i) ){
			statusline[0] = i;
		}
	}
	/* long status - trim lines */
	DEBUG3("Read_status_info: Longformat %d, Displayformat %d",
		Longformat, Displayformat );
	do{
		DEBUG3("Read_status_info: left '%s'", statusline );
		s = &statusline[strlen(statusline)];
		i = (sizeof(statusline)-2) - strlen(statusline);
		if( i <= 0 ){
			s[0] = '\n';
			s[1] = 0;
		} else {
			/* read the status line */
			status = Link_read( host, &sock, timeout, s, &i );
			if( status || i == 0 ) break;
			s[i] = 0;
		}
		DEBUG3("Read_status_info: got '%s'", s );
		/* now we have to split line up */
		for( s = statusline; s ; s = end ){
			end = strchr( s, '\n' );
			if( end ){
				*end++ = 0;
			} else {
				/* copy uncompleted line to start of line */
				if( s != statusline ){
					len = strlen(s);
					if( len > sizeof(statusline) - 2 ){
						len = sizeof(statusline) - 2;
						s[len] = 0;
					}
					/* strcpy does not do overlapping strings */
					for( i = 0; (statusline[i] = s[i]); ++i );
				}
				DEBUG3("Read_status_info: reached end, left '%s'", statusline );
				continue;
			}
			DEBUG3("Read_status_info: line found '%s'", s );
			/* this is ugly, but it works to eliminate problems */
			plp_snprintf( line, sizeof(line)-2, "%s", s );
			strcat( line, "\n" );
			if( Save_line( line, output ) < 0 ) return(1);
		}
	}while( status == 0 );
	if( strlen( statusline ) ){
		plp_snprintf( line, sizeof(line) -2, "%s", s );
		strcat( line, "\n" );
		if( Save_line( line, output ) < 0 ) return(1);
	}
	DEBUG1("Read_status_info: EOF");
	if( Save_line( 0, output ) < 0 ) return( 1 );
	return(0);
}


/***************************************************************************
 * static int Save_line( char *line, int output )
 *  - if the Longformat variable is non-zero,  then we save the
 *  status and then process it later.
 * Note that we will only save the status if necessary.
 ***************************************************************************/
static int Analyze( char *line, int index, struct malloc_list *status );

static struct malloc_list status_lines, status_list;
char *last_buffer;
int   last_len;

static int Save_line( char *line, int output )
{
	int index;
	char **buffer;

	if( Longformat ){
		if( status_lines.count + 2 >= status_lines.max ){
			extend_malloc_list( &status_lines, sizeof(buffer[0]),
			10,__FILE__,__LINE__  );
		}
		buffer = (void *)status_lines.list;
		DEBUG1("Save_line: status_lines buff 0x%x, count %d, max %d",
			buffer, status_lines.count, status_lines.max );
		index = status_lines.count;
		if( line ){
			line = add_str( &status_list, line,__FILE__,__LINE__  );
			buffer[status_lines.count++] = line;
		}
		buffer[status_lines.count] = 0;
		Analyze( line, index, &status_lines );
	} else {
		if( Write_fd_str( output, line ) < 0 ) cleanup(0);
	}
	return(0);
}

static char printer[LINEBUFFER];
static char lastjob[LINEBUFFER];
static char text[LINEBUFFER];
static char offline[LINEBUFFER];
static char disabled[LINEBUFFER];
static char available[LINEBUFFER];
static int  pr_start;
static int spooling;
static int unavailable;
static char jobstatus[LINEBUFFER];
static int  jobs;
static int  status_index;
static int  status_count;

#define PR_COL 1
#define ST_COL 2
#define QU_COL 3
#define WA_COL 4
#define RA_COL 5
#define PS_COL 6
#define SP_COL 7
#define SR_COL 8

struct keywords colonkey[] = {
	{ "Printer:", PR_COL },
	{ "Queue:", QU_COL },
	{ "Warning:", WA_COL },
	{ "Status:", ST_COL },
	{ "Rank", RA_COL },
	{ "(printing", PS_COL },
	{ "(spooling", SP_COL },
	{ "Server", SR_COL },
	{ 0 }
};

/***************************************************************************
 * static int Analyze( char *line, int index, struct malloc_list *status )
 * Analyze the returned status,  and reformat it.
 *  Note:  this also does not print status information that has already
 *  been displayed.
 ***************************************************************************/

static int pr_count;
static int Analyze( char *line, int index, struct malloc_list *status )
{
	char msg[LINEBUFFER];
	char *field[LINEBUFFER+1];
	char **fields = &field[0];
	int fieldcount, i, j, c, colon, len;
	char *s, *t, *end;
	char copy[LINEBUFFER];
	char **lines;

	fieldcount = 0;
	colon = 0;
	fields[0] = 0;
	if( line ){
		safestrncpy( copy, line );
		for( s = copy;s && *s; s = end ){
			while( isspace( *s ) ) ++s;
			if( *s == 0 ) break;
			end = strpbrk( s, " \t\n" );
			if( end ) *end++ = 0;
			fields[fieldcount++] = s;
			fields[fieldcount] = 0;
		}
	}
	if( (s = fields[0]) ){
		for( i = 0; colon == 0 && (t = colonkey[i].keyword); ++i ){
			if( strcmp(s, t ) == 0 ){
				colon = colonkey[i].type;
			}
		}
	}
	if(DEBUGL3){
		logDebug( "Analyze: fieldcount %d, colon %d, line '%s'",
			fieldcount, colon, line );
		for( i = 0; fields[i]; ++i ){
			logDebug( " [%d] '%s'", i, fields[i] );
		}
	}

	if( fieldcount == 0 || colon == PR_COL || colon == SR_COL ){
		if( printer[0] ){
			if( Pr_status_check( printer ) == 0 ){
			lines = status->list;
			if( LP_mode ){
				if( pr_count++ && Lp_status && ( Write_fd_str( 1, "\n" ) < 0 ) ) cleanup(0);
				status_count = 0;
				if( status_index ){
					status_count = index - status_index;
				}
				if( jobs ){
					strcpy( available, "unavailable" );
				}
				DEBUG0( "PRINTER '%s' DISABLED %s OFFLINE %s", 
					printer, disabled, offline );
				DEBUG0( "   JOBCOUNT %d", jobs );
				DEBUG0( "   STATUS time '%s' value '%s'", 
					lastjob, jobstatus );
				DEBUG0( "   UNAVAILABLE %d", unavailable );
				DEBUG0( "   AVAILABLE %s", available );
				DEBUG0( "   STATUS_INDEX %d, STATUS_COUNT %d",
					status_index, status_count );
				if( Lp_accepting && spooling == 0 ){
					/* $pr accepting requests since $lastjob */
					s = lastjob;
					if( lastjob[0] == 0 ){
						s = Time_str( 1, time( 0 ) );
					}
					plp_snprintf(msg, sizeof(msg),
						"%s accepting requests since %s\n",
							printer, s );
					if( Write_fd_str( 1, msg ) < 0 ) cleanup(0);
				}
				if( Lp_status ){
					s = lastjob;
					if( lastjob[0] == 0 ){
						s = Time_str( 1, time( 0 ) );
					}
					plp_snprintf( msg, sizeof(msg),
						"printer %s %s. %s since %s. %s.\n",
							printer, offline, disabled, s, available );
					if( Write_fd_str( 1, msg ) < 0 ) cleanup(0);
					if( status_count > 0 ){
						for( i = -1; i < status_count; ++i ){
							if( Write_fd_str( 1, lines[i+status_index] ) < 0 ) cleanup(0);
						}
					}
				}
			} else {
				DEBUG2( "Analyze: pr_start %d, index %d, Status_line_count %d",
					pr_start, index, Status_line_count );
				/* we scan the lines for duplicate information */
				if( Status_line_count ){
					char *colon;
					char header[LINEBUFFER];
					int header_len = 0;
					int count = 0;
					header[0] = 0;

					/*
					 * we walk through the lines, looking for duplicates
					 * on the following lines.
					 * header: the previous line header - if same as this line,
					 *   this line is a candidate.
					 * header_len: the length of the header.
					 * count: numbers of lines in succession the same
					 */
					for( i = pr_start; i < index; ++i ){
						s = lines[i];
						if( s == 0 || *s == 0 ){
							header_len = 0;
							continue;
						} 
						DEBUG2( "Analyze: header_len %d, header '%s', line [%d] '%s'",
							header_len, header, i, s );
						if( header_len && strncmp( header, s, header_len ) == 0){
							if( count >= Status_line_count ){
								DEBUG2( "Analyze: deleting %d", i-count );
								lines[i-count] = 0;
							} else {
								++count;
							}
						} else {
							header_len = 0;
							DEBUG2( "Analyze: new header %d", i );
							if( (colon = strchr( s, ':' )) ){
								header_len = (colon - s) + 1;
								if( header_len >= sizeof( header )-1 ){
									header_len = sizeof( header )-1;
								}
								strncpy( header, s, header_len );
								header[header_len] = 0;
								count = 1;
							}
						}
					}
				}
				if( pr_count++ && ( Write_fd_str( 1, "\n" ) < 0 ) ) cleanup(0);
				for( i = pr_start; i < index; ++i ){
					if( (s = lines[i]) && *s ){
						if( Write_fd_str( 1, s ) < 0 ) cleanup(0);
					}
				}
			}
		} /* we have done the printer */
		} else {
			/* we simply dump the saved information */
			lines = status->list;
			for( i = pr_start; i < index; ++i ){
				if( (s = lines[i]) && *s ){
					if( Write_fd_str( 1, s ) < 0 ) cleanup(0);
				}
			}
		}
		printer[0] = 0;
		strcpy( offline, "online" );
		strcpy( disabled, "enabled" );
		strcpy( available, "available" );
		spooling = 0;
		pr_start = index;
		unavailable = 0;
		lastjob[0] = 0;
		jobstatus[0] = 0;
		text[0] = 0;
		jobs = 0;
		status_index = 0;
		status_count = 0;
	}
	switch( colon ){
	case SR_COL:
		++fields;
	case PR_COL:
		/* next printer entry */
		DEBUG2("Analyze: next entry '%s'", fields[1] );
		strcpy( printer, fields[1] );
		pr_start = index;
		if( (s = strchr( line, '\'' )) ){
			t = strchr( s+1, '\'' );
			if( t ){
				len = t - s;
				strncpy( text, s, len );
				text[len] = 0;
			}
		}
		for( i = 2; (s = fields[i]); ++i ){
			for( j = 0; (t = colonkey[j].keyword); ++j ){
				if( strcmp(s, t ) == 0 ){
					/* we found a match */
					c =  colonkey[j].type;
					switch( c ){
					case PS_COL: strcpy( offline, "offline" ); break;
					case SP_COL:
						spooling = 1;
						strcpy( disabled, "disabled");
						strcpy( available, "unavailable");
						break;
					}
				}
			}
		}
		break;
	case QU_COL:
		jobs = atoi( fields[1] );
		DEBUG2("Analyze: job count '%d'", jobs );
		break;
	case ST_COL:
		DEBUG2("Analyze: status '%s'", line );
		s = strchr( line, ':' );
		if( s ){
			++s;
			while( isspace( *s ) ) ++s;
			safestrncpy( jobstatus, s );
			if( (s = strstr( jobstatus, " at " )) ){
				*s = 0;
			}
		}
		/* find the time */
		s = fields[fieldcount-1];
		if( isdigit( s[0] ) ){
			safestrncpy( lastjob, s );
			DEBUG2("Analyze: lastjob '%s'", lastjob );
		}
		break;
	case WA_COL:
		if( (s = strstr( line, "no server" )) ){
			unavailable = 1;
		}
		DEBUG2("Analyze: unavailable '%d'", unavailable );
		break;
	case RA_COL:
		status_index = index+1;
		break;
	default:
		DEBUG2("Analyze: default '%s'", line );
		break;
	}
	return(0);
}

static struct malloc_list pr_sent;

/***************************************************************************
 * int Pr_status_check( char *name )
 *  Check to see if the printer with the same status name
 *  has been printed.  As a side effect,
 *  if name is 0 (null),  then we reset the list
 *  as well as status printing.
 ***************************************************************************/
int Pr_status_check( char *name )
{
	char **list;
	int i;

	DEBUG0("Pr_status_check: checking '%s'", name );

	if( name == 0 ){
		pr_count = 0;
		clear_malloc_list( &pr_sent, 1 );
		clear_malloc_list( &status_lines, 0 );
		clear_malloc_list( &status_list, 1 );
		last_buffer = 0;
		last_len = 0;
		return( 0 );
	}
	if( name[0] ){
		if( pr_sent.count+2 >= pr_sent.max ){
			extend_malloc_list( &pr_sent, sizeof( list[0] ),
			pr_sent.count+10,__FILE__,__LINE__  );
		}
		list = pr_sent.list;
		for( i = 0; i < pr_sent.count; ++i ){
			if( strcmp( name, list[i] ) == 0 ){
				DEBUG0("Pr_status_check: found '%s'", name );
				return( 1 );
			}
		}
		list[pr_sent.count++] = safestrdup( name );
		DEBUG0("Pr_status_check: added '%s'", name );
	} else {
		return(1);
	}
	return(0);
}
