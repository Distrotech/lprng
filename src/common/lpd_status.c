/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: lpd_status.c,v 5.2 1999/10/12 18:50:33 papowell Exp papowell $";


#include "lp.h"
#include "getopt.h"
#include "gethostinfo.h"
#include "proctitle.h"
#include "getprinter.h"
#include "getqueue.h"
#include "child.h"
#include "fileopen.h"
#include "sendreq.h"
#include "globmatch.h"
#include "permission.h"

#include "lpd_status.h"

/**** ENDINCLUDE ****/

/***************************************************************************
 * Commentary:
 * Patrick Powell Tue May  2 09:32:50 PDT 1995
 * 
 * Return status:
 * 	status has two formats: short and long
 * 
 * Status information is obtained from 3 places:
 * 1. The status file
 * 2. any additional progress files indicated in the status file
 * 3. job queue
 * 
 * The status file is maintained by the current unspooler process.
 * It updates this file with the following information:
 * 
 * PID of the unspooler process   [line 1]
 * active job and  status file name
 * active job and  status file name
 * 
 * Example 1:
 * 3012
 * cfa1024host status
 * 
 * Example 2:
 * 3015
 * cfa1024host statusfd2
 * cfa1026host statusfd1
 * 
 * Format of the information reporting:
 * 
 * 0        1         2         3         4         5         6         7
 * 12345678901234567890123456789012345678901234567890123456789012345678901234
 *  Rank   Owner/ID                   Class Job  Files               Size Time
 * 1      papowell@astart4+322          A  322 /tmp/hi                3 17:33:47
 * x     Sx                           SxSx    Sx                 Sx    Sx       X
 *                                                                              
 ***************************************************************************/

#define RANKW 7
#define OWNERW 29
#define CLASSW 2
#define JOBW 6
#define FILEW 18
#define SIZEW 6
#define TIMEW 8


int Job_status( int *sock, char *input )
{
	char *s, *t, *name;
	int displayformat, status_lines = 0, i;
	struct line_list l, listv;
	struct line_list done_list;
	char error[SMALLBUFFER];
	int db, dbflag;

	Init_line_list(&l);
	Init_line_list(&listv);
	Init_line_list(&done_list);
	db = Debug;
	dbflag = DbgFlag;

	Name = "Job_status";

	/* get the format */
	if( (s = safestrchr(input, '\n' )) ) *s = 0;
	displayformat = *input++;

	/*
	 * if we get a short/long request from these hosts,
	 * reverse the sense of question
	 */
	if( Reverse_lpq_status_DYN
		&& (displayformat == REQ_DSHORT || displayformat==REQ_DLONG)  ){
		Free_line_list(&l);
		Split(&l,Reverse_lpq_status_DYN,File_sep,0,0,0,0,0);
		if( Match_ipaddr_value( &l, &RemoteHost_IP ) == 0 ){
			DEBUGF(DLPQ1)("Job_status: reversing status sense");
			if( displayformat == REQ_DSHORT ){
				displayformat = REQ_DLONG;
			} else {
				displayformat = REQ_DSHORT;
			}
		}
		Free_line_list(&l);
	}
	/*
	 * we have a list of hosts with format of the form:
	 *  Key=list; Key=list;...
	 *  key is s for short, l for long
	 */
	DEBUGF(DLPQ1)("Job_status: Force_lpq_status_DYN '%s'", Force_lpq_status_DYN);
	if( Force_lpq_status_DYN ){
		Free_line_list(&listv);
		Split(&listv,Force_lpq_status_DYN,";",0,0,0,0,0);
		for(i = 0; i < listv.count; ++i ){
			s = listv.list[i];
			if( (t = safestrpbrk(s,Value_sep)) ) *t++ = 0;
			Free_line_list(&l);
			Split(&l,t,Value_sep,0,0,0,0,0);
			DEBUGF(DLPQ1)("Job_status: Force_lpq_status '%s'='%s'", s,t);
			if( Match_ipaddr_value( &l, &RemoteHost_IP ) == 0 ){
				DEBUGF(DLPQ1)("Job_status: forcing status '%s'", s);
				if( safestrcasecmp(s,"s") == 0 ){
					displayformat = REQ_DSHORT;
				} else if( safestrcasecmp(s,"l") == 0 ){
					displayformat = REQ_DLONG;
				}
				status_lines = Short_status_length_DYN;
				break;
			}
		}
		Free_line_list(&l);
		Free_line_list(&listv);
	}

	/*
	 * check for short status to be returned
	 */

	if( Return_short_status_DYN && displayformat == REQ_DLONG ){
		Free_line_list(&l);
		Split(&l,Return_short_status_DYN,File_sep,0,0,0,0,0);
		if( Match_ipaddr_value( &l, &RemoteHost_IP ) == 0 ){
			status_lines = Short_status_length_DYN;
			DEBUGF(DLPQ1)("Get_queue_status: truncating status to %d",
				status_lines);
		}
		Free_line_list(&l);
	}

	DEBUGF(DLPQ2)("Job_status: doing '%s'", input );
	Free_line_list(&l);
	Split(&l,input,Whitespace,0,0,0,0,0);
	if( l.count == 0 ){
		plp_snprintf( error, sizeof(error), "zero length command line");
		goto error;
	}

	name = l.list[0];
	
	if( (s = Clean_name( name )) ){
		plp_snprintf( error, sizeof(error),
			_("printer '%s' has illegal character at '%s' in name"), name, s );
		goto error;
	}

	Set_DYN(&Printer_DYN,name);
	name = Printer_DYN;
	setproctitle( "lpd %s '%s'", Name, name );
	Remove_line_list(&l, 0 );
	if( safestrcasecmp( name, ALL ) ){
		DEBUGF(DLPQ3)("Job_status: checking printcap entry '%s'",  name );
		Get_queue_status( &l, sock, displayformat, status_lines,
			&done_list, Max_status_size_DYN );
	} else {
		/* we work our way down the printcap list, checking for
			ones that have a spool queue */
		/* note that we have already tried to get the 'all' list */
		
		Get_all_printcap_entries();
		for( i = 0; i < All_line_list.count; ++i ){
			Set_DYN(&Printer_DYN, All_line_list.list[i] );
			Debug = db;
			DbgFlag = dbflag;
			Get_queue_status( &l, sock, displayformat, status_lines,
				&done_list, Max_status_size_DYN );
		}
	}
	Free_line_list( &l );
	Free_line_list( &listv );
	Free_line_list( &done_list );
	DEBUGF(DLPQ3)("Job_status: DONE" );
	return(0);

 error:
	DEBUGF(DLPQ2)("Job_status: error msg '%s'", error );
	i = strlen(error);
	if( (i = strlen(error)) >= sizeof(error)-2 ){
		i = sizeof(error) - 2;
	}
	error[i++] = '\n';
	error[i] = 0;
	Free_line_list( &l );
	Free_line_list( &listv );
	Free_line_list( &done_list );
	if( Write_fd_str( *sock, error ) < 0 ) cleanup(0);
	DEBUGF(DLPQ3)("Job_status: done" );
	return(0);
}

/***************************************************************************
 * void Get_queue_status
 * sock - used to send information
 * displayformat - REQ_DSHORT, REQ_DLONG, REQ_VERBOSE
 * tokens - arguments
 *  - get the printcap entry (if any)
 *  - check the control file for current status
 *  - find and report the spool queue entries
 ***************************************************************************/
void Get_queue_status( struct line_list *tokens, int *sock,
	int displayformat, int status_lines, struct line_list *done_list,
	int max_size )
{
	char msg[SMALLBUFFER], buffer[SMALLBUFFER], error[SMALLBUFFER],
		number[LINEBUFFER], header[LARGEBUFFER];
	char sizestr[SIZEW+TIMEW+32];
	char *pr, *s, *path, *identifier,
		*jobname, *joberror, *class, *priority, *d_identifier,
		*job_time, *d_error, *d_dest, *openname, *hf_name, *filenames;
	struct line_list outbuf, info, lineinfo;
	int status = 0, len, ix, nx, flag, count, held, move,
		server_pid, unspooler_pid, fd, nodest,
		printable, dcount, destinations = 0,
		d_copies, d_copy_done, permission, jobsize, jobnumber, db, dbflag;
	struct stat statb;
	struct job job;

	DEBUG1("Get_queue_status: checking '%s'", Printer_DYN );
	if(DEBUGL1)Dump_line_list( "Get_queue_status: done_list", done_list );

	/* set printer name and printcap variables */

	Init_job(&job);
	Init_line_list(&info);
	Init_line_list(&lineinfo);
	Init_line_list(&outbuf);

	Check_max(tokens,2);
	tokens->list[tokens->count] = 0;
	msg[0] = 0;
	header[0] = 0;
	error[0] = 0;
	pr = s = 0;

	safestrncpy(buffer,Printer_DYN);
	status = Setup_printer( Printer_DYN, error, sizeof(error));

	db = Debug;
	dbflag = DbgFlag;
	s = Find_str_value(&Spool_control,DEBUG,Value_sep);
	if( !s ) s = New_debug_DYN;
	Parse_debug( s, 0 );
	if( !(DLPQMASK & DbgFlag) ){
		Debug = db;
		DbgFlag = dbflag;
	} else {
		int odb, odbf;
		odb = Debug;
		odbf = DbgFlag;
		Debug = db;
		DbgFlag = dbflag;
		if( Log_file_DYN ){
			fd = Trim_status_file( Log_file_DYN, Max_log_file_size_DYN,
				Min_log_file_size_DYN );
			if( fd > 0 && fd != 2 ){
				dup2(fd,2);
				close(fd);
				close(fd);
			}
		}
		Debug = odb;
		DbgFlag = odbf;
	}

	DEBUGF(DLPQ3)("Get_queue_status: Setup_printer status %d '%s'", status, error );
	/* set up status */
	if( Find_exists_value(done_list,Printer_DYN,Value_sep ) ){
		return;
	}
	Add_line_list(done_list,Printer_DYN,Value_sep,1,1);

	if( displayformat != REQ_DSHORT ){
		plp_snprintf( header, sizeof(header), "%s: ",
			Server_queue_name_DYN?"Server Printer":"Printer" );
	}
	len = strlen(header);
	plp_snprintf( header+len, sizeof(header)-len, "%s@%s ",
		Printer_DYN, Report_server_as_DYN?Report_server_as_DYN:ShortHost_FQDN );
	if( safestrcasecmp( buffer, Printer_DYN ) ){
		len = strlen(header);
		plp_snprintf( header+len, sizeof(header)-len, _("(originally %s) "), buffer );
	}

	if( status ){
		len = strlen( header );
		if( displayformat == REQ_VERBOSE ){
			safestrncat( header, _("\n Error: ") );
			len = strlen( header );
		}
		if( error[0] ){
			plp_snprintf( header+len, sizeof(header)-len,
				_(" - %s"), error );
		} else if( !Spool_dir_DYN ){
			plp_snprintf( header+len, sizeof(header)-len,
				_(" - printer %s@%s not in printcap"), Printer_DYN,
					Report_server_as_DYN?Report_server_as_DYN:ShortHost_FQDN );
		} else {
			plp_snprintf( header+len, sizeof(header)-len,
				_(" - printer %s@%s has bad printcap entry"), Printer_DYN,
					Report_server_as_DYN?Report_server_as_DYN:ShortHost_FQDN );
		}
		safestrncat( header, "\n" );
		DEBUGF(DLPQ3)("Get_queue_status: forward header '%s'", header );
		if( Write_fd_str( *sock, header ) < 0 ) cleanup(0);
		header[0] = 0;
		goto done;
	}
	/* check for permissions */

	Perm_check.service = 'Q';
	Perm_check.printer = Printer_DYN;
	Free_line_list(&Perm_line_list);

	Merge_line_list(&Perm_line_list,&RawPerm_line_list,0,0,0);
	if( Perm_filters_line_list.count ){
		Filterprintcap( &Perm_line_list, &Perm_filters_line_list,
			Printer_DYN);
	}
	permission = Perms_check( &Perm_line_list, &Perm_check, 0, 0 );
	DEBUGF(DLPQ2)("Job_status: permission '%s'", perm_str(permission));
	if( permission == P_REJECT ){
		plp_snprintf( error, sizeof(error),
			_("%s: no permission to show status"), Printer_DYN );
		goto error;
	}

	if( displayformat == REQ_VERBOSE ){
		safestrncat( header, "\n" );
		if( Write_fd_str( *sock, header ) < 0 ) cleanup(0);
		header[0] = 0;
	}

	/* get the spool entries */
	Free_line_list( &Sort_order );
	Free_line_list( &outbuf );
	Scan_queue( Spool_dir_DYN, &Spool_control, &Sort_order,
		&printable,&held,&move, 0 );

	DEBUGF(DLPQ3)("Get_queue_status: total files %d", Sort_order.count );
	DEBUGFC(DLPQ3)Dump_line_list("Get_queue_status- Sort_order", &Sort_order );

	/* set up the short format for folks */

	if( displayformat == REQ_DLONG && Sort_order.count > 0 ){
		/*
		 Rank  Owner/ID  Class Job Files   Size Time
		*/
		Add_line_list(&outbuf,
" Rank   Owner/ID                  Class Job Files                 Size Time"
		,0,0,0);
	}
	error[0] = 0;

	for( count = 0;
		displayformat != REQ_DSHORT && count < Sort_order.count;
		++count ){

		Free_job(&job);
		number[0] = 0;
		error[0] = 0;
		msg[0] = 0;
		nodest = 0;
		s = Sort_order.list[count];
		if( (s = safestrchr(s,';')) ){
			Split(&job.info,s+1,";",1,Value_sep,1,1,0);
		}
		DEBUGFC(DLPQ4)Dump_job("Get_queue_status - info", &job );
		if( job.info.count == 0 ) continue;

		if( tokens->count && Patselect( tokens, &job.info, 0) ){
			continue;
		}

		s = Find_str_value(&job.info,PRSTATUS,Value_sep);
		if( s == 0 ){
			plp_snprintf(number,sizeof(number),"%d",count+1);
		} else {
			plp_snprintf(number,sizeof(number),"%s",s);
		}
		identifier = Find_str_value(&job.info,IDENTIFIER,Value_sep);
		if( identifier == 0 ){
			identifier = Find_str_value(&job.info,LOGNAME,Value_sep);
		}
		if( identifier == 0 ){
			identifier = "???";
		}
		priority = Find_str_value(&job.info,PRIORITY,Value_sep);
		class = Find_str_value(&job.info,CLASS,Value_sep);
		jobname = Find_str_value(&job.info,JOBNAME,Value_sep);
		filenames = Find_str_value(&job.info,FILENAMES,Value_sep);
		jobnumber = Find_decimal_value(&job.info,NUMBER,Value_sep);
		joberror = Find_str_value(&job.info,ERROR,Value_sep);
		jobsize = Find_flag_value(&job.info,SIZE,Value_sep);
		job_time = Find_str_value(&job.info,JOB_TIME,Value_sep );
		destinations = Find_flag_value(&job.info,DESTINATIONS,Value_sep);

		openname = Find_str_value(&job.info,OPENNAME,Value_sep);
		if( !openname ){
			DEBUGF(DLPQ4)("Get_queue_status: no openname");
			continue;
		}
		hf_name = Find_str_value(&job.info,HF_NAME,Value_sep);
		if( !hf_name ){
			DEBUGF(DLPQ4)("Get_queue_status: no hf_name");
			continue;
		}

		/* we report this jobs status */

		DEBUGF(DLPQ3)("Get_queue_status: joberror '%s'", joberror );
		DEBUGF(DLPQ3)("Get_queue_status: class '%s', priority '%s'",
			class, priority );

		if( (Class_in_status_DYN && class) || priority == 0 ){
			priority = class;
		}

		if( displayformat == REQ_DLONG ){
			plp_snprintf( msg, sizeof(msg),
				"%-*s %-*s ", RANKW-1, number, OWNERW-1, identifier );
			while( (len = strlen(msg)) > (RANKW+OWNERW)
				&& isspace(cval(msg+len-1)) && isspace(cval(msg+len-2)) ){
				msg[len-1] = 0;
			}
			plp_snprintf( buffer, sizeof(buffer), "%-*s %*d ",
				CLASSW-1,priority, JOBW-1,jobnumber);
			DEBUGF(DLPQ3)("Get_queue_status: msg len %d '%s', buffer %d, '%s'",
				strlen(msg),msg, strlen(buffer), buffer );
			DEBUGF(DLPQ3)("Get_queue_status: RANKW %d, OWNERW %d, CLASSW %d, JOBW %d",
				RANKW, OWNERW, CLASSW, JOBW );
			s = buffer;
			while( strlen(buffer) > CLASSW+JOBW && (s = safestrchr(s,' ')) ){
				if( cval(s+1) == ' ' ){
					memmove(s,s+1,strlen(s)+1);
				} else {
					++s;
				}
			}
			s = msg+strlen(msg)-1;
			while( strlen(msg) + strlen(buffer) > RANKW+OWNERW+CLASSW+JOBW ){
				if( cval(s) == ' ' && cval(s-1) == ' ' ){
					*s-- = 0;
				} else {
					break;
				}
			}
			s = buffer;
			while( strlen(msg) + strlen(buffer) > RANKW+OWNERW+CLASSW+JOBW
				&& (s = safestrchr(s,' ')) ){
				if( cval(s+1) == ' ' ){
					memmove(s,s+1,strlen(s)+1);
				} else {
					++s;
				}
			}
			len = strlen(msg);

			plp_snprintf(msg+len, sizeof(msg)-len,"%s",buffer);
			if( joberror ){
				len = strlen(msg);
					plp_snprintf(msg+len,sizeof(msg)-len,
					"ERROR: %s", joberror );
			} else {
				DEBUGF(DLPQ3)("Get_queue_status: jobname '%s'", jobname );

				len = strlen(msg);
				plp_snprintf(msg+len,sizeof(msg)-len,"%-s",jobname?jobname:filenames);

				DEBUGF(DLPQ3)("Get_queue_status: jobtime '%s'", job_time );
				job_time = Time_str(1, Convert_to_time_t(job_time));
				if( !Full_time_DYN && (s = safestrchr(job_time,'.')) ) *s = 0;

				plp_snprintf( sizestr, sizeof(sizestr), "%*d %-s",
					SIZEW-1,jobsize, job_time );

				len = Max_status_line_DYN;
				if( len >= sizeof(msg)) len = sizeof(msg)-1;
				len = len-strlen(sizestr);
				if( len > 0 ){
					/* pad with spaces */
					for( nx = strlen(msg); nx < len; ++nx ){
						msg[nx] = ' ';
					}
					msg[nx] = 0;
				}
				/* remove spaces if necessary */
				while( strlen(msg) + strlen(sizestr) > Max_status_line_DYN ){
					if( isspace( cval(sizestr) ) ){
						memmove(sizestr, sizestr+1, strlen(sizestr)+1);
					} else {
						s = msg+strlen(msg)-1;
						if( isspace(cval(s)) && isspace(cval(s-1)) ){
							s[0] = 0;
						} else {
							break;
						}
					}
				}
				if( strlen(msg) + strlen(sizestr) > Max_status_line_DYN ){
					len = Max_status_line_DYN - strlen(sizestr);
					msg[len-1] = ' ';
					msg[len] = 0;
				}
				strcpy( msg+strlen(msg), sizestr );
			}

			if( Max_status_line_DYN < sizeof(msg) ) msg[Max_status_line_DYN] = 0;

			DEBUGF(DLPQ3)("Get_queue_status: adding '%s'", msg );
			Add_line_list(&outbuf,msg,0,0,0);
			DEBUGF(DLPQ3)("Get_queue_status: destinations '%d'", destinations );
			if( nodest == 0 && destinations ){
				for( dcount = 0; dcount < destinations; ++dcount ){
					if( Get_destination( &job, dcount ) ) continue;
					DEBUGFC(DLPQ3)Dump_line_list("Get_queue_status: destination",
						&job.destination);
					d_error =
						Find_str_value(&job.destination,ERROR,Value_sep);
					d_dest =
						Find_str_value(&job.destination,DEST,Value_sep);
					d_copies = 
						Find_flag_value(&job.destination,COPIES,Value_sep);
					d_copy_done = 
						Find_flag_value(&job.destination,COPY_DONE,Value_sep);
					d_identifier =
						Find_str_value(&job.destination,IDENTIFIER,Value_sep);
					s = Find_str_value(&job.destination, PRSTATUS,Value_sep);
					if( !s ) s = "";
					plp_snprintf(number, sizeof(number)," - %-8s", s );
					plp_snprintf( msg, sizeof(msg),
						"%-*s %-*s ", RANKW, number, OWNERW, d_identifier );
					len = strlen(msg);
					plp_snprintf(msg+len, sizeof(msg)-len, " ->%s", d_dest );
					if( d_copies > 1 ){
						len = strlen( msg );
						plp_snprintf( msg+len, sizeof(msg)-len,
							_(" <cpy %d/%d>"), d_copy_done, d_copies );
					}
					if( d_error ){
						len = strlen(msg);
						plp_snprintf( msg+len, sizeof(msg)-len, " ERROR: %s", d_error );
					}
					Add_line_list(&outbuf,msg,0,0,0);
				}
			}
			DEBUGF(DLPQ3)("Get_queue_status: after dests" );
		} else if( displayformat == REQ_VERBOSE ){
			plp_snprintf( header, sizeof(header),
				_(" Job: %s"), identifier );
			plp_snprintf( msg, sizeof(msg), _("%s status= %s"),
				header, number );
			Add_line_list(&outbuf,msg,0,0,0);
			plp_snprintf( msg, sizeof(msg), _("%s size= %d"),
				header, jobsize );
			Add_line_list(&outbuf,msg,0,0,0);
			plp_snprintf( msg, sizeof(msg), _("%s time= %s"),
				header, job_time );
			Add_line_list(&outbuf,msg,0,0,0);
			if( joberror ){
				plp_snprintf( msg, sizeof(msg), _("%s error= %s"),
						header, joberror );
				Add_line_list(&outbuf,msg,0,0,0);
			}
			plp_snprintf( msg, sizeof(msg), _("%s CONTROL="), header );
			Add_line_list(&outbuf,msg,0,0,0);
			s = Get_file_image(0,openname,0);
			Add_line_list(&outbuf,s,0,0,0);
			if( s ) free(s); s = 0;

			plp_snprintf( msg, sizeof(msg), _("%s HOLDFILE="), header );
			Add_line_list(&outbuf,msg,0,0,0);
			s = Get_file_image(0,hf_name,0);
			Add_line_list(&outbuf,s,0,0,0);
			if( s ) free(s); s = 0;
		}

	}

	DEBUGFC(DLPQ4)Dump_line_list("Get_queue_status: job status",&outbuf);

	DEBUGF(DLPQ3)(
		"Get_queue_status: RemoteHost_DYN '%s', RemotePrinter_DYN '%s', Lp '%s'",
		RemoteHost_DYN, RemotePrinter_DYN, Lp_device_DYN );

	if( RemoteHost_DYN && RemotePrinter_DYN ){
		len = strlen( header );
		s = Frwarding(&Spool_control);
		if( displayformat == REQ_VERBOSE ){
			if( s ){
				plp_snprintf( header+len, sizeof(header)-len,
					"\n Forwarding: %s", s );
			} else {
				plp_snprintf( header+len, sizeof(header)-len,
					"\n Destination: %s@%s", RemotePrinter_DYN, RemoteHost_DYN );
			}
		} else {
			if( s ){
				plp_snprintf( header+len, sizeof(header)-len,
				_("(forwarding %s)"), s );
			} else {
				plp_snprintf( header+len, sizeof(header)-len,
				_("(dest %s@%s)"), RemotePrinter_DYN, RemoteHost_DYN );
			}
		}
	}

	if( displayformat != REQ_DSHORT ){
		s = 0;
		if( (s = Comment_tag_DYN) == 0 ){
			if( (nx = PC_alias_line_list.count) > 1 ){
				s = PC_alias_line_list.list[nx-1];
			}
		}
		if( s ){
			s = Fix_str(s);
			len = strlen( header );
			if( displayformat == REQ_VERBOSE ){
				plp_snprintf( header+len, sizeof(header) - len, _(" Comment: %s"), s );
			} else {
				plp_snprintf( header+len, sizeof(header) - len, " '%s'", s );
			}
			if(s) free(s); s = 0;
		}
	}

	len = strlen( header );
	if( displayformat == REQ_VERBOSE ){
		plp_snprintf( header+len, sizeof(header) - len,
			_("\n Printing: %s\n Aborted: %s\n Spooling: %s"),
				Pr_disabled(&Spool_control)?"no":"yes",
				Pr_aborted(&Spool_control)?"no":"yes",
				Sp_disabled(&Spool_control)?"no":"yes");
	} else if( displayformat == REQ_DLONG ){
		flag = 0;
		if( Pr_disabled(&Spool_control) || Sp_disabled(&Spool_control) || Pr_aborted(&Spool_control) ){
			plp_snprintf( header+len, sizeof(header) - len, " (" );
			len = strlen( header );
			if( Pr_disabled(&Spool_control) ){
				plp_snprintf( header+len, sizeof(header) - len, "%s%s",
					flag?", ":"", "printing disabled" );
				flag = 1;
				len = strlen( header );
			}
			if( Pr_aborted(&Spool_control) ){
				plp_snprintf( header+len, sizeof(header) - len, "%s%s",
					flag?", ":"", "printing aborted" );
				flag = 1;
				len = strlen( header );
			}
			if( Sp_disabled(&Spool_control) ){
				plp_snprintf( header+len, sizeof(header) - len, "%s%s",
					flag?", ":"", "spooling disabled" );
				len = strlen( header );
			}
			plp_snprintf( header+len, sizeof(header) - len, ")" );
			len = strlen( header );
		}
	}
	if( Bounce_queue_dest_DYN ){
		len = strlen( header );
		if( displayformat == REQ_VERBOSE ){
			plp_snprintf( header+len, sizeof(header) - len,
				_("\n Bounce_queue: %s"), Bounce_queue_dest_DYN );
			if( Routing_filter_DYN ){
				len = strlen( header );
				plp_snprintf( header+len, sizeof(header) - len,
					_("\n Routing_filter_DYN: %s"), Routing_filter_DYN );
			}
		} else {
			plp_snprintf( header+len, sizeof(header) - len,
				_(" (%sbounce to %s)"),
				Routing_filter_DYN?"routed/":"", Bounce_queue_dest_DYN );
		}
	}

	/*
	 * check to see if this is a server or subserver.  If it is
	 * for subserver,  then you can forget starting it up unless started
	 * by the server.
	 */
	if( (s = Server_names_DYN) || (s = Destinations_DYN) ){
		Split( &info, s, File_sep, 0,0,0,0,0);
		len = strlen( header );
		if( displayformat == REQ_VERBOSE ){
			if ( Server_names_DYN ) {
				s = "Subservers";
			} else {
				s = "Destinations";
			}
			plp_snprintf( header+len, sizeof(header) - len,
			_("\n %s: "), s );
		} else {
			if ( Server_names_DYN ) {
				s = "subservers";
			} else {
				s = "destinations";
			}
			plp_snprintf( header+len, sizeof(header) - len,
			_(" (%s"), s );
		}
		for( ix = 0; ix < info.count; ++ix ){
			len = strlen( header );
			plp_snprintf( header+len, sizeof(header) - len,
			"%s%s", (ix > 0)?", ":" ", info.list[ix] );
		}
		Free_line_list( &info );
		if( displayformat != REQ_VERBOSE ){
			safestrncat( header, ") " );
		}
	}
	if( Server_queue_name_DYN ){
		len = strlen( header );
		if( displayformat == REQ_VERBOSE ){
			plp_snprintf( header+len, sizeof(header) - len,
				_("\n Serving: %s"), Server_queue_name_DYN );
		} else {
			plp_snprintf( header+len, sizeof(header) - len,
				_(" (serving %s)"), Server_queue_name_DYN );
		}
	}
	if( displayformat == REQ_VERBOSE ){
		if( Bounce_queue_dest_DYN ){
			len = strlen( header );
			plp_snprintf( header+len, sizeof(header) - len,
				_("\n Bounce_queue_dest_DYN: %s"), Bounce_queue_dest_DYN );
		}
		if( Hld_all(&Spool_control) ){
			len = strlen( header );
			plp_snprintf( header+len, sizeof(header) - len,
			_("\n Hold_all: on") );
		}
		if( Auto_hold_DYN ){
			len = strlen( header );
			plp_snprintf( header+len, sizeof(header) - len,
			"\n Auth_hold: on" );
		}
		if( (s = Frwarding(&Spool_control)) ){
			len = strlen( header );
			plp_snprintf( header+len, sizeof(header) - len,
			"\n Redirected_to: %s", s );
		}
	}

	if( (s = Find_str_value( &Spool_control,MSG,Value_sep )) ){
		len = strlen( header );
		if( displayformat == REQ_VERBOSE ){
			plp_snprintf( header+len, sizeof(header) - len,
				_("\n Message: %s"), s );
		} else {
			plp_snprintf( header+len, sizeof(header) - len,
				_(" (message: %s)"), s );
		}
	}

	/* this gives a short 1 line format with minimum info */
	if( displayformat == REQ_DSHORT ){
		len = strlen( header );
		plp_snprintf( header+len, sizeof(header) - len, _(" %d job%s"),
			printable, (printable == 1)?"":"s" );
	}
	safestrncat( header, "\n" );
	if( Write_fd_str( *sock, header ) < 0 ) cleanup(0);
	header[0] = 0;

	if( displayformat == REQ_DSHORT ) goto remote;

	/* now check to see if there is a server and unspooler process active */
	path = Make_pathname( Spool_dir_DYN, Queue_lock_file_DYN );
	server_pid = 0;
	if( (fd = Checkread( path, &statb ) ) >= 0 ){
		server_pid = Read_pid( fd, (char *)0, 0 );
		close( fd );
	}
	DEBUGF(DLPQ3)("Get_queue_status: checking server pid %d", server_pid );
	free(path);
	if( server_pid > 0 && kill( server_pid, 0 ) ){
		DEBUGF(DLPQ3)("Get_queue_status: server %d not active", server_pid );
		server_pid = 0;
	}

	path = Make_pathname( Spool_dir_DYN, Queue_unspooler_file_DYN );
	unspooler_pid = 0;
	if( (fd = Checkread( path, &statb ) ) >= 0 ){
		unspooler_pid = Read_pid( fd, (char *)0, 0 );
		close( fd );
	}
	if(path) free(path); path=0;
	DEBUGF(DLPQ3)("Get_queue_status: checking unspooler pid %d", unspooler_pid );
	if( unspooler_pid > 0 && kill( unspooler_pid, 0 ) ){
		DEBUGF(DLPQ3)("Get_queue_status: unspooler %d not active", unspooler_pid );
		unspooler_pid = 0;
	}

	if( printable == 0 ){
		safestrncpy( msg, _(" Queue: no printable jobs in queue\n") );
	} else {
		/* check to see if there are files and no spooler */
		plp_snprintf( msg, sizeof(msg), _(" Queue: %d printable job%s\n"),
			printable, printable > 1 ? "s" : "" );
	}
	if( Write_fd_str( *sock, msg ) < 0 ) cleanup(0);
	if( held ){
		plp_snprintf( msg, sizeof(msg), 
		_(" Holding: %d held jobs in queue\n"), held );
		if( Write_fd_str( *sock, msg ) < 0 ) cleanup(0);
	}

	msg[0] = 0;
	if( count && server_pid == 0 ){
		safestrncpy(msg, _(" Server: no server active") );
	} else if( server_pid ){
		len = strlen(msg);
		plp_snprintf( msg+len, sizeof(msg)-len, _(" Server: pid %d active"),
			server_pid );
	}
	if( unspooler_pid ){
		if( msg[0] ){
			safestrncat( msg, (displayformat == REQ_VERBOSE )?", ":"\n");
		}
		len = strlen(msg);
		plp_snprintf( msg+len, sizeof(msg)-len, _(" Unspooler: pid %d active"),
			unspooler_pid );
	}
	if( msg[0] ){
		safestrncat( msg, "\n" );
	}
	if( (s = Clsses(&Spool_control)) ){
		len = strlen(msg);
		plp_snprintf( msg+len, sizeof(msg)-len, _(" Classes: %s\n"), s );
	}
	if( msg[0] ){
		if( Write_fd_str( *sock, msg ) < 0 ) cleanup(0);
	}
	msg[0] = 0;

	if( displayformat == REQ_VERBOSE ){
		plp_snprintf( msg, sizeof(msg), _("%s SPOOLCONTROL=\n"), header );
		if( Write_fd_str( *sock, msg ) < 0 ) cleanup(0);
		msg[0] = 0;
		for( ix = 0; ix < Spool_control.count; ++ix ){
			s = safestrdup3("   ",Spool_control.list[ix],"\n",__FILE__,__LINE__);
			if( Write_fd_str( *sock, s ) < 0 ) cleanup(0);
			free(s);
		}
	}

	/*
	 * get the last status of the spooler
	 */
	Print_status_info( sock, Spool_dir_DYN, Queue_status_file_DYN,
		_(" Status: "), status_lines, max_size );

	if( Status_file_DYN ){
		Print_status_info( sock, Spool_dir_DYN, Status_file_DYN,
			_(" Filter_status: "), status_lines, max_size );
	}

	s = Join_line_list(&outbuf,"\n");
	if( s ){
		if( Write_fd_str(*sock,s) < 0 ) cleanup(0);
		free(s);
	}
	Free_line_list(&outbuf);

 remote:
	if( Server_names_DYN ){
		Free_line_list(&info);
		Split(&info, Server_names_DYN, File_sep, 0,0,0,0,0);
		for( ix = 0; ix < info.count; ++ix ){
			DEBUGF(DLPQ3)("Get_queue_status: getting subserver status '%s'", 
				info.list[ix] );
			Set_DYN(&Printer_DYN,info.list[ix]);
			Get_local_or_remote_status( tokens, sock, displayformat,
				status_lines, done_list, max_size );
			DEBUGF(DLPQ3)("Get_queue_status: finished subserver status '%s'", 
				info.list[ix] );
		}
	} else if( Destinations_DYN ){
		Free_line_list(&info);
		Split(&info, Destinations_DYN, File_sep, 0,0,0,0,0);
		for( ix = 0; ix < info.count; ++ix ){
			DEBUGF(DLPQ3)("Get_queue_status: getting destination status '%s'", 
				info.list[ix] );
			Set_DYN(&Printer_DYN,info.list[ix]);
			Get_local_or_remote_status( tokens, sock, displayformat,
				status_lines, done_list, max_size );
			DEBUGF(DLPQ3)("Get_queue_status: finished destination status '%s'", 
				info.list[ix] );
		}
	} else if( Bounce_queue_dest_DYN ){
		DEBUGF(DLPQ3)("Get_queue_status: getting bouncequeue dest status '%s'", 
			Bounce_queue_dest_DYN);
		Set_DYN(&Printer_DYN,Bounce_queue_dest_DYN);
		Get_local_or_remote_status( tokens, sock, displayformat,
			status_lines, done_list, max_size );
		DEBUGF(DLPQ3)("Get_queue_status: finished subserver status '%s'", 
			Bounce_queue_dest_DYN );
	} else if( RemoteHost_DYN ){
		fd = Send_request( 'Q', displayformat, tokens->list, Connect_timeout_DYN,
			Send_query_rw_timeout_DYN, *sock );
		if( fd >= 0 ){
			while( (nx = read(fd,msg,sizeof(msg))) > 0 ){
				if( Write_fd_len(*sock,msg,nx) < 0 ) cleanup(0);
			}
			close(fd);
		}
	}

	DEBUGF(DLPQ3)("Get_queue_status: finished '%s'", Printer_DYN );
	goto done;

 error:
	DEBUGF(DLPQ2)("Get_queue_status: error msg '%s'", error );
	safestrncpy( header, _(" ERROR: ") );
	safestrncat( header, error );
	safestrncat( header, "\n" );
	if( Write_fd_str( *sock, header ) < 0 ) cleanup(0);
 done:
	Free_line_list(&info);
	Free_line_list(&lineinfo);
	Free_line_list(&outbuf);
	return;
}

void Print_status_info( int *sock, char *dir, char *file,
	char *prefix, int status_lines, int max_size )
{
	char *image = Get_file_image(dir,file, max_size);
	static char *atmsg = " at ";
	struct line_list l;
	int start, i;
	Init_line_list(&l);
	Split(&l,image,Line_ends,0,0,0,0,0);

	start = 0;
	if( status_lines ){
		start = l.count - status_lines;	
		if( start < 0 ) start = 0;
	}
	for( i = start; i < l.count; ++i ){
		char *s, *t, *u;
		s = l.list[i];
		if( (t = strstr( s, " ## " )) ){
			*t = 0;
		}
		/* make the date format short */
		if( Short_status_date_DYN ){
			for( u = s; (t = strstr(u,atmsg)); u = t+strlen(atmsg) );
			if( u != s && (t = strrchr( u, '-' )) ){
				memmove( u, t+1, strlen(t)+1 );
			}
		}
		if( prefix && Write_fd_str(*sock,prefix) < 0 ) cleanup(0);
		if( Write_fd_str(*sock,s) < 0 ) cleanup(0);
		if( Write_fd_str(*sock,"\n") < 0 ) cleanup(0);
	}
	Free_line_list(&l);
	if( image) free(image); image = 0;
}

void Get_local_or_remote_status( struct line_list *tokens, int *sock,
	int displayformat, int status_lines, struct line_list *done_list,
	int max_size )
{
	char msg[LARGEBUFFER];
	int fd, n;

	/* we have to see if the host is on this machine */

	DEBUGF(DLPQ1)("Get_local_or_remote_status: %s", Printer_DYN );
	if( !safestrchr(Printer_DYN,'@') ){
		DEBUGF(DLPQ1)("Get_local_or_remote_status: doing local");
		Get_queue_status( tokens, sock, displayformat, status_lines,
			done_list, max_size );
		return;
	}
	Fix_Rm_Rp_info();
	/* now we look at the remote host */
	if( Find_fqdn( &LookupHost_IP, RemoteHost_DYN )
		&& ( !Same_host(&LookupHost_IP,&Host_IP )
			|| !Same_host(&LookupHost_IP,&Host_IP )) ){
		DEBUGF(DLPQ1)("Get_local_or_remote_status: doing local");
		Get_queue_status( tokens, sock, displayformat, status_lines,
			done_list, max_size );
		return;
	}
	DEBUGF(DLPQ1)("Get_local_or_remote_status: doing remote");
	fd = Send_request( 'Q', displayformat, tokens->list, Connect_timeout_DYN,
		Send_query_rw_timeout_DYN, *sock );
	if( fd >= 0 ){
		while( (n = read(fd,msg,sizeof(msg))) > 0 ){
			if( Write_fd_len(*sock,msg,n) < 0 ) cleanup(0);
		}
		close(fd);
	}
}
