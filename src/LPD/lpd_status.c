/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpd_status.c
 * PURPOSE: return status
 **************************************************************************/

static char *const _id =
"$Id: lpd_status.c,v 3.14 1997/03/24 00:45:58 papowell Exp papowell $";

#include "lp.h"
#include "printcap.h"
#include "cleantext.h"
#include "fileopen.h"
#include "gethostinfo.h"
#include "getqueue.h"
#include "jobcontrol.h"
#include "killchild.h"
#include "linksupport.h"
#include "malloclist.h"
#include "pathname.h"
#include "patselect.h"
#include "permission.h"
#include "sendlpq.h"
#include "serverpid.h"
#include "setup_filter.h"
#include "setupprinter.h"
#include "utilities.h"
#include "checkremote.h"

/**** ENDINCLUDE ****/

/***************************************************************************
Commentary:
Patrick Powell Tue May  2 09:32:50 PDT 1995

Return status:
	status has two formats: short and long

Status information is obtained from 3 places:
1. The status file
2. any additional progress files indicated in the status file
3. job queue

The status file is maintained by the current unspooler process.
It updates this file with the following information:

PID of the unspooler process   [line 1]
active job and  status file name
active job and  status file name

Example 1:
3012
cfa1024host status

Example 2:
3015
cfa1024host statusfd2
cfa1026host statusfd1

Format of the information reporting:

0        1         2         3         4         5         6         7
12345678901234567890123456789012345678901234567890123456789012345678901234
 Rank  Owner           Class Job  Files                                 Size
1      papowell@hostname   A 040  standard input                        3
2      papowell@hostname   A 041  standard input                        3
     -7                 -20 1 -4 2                                34    5
 ***************************************************************************/

#define RANKW 7
#define OWNERW 30
#define CLASSW 1
#define JOBW 4
#define FILEW 18
#define SIZEW 5
#define TIMEW 8

#define MAXLEN 79

static void Get_queue_status( char *name, int *socket, int displayformat,
	int tokencount, struct token *tokens, struct printcap_entry *pc );
static void Print_status_info( int *socket, char *path, char *header );

int Job_status( int *socket, char *input, int maxlen )
{
	struct token tokens[20];
	char error[LINEBUFFER];
	int tokencount;
	int i, c;
	char *name, *s;
	int permission;
	int displayformat;

	Name = "Job_status";

	/* get the format */
	displayformat = input[0];

	++input;
	if( (s = strchr(input, '\n' )) ) *s = 0;
	DEBUG2("Job_status: doing '%s'", input );

	/* check printername for characters, underscore, digits */
	tokencount = Crackline(input, tokens, sizeof(tokens)/sizeof(tokens[0]));

	if( tokencount == 0 ){
		plp_snprintf( error, sizeof(error),
			_("missing printer name"));
		goto error;
	}

	for( i = 0; i < tokencount; ++i ){
		tokens[i].start[tokens[i].length] = 0;
	}

	name = tokens[0].start;
	setproctitle( "lpd %s '%s'", Name, name );
	
	if( (s = Clean_name( name )) ){
		plp_snprintf( error, sizeof(error),
			_("printer '%s' has illegal char '%c' in name"), name, *s );
		goto error;
	}

	Printer = name;
	Perm_check.service = 'Q';
	Perm_check.printer = Printer;
	Init_perms_check();
	if( (permission = Perms_check( &Perm_file, &Perm_check,
			Cfp_static )) == REJECT
		|| (permission == 0 && Last_default_perm == REJECT) ){
		plp_snprintf( error, sizeof(error),
			_("%s: no permission to show status"), Printer );
		goto error;
	}


	if( strcmp( name, "all" ) ){
		DEBUG3( "Job_status: checking printcap entry '%s'",  name );
		Get_queue_status( name, socket, displayformat,
			tokencount - 1, &tokens[1], (void *)0 );
	} else {
		/* we work our way down the printcap list, checking for
			ones that have a spool queue */
		/* note that we have already tried to get the 'all' list */
		if( All_list.count ){
			char **line_list;
			struct printcap_entry *entry;
			DEBUG3("Job_status: using the All_list" );
			line_list = All_list.list;
			for( i = 0; i < All_list.count; ++i ){
				char orig_name[LINEBUFFER];
				Printer = line_list[i];
				if( Printer == 0 || *Printer == 0 || ispunct( *Printer ) ){
					continue;
				}
				safestrncpy( orig_name, Printer );
				Printer = Find_printcap_entry( orig_name, &entry );
				if( Printer == 0 || *Printer == 0 || ispunct( *Printer ) ){
					continue;
				}
				if( entry->status_done ) continue;
				DEBUG3("Job_status: checking printcap entry '%s'",orig_name);
				Get_queue_status( orig_name, socket, displayformat,
					tokencount - 1, &tokens[1], entry );
			}
		} else if( Expanded_printcap_entries.count > 0 ){
			struct printcap_entry *entries, *entry;
			DEBUG3("checkpc: using the printcap list" );
			entries = (void *)Expanded_printcap_entries.list;
			c = Expanded_printcap_entries.count;
			for( i = 0; i < c; ++i ){
				entry = &entries[i];
				Printer = entry->names[0];
				DEBUG3("Job_status: printcap entry [%d of %d] '%s'",
					i, c,  Printer );
				if( Printer == 0 || *Printer == 0 || ispunct( *Printer ) ){
					continue;
				}
				if( entry->status_done ) continue;
				Get_queue_status( Printer, socket, displayformat,
					tokencount - 1, &tokens[1], entry );
			}
		}
	}
	DEBUG3( "Job_status: DONE" );
	return(0);

error:
	log( LOG_INFO, _("Job_status: error '%s'"), error );
	DEBUG2("Job_status: error msg '%s'", error );
	i = strlen(error);
	if( i >= sizeof(error) ){
		i = sizeof(error) - 1;
	}
	error[i] = '\n';
	if( Write_fd_str( *socket, error ) < 0 ) cleanup(0);
	DEBUG3( "Job_status: done" );
	return(0);
}


/***************************************************************************
 * void Get_queue_status( char *name, int *socket, int displayformat,
 *	int tokencount, struct token *tokens, struct printcap *pc )
 * name  - printer name
 * socket - used to send information
 * displayformat - REQ_DSHORT, REQ_DLONG, REQ_VERBOSE
 * tokencount, tokens - cracked arguement string
 *  - get the printcap entry (if any)
 *  - check the control file for current status
 *  - find and report the spool queue entries
 ***************************************************************************/

static int subserver;
static char *buffer;
static int bsize;

static void Get_queue_status( char *name, int *socket, int displayformat,
	int tokencount, struct token *tokens, struct printcap_entry *pc )
{
	struct malloc_list servers;
	struct server_info *server_info;
	char *s, *host, *logname; /* ACME pointer for writing */
	int i, j, status, len, count, hold_count;
	char number[LINEBUFFER];		/* printable form of small integer */
	int serverpid, unspoolerpid;	/* pid of server and unspooler */
	int fd;					/* file descriptor for file */
	struct stat statb;		/* stat of file */
	struct control_file *cfp, **cfpp;	/* pointer to control file */
	char error[LINEBUFFER];	/* for errors */
	char msg[SMALLBUFFER];	/* for messages */
	char header[LINEBUFFER];	/* for a header */
	int select;				/* select this job for display */
	char *path;				/* path to use */
	char *pr;
	int jobnumber;			/* job number */
	struct destination *destination, *d;
	int nodest;				/* no destination information */
	int permission;			/* permission */
	char orig_name[LINEBUFFER];

	DEBUG3( "Get_queue_status: checking '%s'", name );

	/* set printer name and printcap variables */
	memset( &servers, 0, sizeof(servers) );
	error[0] = 0;
	header[0] = 0;
	msg[0] = 0;
	safestrncpy( orig_name, name );

	if(DEBUGL4 ){
		for( i = 0; i < tokencount; ++i ){
			logDebug( "token[%d] '%s'", i, tokens[i].start );
		}
	}

	status = Setup_printer( orig_name, error, sizeof(error),
		debug_vars,1, (void *)0, &pc );


	/* check for permissions */
	Perm_check.printer = Printer;
	Init_perms_check();
	if( (permission = Perms_check( &Perm_file, &Perm_check,
			Cfp_static )) == REJECT
		|| (permission == 0 && (permission = Perms_check( &Local_perm_file,
				&Perm_check, Cfp_static )) == REJECT)
		|| (permission == 0 && Last_default_perm == REJECT) ){
		plp_snprintf( error, sizeof(error),
			_("%s: no permission to list jobs"), Printer );
		goto error;
	}

	/* set up status */

	if( pc ){
		if( pc->status_done ){
			return;
		}
		pc->status_done = 1;
	}

	if( displayformat != REQ_DSHORT ){
		plp_snprintf( msg, sizeof(msg), "%s: ",
			subserver?"Server Printer":"Printer" );
	}
	len = strlen(msg);
	plp_snprintf( msg+len, sizeof(msg)-len, "%s@%s ",
		Printer, ShortHost );
	if( strcmp( orig_name, Printer ) ){
		len = strlen(msg);
		plp_snprintf( msg+len, sizeof(msg)-len, _("(originally %s) "), orig_name );
	}

	if( status ){
		DEBUG3("Get_queue_status: Setup_printer status %d '%s'", status, error );
		if( status != 2 ){
			goto error;
		}
		pr = RemotePrinter;
		host = RemoteHost;
		len = strlen( msg );
		if( pr == 0 && host == 0 ){
			if( displayformat == REQ_VERBOSE ){
				safestrncat( msg, _("\n Error: ") );
				len = strlen( msg );
			}
			plp_snprintf( msg+len, sizeof(msg)-len,
				_(" printer %s@%s not in printcap"), Printer, ShortHost );
		} else {
			if( host == 0 ) host = Default_remote_host;
			if( pr == 0 ) pr = Printer;
			if( displayformat == REQ_VERBOSE ){
				plp_snprintf( msg+len, sizeof(msg)-len,
					_("\n Forwarding_only: %s@%s"), pr, host );
			} else {
				plp_snprintf( msg+len, sizeof(msg)-len,
					_(" (forwarding to %s@%s)"), pr, host );
			}
		}
		safestrncat( msg, "\n" );
		DEBUG3("Get_queue_status: forward msg '%s'", msg );
		if( Write_fd_str( *socket, msg ) < 0 ) cleanup(0);
		goto remote;
	}

	/* get the spool entries */
	Scan_queue( 1, 1 );
	DEBUG3("Get_queue_status: total files %d", C_files_list.count );
	error[0] = 0;

	DEBUG3("Get_queue_status: RemoteHost '%s', RemotePrinter '%s', Lp '%s'",
		RemoteHost, RemotePrinter, Lp_device );
	if( RemoteHost && RemotePrinter ){
		len = strlen( msg );
		if( displayformat == REQ_VERBOSE ){
			plp_snprintf( msg+len, sizeof(msg)-len,
				"\n %s: %s@%s",
					Forwarding?"Forwarding":"Destination",
			RemotePrinter, RemoteHost );
		} else {
			plp_snprintf( msg+len, sizeof(msg)-len,
				_("(dest %s@%s)"), RemotePrinter, RemoteHost );
		}
	}

	if( displayformat != REQ_DSHORT ){
		s = 0;
		if( (s = Comment_tag) == 0 ){
			if( pc->namecount > 1 ){
				s = pc->names[pc->namecount-1];
			}
		}
		if( s ){
			len = strlen( msg );
			if( displayformat == REQ_VERBOSE ){
				plp_snprintf( msg+len, sizeof(msg) - len, _("\n Comment: %s"), s );
			} else {
				plp_snprintf( msg+len, sizeof(msg) - len, " '%s'", s );
			}
		}
	}

	memset(&statb, 0, sizeof (statb));
	Get_spool_control(&statb, (void *)0 );
	len = strlen( msg );
	if( displayformat == REQ_VERBOSE ){
		plp_snprintf( msg+len, sizeof(msg) - len,
			_("\n Printing: %s\n Spooling: %s"),
				Printing_disabled?"no":"yes",
				Spooling_disabled?"no":"yes");
	} else {
		if( Printing_disabled || Spooling_disabled ){
			plp_snprintf( msg+len, sizeof(msg) - len,
				" (%s%s%s)",
				Printing_disabled?"printing disabled":"",
				(Printing_disabled&&Spooling_disabled)?", ":"",
				Spooling_disabled?"spooling disabled":"" );
		}
	}
	if( Bounce_queue_dest ){
		len = strlen( msg );
		if( displayformat == REQ_VERBOSE ){
			plp_snprintf( msg+len, sizeof(msg) - len,
				_("\n Bounce_queue: %s"), Bounce_queue_dest );
			if( Routing_filter ){
				len = strlen( msg );
				plp_snprintf( msg+len, sizeof(msg) - len,
					_("\n Routing_filter: %s"), Routing_filter );
			}
		} else {
			plp_snprintf( msg+len, sizeof(msg) - len,
				_(" (%sbounce to %s)"),
				Routing_filter?"routed/":"", Bounce_queue_dest );
		}
	}

	/*
	 * check to see if this is a server or subserver.  If it is
	 * for subserver,  then you can forget starting it up unless started
	 * by the server.
	 */

	if( Server_names ){
		if( subserver ){
			plp_snprintf( error, sizeof(error), _("%s is already a subserver!"),
				Printer );
			goto error;
		}
		Get_subserver_info( &servers, Server_names );
		len = strlen( msg );
		if( displayformat == REQ_VERBOSE ){
			plp_snprintf( msg+len, sizeof(msg) - len,
				_("\n Subservers: ") );
		} else {
			plp_snprintf( msg+len, sizeof(msg) - len,
				_(" (subservers") );
		}
		server_info = (void *)servers.list;
		for( i = 0; i < servers.count; ++i ){
			len = strlen( msg );
			plp_snprintf( msg+len, sizeof(msg) - len,
				"%s %s", (i > 0)?",":"", server_info[i].name );
		}
		if( displayformat != REQ_VERBOSE ){
			safestrncat( msg, ") " );
		}
	}
	if( Server_queue_name ){
		len = strlen( msg );
		if( displayformat == REQ_VERBOSE ){
			plp_snprintf( msg+len, sizeof(msg) - len,
				_("\n Serving: %s"), Server_queue_name );
		} else {
			plp_snprintf( msg+len, sizeof(msg) - len,
				_(" (serving %s)"), Server_queue_name );
		}
	}

	/* set up the short format for folks */
	cfpp = (void *)C_files_list.list;
	Job_count( &hold_count, &count );

	/* this gives a short 1 line format with minimum info */
	if( displayformat == REQ_DSHORT ){
		len = strlen( msg );
		plp_snprintf( msg+len, sizeof(msg) - len, _(" %d job%s\n"),
			count, (count == 1)?"":"s" );
	} else {
		safestrncat( msg, "\n" );
	}
	if( Write_fd_str( *socket, msg ) < 0 ) cleanup(0);
	msg[0] = 0;

	if( displayformat == REQ_DSHORT ) goto remote;

	/* now check to see if there is a server and unspooler process active */
	path = Add_path( CDpathname, Printer );
	serverpid = 0;
	if( (fd = Checkread( path, &statb ) ) >= 0 ){
		serverpid = Read_pid( fd, (char *)0, 0 );
		close( fd );
	}
	DEBUG3("Get_queue_status: server pid %d", serverpid );
	if( serverpid > 0 && kill( serverpid, 0 ) ){
		DEBUG3("Get_queue_status: server %d not active", serverpid );
		serverpid = 0;
	} /**/

	path = Add2_path( CDpathname, "unspooler.", Printer );
	unspoolerpid = 0;
	if( (fd = Checkread( path, &statb ) ) >= 0 ){
		unspoolerpid = Read_pid( fd, (char *)0, 0 );
		close( fd );
	}

	DEBUG3("Get_queue_status: unspooler pid %d", unspoolerpid );
	if( unspoolerpid > 0 && kill( unspoolerpid, 0 ) ){
		DEBUG3("Get_queue_status: unspooler %d not active", unspoolerpid );
		unspoolerpid = 0;
	} /**/

	if( count == 0 ){
		safestrncpy( msg, _(" Queue: no printable jobs in queue\n") );
	} else {
		/* check to see if there are files and no spooler */
		plp_snprintf( msg, sizeof(msg), _(" Queue: %d printable job%s\n"),
			count, count > 1 ? "s" : "" );
	}
	if( Write_fd_str( *socket, msg ) < 0 ) cleanup(0);
	if( hold_count ){
		plp_snprintf( msg, sizeof(msg), 
		_("  Holding: %d held jobs in queue\n"), hold_count );
		if( Write_fd_str( *socket, msg ) < 0 ) cleanup(0);
	}

	msg[0] = 0;
	if( count && serverpid == 0 ){
		safestrncpy(msg, _(" Server: no server active") );
	} else if( serverpid ){
		len = strlen(msg);
		plp_snprintf( msg+len, sizeof(msg)-len, _(" Server: pid %d active"),
			serverpid );
	}
	if( unspoolerpid ){
		if( msg[0] ){
			safestrncat( msg, (displayformat == REQ_VERBOSE )?", ":"\n");
		}
		len = strlen(msg);
		plp_snprintf( msg+len, sizeof(msg)-len, _(" Unspooler: pid %d active"),
			unspoolerpid );
	}
	if( msg[0] ){
		safestrncat( msg, "\n" );
		if( Write_fd_str( *socket, msg ) < 0 ) cleanup(0);
	}

	if( Classes ){
		plp_snprintf( msg, sizeof(msg), _(" Classes: %s\n"), Classes );
		if( Write_fd_str( *socket, msg ) < 0 ) cleanup(0);
	}

	/*
	 * get the last status of the spooler
	 */
	DEBUG3("Get_queue_status: Max_status_size '%d'", Max_status_size );
	if( Max_status_size <= 0 ) Max_status_size = 1;
	len = Max_status_size*1024;
	if( bsize < len ){
		if( buffer ) free( buffer );
		bsize = len;
		malloc_or_die( buffer, bsize+2 );
	}
	path = Add2_path( CDpathname, "status.", Printer );
	Print_status_info( socket, path, " Status" );

	if( Status_file && *Status_file ){
		if( Status_file[0] == '/' ){
			path = Status_file;
		} else {
			path = Add_path( SDpathname, Status_file );
		}
		Print_status_info( socket, path, _(" Filter_status") );
	}

	if( C_files_list.count > 0 ){
		/* send header */
/*
 Rank  Owner/ID        Class Job Files                           Size Time
*/
		if( displayformat != REQ_VERBOSE ){
			plp_snprintf( msg, sizeof(msg),
				"%-*s %-*s %-*s %-*s %-*s %*s %-*s\n",
				RANKW, " Rank", OWNERW-4, "Owner/ID",CLASSW,"Class",
				JOBW,"Job",FILEW,"Files",SIZEW,"Size",TIMEW,"Time" );
			if( Write_fd_str( *socket, msg ) < 0 ) cleanup( 0 );
		}
		jobnumber = 0;
		for( i = 0; i < C_files_list.count; ++i ){
			cfp = cfpp[i];

			DEBUG3("Get_queue_status: job name '%s' id '%s'",
				cfp->original, cfp->identifier+1 );

			/*
			 * check to see if this entry matches any of the patterns
			 */

			destination = 0;
			if( tokencount > 0 ){
				select = 1;
				for( j = 0; select && j < tokencount; ++j ){
					select = Patselect( &tokens[j], cfp, &destination );
				}
				DEBUG3("Get_queue_status: job name '%s' select '%d'",
					select );
				if( !select ) continue;
			}
			/* set a flag to suppres destinations */
			nodest = 0;

			/* we report this jobs status */

			number[0] = 0;
			error[0] = 0;
			if( cfp->error[0] ){
				strcpy( number, "error" );
				plp_snprintf( error, sizeof( error ),
					"ERROR: %s", cfp->error );
				nodest = 1;
			} else if( cfp->statb.st_size == 0 ){
				/* ignore zero length control files */
				continue;
			} else if( cfp->hold_info.receiver > 0
				&& kill( cfp->hold_info.receiver, 0 ) == 0 ){
				/* ignore jobs being transferred */
				continue;
			} else if( cfp->hold_info.hold_time ){
				strcpy( number, "hold" );
				nodest = 1;
			} else if( cfp->hold_info.remove_time ){
				strcpy( number, "remove" );
				nodest = 1;
			} else if( cfp->hold_info.done_time ){
				strcpy( number, "done" );
				nodest = -1;
			} else if( cfp->hold_info.held_class ){
				strcpy( number, "class" );
			} else if( cfp->hold_info.active > 0
				&& kill( cfp->hold_info.active, 0 ) == 0 ){
				strcpy( number, "active" );
				nodest = -1;
			} else if( cfp->hold_info.redirect[0] ){
				plp_snprintf( number, sizeof(number), _("redirect->%s"),
					cfp->hold_info.redirect );
				nodest = 1;
			} else {
				plp_snprintf( number, sizeof(number), "%d",
					jobnumber+1, cfp->hold_info.redirect );
			}
			++jobnumber;

			if( displayformat != REQ_VERBOSE ){
				if( error[0] == 0 && cfp->JOBNAME ){
					safestrncat( error, cfp->JOBNAME+1 );
				}
				host = cfp->FROMHOST?cfp->FROMHOST+1:"???";
				if( (s = strchr( host, '.' )) ) *s = 0;
				logname = cfp->LOGNAME?cfp->LOGNAME+1:"???";

				DEBUG3("Get_queue_status: destination count '%d', nodest %d",
					cfp->destination_list.count, nodest );
				/* do the number, owner, and job information */
				plp_snprintf( msg, sizeof(msg),
					"%-*s %-*s %-*c %*d %-*s",
					RANKW, number, OWNERW,
					cfp->identifier+1,CLASSW,cfp->priority,
					JOBW,cfp->number,FILEW, error );
				if( cfp->error[0] == 0 ){
					char sizestr[SIZEW+TIMEW+32];
					/* pad with blanks */
					for( len = strlen(msg); len <= MAXLEN; ++len ){
						msg[len] = ' ';
					}
					plp_snprintf( sizestr, sizeof(sizestr), " %d %s",
						cfp->jobsize, Time_str( 1, cfp->statb.st_ctime ) );
					len = strlen( sizestr );
					strncpy( msg+MAXLEN-len, sizestr, len+1 );
				}
				safestrncat(msg, "\n");
				if( Write_fd_str( *socket, msg ) < 0 ) cleanup( 0 );
			} else {
				plp_snprintf( header, sizeof(header),
					_(" Job: %s"),
					cfp->identifier[1]?cfp->identifier+1: cfp->transfername );
				plp_snprintf( msg, sizeof(msg), _("%s status= %s\n"),
					header, number );
				if( Write_fd_str( *socket, msg ) < 0 ) cleanup( 0 );
				plp_snprintf( msg, sizeof(msg), _("%s size= %d\n"),
					header, cfp->jobsize );
				if( Write_fd_str( *socket, msg ) < 0 ) cleanup( 0 );
				plp_snprintf( msg, sizeof(msg), _("%s time= %s\n"),
					header, Time_str( 1, cfp->statb.st_ctime ) );
				if( Write_fd_str( *socket, msg ) < 0 ) cleanup( 0 );
				if( cfp->error[0] ){
					plp_snprintf( msg, sizeof(msg), _("%s error= %s\n"),
							header, cfp->error );
					if( Write_fd_str( *socket, msg ) < 0 ) cleanup( 0 );
				}
				plp_snprintf( msg, sizeof(msg), _("%s CONTROL=\n"), header );
				if( Write_fd_str( *socket, msg ) < 0 ) cleanup( 0 );
				s = Copy_hf( &cfp->control_file_lines,
						&cfp->control_file_print, 0, " - " );
				if( Write_fd_str( *socket, s ) < 0 ) cleanup( 0 );
				plp_snprintf( msg, sizeof(msg), _("%s HOLDFILE=\n"), header );
				if( Write_fd_str( *socket, msg ) < 0 ) cleanup( 0 );
				s = Copy_hf( &cfp->hold_file_lines, &cfp->hold_file_print,
						0, " - " );
				DEBUG1("Get_queue_status: hold file '%s'", s );
				if( s && Write_fd_str( *socket, s ) < 0 ) cleanup( 0 );
				continue;
			}
	
			if( cfp->destination_list.count > 0 ){
				/* put in the destination information */
				destination = (void *)cfp->destination_list.list;
				for( j = 0; j < cfp->destination_list.count; ++j ){
					d = &destination[j];
					error[0] = 0;
					plp_snprintf( error, sizeof(error), "->%s ",
						d->destination );
					DEBUG3("Get_queue_status: destination active '%d'",
						d->active );
					if( d->active > 0 && kill( d->active, 0 ) ){
						d->active = 0;
					}
					if( d->active ) ++d->copy_done;
					if( d->copies > 1 ){
						len = strlen( error );
						plp_snprintf( error+len, sizeof(error)-len,
							_("<cpy %d/%d> "),
							d->copy_done, d->copies );
					}
					safestrncpy( number, " -" );
					if( d->error[0] ){
						safestrncat( number, _("rterror") );
						len = strlen( error );
						plp_snprintf( error+len, sizeof( error )-len,
							_("ERROR: %s"), d->error );
					} else if( d->active ){
						safestrncat( number, _("actv") );
					} else if( d->hold ){
						safestrncat( number, _("hold") );
					} else if( d->done ){
						safestrncat( number, _("done") );
					}
					plp_snprintf( msg, sizeof(msg),
						"%-*s %-*s %-*c %*d %-*s",
						RANKW, number, OWNERW,
						d->identifier+1,CLASSW,cfp->priority,
						JOBW,cfp->number,FILEW, error );
					if( d->error[0] == 0 ){
						char sizestr[SIZEW+TIMEW+32];
						/* pad with blanks */
						for( len = strlen(msg); len <= MAXLEN; ++len ){
							msg[len] = ' ';
						}
						plp_snprintf( sizestr, sizeof(sizestr), " %d %s",
							cfp->jobsize, Time_str( 1, cfp->statb.st_ctime ) );
						len = strlen( sizestr );
						strncpy( msg+MAXLEN-len, sizestr, len+1 );
					}
					safestrncat(msg, "\n");
					if( Write_fd_str( *socket, msg ) < 0 ) cleanup( 0 );
				}
			}
		}
	}

remote:
	if( Server_names && !subserver ){
		server_info = (void *)servers.list;
		subserver = 1;
		for( i = 0; i < servers.count; ++i ){
			DEBUG3("Get_queue_status: getting subserver status '%s'", 
				server_info[i].name );
			Get_queue_status( server_info[i].name, socket, displayformat,
				tokencount, tokens, 0 );
			DEBUG3("Get_queue_status: finished subserver status '%s'", 
				server_info[i].name );
		}
		if( servers.list ) free( servers.list );
		memset( &servers, 0, sizeof( servers ) );
		subserver = 0;
		RemoteHost = RemotePrinter = 0;
	} else if( Bounce_queue_dest ){
		DEBUG3("Get_queue_status: getting bouncequeue dest status '%s'", 
			Bounce_queue_dest);
		RemoteHost = 0;
		RemotePrinter = 0;
		if( strchr( Bounce_queue_dest, '@' ) ){
			Lp_device = Bounce_queue_dest;
			Check_remotehost();
			if( Check_loop() ){ 
				plp_snprintf( error, sizeof(error),
				"printer '%s' loop to bounce queue '%s'", Bounce_queue_dest );
			}
		}
		if( RemotePrinter == 0 ){
			RemotePrinter = Bounce_queue_dest;
		}
	} else if( Lp_device &&
		(Lp_device[0] == '|' || strchr( Lp_device, '@' ) == 0) ){
			/* if lp=pr@host then we have already set rm:rp
			 * else if lp!=pr@host then we do not want to go to remote host
			 * else if we have :rm:rp: only, and this is pointing to us,
			 * then we have a loop
			 */
		RemoteHost = RemotePrinter = 0;
	}
	if( RemotePrinter && RemoteHost == 0 ){
		RemoteHost = Default_remote_host;
		if( RemoteHost == 0 ){
			RemoteHost = FQDNHost;;
		}
	}
	if( RemoteHost && RemotePrinter ){
		static struct malloc_list args;
		char **list;

		if( subserver ){
			plp_snprintf( error, sizeof(error),
				_("printer '%s' cannot be remote and subserver"), RemotePrinter );
			goto error;
		}
		/* see if on the same host */

		Find_fqdn( &RemoteHostIP, RemoteHost, 0 );
		if( Same_host( &RemoteHostIP, &HostIP ) == 0 ){
			DEBUG3("Get_queue_status: same host, recursing" );
			Get_queue_status( RemotePrinter, socket, displayformat,
				tokencount, tokens, 0);
			DEBUG3("Get_queue_status: finished status '%s'", orig_name );
		} else {
			if( tokencount+1 >= args.max ){
				extend_malloc_list( &args, sizeof( char *), tokencount+1 );
			}
			args.count = 0;
			list = (void *)args.list;
			for( args.count = 0, i = 0; i < tokencount; ++i, ++args.count ){
				list[args.count] = tokens[i].start;
			}
			list[args.count] = 0;
			DEBUG3("Get_queue_status: getting status from remote host '%s'", 
				RemoteHost );
			/* get extended format */
			Send_lpqrequest( RemotePrinter, RemoteHost, 
				displayformat, list,
				Connect_timeout, Send_timeout, *socket );
		}
	}
	DEBUG3("Get_queue_status: finished '%s'", name );
	return;

error:
	log( LOG_INFO, _("Get_queue_status: error '%s'"), error );
	DEBUG2("Get_queue_status: error msg '%s'", error );
	safestrncpy( header, _(" ERROR: ") );
	safestrncat( header, error );
	safestrncat( header, "\n" );
	if( Write_fd_str( *socket, header ) < 0 ) cleanup(0);
	DEBUG3( "Get_queue_status: done" );
	return;
}

static void Print_status_info( int *socket, char *path, char *header )
{
	int fd, len, i;
	off_t off = 0;
	char *s, *endbuffer;
	struct stat statb;		/* stat of file */
	char line[LINEBUFFER];

	DEBUG1("Print_status_info: socket %d, path '%s', header '%s'",
		*socket, path, header );
	if( (fd = Checkread( path, &statb ) ) >= 0 ){
		if( statb.st_size > bsize ){
			off = statb.st_size - bsize;
			if( lseek( fd, off, SEEK_SET ) < 0 ){
				logerr_die( LOG_ERR, _("setstatus: cannot seek '%s'"), path );
			}
		}
		for( len = bsize, s = buffer;
			len > 0 && (i = read( fd, s, len ) ) > 0;
			len -= i, s += i );
		*s = 0;
		close(fd);
		if( off && (s = strchr(buffer, '\n'))
			&& (endbuffer = strrchr( s, '\n' )) ){
			++s;
			*++endbuffer = 0;
		} else {
			s = buffer;
		}
		/*
		 * print out only the last complete lines
		 */
		while( s && *s ){
			if( (endbuffer = strchr( s, '\n' )) ) *endbuffer++ = 0;
			plp_snprintf( line, sizeof(line)-2, "%s: %s", header, s );
			DEBUG2("Print_status_info: '%s'", line );
			/* this is safe */
			safestrncat( line, "\n" );
			if( Write_fd_str( *socket, line ) < 0 ) cleanup(0);
			s = endbuffer;
		}
	}
}
