/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2000, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: getprinter.c,v 5.25 2000/12/28 01:32:55 papowell Exp papowell $";


#include "lp.h"
#include "gethostinfo.h"
#include "getprinter.h"
#include "getqueue.h"
#include "child.h"
/**** ENDINCLUDE ****/

/***************************************************************************
 Get_printer()
    determine the name of the printer - Printer_DYN variable
	Note: this is used by clients to find the name of default printer
	or by server to find forwarding information.  If the printcap
	RemotePrinter_DYN is specified this overrides the printer name.
	1. -P option
	2. $PRINTER, $LPDEST, $NPRINTER, $NGPRINTER argument variable
	3. printcap file
	4. "lp" if none specified
	5. Get the printcap entry (if any),  and re-extract information-
        - printer name (primary name)
		- lp=printer@remote or rp@rm information
    6. recheck the printer name for printer@hostname form,
       and set RemoteHost_DYN to the hostname
	Note: this appears to cover all the cases, with the exception that
	a primary name of the form printer@host will be detected as the
	destination.  Sigh...
 ***************************************************************************/

char *Get_printer(void)
{
	char *s = Printer_DYN;

	DEBUG1("Get_printer: original printer '%s'", s );
	if( s == 0 ) s = getenv( "PRINTER" );
	if( s == 0 ) s = getenv( "LPDEST" );
	if( s == 0 ) s = getenv( "NPRINTER" );
	if( s == 0 ) s = getenv( "NGPRINTER" );

	if( s == 0 ){
		Get_all_printcap_entries();
		if( All_line_list.count ){
			s = All_line_list.list[0];
		}
	}
	if( s == 0 ) s = Default_printer_DYN;
	if( s == 0 ){
		FATAL(LOG_ERR) "Get_printer: no printer name available" );
	}
	Set_DYN(&Printer_DYN,s);
	Expand_vars();
	DEBUG1("Get_printer: final printer '%s'",Printer_DYN);
	return(Printer_DYN);
}

/***************************************************************************
 * Fix_Rm_Rp_info
 *  - get the remote host and remote printer information
 *  - we assume this is called by clients trying to get remote host
 *    connection information
 *  - we may want to get the printcap information as a side effect
 *
 ***************************************************************************/


void Fix_Rm_Rp_info(char *report_conflict, int report_len )
{
	char *s;

	DEBUG1("Fix_Rm_Rp_info: printer name '%s'", Printer_DYN );

	/*
	 * now check to see if we have a remote printer
	 * 1. printer@host form overrides
	 * 2. printcap entry, we use lp=pr@host
	 * 3. printcap entry, we use remote host, remote printer
	 * 4. no printcap entry, we use default printer, default remote host
	 */
	s = Printer_DYN;
	Printer_DYN = 0;
	Reset_config();
	Printer_DYN = s;
	Free_line_list(&PC_alias_line_list);
	Free_line_list(&PC_entry_line_list);
	Set_DYN(&Lp_device_DYN, 0 );
	Set_DYN(&RemotePrinter_DYN, 0 );
	Set_DYN(&RemoteHost_DYN, 0 );

	if( (s = safestrchr( Printer_DYN, '@' ))  ){
		Set_DYN(&RemotePrinter_DYN, Printer_DYN );
		*s = 0;
		Set_DYN(&Queue_name_DYN, Printer_DYN );
		s = safestrchr( RemotePrinter_DYN, '@');
		*s++ = 0;
		Set_DYN(&RemoteHost_DYN, s );
		if( (s = safestrchr(RemoteHost_DYN,'%')) ){
			*s++ = 0;
			Set_DYN(&Lpd_port_DYN,s);
		}
	} else {
		/* we search for the values in the printcap */
		Set_DYN(&Queue_name_DYN, Printer_DYN );
		s = 0;
		if(
			(s = Select_pc_info(Printer_DYN,
			&PC_entry_line_list,
			&PC_alias_line_list,
			&PC_names_line_list, &PC_order_line_list,
			&PC_info_line_list, 0, 0, 0 ))
			||
			(s = Select_pc_info("*",
			&PC_entry_line_list,
			&PC_alias_line_list,
			&PC_names_line_list, &PC_order_line_list,
			&PC_info_line_list, 0, 0, 0 ))
		){

			if( !safestrcmp( s, "*" ) ){
				s = Queue_name_DYN;
			}
			Set_DYN(&Printer_DYN,s);

			DEBUG2("Fix_Rm_Rp_info: found '%s'", Printer_DYN );
			if(DEBUGL2)Dump_line_list("Fix_Rm_Rp_info - PC_alias_line_list",
				&PC_alias_line_list );
			if(DEBUGL2)Dump_line_list("Fix_Rm_Rp_info - PC_entry_line_list",
				&PC_entry_line_list );
		}
		if( !Is_server
			&& (
			(s = Select_pc_info(Printer_DYN,
			&PC_entry_line_list,
			&PC_alias_line_list,
			&PC_names_line_list, &PC_order_line_list,
			&PC_info_line_list, 0, &User_PC_names_line_list, &User_PC_info_line_list ))
			||
			(s = Select_pc_info("*",
			&PC_entry_line_list,
			&PC_alias_line_list,
			&PC_names_line_list, &PC_order_line_list,
			&PC_info_line_list, 0, &User_PC_names_line_list, &User_PC_info_line_list ))
			)
		){

			if( !safestrcmp( s, "*" ) ){
				s = Queue_name_DYN;
			}
			Set_DYN(&Printer_DYN,s);

			DEBUG2("Fix_Rm_Rp_info: User_PC found '%s'", Printer_DYN );
			if(DEBUGL2)Dump_line_list("Fix_Rm_Rp_info - User_PC",
				&PC_entry_line_list );
		}
		if(DEBUGL2)Dump_line_list("Fix_Rm_Rp_info - final PC_entry_line_list",
			&PC_entry_line_list );
		Set_var_list( Pc_var_list, &PC_entry_line_list);
		if( RemoteHost_DYN && Lp_device_DYN && report_conflict ){
			SNPRINTF(report_conflict,report_len)
				"conflicting printcap entries :lp=%s:rm=%s",
				Lp_device_DYN, RemoteHost_DYN );
		}
		if( !Is_server && Force_localhost_DYN ){
			/* we force a connection to the localhost using
			 * the print queue primary name
			 */
			if( safestrchr( Lp_device_DYN, '@' ) ){
				Set_DYN(&RemotePrinter_DYN, Lp_device_DYN );
				s = safestrchr( RemotePrinter_DYN, '@');
				if( s ) *s++ = 0;
			} else {
				Set_DYN( &RemotePrinter_DYN, Printer_DYN );
			}
			Set_DYN( &RemoteHost_DYN, LOCALHOST );
			Set_DYN( &Lp_device_DYN, 0 );
		} else if( safestrchr( Lp_device_DYN, '@' ) ){
			Set_DYN(&RemotePrinter_DYN, Lp_device_DYN );
			s = safestrchr( RemotePrinter_DYN, '@');
			if( s ) *s++ = 0;
			if( *s == 0 ) s = 0;
			Set_DYN(&RemoteHost_DYN, s );
			if( (s = safestrchr(RemoteHost_DYN,'%')) ){
				*s++ = 0;
				Set_DYN(&Lpd_port_DYN,s);
			}
			Set_DYN(&Lp_device_DYN,0);
			goto set_default;
		} else if( Lp_device_DYN && Is_server ){
			Set_DYN(&RemoteHost_DYN,0);
			Set_DYN(&RemotePrinter_DYN,0);
		} else if( RemoteHost_DYN && Is_server ){
			if( RemotePrinter_DYN == 0 || *RemotePrinter_DYN == 0 ){
				Set_DYN( &RemotePrinter_DYN, Printer_DYN );
			}
		} else if( Is_server && Server_names_DYN == 0 ){
			if( report_conflict ){
				SNPRINTF(report_conflict,report_len)
					"no :rm, :lp, or :sv entry" );
			}
		} else {
   set_default:
			if( RemoteHost_DYN == 0 || *RemoteHost_DYN == 0 ){
				Set_DYN( &RemoteHost_DYN, Default_remote_host_DYN );
			}
			if( RemoteHost_DYN == 0 || *RemoteHost_DYN == 0 ){
				Set_DYN( &RemoteHost_DYN, FQDNHost_FQDN );
			}
			if( RemotePrinter_DYN == 0 || *RemotePrinter_DYN == 0 ){
				Set_DYN( &RemotePrinter_DYN, Printer_DYN );
			}
		}
	}

	Expand_vars();
	DEBUG1("Fix_Rm_Rp_info: Printer '%s', Queue '%s', Lp '%s', Rp '%s', Rh '%s'",
		Printer_DYN, Queue_name_DYN, Lp_device_DYN,
		RemotePrinter_DYN, RemoteHost_DYN );
	if(DEBUGL2)Dump_parms("Fix_Rm_Rp_info", Pc_var_list);
}

/***************************************************************************
 * Get_all_printcap_entries( char *s )
 *  - get the remote host and remote printer information
 *  - we assume this is called by clients trying to get remote host
 *    connection information
 *  - we may want to get the printcap information as a side effect
 *
 ***************************************************************************/

void Get_all_printcap_entries(void)
{
	char *s, *t;
	int i;

	/*
	 * now check to see if we have an entry for the 'all:' printcap
	 */
	s = t = 0;
	DEBUG1("Get_all_printcap_entries: starting");
	Free_line_list( &All_line_list );
	if( (s = Select_pc_info(ALL,
			&PC_entry_line_list,
			&PC_alias_line_list,
			&PC_names_line_list, &PC_order_line_list,
			&PC_info_line_list, 0, 0, 0 )) ){
		if( !(t = Find_str_value( &PC_entry_line_list, ALL, Value_sep )) ){
			t = "all";
		}
		DEBUG1("Get_all_printcap_entries: '%s' has '%s'",s,t);
	} else if( (s = Select_pc_info("*",
			&PC_entry_line_list,
			&PC_alias_line_list,
			&PC_names_line_list, &PC_order_line_list,
			&PC_info_line_list, 0, 0, 0 )) ){
		/* we check to see if we have the main wildcard entry */
		if( safestrcmp( s, "*" ) ){
			/* no, we use the entries in the list */
			s = 0;
		} else if( !(t = Find_str_value( &PC_entry_line_list, ALL, Value_sep )) ){
			/* we do have the main wildcard, but it does not have an all entry
			 * so we use the list entries
			 */
			s = 0;
		}
		DEBUG1("Get_all_printcap_entries: '%s' has '%s'",s,t);
	}
	Split(&All_line_list,t,File_sep,0,0,0,1,0);
	if( s == 0 ){
		for( i = 0; i < PC_order_line_list.count; ++i ){
			s = PC_order_line_list.list[i];
			if( !s || !*s || !safestrcmp( ALL, s ) ) continue;
			if( !safestrcmp(s,"*") || !ispunct( cval( s ) ) ){
				Add_line_list(&All_line_list,s,0,0,0);
			}
		}
	}
	/* now we check for the user specified printcap information */
	if( !Is_server ){
		s = t = 0;
		DEBUG1("Get_all_printcap_entries: User_PC starting");
		if( (s = Select_pc_info(ALL,
				&PC_entry_line_list,
				&PC_alias_line_list,
				&PC_names_line_list, &PC_order_line_list,
				&PC_info_line_list, 0,
				&User_PC_names_line_list, &User_PC_info_line_list )) ){
			if( safestrcmp( s, "*" ) ){
				/* no, we use the entries in the list */
				s = 0;
			} else if( !(t = Find_str_value( &PC_entry_line_list, ALL, Value_sep )) ){
				/* we do have the main wildcard, but it does not have an all entry
				 * so we use 'all'
				 */
				s = 0;
			}
			DEBUG1("Get_all_printcap_entries: looking for 'all' User printcap '%s' has '%s'",s,t);
		} else if( (s = Select_pc_info("*",
				&PC_entry_line_list,
				&PC_alias_line_list,
				&PC_names_line_list, &PC_order_line_list,
				&PC_info_line_list, 0,
				&User_PC_names_line_list, &User_PC_info_line_list )) ){
			/* we check to see if we have the main wildcard entry */
			if( safestrcmp( s, "*" ) ){
				/* no, we use the entries in the list */
				s = 0;
			} else if( !(t = Find_str_value( &PC_entry_line_list, ALL, Value_sep )) ){
				/* we do have the main wildcard, but it does not have an all entry
				 * so we use the list entries
				 */
				s = 0;
			}
			DEBUG1("Get_all_printcap_entries: looking for wildcard, User printcap '%s' has '%s'",s,t);
		}
		if( t && *t ){
			Free_line_list( &All_line_list );
			Split(&All_line_list,t,File_sep,0,0,0,1,0);
		}
		if(DEBUGL1)Dump_line_list("Get_all_printcap_entries- after User", &User_PC_order_line_list );
		if( s == 0 ){
			struct line_list l;
			Init_line_list(&l);
			DEBUG1("Get_all_printcap_entries: prefixing user");
			Merge_line_list( &l, &User_PC_order_line_list, 0, 0, 0);
			Merge_line_list( &l, &All_line_list, 0, 0, 0);
			Free_line_list( &All_line_list );
			for( i = 0; i < l.count; ++i ){
				s = l.list[i];
				if( !safestrcmp( ALL, s ) ) continue;
				if( !safestrcmp( "*", s ) ) continue;
				Add_line_list(&All_line_list,s, 0, 0, 0);
			}
			Remove_duplicates_line_list(&All_line_list);
			Free_line_list(&l);
		}
	}

	if(DEBUGL1)Dump_line_list("Get_all_printcap_entries- All_line_list", &All_line_list );
}

void Show_formatted_info( void )
{
	char *s;
	char error[SMALLBUFFER];

	DEBUG1("Show_formatted_info: getting printcap information for '%s'", Printer_DYN );
	error[0] = 0;
	Fix_Rm_Rp_info(error,sizeof(error));
	if( error[0] ){
		WARNMSG(
			"%s: '%s'",
			Printer_DYN, error );
	}
	if(DEBUGL1)Dump_line_list("Aliases",&PC_alias_line_list);
	s = Join_line_list_with_sep(&PC_alias_line_list,"|");
	if( Write_fd_str( 1, s ) < 0 ) cleanup(0);
	if(s) free(s); s = 0;
	Escape_colons( &PC_entry_line_list );
	s = Join_line_list_with_sep(&PC_entry_line_list,"\n :");
	Expand_percent( &s );
	if( s ){
		if( Write_fd_str( 1, "\n :" ) < 0 ) cleanup(0);
		if( Write_fd_str( 1, s ) < 0 ) cleanup(0);
	}
	if( s ) free(s); s =0;
	if( Write_fd_str( 1, "\n" ) < 0 ) cleanup(0);
}

void Show_all_printcap_entries( void )
{
	char *s;
	int i;

	s = 0;
	Get_all_printcap_entries();
	s = Join_line_list_with_sep(&PC_names_line_list,"\n :");
	if( Write_fd_str( 1, "\n.names\n" ) < 0 ) cleanup(0);
	if( s && *s ){
		if( Write_fd_str( 1, " :" ) < 0 ) cleanup(0);
		if( Write_fd_str( 1, s ) < 0 ) cleanup(0);
		if( Write_fd_str( 1, "\n" ) < 0 ) cleanup(0);
	}
	if(s) free(s); s = 0;

	s = Join_line_list_with_sep(&All_line_list,"\n :");
	if( Write_fd_str( 1, "\n.all\n" ) < 0 ) cleanup(0);
	if( s && *s ){
		if( Write_fd_str( 1, " :" ) < 0 ) cleanup(0);
		if( Write_fd_str( 1, s ) < 0 ) cleanup(0);
		if( Write_fd_str( 1, "\n" ) < 0 ) cleanup(0);
	}
	if( s ) free(s); s =0;

	if( Write_fd_str( 1,"\n#Printcap Information\n") < 0 ) cleanup(0);
	for( i = 0; i < All_line_list.count; ++i ){
		Set_DYN(&Printer_DYN,All_line_list.list[i]);
		Show_formatted_info();
	}
}
