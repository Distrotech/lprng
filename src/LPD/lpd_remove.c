/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpd_remove.c
 * PURPOSE: return status
 **************************************************************************/

static char *const _id =
"$Id: lpd_remove.c,v 3.2 1996/08/25 22:20:05 papowell Exp papowell $";

#include "lpd.h"
#include "printcap.h"
#include "lp_config.h"
#include "permission.h"
#include "decodestatus.h"
#include "pr_support.h"
#include "jobcontrol.h"

/***************************************************************************
Commentary:
Patrick Powell Tue May  2 09:32:50 PDT 1995

Remove a Job.
This is very similar to the status program.

1. We check for permissions first
   - first we check to see if the remote host has permissions
   - next we check to see if the user has permissions
Note:
  if we have control permissions,  then we can remove any job.
  Normally,  the options passed are 'user jobnumber' and we
  use the user name and/or job number to select them.  When we have
  control permissions,  we are not restricted to our own jobs.
    so: if we have control permissions,  AND pass an option, we
        do not check for our name.
        if we have control permissions AND we do not pass an option,
        we check for our name.
 ***************************************************************************/

void Get_queue_remove( char *user, char *name, int *socket,
	int tokencount, struct token *tokens );
void Get_queue_remove_server( char *user, char *name, int *socket,
	int tokencount, struct token *tokens );

static int controlperm;

int Job_remove( int *socket, char *input, int maxlen )
{
	struct token tokens[20];
	char error[LINEBUFFER];
	int tokencount;
	int i, c;
	int noperm;			/* no permissions */
	char *name, *s, *end, *user;
	struct printcap *pc, *ppc;

	Name = "Job_remove";

	/* get the format */
	++input;
	if( (s = strchr(input, '\n' )) ) *s = 0;
	DEBUG3("Job_remove: doing '%s'", input );

	/* check printername for characters, underscore, digits */
	tokencount = Crackline(input, tokens, sizeof(tokens)/sizeof(tokens[0]));

	DEBUG6("Job_remove: tokencount %d", tokencount );
	if( tokencount == 0 ){
		plp_snprintf( error, sizeof(error),
			"missing printer name");
		goto error;
	}
	if( tokencount == 1 ){
		plp_snprintf( error, sizeof(error),
			"missing user name");
		goto error;
	}

	for( i = 0; i < tokencount; ++i ){
		tokens[i].start[tokens[i].length] = 0;
	}

	name = tokens[0].start;
	
	if( (s = Clean_name( name )) ){
		plp_snprintf( error, sizeof(error),
			"printer '%s' has illegal char '%c' in name", name, *s );
		goto error;
	}
	Printer = name;
	setproctitle( "lpd %s '%s'", Name, name );

	user = tokens[1].start;

	/* first check to see if you have control permissions */
	Perm_check.printer = name;
	Perm_check.remoteuser = user;
	Perm_check.user = user;
	Perm_check.service = 'C';
	Init_perms_check();
	controlperm =  Perms_check( &Perm_file, &Perm_check,
			(struct control_file *)0 );
	if( controlperm == REJECT ){
		controlperm = 0;
	}
	DEBUG3("Job_remove: controlperm after C check '%s'",
		perm_str(controlperm) );

	noperm = controlperm;
	if( noperm == 0 ){
		Perm_check.service = 'M';
		Init_perms_check();
		noperm = Perms_check( &Perm_file, &Perm_check,
				(struct control_file *)0 );
	}
	DEBUG3("Job_remove: noperm after M check '%s'", perm_str(noperm) );

	if( noperm == 0 ){
		noperm = Last_default_perm;
	}
	DEBUG3("Job_remove: noperm final value '%s'", perm_str(noperm) );

	if( noperm == REJECT ){
		plp_snprintf( error, sizeof(error),
			"%s: no permission to remove jobs", Printer );
		goto error;
	}

	if( strcmp( name, "all" ) ){
		DEBUG4( "Job_remove: checking printcap entry '%s'",  name );
		Get_queue_remove( user, name, socket, tokencount - 1, &tokens[1] );
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
				Get_queue_remove_server( user, Printer, socket,
					tokencount-1,&tokens[1]);
			}
		} else {
			for( i = 0; i < Printcapfile.pcs.count; ++i ){
				ppc = (void *)Printcapfile.pcs.list;
				pc = &ppc[i];
				Printer = pc->pcf->lines.list[pc->name];
				DEBUG4( "Job_control: checking printcap entry '%s'",  Printer );

				/* this is a printcap entry with spool directory -
					we need to check it out
				 */
				s = Get_pc_option( "sd", pc );
				if( s ){
					Get_queue_remove_server( user, Printer, socket,
						tokencount-1,&tokens[1]);
				}
			}
		}
	}
	goto done;

error:
	log( LOG_INFO, "Job_remove: error '%s'", error );
	DEBUG3("Job_remove: error msg '%s'", error );
	(void)Link_send( ShortRemote, socket, Send_timeout,
		0x00, error, '\n', 0 );

done:
	DEBUG4( "Job_remove: done" );
	return( 0 );
}


/***************************************************************************
 * Get_queue_remove_server()
 * create a child for the scanning information
 * and call Get_queue_remote()
 ***************************************************************************/

void Get_queue_remove_server( char *user, char *name, int *socket,
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
	Get_queue_remove( user, name, socket, tokencount, tokens );
	exit(0);
}

/***************************************************************************
 * void Get_queue_remove( char *user, char *name, int *socket,
 *	int tokencount, struct token *tokens )
 * name  - printer name
 * socket - used to send information
 * tokencount, tokens - cracked arguement string
 *  - get the printcap entry (if any)
 *  - check the control file for current status
 *  - find and remove the spool queue entries
 ***************************************************************************/

void Get_queue_remove( char *user, char *name, int *socket,
	int tokencount, struct token *tokens )
{
	int i, j, status;		/* ACME integers... sigh */
	char error[LINEBUFFER];	/* for errors */
	char msg[SMALLBUFFER];	/* and messages */
	struct control_file *cfp, **cfpp;	/* pointer to control file */
	int select;				/* select this job for removal */
	int noperm;			/* no permissions */
	int original_tokencount = tokencount;
	struct token *original_tokens = tokens;
	int allflag = 0;
	struct destination *destination;

	/* set printer name and printcap variables */
	Name = "Get_queue_remove";
	DEBUG4( "Get_queue_remove: user '%s' name '%s' tokencount %d",
		user, name, tokencount );
	if( Debug > 4 ){
		for( i = 0; i < tokencount; ++i ){
			logDebug( "token[%d] '%s'", i, tokens[i].start );
		}
	}

	/* check for 'username all' option */
	for( i = 1; !allflag && i < tokencount; ++i ){
		allflag = !strcasecmp( "all", tokens[i].start );
	}
	DEBUG4("Get_queue_remove: allflag %d", allflag );

	strcpy( error, "??? odd error???" );
	Errorcode = 0;
	setproctitle( "lpd %s '%s'", Name, name );
	if( Setup_printer( name, error, sizeof(error),
		(void *)0, debug_vars, 0, (void *)0 ) ){
		DEBUG4("Get_queue_remove: %s", error );
		goto error;
	}
	DEBUG4("Get_queue_remove: RemoteHost '%s', RemotePrinter '%s', Lp '%s'",
		RemoteHost, RemotePrinter, Lp_device );

	plp_snprintf( msg, sizeof(msg), "Printer %s@%s: ", Printer, ShortHost );
	(void)Link_send( ShortRemote, socket, Send_timeout,
		0x00, msg, '\n', 0 );

	Perm_check.printer = Printer;
	Perm_check.service = 'C';
	if( controlperm == 0 ){
		Init_perms_check();
		controlperm = Perms_check( &Local_perm_file, &Perm_check,
			(struct control_file *)0 );
	}
	if( controlperm == REJECT ){
		controlperm = 0;
	}
	DEBUG3("Get_queue_remove: controlperm after C check '%s'", perm_str(controlperm) );

	/*
	 * handle special case when we have control permissions AND
	 * we are going to delete user files.  We normally have
	 * user key key key  ... but we know that we can remove
	 * files, so we just select on key key key
	 */
	noperm = controlperm;
	if( noperm == 0 ){
		Perm_check.service = 'M';
		Init_perms_check();
		noperm = Perms_check( &Perm_file, &Perm_check,
				(struct control_file *)0 );
		if( noperm == 0 ){
			noperm = Perms_check( &Local_perm_file, &Perm_check,
				(struct control_file *)0 );
		}
	}
	DEBUG3("Get_queue_remove: noperm after M check '%s'", perm_str(noperm) );
	if( noperm == 0 ){
		noperm = Last_default_perm;
	}
	DEBUG3("Get_queue_remove: noperm final value '%s'", perm_str(noperm) );

	if( noperm == REJECT ){
		plp_snprintf( error, sizeof(error),
			"%s: no permission to remove files", Printer );
		goto error;
	}

	/* set up status */

	if( SDpathname == 0 || SDpathname->pathname[0] == 0 ){
		goto remote;
	}

	/* get the spool entries */
	Scan_queue( 0 );
	DEBUG8("Get_queue_remove: total files %d", C_files_list.count );

	/* scan the files to see if there is one which matches */

	cfpp = (void *)C_files_list.list;

	status = 0;
	for( i = 0; status == 0 && i < C_files_list.count; ++i ){
		cfp = cfpp[i];

		/*
		 * check to see if this entry matches any of the patterns
		 */

		DEBUG4("Get_queue_remove: checking '%s'", cfp->name );
		plp_snprintf( msg, sizeof(msg), "checking '%s'", cfp->name );
		status = Link_send( ShortRemote, socket, Send_timeout,
			0x00, msg, '\n', 0 );

		destination = 0;
		if( allflag || tokencount == 1 || controlperm ){
			select = 1;
		} else {
			select = 0;
			for( j = 0; !select && j < tokencount; ++j ){
				select |= patselect( &tokens[j], cfp, &destination );
			}
		}
		if( !select ) continue;
		DEBUG4("Get_queue_remove: considering '%s'", cfp->name );
		plp_snprintf( msg, sizeof(msg), "  checking perms '%s'", cfp->name );
		status = Link_send( ShortRemote, socket, Send_timeout,
			0x00, msg, '\n', 0 );

		/* we check to see if we can remove this one if we are the
			user */

		noperm = controlperm;
		if( noperm == 0 ){
			/* now we get the user name and IP address */
			if( cfp->LOGNAME && cfp->LOGNAME[1] ){
				Perm_check.user = cfp->LOGNAME+1;
			} else {
				Perm_check.user = 0;
			}
			if( cfp->FROMHOST && cfp->FROMHOST[1] ){
				Perm_check.host = Find_fqdn( cfp->FROMHOST+1, Domain_name );
				Perm_check.ip = ntohl(Find_ip( Perm_check.host ));
			} else {  
				Perm_check.host = 0;
				Perm_check.ip = 0;
			}
			Perm_check.service = 'M';
			Init_perms_check();
			noperm = Perms_check( &Perm_file, &Perm_check, cfp );
			if( noperm == 0 ){
				noperm = Perms_check( &Local_perm_file, &Perm_check, cfp );
			}
			if( noperm == 0 ){
				noperm = Last_default_perm;
			}
		}
		if( noperm == REJECT ){
			plp_snprintf( msg, sizeof(msg), "  no permissions '%s'", cfp->name );
			status = Link_send( ShortRemote, socket, Send_timeout,
				0x00, msg, '\n', 0 );

			continue;
		}

		destination = 0;

next_destination:
		if( allflag || tokencount == 1){
			select = 1;
		} else {
			select = 0;
			for( j = 1; !select && j < tokencount; ++j ){
				select |= patselect( &tokens[j], cfp, &destination );
			}
		}

		if( !select ){
			plp_snprintf( msg, sizeof(msg), "  not selecting '%s'", cfp->name );
			status = Link_send( ShortRemote, socket, Send_timeout,
				0x00, msg, '\n', 0 );
			continue;
		}

		DEBUG8("Get_queue_remove: removing '%s', destination 0x%x", cfp->name,
			destination );
		/* we report this job being removed */
		if( destination ){
			DEBUG8("Get_queue_remove: removing '%s' destination '%s'", cfp->name,
				destination->identifier );
			plp_snprintf( msg, sizeof(msg), "  removing '%s' destination %s",
				cfp->name, destination->identifier );
		} else {
			DEBUG8("Get_queue_remove: removing '%s'", cfp->name );
			plp_snprintf( msg, sizeof(msg), "  removing '%s'", cfp->identifier );
		}
		status = Link_send( ShortRemote, socket, Send_timeout,
			0x00, msg, '\n', 0 );

		if( destination ){
			plp_snprintf( destination->error, sizeof( destination->error ),
				"entry deleted" );
			destination->done = time( (void *)0 );
			Set_job_control( cfp );
			goto next_destination;
		}
		if( Remove_job( cfp ) ){
			plp_snprintf( error, sizeof(error),
				"error: could not remove '%s'", cfp->name ); 
			goto error;
		}
		/* check to see if active */
		if( cfp->active ){
			plp_snprintf( msg, sizeof(msg), "killing subserver '%d'",
				cfp->active );
			status = Link_send( ShortRemote, socket, Send_timeout,
				0x00, msg, '\n', 0 );
			/* we need to kill the unspooler */
			/* we will be gentle here */
			DEBUG4("Get_queue_remove: kill subserver pid '%d'", cfp->active );
			if( kill( cfp->active, SIGUSR1 ) < 0 ){
				logerr( LOG_INFO, "Get_queue_remove: kill pid %d failed" );
			}
			/* sigh ... yes, you may need to start it */
			kill( cfp->active, SIGCONT );
		}
		if( tokencount == 1 && allflag == 0 ){
			DEBUG4("Get_queue_remove: single job remove finished '%s'", name );
			return;
		}
	}
	/*********
	if( status == 0 ){
		status = Link_send( ShortRemote, socket, Send_timeout,
		0x00, 0, '\n', 0 );
	}
	**********/
	DEBUG4( "Get_queue_remove: before bounce user '%s' name '%s' tokencount %d",
		user, name, tokencount );
	if( Debug > 4 ){
		for( i = 0; i < tokencount; ++i ){
			logDebug( "token[%d] '%s'", i, tokens[i].start );
		}
	}

	if( Bounce_queue_dest ){
		DEBUG4( "Get_queue_remove: bounce queue %s", Bounce_queue_dest );
		if( strchr( Bounce_queue_dest, '@' ) == 0 ){
			Errorcode = JABORT;
			fatal( LOG_ERR,
			"Get_queue_remove: bounce queue '%s' missing host",
				Lp_device );
		}
		RemoteHost = Orig_RemoteHost;
		RemotePrinter = Orig_RemotePrinter;
		Lp_device = Bounce_queue_dest;
		Check_remotehost(1);
	}

remote:

	tokencount = original_tokencount;
	tokens = original_tokens;

	/* now get the remote host information */
	if( RemoteHost ){
		static struct malloc_list args;
		char **list;
		while( args.max < tokencount +1 ){
			extend_malloc_list( &args, sizeof( char *), tokencount + 1 );
		}
		args.count = 0;
		list = (void *)args.list;
		for( args.count = 0, i = 1; i < tokencount; ++i, ++args.count ){
			list[args.count] = tokens[i].start;
		}
		list[args.count] = 0;
		DEBUG4("Get_queue_remove: user '%s' on remote host '%s' printer '%s'", 
			user, RemoteHost, RemotePrinter );
		Send_lprmrequest( RemotePrinter?RemotePrinter:Printer,
			RemoteHost, user, list,
			Connect_timeout, Send_timeout, *socket );
	}

	/* see if there are servers for this queue */

	if( Server_names ){
		static struct malloc_list servers;
		struct server_info *server_info;

		Get_subserver_info( &servers, Server_names );
		server_info = (void *)servers.list;
		for( i = 0; i < servers.count; ++server_info, ++i ){
			DEBUG4("Get_queue_remove: removing subservers jobs '%s'", 
				server_info->name );
			Get_queue_remove( user, server_info->name, socket,
				tokencount, tokens );
			DEBUG4("Get_queue_remove: finished removing subserver jobs '%s'", 
				server_info->name );
		}
	}

	DEBUG4("Get_queue_remove: finished '%s'", name );
	return;

error:
	log( LOG_INFO, "Get_queue_remove: error '%s'", error );
	DEBUG3("Get_queue_remove: error msg '%s'", error );
	(void)Link_send( ShortRemote, socket, Send_timeout,
		0x00, error, '\n', 0 );
	DEBUG4( "Get_queue_remove: done" );
	return;
}
