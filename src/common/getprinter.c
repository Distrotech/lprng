/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: getprinter.c,v 5.3 1999/10/24 19:47:59 papowell Exp papowell $";


#include "lp.h"
#include "gethostinfo.h"
#include "getprinter.h"
#include "getqueue.h"
/**** ENDINCLUDE ****/

/***************************************************************************
Get_printer()
    determine the name of the printer - Printer_DYN variable
	Note: this is used by clients to find the name of default printer
	or by server to find forwarding information.  If the printcap
	RemotePrinter_DYN is specified this overrides the printer name.
	1. -P option
	2. $PRINTER argument variable
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
	if( s == 0 ) s = getenv( "NGPRINTER" );

	if( s == 0 ){
		Get_all_printcap_entries();
		if( All_line_list.count ){
			s = All_line_list.list[0];
		}
	}
	if( s == 0 ) s = Default_printer_DYN;
	if( s == 0 ){
		fatal( LOG_ERR, "Get_printer: no printer name available" );
	}
	Set_DYN(&Printer_DYN,s);
	Expand_vars();
	DEBUG1("Get_printer: final printer '%s'",Printer_DYN);
	return(Printer_DYN);
}

/***************************************************************************
 * Fix_Rm_Rp_info( char *s )
 *  - get the remote host and remote printer information
 *  - we assume this is called by clients trying to get remote host
 *    connection information
 *  - we may want to get the printcap information as a side effect
 *
 ***************************************************************************/


void Fix_Rm_Rp_info(void)
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
		if( (s = Select_pc_info(Printer_DYN, &PC_alias_line_list,
			&PC_entry_line_list,
			&PC_names_line_list, &PC_order_line_list,
			&PC_info_line_list, 0 ))
			||
			(Is_server && Default_printer_when_unknown
				&& (s = Select_pc_info(Default_printer_when_unknown,
					&PC_alias_line_list, &PC_entry_line_list,
					&PC_names_line_list, &PC_order_line_list,
					&PC_info_line_list, 0 )) ) ){

			Set_DYN(&Printer_DYN,s);

			DEBUG2("Fix_Rm_Rp_info: found '%s'", Printer_DYN );
			if(DEBUGL2)Dump_line_list("Fix_Rm_Rp_info - PC_alias_line_list",
				&PC_alias_line_list );
			if(DEBUGL2)Dump_line_list("Fix_Rm_Rp_info - PC_entry_line_list",
				&PC_entry_line_list );
			Set_var_list( Pc_var_list, &PC_entry_line_list);
		}
		if( !Is_server && Force_localhost_DYN ){
			/* we force a connection to the localhost using
			 * the print queue primary name
			 */
			Set_DYN( &RemoteHost_DYN, LOCALHOST );
			Set_DYN( &RemotePrinter_DYN, Printer_DYN );
		} else if( Lp_device_DYN && (s = safestrchr( Lp_device_DYN, '@' )) ){
			Set_DYN(&RemotePrinter_DYN, Lp_device_DYN );
			s = safestrchr( RemotePrinter_DYN, '@');
			if( s ) *s++ = 0;
			Set_DYN(&RemoteHost_DYN, s );
			if( (s = safestrchr(RemoteHost_DYN,'%')) ){
				*s++ = 0;
				Set_DYN(&Lpd_port_DYN,s);
			}
			if( Is_server ){
				Set_DYN(&Lp_device_DYN,0);
			}
		} else if( Lp_device_DYN && Is_server ){
			Set_DYN(&RemoteHost_DYN,0);
			Set_DYN(&RemotePrinter_DYN,0);
		} else {
			if( RemoteHost_DYN == 0 ){
				Set_DYN( &RemoteHost_DYN, Default_remote_host_DYN );
			}
			if( RemoteHost_DYN == 0 ){
				Set_DYN( &RemoteHost_DYN, FQDNHost_FQDN );
			}
			if( RemotePrinter_DYN == 0 ){
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

	DEBUG1("Get_all_printcap_entries: starting");

	/*
	 * now check to see if we have an entry for the 'all:' printcap
	 */
	Free_line_list(&PC_alias_line_list);
	Free_line_list(&PC_entry_line_list);
	Free_line_list(&All_line_list);
	DEBUG1("Get_all_printcap_entries: starting");
	if( !Find_str_value( &PC_names_line_list, ALL, Value_sep)
		&& PC_filters_line_list.count ){
		struct line_list raw;

		/* try the filter list to get the information */
		Init_line_list(&raw);
		Filterprintcap( &raw, &PC_filters_line_list, ALL);
		Build_printcap_info( &PC_names_line_list, &PC_order_line_list,
			&PC_info_line_list, &raw, &Host_IP );
		/* now we can free up the raw list */
		Free_line_list( &raw );
		if(DEBUGL3){
		Dump_line_list("Get_all_printcap_entries: PC names", &PC_names_line_list );
		Dump_line_list("Get_all_printcap_entries: PC order", &PC_order_line_list );
		Dump_line_list("Get_all_printcap_entries: PC info", &PC_info_line_list );
		Dump_line_list("Get_all_printcap_entries: PC names", &PC_names_line_list );
		}
	}

	/* look for the information */
	if( (s = Select_pc_info( ALL, &PC_alias_line_list,
		&PC_entry_line_list,
		&PC_names_line_list, &PC_order_line_list,
		&PC_info_line_list, 0 )) ){
		t = Find_str_value( &PC_entry_line_list, ALL, Value_sep );
		DEBUG2("Get_all_printcap_entries: found '%s'='%s'", s,t );
		Split(&All_line_list,t,File_sep,0,0,0,1,0);
	} else {
		Merge_line_list(&All_line_list,&PC_order_line_list,0,0,0);
	}

	for( i = 0; i < All_line_list.count; ++i ){
		if( !safestrcasecmp( ALL, All_line_list.list[i] )
			|| ispunct( cval( All_line_list.list[i] ) )
			|| strstr( ":tc_only", All_line_list.list[i] )
			){
			Remove_line_list( &All_line_list, i );
			--i;
		}
	}
	if(DEBUGL1)Dump_line_list("Get_all_printcap_entries- All_line_list", &All_line_list );
}
