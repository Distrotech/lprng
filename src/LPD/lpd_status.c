/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpd_status.c
 * PURPOSE: return status
 **************************************************************************/

static char *const _id =
"$Id: lpd_status.c,v 3.4 1996/09/09 14:24:41 papowell Exp papowell $";

#include "lpd.h"
#include "printcap.h"
#include "lp_config.h"
#include "decodestatus.h"
#include "jobcontrol.h"
#include "permission.h"
#include "globmatch.h"

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
                                                                        ^ MAXLEN

 ***************************************************************************/

#define MAXLEN 69
void Get_queue_status( char *name, int *socket, int longformat, int allflag,
	int tokencount, struct token *tokens );
void Get_queue_server( char *name, int *socket, int longformat, int allflag,
	int tokencount, struct token *tokens );
void Print_status_info( int *socket, int longformat, char *path, char *header );

int Job_status( int *socket, char *input, int maxlen )
{
	struct token tokens[20];
	char error[LINEBUFFER];
	int tokencount;
	int i, c;
	char *name, *s, *end;
	int longformat;
	struct printcap *pc, *ppc;

	Name = "Job_status";

	/* get the format */
	longformat = 0;
	if( input[0] != REQ_DSHORT ) longformat = 1;

	++input;
	if( (s = strchr(input, '\n' )) ) *s = 0;
	DEBUG3("Job_status: doing '%s'", input );

	/* check printername for characters, underscore, digits */
	tokencount = Crackline(input, tokens, sizeof(tokens)/sizeof(tokens[0]));

	if( tokencount == 0 ){
		plp_snprintf( error, sizeof(error),
			"missing printer name");
		goto error;
	}

	for( i = 0; i < tokencount; ++i ){
		tokens[i].start[tokens[i].length] = 0;
	}

	name = tokens[0].start;
	setproctitle( "lpd %s '%s'", Name, name );
	
	if( (s = Clean_name( name )) ){
		plp_snprintf( error, sizeof(error),
			"printer '%s' has illegal char '%c' in name", name, *s );
		goto error;
	}

	Printer = name;
	Perm_check.service = 'Q';
	Perm_check.printer = Printer;
	Init_perms_check();
	if( Perms_check( &Perm_file, &Perm_check,
			(struct control_file *)0 ) == REJECT
		|| Last_default_perm == REJECT ){
		plp_snprintf( error, sizeof(error),
			"%s: no permission to show status", Printer );
		goto error;
	}


	if( strcmp( name, "all" ) ){
		DEBUG4( "Job_status: checking printcap entry '%s'",  name );
		Get_queue_status( name, socket, longformat, 0,
			tokencount - 1, &tokens[1] );
	} else {
		/* we work our way down the printcap list, checking for
			ones that have a spool queue */
		/* note that we have already tried to get the 'all' list */
		if( All_list && *All_list ){
			static char *list;
			if( list ){
				free( list );
				list = 0;
			}
			list = safestrdup( All_list );
			for( s = list; s && *s; s = end ){
				end = strpbrk( s, ", \t" );
				if( end ){
					*end++ = 0;
				}
				while( (c = *s) && isspace(c) ) ++s;
				if( c == 0 ) continue;

				Printer = s;
				DEBUG4( "Job_control: checking printcap entry '%s'",  Printer );
				Get_queue_server( Printer, socket, longformat, 1,
					tokencount - 1, &tokens[1] );
			}
		} else {
			for( i = 0; i < Printcapfile.pcs.count; ++i ){
				ppc = (void *)Printcapfile.pcs.list;
				pc = &ppc[i];
				Printer = pc->pcf->lines.list[pc->name];
				DEBUG4( "Job_control: checking printcap entry '%s'",  Printer );

				Get_queue_server( Printer, socket, longformat, 1,
					tokencount - 1, &tokens[1] );
			}
		}
	}
	DEBUG4( "Job_status: DONE" );
	return(0);

error:
	log( LOG_INFO, "Job_status: error '%s'", error );
	DEBUG3("Job_status: error msg '%s'", error );
	if( Link_send( ShortRemote, socket, Send_timeout,
		0x00, error, '\n', 0 ) ) exit(0);
	DEBUG4( "Job_status: done" );
	return(0);
}


/***************************************************************************
 * Get_queue_server()
 * create a child for the scanning information
 * and call Get_queue_status()
 ***************************************************************************/

void Get_queue_server( char *name, int *socket, int longformat, int allflag,
	int tokencount, struct token *tokens )
{
	pid_t pid, result;
	plp_status_t status;

	if( (pid = fork()) < 0 ){
		logerr_die( LOG_ERR, "Get_queue_server: fork failed" );
	} else if( pid ){
		do{
			result = plp_waitpid( pid, &status, 0 );
			DEBUG8( "Get_queue_server: result %d, '%s'",
				result, Decode_status( &status ) );
			removepid( result );
		} while( result != pid );
		return;
	}
	Get_queue_status( name, socket, longformat, allflag, tokencount, tokens );
	exit(0);
}

/***************************************************************************
 * void Get_queue_status( char *name, int *socket, int longformat, int allflag,
 *	int tokencount, struct token *tokens )
 * name  - printer name
 * socket - used to send information
 * longformat - if 1 use long format, 0 use short
 * allflag - do all information, i.e.- subservers as well
 * tokencount, tokens - cracked arguement string
 *  - get the printcap entry (if any)
 *  - check the control file for current status
 *  - find and report the spool queue entries
 ***************************************************************************/

static int subserver;
static char *buffer;
static int bsize;

void Get_queue_status( char *name, int *socket, int longformat, int allflag,
	int tokencount, struct token *tokens )
{
	struct malloc_list servers;
	struct server_info *server_info;
	char msg[SMALLBUFFER];
	char *s, *host, *logname; /* ACME pointer for writing */
	int i, j, status, len, count, hold_count;
	char number[LINEBUFFER];		/* printable form of small integer */
	int serverpid, unspoolerpid;	/* pid of server and unspooler */
	int fd;					/* file descriptor for file */
	struct stat statb;		/* stat of file */
	struct control_file *cfp, **cfpp;	/* pointer to control file */
	int priority;
	char error[LINEBUFFER];	/* for errors */
	int select;				/* select this job for display */
	char *path;				/* path to use */
	char *pr;
	int jobnumber;			/* job number */
	struct destination *destination, *d;
	int nodest;				/* no destination information */

	/* set printer name and printcap variables */
	DEBUG4( "Get_queue_status: checking '%s'", name );
	if( Debug > 6 ){
		for( i = 0; i < tokencount; ++i ){
			logDebug( "token[%d] '%s'", i, tokens[i].start );
		}
	}
	strcpy( error, "??? odd error???" );

	error[0] = 0;
	status = Setup_printer( name, error, sizeof(error),(void *)0,
		debug_vars,1, (void *)0 );
	/* set up status */

	msg[0] = 0;
	if( !longformat ){
		plp_snprintf( msg, sizeof(msg), "%s@%s:", Printer, ShortHost );
	} else {
		strcpy( msg, subserver?"Server Printer: ":"Printer: " );
		len = strlen( msg );
		plp_snprintf( msg+len, sizeof(msg) - len, "%s@%s", Printer, ShortHost );
	}
	if( status ){
		DEBUG4("Get_queue_status: Setup_printer '%s'", error );
		if( status != 2 ){
			goto error;
		} else {
			pr = RemotePrinter;
			host = RemoteHost;
			if( host == 0 ) host = Default_remote_host;
			len = strlen( msg );
			if( host == 0 || pr == 0 ){
				plp_snprintf( msg+len, sizeof(msg)-len,
		" no queue, printer and host information - bad printcap entry!" );
			} else {
				plp_snprintf( msg+len, sizeof(msg)-len,
					" no queue, forwarding to %s@%s", pr, host );
			}
			if( Link_send( ShortRemote, socket, Send_timeout,
				0x00, msg, '\n', 0 ) ) exit(0);
			goto remote;
		}
	}
	error[0] = 0;

	Perm_check.printer = Printer;
	Init_perms_check();
	if( Perms_check( &Perm_file, &Perm_check,
			(struct control_file *)0 ) == REJECT
		|| Perms_check( &Local_perm_file, &Perm_check,
			(struct control_file *)0 ) == REJECT
		|| Last_default_perm == REJECT ){
		plp_snprintf( error, sizeof(error),
			"%s: no permission to list jobs", Printer );
		goto error;
	}

	/* do not show status if subserver and 'all' requested */
	if( allflag && Server_queue_name && !subserver ){
		DEBUG4( "Get_queue_status: 'all' and subserver for '%s'",
			Server_queue_name );
		return;
	}

	DEBUG4("Get_queue_status: RemoteHost '%s', RemotePrinter '%s', Lp '%s'",
		RemoteHost, RemotePrinter, Lp_device );

	if( longformat ){
		s = Comment_tag;
		if( s == 0 ){
			s = Get_printer_comment( &Printcapfile, Printer );
		}
		if( s ){
			len = strlen( msg );
			plp_snprintf( msg+len, sizeof(msg) - len, " '%s'", s );
		}
	}

	if( SDpathname == 0 || SDpathname->pathname[0] == 0 ){
		if( RemoteHost == 0 ){
			plp_snprintf( error, sizeof(error),
			"'%s' does not have spool directory", Printer );
			goto error;
		}

		/* no spool directory, just forwarding it? */

		if( RemoteHost ){
			len = strlen(msg);
			plp_snprintf( msg+len, sizeof(msg)-len, " (directly forwarding to %s@%s)",
				RemotePrinter, RemoteHost );
			if( Link_send( ShortRemote, socket, Send_timeout,
				0x00, msg, '\n', 0 ) ) exit(0);
			goto remote;
		}
	}

	/* get the spool entries */

	Scan_queue( 1 );
	DEBUG8("Get_queue_status: total files %d", C_files_list.count );

	memset(&statb, 0, sizeof (statb));
	Get_spool_control(&statb);
	if( Printing_disabled || Spooling_disabled ){
		len = strlen( msg );
		plp_snprintf( msg+len, sizeof(msg) - len,
			" (%s%s%s)",
			Printing_disabled?"printing disabled":"",
			(Printing_disabled&&Spooling_disabled)?", ":"",
			Spooling_disabled?"spooling disabled":"" );
	}
	if( Bounce_queue_dest ){
		len = strlen( msg );
		plp_snprintf( msg+len, sizeof(msg) - len,
			" (%sbounce queue to '%s')",
			Routing_filter?"routed/":"", Bounce_queue_dest );
	}

	/*
	 * check to see if this is a server or subserver.  If it is
	 * for subserver,  then you can forget starting it up unless started
	 * by the server.
	 */

	if( Server_names ){
		if( subserver ){
			plp_snprintf( error, sizeof(error), "%s is already a subserver!",
				Printer );
			goto error;
		}
		memset( &servers, 0, sizeof( servers ) );
		Get_subserver_info( &servers, Server_names );
		len = strlen( msg );
		plp_snprintf( msg+len, sizeof(msg) - len,
			" (Subservers " );
		server_info = (void *)servers.list;
		for( i = 0; i < servers.count; ++i ){
			len = strlen( msg );
			plp_snprintf( msg+len, sizeof(msg) - len,
				"%s %s", (i > 0)?",":"", server_info[i].name );
		}
		len = strlen( msg );
		plp_snprintf( msg+len, sizeof(msg) - len,
			") " );
		server_info = (void *)servers.list;
	}
	if( Server_queue_name ){
		len = strlen( msg );
		plp_snprintf( msg+len, sizeof(msg) - len,
			" (Serving %s)",
			Server_queue_name );
	}

	/* indicate if forwarding to remote host */

	if( RemoteHost ){
		DEBUG4( "Get_queue_status: RemoteHost '%s'", RemoteHost );
		len = strlen( msg );
		plp_snprintf( msg+len, sizeof(msg) - len, " (forwarding to %s@%s)",
			RemotePrinter, RemoteHost);
		if( Forwarding ){
			len = strlen( msg );
			plp_snprintf( msg+len, sizeof(msg) - len, " (redirected)" );
		}
	}


	/* set up the short format for folks */
	cfpp = (void *)C_files_list.list;
	job_count( &hold_count, &count );

	/* this gives a short 1 line format with minimum info */
	if( !longformat ){
		len = strlen( msg );
		plp_snprintf( msg+len, sizeof(msg) - len, " %d jobs",
			count );
	}

	if( Link_send( ShortRemote, socket, Send_timeout,
		0x00, msg, '\n', 0 ) ) exit(0);

	if( !longformat ) goto remote;

	/* now check to see if there is a server and unspooler process active */
	path = Add_path( CDpathname, Printer );
	serverpid = 0;
	if( (fd = Checkread( path, &statb ) ) >= 0 ){
		serverpid = Read_pid( fd, (char *)0, 0 );
		close( fd );
	}
	DEBUG8("Get_queue_status: server pid %d", serverpid );
	if( serverpid && kill( serverpid, 0 ) ){
		DEBUG8("Get_queue_status: server %d not active", serverpid );
		serverpid = 0;
	} /**/

	path = Add2_path( CDpathname, "unspooler.", Printer );
	unspoolerpid = 0;
	if( (fd = Checkread( path, &statb ) ) >= 0 ){
		unspoolerpid = Read_pid( fd, (char *)0, 0 );
		close( fd );
	}

	DEBUG8("Get_queue_status: unspooler pid %d", unspoolerpid );
	if( unspoolerpid && kill( unspoolerpid, 0 ) ){
		DEBUG8("Get_queue_status: unspooler %d not active", unspoolerpid );
		unspoolerpid = 0;
	} /**/

	if( count == 0 ){
		if( Link_send( ShortRemote, socket, Send_timeout,
			0x00, "  Queue: no printable jobs in queue", '\n', 0 ) ) exit(0);
	} else {
		/* check to see if there are files and no spooler */
		plp_snprintf( msg, sizeof(msg), "  Queue: %d printable job%s", count,
			count > 1 ? "s" : "" );
		if( Link_send( ShortRemote, socket, Send_timeout,
			0x00, msg, '\n', 0 ) ) exit(0);
	}
	if( hold_count ){
		plp_snprintf( msg, sizeof(msg), 
		"  Warning: %d held jobs in queue", hold_count );
		if( Link_send( ShortRemote, socket, Send_timeout,
			0x00, msg, '\n', 0 ) ) exit(0);
	}

	if( count && serverpid == 0 ){
		if( Link_send( ShortRemote, socket, Send_timeout,
			0x00, "  Warning: no server present",
			'\n', 0 ) ) exit(0);
	}
	if( serverpid ){
		plp_snprintf( msg, sizeof(msg), "  Server: pid %d active", serverpid );
		if( unspoolerpid ){
			int n;
			n = strlen( msg );
			plp_snprintf( msg+n, sizeof(msg)-n, ", Unspooler: pid %d active",
				unspoolerpid );
		}
		if( Link_send( ShortRemote, socket, Send_timeout,
			0x00, msg, '\n', 0 ) ) exit(0);
	}
	if( Classes ){
		plp_snprintf( msg, sizeof(msg), "  Classes: printing %s", Classes );
		if( Link_send( ShortRemote, socket, Send_timeout,
			0x00, msg, '\n', 0 ) ) exit(0);
	}

	/*
	 * get the last status of the spooler
	 */
	DEBUG8("Get_queue_status: Max_status_size '%d'", Max_status_size );
	if( buffer == 0 ){
		bsize = Max_status_size*1024;
		if( bsize == 0 ) bsize = 1024;
		malloc_or_die( buffer, bsize+2 );
	}
	path = Add2_path( CDpathname, "status.", Printer );
	Print_status_info( socket, longformat, path, "Status" );

	if( Status_file && *Status_file ){
		if( Status_file[0] == '/' ){
			path = Status_file;
		} else {
			path = Add_path( SDpathname, Status_file );
		}
		Print_status_info( socket, longformat, path, "Filter_status" );
	}

	if( C_files_list.count > 0 ){
		/* send header */
		s =
" Rank  Owner/ID        Class Job Files                           Size Time";
		if( Link_send( ShortRemote, socket, Send_timeout,
			0x00,  s, '\n', 0 ) ) exit( 0 );
		jobnumber = 0;
		for( i = 0; i < C_files_list.count; ++i ){
			cfp = cfpp[i];

			DEBUG8("Get_queue_status: job name '%s' id '%s'",
				cfp->name, cfp->identifier );

			/*
			 * check to see if this entry matches any of the patterns
			 */

			destination = 0;
			if( tokencount > 0 ){
				select = 1;
				for( j = 0; select && j < tokencount; ++j ){
					select = patselect( &tokens[j], cfp, &destination );
				}
				DEBUG8("Get_queue_status: job name '%s' select '%d'",
					select );
				if( !select ) continue;
			}
			/* set a flag to suppres destinations */
			nodest = 0;

			/* we report this jobs status */

			error[0] = 0;
			if( cfp->JOBNAME ){
				safestrncat( error, cfp->JOBNAME+1 );
			}
			s = strrchr( cfp->name, '/' );
			if( s ){
				s = s+1;
			} else {
				s = cfp->name;
			}
			priority = s[2];
			host = cfp->FROMHOST?cfp->FROMHOST+1:"???";
			if( (s = strchr( host, '.' )) ) *s = 0;
			logname = cfp->LOGNAME?cfp->LOGNAME+1:"???";
			number[0] = 0;
			if( cfp->error[0] ){
				strcpy( number, "error" );
				plp_snprintf( error, sizeof( error ),
					"ERROR: %s", cfp->error );
				nodest = 1;
			} else if( cfp->hold_time ){
				strcpy( number, "hold" );
				nodest = 1;
			} else if( cfp->move_time ){
				strcpy( number, "move" );
				nodest = 1;
			} else if( cfp->remove_time ){
				strcpy( number, "remove" );
				nodest = 1;
			} else if( cfp->done_time ){
				strcpy( number, "done" );
				nodest = -1;
			} else if( cfp->held_class ){
				strcpy( number, "class" );
			} else if( cfp->active ){
				strcpy( number, "active" );
				nodest = -1;
			} else if( cfp->redirect[0] ){
				plp_snprintf( number, sizeof(number), "redirect->%s",
					cfp->redirect );
				nodest = 1;
			}

			DEBUG8("Get_queue_status: destination count '%d', nodest %d",
				cfp->destination_list.count, nodest );
			/* do the number, owner, and job information */
			if( number[0] == 0 ){
				plp_snprintf( number, sizeof(number), "%d",
					jobnumber+1, cfp->redirect );
			}
			++jobnumber;
			plp_snprintf( msg, sizeof(msg), "%-6s %-19s %c %03d %-s",
				number, cfp->identifier, priority, cfp->number, error );
			if( cfp->error[0] == 0 ){
				char sizestr[16];
				len = strlen(msg);
				if( len > MAXLEN){
					len = MAXLEN;
				} else {
					while( len < MAXLEN ){
						msg[len++] = ' ';
					}
				}
				plp_snprintf( sizestr, sizeof(sizestr), "%0d",cfp->jobsize);
				len -= ( strlen( sizestr ) + 1 );
				DEBUG8("Get_queue_status: MAXLEN '%d', sizestr '%s'",
					len, sizestr );
				plp_snprintf( msg+len, sizeof(msg)-len, " %s %s",
					sizestr, Time_str( 1, cfp->statb.st_ctime ) );
			}
			if( Link_send( ShortRemote, socket, Send_timeout,
				0x00, msg, '\n', 0 ) ) exit( 0 );
			if( cfp->destination_list.count > 0 ){
				/* put in the destination information */
				destination = (void *)cfp->destination_list.list;
				for( j = 0; j < cfp->destination_list.count; ++j ){
					d = &destination[j];
					error[0] = 0;
					plp_snprintf( error, sizeof(error), "->%s ",
						d->destination );
					DEBUG8("Get_queue_status: destination active '%d'",
						d->active );
					if( d->active && kill( d->active, 0 ) ){
						d->active = 0;
					}
					if( d->active ) ++d->copy_done;
					if( d->copies > 1 ){
						len = strlen( error );
						plp_snprintf( error+len, sizeof(error)-len,
							"<cpy %d/%d> ",
							d->copy_done, d->copies );
					}
					safestrncpy( number, " -" );
					if( d->error[0] ){
						safestrncat( number, "rterror" );
						len = strlen( error );
						plp_snprintf( error+len, sizeof( error )-len,
							"ERROR: %s", d->error );
					} else if( d->active ){
						safestrncat( number, "actv" );
					} else if( d->hold ){
						safestrncat( number, "hold" );
					} else if( d->done ){
						safestrncat( number, "done" );
					}
					plp_snprintf( msg, sizeof(msg), "%-6s %-19s %c %03d %-s",
						number, d->identifier, priority, cfp->number, error );
					if( d->error[0] == 0 ){
						char sizestr[16];
						len = strlen(msg);
						if( len > MAXLEN){
							len = MAXLEN;
						} else {
							while( len < MAXLEN ){
								msg[len++] = ' ';
							}
						}
						plp_snprintf( sizestr, sizeof(sizestr), "%0d",cfp->jobsize);
						len -= ( strlen( sizestr ) + 1 );
						DEBUG8("Get_queue_status: MAXLEN '%d', sizestr '%s'",
							len, sizestr );
						plp_snprintf( msg+len, sizeof(msg)-len, " %s %s",
							sizestr, Time_str( 1, cfp->statb.st_ctime ) );
					}
					if( Link_send( ShortRemote, socket, Send_timeout,
						0x00, msg, '\n', 0 ) ) exit( 0 );
				}
			}
		}
	}

	if( Bounce_queue_dest ){
		DEBUG4("Get_queue_status: getting bouncequeue dest status '%s'", 
			Bounce_queue_dest);
		if( (s = strchr( Bounce_queue_dest, '@' )) ){
			RemotePrinter = Bounce_queue_dest;
			*s = 0;
			RemoteHost = s+1;
		} 
	}

remote:

	if( Server_names && !subserver ){
		server_info = (void *)servers.list;
		subserver = 1;
		for( i = 0; i < servers.count; ++i ){
			DEBUG4("Get_queue_status: getting subserver status '%s'", 
				server_info[i].name );
			Get_queue_status( server_info[i].name, socket, longformat, allflag,
				tokencount, tokens );
			DEBUG4("Get_queue_status: finished subserver status '%s'", 
				server_info[i].name );
		}
		subserver = 0;
	} else if( RemoteHost ){
	/* now get the remote host information */
		static struct malloc_list args;
		char **list;

		if( subserver ){
			plp_snprintf( error, sizeof(error),
				"printer '%s' cannot be remote and subserver", Printer );
			goto error;
		}
		while( args.max < tokencount +1 ){
			extend_malloc_list( &args, sizeof( char *), tokencount + 1 );
		}
		args.count = 0;
		list = (void *)args.list;
		for( args.count = 0, i = 0; i < tokencount; ++i, ++args.count ){
			list[args.count] = tokens[i].start;
		}
		list[args.count] = 0;
		DEBUG4("Get_queue_status: getting status from remote host '%s'", 
			RemoteHost );
		/* get extended format */
		Send_statusrequest( RemotePrinter, RemoteHost, 
			longformat?2:0, list,
			Connect_timeout, Send_timeout, *socket );
	} else if( !subserver ){
		msg[0] = 0;
		if( Link_send( ShortRemote, socket, Send_timeout,
			0x00, msg, '\n', 0 ) ) exit(0);
	}
	DEBUG4("Get_queue_status: finished '%s'", name );
	return;


error:
	log( LOG_INFO, "Get_queue_status: error '%s'", error );
	DEBUG3("Get_queue_status: error msg '%s'", error );
	if( Link_send( ShortRemote, socket, Send_timeout,
		0x00, error, '\n', 0 ) ) exit(0);
	DEBUG4( "Get_queue_status: done" );
	return;
}

/***************************************************************************
 * int patselect( struct token *token, struct control_file *cfp );
 *    check to see that the token value matches one of the following
 *    in the control file:
 *  token is INTEGER: then matches the job number
 *  token is string: then matches either the user name or host name
 *    then try glob matching job ID
 *
 ***************************************************************************/

int patselect( struct token *token, struct control_file *cfp,
	struct destination **destination )
{
	char *s, *end;
	int val, len, j;
	struct destination *destination_list, *d;

	s = token->start;
	len = token->length;

	DEBUG6("patselect: '%s'", s );

	/* handle wildcard match */
	if( strcasecmp( s, "all" ) == 0 ){
		return( 1 );
	}
	end = s;
	val = strtol( s, &end, 10 );
	if( (end - s) == len ){
		/* we check job number */
		DEBUG6("patselect: job number check '%d' to job %d",
			val, cfp->number );
		return( val == cfp->number );
	} else {
		/* now we check to see if we have a name match */
		if( cfp->LOGNAME && strcasecmp( s, cfp->LOGNAME+1 ) == 0 ){
			DEBUG6("patselect: job logname '%s' match", cfp->LOGNAME );
			return(1);
		}
		if( strcasecmp( s, cfp->identifier ) == 0 ){
			DEBUG6("patselect: job identifier '%s' match", cfp->identifier );
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
				if( strcasecmp( s, d->identifier ) == 0 ){
					DEBUG6("patselect: job identifier '%s' match", d->identifier );
					*destination = d;
					return(1);
				} else if( strchr( s, '*' ) && Globmatch( s, d->identifier) == 0 ){
					DEBUG6("patselect: job identifier '%s' globmatch",d->identifier);
					*destination = d;
					return(1);
				}
			}
		}
		if( strchr( s, '*' ) && Globmatch( s, cfp->identifier) == 0 ){
			DEBUG6("patselect: job identifier '%s' globmatch",cfp->identifier);
			return(1);
		}
	}
	return(0);
}

void Print_status_info( int *socket, int longformat, char *path, char *header )
{
	int fd, len, i;
	off_t off = 0;
	char *s, *endbuffer;
	struct stat statb;		/* stat of file */
	char line[LINEBUFFER];

	if( (fd = Checkread( path, &statb ) ) >= 0 ){
		if( statb.st_size > bsize ){
			off = statb.st_size - bsize;
			if( lseek( fd, off, SEEK_SET ) < 0 ){
				logerr_die( LOG_ERR, "setstatus: cannot seek '%s'", path );
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
			endbuffer = strchr( s, '\n' );
			*endbuffer++ = 0;
			plp_snprintf( line, sizeof(line), "  %s: %s", header, s );
			if( Link_send( ShortRemote, socket, Send_timeout,
				0x00, line, '\n', 0 ) ) exit(0);
			s = endbuffer;
		}
	}
}

void job_count( int *hc, int *cnt )
{
	struct control_file *cfp, **cfpp;	/* pointer to control file */
	int i, count, hold_count;

	count = 0; hold_count = 0;
	cfpp = (void *)C_files_list.list;
	for( i = 0; i < C_files_list.count; ++i ){
		cfp = cfpp[i];
		if( cfp->error[0] || cfp->remove_time || cfp->move_time
			|| cfp->redirect[0] || cfp->held_class
			|| cfp->done_time ){
			continue;
		}
		if( cfp->hold_time ){
			++hold_count;
			continue;
		}
		++count;
	}
	if( hc ) *hc = hold_count;
	if( cnt ) *cnt = count;
}
