/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpd_remove.c
 * PURPOSE: return status
 **************************************************************************/

static char *const _id =
"$Id: lpd_remove.c,v 3.16 1998/01/18 00:10:32 papowell Exp papowell $";

#include "lp.h"
#include "printcap.h"
#include "checkremote.h"
#include "cleantext.h"
#include "decodestatus.h"
#include "errorcodes.h"
#include "gethostinfo.h"
#include "jobcontrol.h"
#include "killchild.h"
#include "linksupport.h"
#include "malloclist.h"
#include "patselect.h"
#include "permission.h"
#include "pr_support.h"
#include "sendlprm.h"
#include "setstatus.h"
#include "setupprinter.h"
#include "waitchild.h"
#include "removejob.h"
/**** ENDINCLUDE ****/

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
	char *name, *s, *user;

	Name = "Job_remove";

	/* get the format */
	++input;
	if( (s = strchr(input, '\n' )) ) *s = 0;
	DEBUG2("Job_remove: doing '%s'", input );

	/* check printername for characters, underscore, digits */
	tokencount = Crackline(input, tokens, sizeof(tokens)/sizeof(tokens[0]));

	DEBUG3("Job_remove: tokencount %d", tokencount );
	if( tokencount == 0 ){
		plp_snprintf( error, sizeof(error),
			_("missing printer name"));
		goto error;
	}
	if( tokencount == 1 ){
		plp_snprintf( error, sizeof(error),
			_("missing user name"));
		goto error;
	}

	for( i = 0; i < tokencount; ++i ){
		tokens[i].start[tokens[i].length] = 0;
	}

	name = tokens[0].start;
	
	if( (s = Clean_name( name )) ){
		plp_snprintf( error, sizeof(error),
			_("printer '%s' has illegal char '%c' in name"), name, *s );
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
			Cfp_static );
	if( controlperm == REJECT ){
		controlperm = 0;
	}
	DEBUG2("Job_remove: controlperm after C check '%s'",
		perm_str(controlperm) );

	noperm = controlperm;
	if( noperm == 0 ){
		Perm_check.service = 'M';
		Init_perms_check();
		noperm = Perms_check( &Perm_file, &Perm_check,
				Cfp_static );
	}
	DEBUG2("Job_remove: noperm after M check '%s'", perm_str(noperm) );

	if( noperm == 0 ){
		noperm = Last_default_perm;
	}
	DEBUG2("Job_remove: noperm final value '%s'", perm_str(noperm) );

	if( noperm == REJECT ){
		plp_snprintf( error, sizeof(error),
			_("%s: no permission to remove jobs"), Printer );
		goto error;
	}

	if( strcmp( name, "all" ) ){
		DEBUG3( "Job_remove: checking printcap entry '%s'",  name );
		Get_queue_remove( user, name, socket, tokencount - 1, &tokens[1] );
	} else {
		/* we work our way down the printcap list, checking for
			ones that have a spool queue */
		/* note that we have already tried to get the 'all' list */
		if( All_list.count ){
			char **line_list;
			struct printcap_entry *entry;
			DEBUG4("checkpc: using the All_list" );
			line_list = All_list.list;
			for( i = 0; i < All_list.count; ++i ){
				Printer = line_list[i];
				if( Printer == 0 || *Printer == 0 || ispunct( *Printer ) ){
					continue;
				}
				Printer = Find_printcap_entry( Printer, &entry );
				if( Printer == 0 || *Printer == 0 || ispunct( *Printer ) ){
					continue;
				}
				s = Get_pc_option_value( "sd", entry );
				if( s ){
					Get_queue_remove( user, Printer, socket,
						tokencount-1,&tokens[1]);
				}
			}
		} else if( Expanded_printcap_entries.count > 0 ){
			struct printcap_entry *entries, *entry;
			DEBUG4("checkpc: using the printcap list" );
			entries = (void *)Expanded_printcap_entries.list;
			c = Expanded_printcap_entries.count;
			for( i = 0; i < c; ++i ){
				entry = &entries[i];
				Printer = entry->names[0];
				DEBUG4("checkpc: printcap entry [%d of %d] '%s'",
					i, c,  Printer );
				if( Printer == 0 || *Printer == 0 || ispunct( *Printer ) ){
					continue;
				}
				s = Get_pc_option_value( "sd", entry );
				if( s ){
					Get_queue_remove( user, Printer, socket,
						tokencount-1,&tokens[1]);
				}
			}
		}
	}
	goto done;

error:
	log( LOG_INFO, _("Job_remove: error '%s'"), error );
	DEBUG2("Job_remove: error msg '%s'", error );
	safestrncat(error,"\n");
	if( Write_fd_str( *socket, error ) < 0 ) cleanup(0);
done:
	DEBUG3( "Job_remove: done" );
	return( 0 );
}

static void
Remote_remove(int *socket, int tokencount,
	      struct token *tokens, char *user, char *orig_name)
{
  DEBUG3( "Remote_remove: checking '%s'", RemotePrinter );
  if( RemotePrinter && RemoteHost == 0 ){
    RemoteHost = Default_remote_host;
    if( RemoteHost == 0 ){
      RemoteHost = FQDNHost;
    }
  }
  if( RemotePrinter ){
    static struct malloc_list args;
    char **list;
    int i;
    
    Find_fqdn( &RemoteHostIP, RemoteHost, 0 );
    if( Same_host( &RemoteHostIP, &HostIP ) == 0 ){
      DEBUG3("Remote_remove: same host, using recursion" );
      Get_queue_remove( user, RemotePrinter, socket,
			tokencount, tokens );
      DEBUG3("Remote_remove: finished removal %s", orig_name );
    } else {
      if( tokencount+1 >= args.max ){
	extend_malloc_list( &args, sizeof( char *), tokencount+1 );
      }
      args.count = 0;
      list = (void *)args.list;
      for( args.count = 0, i = 1; i < tokencount; ++i, ++args.count ){
	list[args.count] = tokens[i].start;
      }
      list[args.count] = 0;
      DEBUG3("Remote_remove: removing jobs from remote host '%s'", 
	     RemoteHost );
      /* get extended format */
      Send_lprmrequest( RemotePrinter, RemoteHost, user, list,
			Connect_timeout, Send_query_rw_timeout, *socket );
    }
  }
  DEBUG3("Remote_remove: finished '%s'", RemotePrinter );
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
	char orig_name[LINEBUFFER];	/* original name */
	char error[LINEBUFFER];	/* for errors */
	char msg[SMALLBUFFER];	/* and messages */
	struct control_file *cfp, **cfpp;	/* pointer to control file */
	int select;				/* select this job for removal */
	int noperm;			/* no permissions */
	int original_tokencount = tokencount;
	struct token *original_tokens = tokens;
	int allflag = 0;
	struct destination *destination;
	int pid;			/* pid of active server */

	/* set printer name and printcap variables */
	Name = "Get_queue_remove";
	DEBUG3( "Get_queue_remove: user '%s' name '%s' tokencount %d",
		user, name, tokencount );
	if(DEBUGL4 ){
		for( i = 0; i < tokencount; ++i ){
			logDebug( _("token[%d] '%s'"), i, tokens[i].start );
		}
	}
	safestrncpy(orig_name, name );

	/* check for 'username all' option */
	for( i = 1; !allflag && i < tokencount; ++i ){
		allflag = !strcasecmp( "all", tokens[i].start );
	}
	DEBUG3("Get_queue_remove: allflag %d", allflag );

	strcpy( error, _("??? odd error???") );
	Errorcode = 0;
	setproctitle( "lpd %s '%s'", Name, name );
	if( Setup_printer( name, error, sizeof(error),
		debug_vars, 1, (void *)0, (void *)0) ){
		DEBUG3("Get_queue_remove: %s", error );
		goto error;
	}
	DEBUG3("Get_queue_remove: RemoteHost '%s', RemotePrinter '%s', Lp '%s'",
		RemoteHost, RemotePrinter, Lp_device );

	plp_snprintf( msg, sizeof(msg), _("Printer %s@%s: \n"), Printer, ShortHost );
	if( Write_fd_str( *socket, msg ) < 0 ) cleanup(0);

	Perm_check.printer = Printer;
	Perm_check.service = 'C';
	if( controlperm == 0 ){
		Init_perms_check();
		controlperm = Perms_check( &Local_perm_file, &Perm_check,
			Cfp_static );
	}
	if( controlperm == REJECT ){
		controlperm = 0;
	}
	DEBUG2("Get_queue_remove: controlperm after C check '%s'", perm_str(controlperm) );

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
				Cfp_static );
		if( noperm == 0 ){
			noperm = Perms_check( &Local_perm_file, &Perm_check,
				Cfp_static );
		}
	}
	DEBUG2("Get_queue_remove: noperm after M check '%s'", perm_str(noperm) );
	if( noperm == 0 ){
		noperm = Last_default_perm;
	}
	DEBUG2("Get_queue_remove: noperm final value '%s'", perm_str(noperm) );

	if( noperm == REJECT ){
		plp_snprintf( error, sizeof(error),
			_("%s: no permission to remove files"), Printer );
		goto error;
	}

	/* set up status */

	if( SDpathname == 0 || SDpathname->pathname[0] == 0 ){
		goto remote;
	}

	DEBUG4("Get_queue_remove: total files %d", C_files_list.count );

	/* scan the files to see if there is one which matches */

	cfpp = (void *)C_files_list.list;

	status = 0;
	for( i = 0; status == 0 && i < C_files_list.count; ++i ){
		cfp = cfpp[i];

		/*
		 * check to see if this entry matches any of the patterns
		 */

		Make_identifier( cfp );
		DEBUG3("Get_queue_remove: checking '%s'", cfp->identifier+1 );
		plp_snprintf( msg, sizeof(msg), _("checking '%s'\n"), cfp->identifier+1 );
		if( Write_fd_str( *socket, msg ) < 0 ) cleanup(0);

		destination = 0;
		if( allflag || tokencount == 1 || controlperm ){
			select = 1;
		} else {
			select = 0;
			for( j = 0; !select && j < tokencount; ++j ){
				select |= Patselect( &tokens[j], cfp, &destination );
			}
		}
		if( !select ) continue;
		DEBUG3("Get_queue_remove: considering '%s'", cfp->identifier+1 );
		plp_snprintf( msg, sizeof(msg), _("  checking perms '%s'\n"), cfp->identifier+1 );
		if( Write_fd_str( *socket, msg ) < 0 ) cleanup(0);

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
			if( cfp->FROMHOST && cfp->FROMHOST[1]
				&& Find_fqdn( &PermcheckHostIP,cfp->FROMHOST+1, 0 ) ){
				Perm_check.host = &PermcheckHostIP;
			} else {  
				Perm_check.host = 0;
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
			plp_snprintf( msg, sizeof(msg), _("  no permissions '%s'\n"),
				cfp->identifier+1 );
			if( Write_fd_str( *socket, msg ) < 0 ) cleanup(0);

			continue;
		}

		destination = 0;

next_destination:
		if( allflag || tokencount == 1){
			select = 1;
		} else {
			select = 0;
			for( j = 1; !select && j < tokencount; ++j ){
				select |= Patselect( &tokens[j], cfp, &destination );
			}
		}

		if( !select ){
			plp_snprintf( msg, sizeof(msg), _("  not selecting '%s'\n"),
				cfp->identifier+1 );
			if( Write_fd_str( *socket, msg ) < 0 ) cleanup(0);
			continue;
		}

		DEBUG4("Get_queue_remove: removing '%s', destination 0x%x",
			cfp->identifier+1,
			destination );
		/* we report this job being removed */
		if( destination ){
			DEBUG4("Get_queue_remove: removing '%s' destination '%s'",
				cfp->identifier+1, destination->identifier+1 );
			plp_snprintf( msg, sizeof(msg), _("  dequeued '%s' destination %s\n"),
				cfp->identifier+1, destination->identifier+1 );
			if( Write_fd_str( *socket, msg ) < 0 ) cleanup(0);
			plp_snprintf( destination->error, sizeof( destination->error ),
				_("entry deleted") );
			destination->done = time( (void *)0 );
			Set_job_control( cfp, (void *)0 );
			goto next_destination;
		}
		/* log this to the world */
		DEBUG4("Get_queue_remove: removing '%s'", cfp->identifier+1 );
		plp_snprintf( msg, sizeof(msg), _("  dequeued '%s'\n"),
			cfp->identifier+1 );
		if( Write_fd_str( *socket, msg ) < 0 ) cleanup(0);

		setmessage( cfp, "LPRM", "%s@%s: lprm STARTING removing job", Printer, FQDNHost );
		if( Remove_job( cfp ) ){
			plp_snprintf( error, sizeof(error),
				_("error: could not remove '%s'"), cfp->identifier+1 ); 
			setmessage( cfp, "LPRM", "%s@%s: lprm FAILED removing job", Printer, FQDNHost );
			goto error;
		}
		setmessage( cfp, "LPRM", "%s@%s: lprm SUCCEEDED removing job", Printer, FQDNHost );
		/* check to see if active */
		if( cfp->hold_info.server > 0 && (pid = cfp->hold_info.subserver) > 0 ){
			plp_snprintf( msg, sizeof(msg), _("killing subserver '%d'\n"), pid );
			if( Write_fd_str( *socket, msg ) < 0 ) cleanup(0);
			/* we need to kill the unspooler */
			/* we will not be gentle here */
			DEBUG3("Get_queue_remove: kill subserver pid '%d'", pid );
			kill( pid, SIGUSR1 );
				/* sigh ... yes, you may need to start it */
			kill( pid, SIGCONT );
		}
		if( tokencount == 1 && allflag == 0 ){
			DEBUG3("Get_queue_remove: single job remove finished '%s'", name );
			return;
		}
	}
	DEBUG3( "Get_queue_remove: before bounce user '%s' name '%s' tokencount %d",
		user, name, tokencount );
	if(DEBUGL4 ){
		for( i = 0; i < tokencount; ++i ){
			logDebug( "token[%d] '%s'", i, tokens[i].start );
		}
	}

	if( Bounce_queue_dest ){
		DEBUG3( "Get_queue_remove: bounce queue %s", Bounce_queue_dest );
		RemoteHost = 0;
		RemotePrinter = 0;
		/* we use the current host for bounce queue */
		if( strchr( Bounce_queue_dest, '@' ) == 0 ){
			RemotePrinter = Bounce_queue_dest;
			RemoteHost = FQDNHost;
		} else {
			Lp_device = Bounce_queue_dest;
			Check_remotehost();
			if( Check_loop() ){
				Errorcode = JABORT;
				fatal( LOG_ERR,
				"Get_queue_remove: bounce queue loop '%s'", Lp_device );
			}
		}
	}

remote:

	tokencount = original_tokencount;
	tokens = original_tokens;


	/* see if there are servers for this queue */

	if( Server_names ){
		static struct malloc_list servers;
		struct server_info *server_info;

		Get_subserver_info( &servers, Server_names );
		server_info = (void *)servers.list;
		for( i = 0; i < servers.count; ++server_info, ++i ){
			DEBUG3("Get_queue_remove: removing subservers jobs '%s'", 
				server_info->name );
			Get_queue_remove( user, server_info->name, socket,
				tokencount, tokens );
			DEBUG3("Get_queue_remove: finished removing subserver jobs '%s'", 
				server_info->name );
		}
		RemotePrinter = RemoteHost = 0;
		if( servers.list ) free( servers.list );
	} else if( Destinations ) {
		static struct malloc_list servers;
		struct server_info *server_info;

		Get_subserver_info( &servers, Destinations );
		server_info = (void *)servers.list;
		for( i = 0; i < servers.count; ++server_info, ++i ){
		  DEBUG3("Get_queue_remove: removing from destination '%s'", 
			 server_info->name);
		  RemoteHost = 0;
		  RemotePrinter = 0;
		  if( strchr( server_info->name, '@' ) ){
		    Lp_device = server_info->name;
		    Check_remotehost();
		    if( Check_loop() ){ 
		      plp_snprintf( error, sizeof(error),
				    "printer '%s' loop to bounce queue '%s'",
				    server_info->name );
		    }
		  }
		  if( RemotePrinter == 0 ){
		    RemotePrinter = server_info->name;
		  }

		  Remote_remove(socket, tokencount, tokens, user, orig_name);
		}
		if( servers.list ) free( servers.list );
		goto done;
	} else if( Bounce_queue_dest ){
		DEBUG3("Get_queue_remove: removing from bouncequeue '%s'", 
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
	Remote_remove(socket, tokencount, tokens, user, orig_name);

done:
	Printer = orig_name;
	DEBUG3("Get_queue_remove: finished '%s'", orig_name );
	/* start the server again if necessary */
	Start_new_server();
	return;

error:
	log( LOG_INFO, "Get_queue_remove: error '%s'", error );
	DEBUG2("Get_queue_remove: error msg '%s'", error );
	safestrncat(error,"\n");
	if( Write_fd_str( *socket, msg ) < 0 ) cleanup(0);
	DEBUG3( "Get_queue_remove: done" );
	return;
}
