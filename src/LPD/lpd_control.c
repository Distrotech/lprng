/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpd_control.c
 * PURPOSE: return status
 **************************************************************************/

static char *const _id =
"$Id: lpd_control.c,v 3.4 1996/08/31 21:11:58 papowell Exp papowell $";

#include "lpd.h"
#include "printcap.h"
#include "lp_config.h"
#include "permission.h"
#include "control.h"
#include "jobcontrol.h"
#include "decodestatus.h"

/***************************************************************************

The control (LPC) interface is sort of a catch-all for hacking.
I have tried to provide the following functionality.

1. Spool Queues have a 'control.printer' file that is read/written by
   the Get_spool_control and Set_spool_control routines.  These routines
   will happily put out the various control strings you need.
	USED BY: start/stop, enable/disable, debug, forward, autohold

2. Individual jobs have a 'hold file' that is read/written by
   the Get_job_control and Set_job_control routines.  These also
   will read/write various control strings.
   USED by topq, hold, release

 ***************************************************************************/

static char status_header[] = "%-20s %8s %8s %4s %7s %7s %8s %s";
static char status_short[] = "%-20s WARNING %s";

void Do_printer_work( char *user, int action, int *socket,
	int tokencount, struct token *tokens, char *error, int errorlen );
int Do_control_lpd( char *user, int action, int *socket,
	int tokencount, struct token *tokens, char *error, int errorlen );
void Do_queue_server( char *user, int action, int *socket,
	int tokencount, struct token *tokens, char *error, int errorlen );
void Do_queue_control( char *user, int action, int *socket,
	int tokencount, struct token *tokens, char *error, int errorlen );
int Do_control_file( char *user, int action, int *socket,
	int tokencount, struct token *tokens, char *error, int errorlen );
int Do_control_lpq( char *user, int action, int *socket,
	int tokencount, struct token *tokens, char *error, int errorlen );
int Do_control_status( char *user, int action, int *socket,
	int tokencount, struct token *tokens, char *error, int errorlen );
int Do_control_redirect( char *user, int action, int *socket,
	int tokencount, struct token *tokens, char *error, int errorlen );
int Do_control_class( char *user, int action, int *socket,
	int tokencount, struct token *tokens, char *error, int errorlen );
int Do_control_printcap( char *user, int action, int *socket,
	int tokencount, struct token *tokens, char *error, int errorlen );
int Do_control_debug( char *user, int action, int *socket,
	int tokencount, struct token *tokens, char *error, int errorlen );

static char *Redirect_str;
static char *Action = "updated";

int Job_control( int *socket, char *input, int maxlen )
{
	struct token tokens[20];
	char error[LINEBUFFER];
	int tokencount;
	int i, action;
	char *name, *user, *s;
	pid_t pid, result;
	plp_status_t status;

	/* get the format */

	Name = "Job_control";
	++input;
	if( (s = strchr(input, '\n' )) ) *s = 0;
	DEBUG3("Job_control: doing '%s'", input );

	/* check printername for characters, underscore, digits */
	tokencount = Crackline(input, tokens, sizeof(tokens)/sizeof(tokens[0]));

	if( tokencount < 3 ){
		plp_snprintf( error, sizeof(error),
			"bad control command '%s'", input );
		goto error;
	}

	for( i = 0; i < tokencount; ++i ){
		tokens[i].start[tokens[i].length] = 0;
	}

	if( Debug > 3 ){
		logDebug("Job_control: tokencount '%d'", tokencount );
		for( i = 0; i < tokencount; ++i ){
			logDebug("Job_control: token [%d] '%s'", i, tokens[i].start );
		}
	}

	/* get the name for the printer */
	/* it is either the default or user specified */

	name = tokens[0].start;
	if( tokencount > 3 ){
		name = tokens[3].start;
	}

	if( (s = Clean_name( name )) ){
		plp_snprintf( error, sizeof(error),
			"printer '%s' has illegal char '%c' in name", name, *s );
		goto error;
	}
	setproctitle( "lpd %s %s", Name, Printer );
	Printer = name;
	user = tokens[1].start;

	s = tokens[2].start;
	action = Get_controlword( s );
	if( action == 0 ){
		plp_snprintf( error, sizeof(error),
			"%s: unknown control request '%s'", Printer, s );
		goto error;
	}

	/* check the permissions for the action */

	Perm_check.printer = Printer;
	Perm_check.remoteuser = user;
	Perm_check.user = user;
	Perm_check.service = 'C';
	switch( action ){
		case LPD:
		case STATUS:
			Perm_check.service = 'S';
			break;
	}

	DEBUG4( "LPC: Printer '%s' user '%s'", Printer, user );

	Init_perms_check();
	if( Perms_check( &Perm_file, &Perm_check,
		(struct control_file *)0 ) == REJECT
		|| Last_default_perm == REJECT ){
		if( Perm_check.service == 'S' ){
			plp_snprintf( error, sizeof(error),
				"%s: no permission to get status", Printer );
		} else {
			plp_snprintf( error, sizeof(error),
				"%s: no permission to control queue", Printer );
		}
		goto error;
	}

	switch( action ){
		case REREAD:
			(void)kill(Server_pid,SIGHUP);
			goto done;
		case LPD:
			if( Do_control_lpd( user, action, socket,
				tokencount-3, &tokens[3], error, sizeof(error) ) ){
				goto error;
			}
			goto done;
		case STATUS:
			plp_snprintf( error, sizeof(error), status_header,
				"Printer", "Printing", "Spooling", "Jobs",
				"Server", "Slave", "Redirect", "Debug" );
			(void)Link_send( ShortRemote, socket, Send_timeout,
				0x00, error, '\n', 0 );
			error[0] = 0;
		case STOP:
		case START:
		case DISABLE:
		case ENABLE:
		case ABORT:
		case UP:
		case DOWN:
		case AUTOHOLD:
		case NOAUTOHOLD:
			/* control line is 'Xprinter user action arg1 arg2
             *                    t[0]   t[1]  t[2]  t[3]
			 */
			if( tokencount > 4 ){
				/* we have a list of printers to use */
				for( i = 3; i < tokencount; ++i ){
					Printer = tokens[i].start;
					if( (pid = fork()) < 0 ){
						logerr_die( LOG_ERR, "Job_control: fork failed" );
					} else if( pid ){
						do{
							result = plp_waitpid( pid, &status, 0 );
							DEBUG8( "Job_control: result %d, '%s'",
								result, Decode_status( &status ) );
							removepid( result );
						} while( result != pid );
					} else {
						Do_printer_work( user, action, socket,
							tokencount, tokens, error, sizeof(error) );
						exit(0);
					}
				}
				goto done;
			}
			break;
		case MOVE:
			/* we have Nprinter user move jobid* target */
			if( tokencount < 5 ){
				plp_snprintf( error, sizeof(error),
					"Use: MOVE printer (user|jobid)* target" );
				goto error;
			}
			break;
	}
	Do_printer_work( user, action, socket,
		tokencount, tokens, error, sizeof(error) );
done:
	DEBUG4( "Job_control: DONE" );
	return(0);

error:
	log( LOG_INFO, "Job_control: error '%s'", error );
	DEBUG3("Job_control: error msg '%s'", error );
	(void)Link_send( ShortRemote, socket, Send_timeout,
		0x00, error, '\n', 0 );
	DEBUG4( "Job_control: done" );
	return(0);
}

void Do_printer_work( char *user, int action, int *socket,
	int tokencount, struct token *tokens, char *error, int errorlen )
{
	char *s, *end;
	int c, i;
	struct printcap *pc;

	if( strcmp( Printer, "all" ) ){
		DEBUG4( "Job_control: checking printcap entry '%s'",  Printer );
		Do_queue_control( user, action, socket,
			tokencount-4, &tokens[4], error, errorlen );
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
				Do_queue_server( user, action, socket,
					tokencount-4, &tokens[4], error, errorlen);
			}
		} else {
			for( i = 0; i < Printcapfile.pcs.count; ++i ){
				char *pn, *npn;
				int j;
				pc = (void *)Printcapfile.pcs.list;
				pc = &pc[i];
				pn = Printer = pc->pcf->lines.list[pc->name];
				DEBUG4( "Job_control: checking printcap entry '%s'",  Printer );
				if( pn == 0 || *pn == 0 ){
					continue;
				}

				/* this is a printcap entry with spool directory -
					we need to check it out
				 */
				Do_queue_server( user, action, socket,
					tokencount-4, &tokens[4], error, errorlen);
				for( j = i + 1; j < Printcapfile.pcs.count; ++j ){
					pc = (void *)Printcapfile.pcs.list;
					pc = &pc[j];
					npn = pc->pcf->lines.list[pc->name];
					if( npn && strcmp(npn, pn) == 0 ){
						DEBUG4( "Job_control: deleting entry '%s'",  npn );
						*npn = 0;
					}
				}
			}
		}
	}
}

/***************************************************************************
 * Do_queue_server()
 * create a child for the scanning information
 * and call Do_queue_control()
 ***************************************************************************/

void Do_queue_server( char *user, int action, int *socket,
	int tokencount, struct token *tokens, char *error, int errorlen )
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
	Do_queue_control( user, action, socket, tokencount,
		tokens, error, errorlen );
	exit(0);
}


/***************************************************************************
 * Do_queue_control()
 * do the actual queue control operations
 * - start, stop, enable, disable are simple
 * - others are more complex, and are handled in Do_control_file
 * We have tokens:
 *   user printer action p1 p2 p3 -> p1 p2 p3
 ***************************************************************************/

static struct pc_used pc_used;		/* for printcap files */

void Do_queue_control( char *user, int action, int *socket,
	int tokencount, struct token *tokens, char *error, int errorlen )
{
	int i;
	pid_t serverpid;			/* server pid to kill off */
	struct stat statb;			/* status of file */
	int fd;						/* file descriptor */
	char line[LINEBUFFER];
	char *pname;
	int status;
	/* first get the printer name */

	/* process the job */
	Name = "Do_queue_control";
	error[0] = 0;

	/*
	 * some of the commands allow a list of printers to be
	 * specified, others only take a single printer
	 * We need to put in the list stuff for the ones that take a list
	 */

	switch( action ){
	case LPQ:
		if( Do_control_lpq( user, action, socket,
			tokencount, tokens, error, errorlen ) ){
			goto error;
		}
		return;
	case PRINTCAP:
		Setup_printer( Printer, error, errorlen, &pc_used,
			debug_vars, 0, (void *)0 );
		Do_control_printcap( user, action, socket,
			tokencount, tokens, error, errorlen );
		return;
	}

	status = Setup_printer( Printer, error, errorlen, &pc_used,
			debug_vars, 0, (void *) 0 );
	if( status ){
		char pr[LINEBUFFER];
		char msg[LINEBUFFER];
		switch( action ){
		case STATUS:
			plp_snprintf( pr, sizeof(pr), "%s@%s", Printer, ShortHost );
			plp_snprintf( msg, sizeof(msg), status_short, pr, error );
			strncpy( error, msg, errorlen );
			if( Link_send( ShortRemote, socket, Send_timeout,
				0x00, error, '\n', 0 ) ) exit(0);
			return;
		default:
			goto error;
		}
	}

	if( Debug > 3 ){
		logDebug("Do_queue_control: tokencount '%d'", tokencount );
		for( i = 0; i < tokencount; ++i ){
			logDebug("Do_queue_control: token [%d] '%s'", i, tokens[i].start );
		}
	}
	Perm_check.printer = Printer;
	Init_perms_check();
	if( Perms_check( &Perm_file, &Perm_check,
			(struct control_file *)0 ) == REJECT
		|| Perms_check( &Local_perm_file, &Perm_check,
			(struct control_file *)0 ) == REJECT
		|| Last_default_perm == REJECT
		 ){
		if( Perm_check.service == 'S' ){
			plp_snprintf( error, sizeof(error),
				"%s: no permission to get status", Printer );
		} else {
			plp_snprintf( error, sizeof(error),
				"%s: no permission to control queue", Printer );
		}
		goto error;
	}

	/* set the conditions */

	switch( action ){
	case STOP: Printing_disabled = 1; break;
	case START: Printing_disabled = 0; break;
	case DISABLE: Spooling_disabled = 1; break;
	case ENABLE: Spooling_disabled = 0; break;
	case ABORT: case KILL: break;
	case UP: Printing_disabled = 0; Spooling_disabled = 0; break;
	case DOWN: Printing_disabled = 1; Spooling_disabled = 1; break;
	case AUTOHOLD: Auto_hold = 1; break;
	case NOAUTOHOLD: Auto_hold = 0; break;

	case HOLD: case RELEASE: case TOPQ:
		if( Do_control_file( user, action, socket,
			tokencount, tokens, error, errorlen ) ){
			goto error;
		}
		break;

	case MOVE:
		Redirect_str = tokens[tokencount-1].start;
		--tokencount;
		if( strlen( Redirect_str ) >=
			sizeof( ((struct control_file *)0)->redirect ) - 2 ){
			plp_snprintf( error, errorlen,
				"%s: destination printer too long '%s'",
				Printer, Redirect_str );
			goto error;
		}
		if( Do_control_file( user, action, socket,
			tokencount, tokens, error, errorlen ) ){
			goto error;
		}
		break;

	case LPRM:
		if( Do_control_lpq( user, action, socket,
			tokencount, tokens, error, errorlen ) ){
			goto error;
		}
		break;
		
	case STATUS:
		if( Do_control_status( user, action, socket,
			tokencount, tokens, error, errorlen ) ){
			goto error;
		}
		break;

	case REDIRECT:
		if( Do_control_redirect( user, action, socket,
			tokencount, tokens, error, errorlen ) ){
			goto error;
		}
		break;

	case CLAss:
		if( Do_control_class( user, action, socket,
			tokencount, tokens, error, errorlen ) ){
			goto error;
		}
		break;

	case DEBUG:
		if( Do_control_debug( user, action, socket,
			tokencount, tokens, error, errorlen ) ){
			goto error;
		}
		break;
		
	default:
		plp_snprintf( error, errorlen, "not implemented yet" );
		goto error;
	}

	/* modify the control file to force rescan of queue */

	switch( action ){
	case STATUS:
	case LPD:
		break;
	default:
		Set_spool_control();
	}

	/* kill off the server */
	switch( action ){
	case STOP:
	case DOWN:
	case ABORT:
	case KILL:
		pname = Add_path( CDpathname, Printer );
		serverpid = 0;
		if( (fd = Checkread( pname, &statb ) ) >= 0 ){
			serverpid = Read_pid( fd, (char *)0, 0 );
			close( fd );
		}
		DEBUG8("Do_queue_control: server pid %d", serverpid );
		if( serverpid && kill( serverpid, SIGINT ) ){
			DEBUG8("Do_queue_control: server %d not active", serverpid );
			serverpid = 0;
		}
		break;
	}

	/* wait for the server to die, then restart it */
	if( action == KILL ){
		sleep(1);
	}

	/* start the server if necessary */
	switch( action ){
	case TOPQ:
	case RELEASE:
	case HOLD:
	case KILL:
	case UP:
	case START:
	case REDIRECT:
	case MOVE:
	case NOAUTOHOLD:
		plp_snprintf( line, sizeof(line), "!%s\n", Printer );
		DEBUG4("Do_queue_control: sending '%s' to LPD", Printer );
		if( Write_fd_str( Lpd_pipe[1], line ) < 0 ){
			logerr_die( LOG_ERR, "Do_queue_control: write to pipe '%d' failed",
				Lpd_pipe[1] );
		}
	}

	switch( action ){
	case STATUS:	Action = 0; break; /* no message */
	case UP:		Action = "enabled and started"; break;
	case DOWN:		Action = "disabled and stopped"; break;
	case STOP:		Action = "stopped"; break;
	case START:		Action = "started"; break;
	case DISABLE:	Action = "disabled"; break;
	case ENABLE:	Action = "enabled"; break;
	case REDIRECT:	Action = "redirected"; break;
	case AUTOHOLD:	Action = "autohold on"; break;
	case NOAUTOHOLD:	Action = "autohold off"; break;
	case MOVE:		Action = "move done"; break;
	case CLAss:		Action = "class updated"; break;
	}
	if( Action ){
		plp_snprintf( line, sizeof(line), "%s %s", Printer, Action );
		status = Link_send( ShortRemote, socket, Send_timeout,
			0x00, line, '\n', 0 );
	}

	return;

error:
	log( LOG_INFO, "Do_queue_jobs: error '%s'", error );

	DEBUG3("Do_queue_control: error msg '%s'", error );

	(void)Link_send( ShortRemote, socket, Send_timeout,
		0x00, error, '\n', 0 );
	DEBUG4( "Do_queue_jobs: done" );
	return;
}


/***************************************************************************
 * Do_control_file:
 *  perform a suitable operation on a control file
 * 1. get the control files
 * 2. check to see if the control file has been selected
 * 3. update the hold file for the control file
 ***************************************************************************/

int Do_control_file( char *user, int action, int *socket,
	int tokencount, struct token *tokens, char *error, int errorlen )
{
	int i, j;						/* ACME! Nothing but the best */
	int status;						/* status of last IO op */
	char msg[SMALLBUFFER];			/* message field */
	struct control_file *cfp, **cfpp;	/* control files */
	int select;						/* select this file */
	struct token token;
	struct destination *destination;

	/* set up default token */
	token.start = user;
	token.length = strlen( user );

	/* get the spool entries */
	Scan_queue( 0 );
	DEBUG8("Do_control_file: total files %d, tokencount %d",
		C_files_list.count, tokencount );

	/* scan the files to see if there is one which matches */

	status = 0;
	cfpp = (void *)C_files_list.list;
	for( i = 0; status == 0 && i < C_files_list.count; ++i ){
		cfp = cfpp[i];

		/*
		 * check to see if this entry matches any of the patterns
		 */

		DEBUG8("Do_control_file: checking '%s'", cfp->name );

		select = 1;
		destination = 0;

next_destination:
		if( tokencount > 0 ){
			for( j = 0; select && j < tokencount; ++j ){
				select = patselect( &tokens[j], cfp, &destination );
			}
		} else {
			select = patselect( &token, cfp, &destination );
		}
		if( !select ) continue;

		/* now we get the user name and IP address */

		DEBUG8("Do_control_file: selected '%s', id '%s', destination '%s'",
			cfp->name, cfp->identifier, destination->destination );
		/* we report this job being selected */
		plp_snprintf( msg, sizeof(msg), "selected '%s'", cfp->identifier );
		if( destination ){
			plp_snprintf( msg, sizeof(msg), "selected '%s'", destination->identifier );
		}
		status = Link_send( ShortRemote, socket, Send_timeout,
			0x00, msg, '\n', 0 );
		switch( action ){
		case HOLD:
			if( destination ){
				destination->hold = time( (void *)0 );
			} else {
				cfp->hold_time = time( (void *)0 );
				destination = (void *)cfp->destination_list.list;
				for( j =0; j < cfp->destination_list.count; ++j ){
					destination[j].hold = cfp->hold_time;
				}
				destination = 0;
			}
			setmessage( cfp, "TRACE", "%s@%s: job held", Printer, FQDNHost );
			break;
		case TOPQ: cfp->priority_time = time( (void *)0 );
			if( destination ){
				cfp->hold_time = 0;
				destination->hold = 0;
			} else {
				cfp->hold_time = 0;
				destination = (void *)cfp->destination_list.list;
				for( j =0; j < cfp->destination_list.count; ++j ){
					destination[j].hold = 0;
				}
				destination = 0;
			}
			setmessage( cfp, "TRACE", "%s@%s: job topq", Printer, FQDNHost );
			break;
		case MOVE:
			strcpy( cfp->redirect, Redirect_str );
			/* and we update the priority to put it at head of queue */
			cfp->priority_time = time( (void *)0 );
			cfp->hold_time = 0;
			setmessage( cfp, "TRACE", "%s@%s: job moved", Printer, FQDNHost );
			break;
		case RELEASE:
			cfp->hold_time = 0;
			if( destination ){
				destination->hold = 0;
			} else {
				destination = (void *)cfp->destination_list.list;
				for( j =0; j < cfp->destination_list.count; ++j ){
					destination[j].hold = 0;
				}
				destination = 0;
			}
			setmessage( cfp, "TRACE", "%s@%s: job released", Printer, FQDNHost );
			break;
		}
		cfp->done_time = 0;
		cfp->error[0] = 0;
		if( destination ){
			destination->error[0] = 0;
			destination->done = 0;
		} else {
			destination = (void *)cfp->destination_list.list;
			for( j =0; j < cfp->destination_list.count; ++j ){
				destination[j].done = 0;
				destination[j].error[0] = 0;
			}
			destination = 0;
		}
		Set_job_control( cfp );
		if( tokencount <= 0 ){
			DEBUG8("Do_control_file: finished '%s'", cfp->name );
			break;
		}
		if( destination ) goto next_destination;
	}
	return( 0 );
}



/***************************************************************************
 * Do_control_lpq:
 *  forward an LPQ or LPRM
 ***************************************************************************/

int Do_control_lpq( char *user, int action, int *socket,
	int tokencount, struct token *tokens, char *error, int errorlen )
{
	char msg[LINEBUFFER];			/* message field */
	int i = 0;

	/* synthesize an LPQ or LPRM line */ 
	msg[sizeof(msg)-1] = 0;
	switch( action ){
	case LPQ:  i = REQ_DSHORT; break;
	case LPRM: i = REQ_REMOVE; break;
	}

	plp_snprintf( msg, sizeof(msg), "%c%s", i, Printer );
	for( i = 0; i < tokencount; ++i ){
		safestrncat( msg, " " );
		safestrncat( msg, tokens[i].start );
	}
	DEBUG4("Do_control_lpq: sending '%d'%s'", msg[0], &msg[1] );
	safestrncat( msg, "\n" );

	switch( action ){
	case LPQ: Job_status( socket,  msg, sizeof(msg) ); break;
	case LPRM: Job_remove( socket,  msg, sizeof(msg) ); break;
	}
	return(0);
}

/***************************************************************************
 * Do_control_status:
 *  report current status
 ***************************************************************************/

int Do_control_status( char *user, int action, int *socket,
	int tokencount, struct token *tokens, char *error, int errorlen )
{
	char msg[SMALLBUFFER];			/* message field */
	char pr[LINEBUFFER];
	char count[32];
	char server[32];
	char spooler[32];
	char forward[LINEBUFFER];
	int serverpid, unspoolerpid;	/* server and unspooler */
	int fd;
	struct stat statb;
	char *path;
	int cnt, hold_cnt;

	/* get the spool entries */
	Scan_queue( 0 );
	DEBUG8("Do_control_status: total files %d, tokencount %d",
		C_files_list.count, tokencount );

	job_count( &hold_cnt, &cnt );

	/* now check to see if there is a server and unspooler process active */
	path = Add_path( CDpathname, Printer );
	serverpid = 0;
	if( (fd = Checkread( path, &statb ) ) >= 0 ){
		serverpid = Read_pid( fd, (char *)0, 0 );
		close( fd );
	}
	DEBUG8("Get_queue_status: server pid %d", serverpid );
	if( serverpid && kill( serverpid, 0 ) < 0 ){
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
	if( unspoolerpid && kill( unspoolerpid, 0 ) < 0 ){
		DEBUG8("Get_queue_status: unspooler %d not active", unspoolerpid );
		unspoolerpid = 0;
	} /**/
	close(fd);


	plp_snprintf( pr, sizeof(pr), "%s@%s", Printer, ShortHost );
	if( Bounce_queue_dest ){
		int len;
		len = strlen(pr);
		plp_snprintf( pr+len, sizeof(pr)-len, " bq->%s", Bounce_queue_dest );
	}
	if( Auto_hold ){
		int len;
		len = strlen(pr);
		plp_snprintf( pr+len, sizeof(pr)-len, " autohold" );
	}
	plp_snprintf( count, sizeof(count), "%d", cnt );
	strcpy( server, "none" );
	strcpy( spooler, "none" );
	if( serverpid ) plp_snprintf( server, sizeof(server),"%d",serverpid );
	if( unspoolerpid ) plp_snprintf( spooler, sizeof(spooler),"%d",unspoolerpid );

	forward[0] = 0;
	if( Forwarding ){
		plp_snprintf( forward, sizeof( forward ), "%s", Forwarding );
	}

	plp_snprintf( msg, sizeof(msg),
		status_header,
		pr,
		Printing_disabled? "disabled" : "enabled",
		Spooling_disabled? "disabled" : "enabled",
		count, server, spooler, forward, Control_debug?Control_debug:"" );
	trunc_str( msg );
	(void)Link_send( ShortRemote, socket, Send_timeout,
		0x00, msg, '\n', 0 );
	return( 0 );
}


/***************************************************************************
 * Do_control_redirect:
 *  perform a suitable operation on a control file
 * 1. get the control files
 * 2. if no options, report redirect name
 * 3. if option = none, remove redirect file
 * 4. if option = printer@host, specify name
 ***************************************************************************/

int Do_control_redirect( char *user, int action, int *socket,
	int tokencount, struct token *tokens, char *error, int errorlen )
{
	char forward[LINEBUFFER];
	char *s;

	/* get the spool entries */
	DEBUG8("Do_control_redirect: tokencount %d", tokencount );

	error[0] = 0;
	forward[0] = 0;
	switch( tokencount ){
	case -1:
	case 0:
		break;
	case 1:
		s = tokens[0].start;
		DEBUG8("Do_control_redirect: redirect to '%s'", s );
		if( strcasecmp( s, "off" ) == 0 ){
			Forwarding = 0;
		} else {
			if( strchr(s, '@' ) == 0 || strpbrk( s, ":; \t;" ) ){
				plp_snprintf( error, errorlen,
					"forward format is printer@host, not '%s'", s );
				goto error;
			}
			Forwarding = s;
		}
		break;

	default:
		strncpy( error, "too many arguments", errorlen );
		goto error;
	}

	if( Forwarding ){
		plp_snprintf( forward, sizeof(forward), "forwarding to '%s'",
			Forwarding );
	} else {
		plp_snprintf( forward, sizeof(forward), "forwarding off" );
	}

	if( forward[0] ){
		(void)Link_send( ShortRemote, socket, Send_timeout,
			0x00, forward, '\n', 0 );
	}
	return( 0 );

error:
	return( 1 );
}


/***************************************************************************
 * Do_control_class:
 *  perform a suitable operation on a control file
 * 1. get the control files
 * 2. if no options, report class name
 * 3. if option = none, remove class file
 * 4. if option = printer@host, specify name
 ***************************************************************************/

int Do_control_class( char *user, int action, int *socket,
	int tokencount, struct token *tokens, char *error, int errorlen )
{
	char forward[LINEBUFFER];
	char *s;

	/* get the spool entries */
	DEBUG8("Do_control_class: tokencount %d", tokencount );

	error[0] = 0;
	forward[0] = 0;
	switch( tokencount ){
	case -1:
	case 0:
		break;
	case 1:
		s = tokens[0].start;
		DEBUG8("Do_control_class: class to '%s'", s );
		if( strcasecmp( s, "off" ) == 0 ){
			Classes = 0;
		} else {
			Classes = s;
		}
		break;

	default:
		strncpy( error, "too many arguments", errorlen );
		goto error;
	}

	if( Classes ){
		plp_snprintf( forward, sizeof(forward), "classes printed '%s'",
			Classes );
	} else {
		plp_snprintf( forward, sizeof(forward), "all classes printed" );
	}

	if( forward[0] ){
		(void)Link_send( ShortRemote, socket, Send_timeout,
			0x00, forward, '\n', 0 );
	}
	return( 0 );

error:
	return( 1 );
}

/***************************************************************************
 * Do_control_debug:
 *  perform a suitable operation on a control file
 * 1. get the control files
 * 2. if no options, report debug name
 * 3. if option = none, remove debug file
 * 4. if option = printer@host, specify name
 ***************************************************************************/

int Do_control_debug( char *user, int action, int *socket,
	int tokencount, struct token *tokens, char *error, int errorlen )
{
	char debugging[LINEBUFFER];
	char *s;

	/* get the spool entries */
	DEBUG8("Do_control_debug: tokencount %d", tokencount );

	error[0] = 0;
	debugging[0] = 0;
	switch( tokencount ){
	case -1:
	case 0:
		break;
	case 1:
		s = tokens[0].start;
		DEBUG8("Do_control_debug: debug to '%s'", s );
		if( strcasecmp( s, "off" ) == 0 ){
			Control_debug = 0;
		} else {
			Control_debug = s;
		}
		break;

	default:
		strncpy( error, "too many arguments", errorlen );
		goto error;
	}

	if( Control_debug ){
		plp_snprintf( debugging, sizeof(debugging),
			"debugging override set to '%s'",
			Control_debug );
	} else {
		plp_snprintf( debugging, sizeof(debugging), "debugging override off" );
	}

	if( debugging[0] ){
		(void)Link_send( ShortRemote, socket, Send_timeout,
			0x00, debugging, '\n', 0 );
	}
	return( 0 );

error:
	return( 1 );
}


/***************************************************************************
 * Do_control_lpd:
 *  get the LPD status
 * 1. get the lpd PID
 * 2. if no options, report PID
 * 3. if option = HUP, send signal
 ***************************************************************************/

int Do_control_lpd( char *user, int action, int *socket,
	int tokencount, struct token *tokens, char *error, int errorlen )
{
	char lpd[LINEBUFFER];

	/* get the spool entries */
	DEBUG8("Do_control_lpd: tokencount %d", tokencount );

	error[0] = 0;
	lpd[0] = 0;
	switch( tokencount ){
	case -1:
	case 0:
		plp_snprintf( lpd, sizeof(lpd), "Server PID %d", Server_pid );
		break;

	case 1:
		if( Server_pid && kill( Server_pid, SIGHUP ) == 0 ){
			plp_snprintf( lpd, sizeof(lpd), "Server PID %d sent SIGHUP", Server_pid );
		} else {
			plp_snprintf( lpd, sizeof(lpd), "Server PID %d, SIGHUP failed %s",
				Server_pid, Errormsg( errno ) );
		}
		break;

	default:
		strncpy( error, "too many arguments", errorlen );
		goto error;
	}

	if( lpd[0] ){
		(void)Link_send( ShortRemote, socket, Send_timeout,
			0x00, lpd, '\n', 0 );
	}
	return( 0 );

error:
	return( 1 );
}


/***************************************************************************
 * Do_control_printcap:
 *  get the LPD status
 * 1. get the printcap PID
 * 2. if no options, report PID
 * 3. if option = HUP, send signal
 ***************************************************************************/

int Do_control_printcap( char *user, int action, int *socket,
	int tokencount, struct token *tokens, char *error, int errorlen )
{
	char *printcap;

	/* get the spool entries */

	DEBUG8("Do_control_printcap: tokencount %d", tokencount );
	printcap = Linearize_pc_list( &pc_used, (char *)0 );

	if( printcap ){
		(void)Link_send( ShortRemote, socket, Send_timeout,
			0x00, printcap, 0, 0 );
	} else {
		(void)Link_send( ShortRemote, socket, Send_timeout,
			0x00, "# No printcap available",'\n', 0 );
	}
	return( 0 );
}
