/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: printjob.c
 * PURPOSE: print a file
 **************************************************************************/

static char *const _id =
"$Id: printjob.c,v 3.22 1998/01/12 20:29:23 papowell Exp $";

#include "lp.h"
#include "printcap.h"
#include "decodestatus.h"
#include "dump.h"
#include "errorcodes.h"
#include "fileopen.h"
#include "jobcontrol.h"
#include "killchild.h"
#include "linksupport.h"
#include "malloclist.h"
#include "pr_support.h"
#include "printcap.h"
#include "printjob.h"
#include "setstatus.h"
#include "setup_filter.h"
#include "waitchild.h"
/**** ENDINCLUDE ****/

/***************************************************************************
Commentary:
Patrick Powell Sat May 13 08:24:43 PDT 1995

The following algorithm is used to print a job

dev_fd - device file descriptor
   This can be the actual device or a pipe to a device
of_fd  - output file descriptor
   This is the OF filter if it has been specified.
   This is used for banners and file separators such as form feeds.
if_fd  - 
   This is the file format filter.

dev_fd = open( device or filter );

do accounting at start;
if( OF ){
	of_fd = open( OF filter -> dev_fd );
} else {
    of_fd = dev_fd;
}

  now we put out the various initialization strings

Leader_on_open -> of_fd;
FF_on_open     -> of_fd;
if( ( Always_banner || !Suppress_bannner) && !Banner_last ){
	banner = bp;
	if( bs ) banner = bs;
	bannner -> of_fd;
	kill off banner printer;
	wait for banner printer to exit;
}

  now we suspend the of filter
if( of_fd != dev_fd ){
	Suspend_string->of_fd;
	wait for suspend;
}

 print out the data files
for( i = 0; i < data_files; ++i ){
	if( IF ){
		if_fd = open( IF filter -> of_fd )
	} else {
		if_fd = dev_fd;
	}
	file -> if_fd;
	if( if_fd != dev_fd ){
		close( if_fd );
		wait for  if_filter to die;
	}
}

 start up the of filter
if( of_fd != dev_fd ){
	wait up of_filter;
}

 put out the termination stuff
if( ( Always_banner || !Suppress_bannner) && Banner_last ){
	banner = bp;
	if( be ) banner = be;
	bannner -> of_fd;
	kill off banner printer;
	wait for banner printer to exit;
}

Trailer_on_close -> of_fd;
FF_on_close     -> of_fd;
close( of_fd );
do accounting at end;
close( dev_fd );
if( of_fd != dev_fd ){
	wait for of_filter;
}

****************************************************************************/

static int Fix_str( char **copy, char *str );

int Print_job( struct control_file *cfp, struct printcap_entry *printcap_entry,
	int transfer_timeout )
{
	int FF_len;				/* FF information length */
	static char *FF_str;	/* FF information converted format */
	int leader_len;			/* leader */
	static char *leader_str;
	int trailer_len;		/* trailer */
	static char *trailer_str;
	struct data_file *data_file;	/* current data file */
	int fd;					/* data file file descriptor */
	struct stat statb;		/* stat buffer for open */
	char *filter;			/* filter to use */
	int i, c, n = 0, err = 0;		/* ACME Integer variables */
	char *s;				/* Sigh... */
	int do_banner;			/* do a banner */
	int pfd;				/* pr program output */
	char *id;				/* id for job */
	int attempt;			/* job attempt */
	char file_name[LINEBUFFER];		/* use for identification */

	Cfp_static = cfp;

	id = cfp->identifier+1;
	if( *id == 0 ) id = cfp->transfername;
	DEBUG1("Print_job: '%s', Use_queuename %d", id, Use_queuename );

	if( (Use_queuename || Force_queuename) &&
		(cfp->QUEUENAME == 0 || cfp->QUEUENAME[1] == 0) ){
		char buffer[M_QUEUENAME];
		s = Force_queuename;
		if( s == 0 || *s == 0 ) s = Queue_name;
		if( s == 0 || *s == 0 ) s = Printer;
		plp_snprintf( buffer, sizeof(buffer), "Q%s", s );
		cfp->QUEUENAME = Insert_job_line( cfp, buffer, 0, 0);
	}
	if( Use_date && cfp->DATE == 0 ){
		char buffer[M_DATE];
		plp_snprintf( buffer, sizeof(buffer), "D%s",
			Time_str( 0, cfp->statb.st_ctime ) );
		cfp->DATE = Insert_job_line( cfp, buffer, 0, 0);
	}
	if( Use_identifier && cfp->IDENTIFIER == 0 ){
		if( Make_identifier( cfp ) ){
			setstatus(cfp,"bad identifier fields '%s' - '%s'",
				id, cfp->error );
			Errorcode = JABORT;
			cleanup(0);
		}
		cfp->IDENTIFIER = Insert_job_line( cfp, cfp->identifier, 1, 0);
	}

	/* we will not try to do anything fancy on exit except kill filters */
	register_exit( (exit_ret)Print_abort, 0 );

	Errorcode = JABORT;
	Device_fd_info.input = -1;
	OF_fd_info.input = -1;
	XF_fd_info.input = -1;
	Pr_fd_info.input = -1;


	/* fix the form feed string */
	FF_len = Fix_str( &FF_str, Form_feed );
	leader_len = Fix_str( &leader_str, Leader_on_open );
	trailer_len = Fix_str( &trailer_str, Trailer_on_close );

	if(DEBUGL3 ){
		dump_control_file( "Print_job", cfp );
	}

	if( Local_accounting ){
		Setup_accounting( cfp, printcap_entry );
	}

	/*
	 * if( OF ){
	 * 	of_fd = open( OF filter -> dev_fd );
	 * } else {
	 * 	of_fd = dev_fd;
	 * }
	 */

	if(Destination){
		attempt = Destination->attempt;
	} else {
		attempt = cfp->hold_info.attempt;
	}
	setstatus(cfp,"printing '%s', start, attempt %d",id,attempt+1);
	if( Print_open( &Device_fd_info, cfp, Connect_timeout, Connect_interval,
		Connect_grace, Max_connect_interval, printcap_entry, Accounting_port ) < 0 ){
		/* we failed to open device */
		Errorcode = JFAIL;
		cleanup( 0 );
	}
	DEBUG3("Print_job: device fd %d, pid %d",
		Device_fd_info.input, Device_fd_info.pid );
	Errorcode = JABORT;
	if( OF_Filter ){
		/* we need to create an OF filter */
		if( Make_filter( 'o', cfp, &OF_fd_info, OF_Filter, 0, 0,
			Device_fd_info.input, printcap_entry, (void *)0,
			Accounting_port, Logger_destination != 0, 0 ) ){
			setstatus( cfp, "%s", cfp->error );
		}
	} else {
		OF_fd_info.input = Device_fd_info.input;
		OF_fd_info.pid = 0;
	}
	if( OF_fd_info.input == -1 ){
		if( Errorcode == 0 ){
			log( LOG_ERR,
				"Print_job: OF_fd_input has bad status, JSUCC errorcode" );
			Errorcode = JABORT;
		}
		cleanup(0);
	}

	/*
	 * do accounting at start
	 */
	DEBUG2("Print_job: Accounting_start '%s', Local_accounting %d'",
		Accounting_start, Local_accounting );
	if( Accounting_start && Local_accounting ){
		setstatus(cfp,"accounting at start '%s'", id);
		i = Do_accounting( 0, Accounting_start, cfp,
			transfer_timeout, printcap_entry, OF_fd_info.input );
		if( i ){
			setstatus(cfp,"accounting failed at start '%s' - %s", id, Server_status(i) );
			if( i != JFAIL && i != JABORT && i != JREMOVE ){
				i = JFAIL;
			}
			Errorcode = i;
			cleanup( 0 );
		}
	}

	/* Leader_on_open -> of_fd; */
	if( leader_len ){
		setstatus(cfp,"printing '%s', sending leader",id);
		i = Print_string( &OF_fd_info, leader_str, leader_len,
			transfer_timeout
			);
		if( i ){
			setstatus(cfp,"printing '%s', error sending leader",id);
			n = Close_filter( 0, &OF_fd_info,
			transfer_timeout,
			"printer (OF)" );
			if( n ) i = n;
			Errorcode = i;
			cleanup( 0 );
		}
	}

	/* FF_on_open     -> of_fd; */
	if( FF_on_open && FF_len ){
		setstatus(cfp,"printing '%s', sending FF on open",id);
		i = Print_string( &OF_fd_info, FF_str, FF_len,
			transfer_timeout);
		if( i ){
			setstatus(cfp,"printing '%s', error sending FF on open",id);
			n = Close_filter( 0, &OF_fd_info,
				transfer_timeout,
				"printer (OF)" );
			if( n ) i = n;
			Errorcode = i;
			cleanup( 0 );
		}
	}

	/*
	 * if( ( Always_banner || !Suppress_bannner) && !Banner_last ){
	 *  we need to have a banner and a banner name
	 * 	bannner -> of_fd;
	 * 	kill off banner printer;
	 * 	wait for banner printer to exit;
	 * }
	 */

	/* we are always going to do a banner; get the user name */
	if( Always_banner && cfp->BNRNAME == 0 ){
			cfp->BNRNAME = cfp->LOGNAME;
			if( cfp->BNRNAME == 0 ){
				cfp->BNRNAME = "ANONYMOUS";
			}
	}

	/* check for the banner printing */
	do_banner = !Suppress_header && cfp->BNRNAME;

	/* now we have a banner, is it at start or end? */
	if( do_banner && ( Banner_start || !Banner_last ) ){
		setstatus(cfp,"printing '%s', printing banner on open",id);
		s = 0;
		DEBUG2( "Print_job: Banner_start '%s', Banner_printer '%s'",
			Banner_start, Banner_printer );
		if( Banner_printer && *Banner_printer ) s = Banner_printer;
		if( Banner_start && *Banner_start ) s = Banner_start;
		i = Print_banner( s, cfp,
			transfer_timeout,
			OF_fd_info.input, FF_len, FF_str, printcap_entry );
		if( i ){
			setstatus(cfp,"printing '%s', error printing banner on open",id);
			Errorcode = i;
			cleanup( 0 );
		}
	}

	/* 
	 *   now we suspend the of filter
	 * if( of_fd != dev_fd ){
	 * 	Suspend_string->of_fd;
	 * 	wait for suspend;
	 * }
	 */

	if( OF_fd_info.input != Device_fd_info.input
		&& (i = of_stop( &OF_fd_info,
			transfer_timeout, cfp ) ) ){
		setstatus(cfp,"printing '%s', error suspending OF filter",id);
		n = Close_filter( 0, &OF_fd_info,
			transfer_timeout,
			"printer (OF)" );
		if( n ) i = n;
		Errorcode = i;
		cleanup( 0 );
	}

	/* 
	 *  print out the data files
	 * for( i = 0; i < data_files; ++i ){
	 *  if( i > 0 ) FF separator -> of_fd;
	 * 	if( IF ){
	 * 		if_fd = open( IF filter -> of_fd )
	 * 	} else {
	 * 		if_fd = dev_fd;
	 * 	}
	 * 	file -> if_fd;
	 * 	if( if_fd != dev_fd ){
	 * 		close( if_fd );
	 * 		wait for  if_filter to die;
	 * 	}
	 * }
	 */

	for( i = 0; i < cfp->data_file_list.count; ++i ){
		data_file = (void *)cfp->data_file_list.list;
		data_file = &data_file[i];

		if( data_file->Ninfo[0] ){
			safestrncpy(file_name, data_file->Ninfo+1);
		} else {
			safestrncpy(file_name, data_file->transfername+1);
		}
		if( i && !No_FF_separator && FF_len ){
			/* FF separator -> of_fd; */
			setstatus(cfp,"printing '%s', FF separator ",id);
			n = 0;
			if( OF_fd_info.input != Device_fd_info.input ){
				n = of_start( &OF_fd_info );
			}
			if( !n ){
				n = Print_string( &OF_fd_info, FF_str, FF_len,
					transfer_timeout );
			}
			if( !n && OF_fd_info.input != Device_fd_info.input ){
				n = of_stop( &OF_fd_info,
					transfer_timeout, cfp );
			}
			if( n ){
				Errorcode = n;
				n = Close_filter( 0, &OF_fd_info, 
					transfer_timeout,
					"printer (OF)" );
				if( n ) Errorcode = n;
				cleanup( 0 );
			}
		}

		setstatus(cfp,"printing '%s', file %d '%s', size %d, format '%c'",
			id, i+1, file_name,(int)(data_file->statb.st_size),
			data_file->format );
		fd = Checkread( data_file->openname, &statb );
		if( fd < 0 ){
			setstatus(cfp,"cannot open data file '%s'",data_file->openname);
			fatal( LOG_ERR, "Print_job: job '%s', cannot open data file '%s'",
				id, data_file->openname );
		}
		/*
		 * check if PR is to be used
		 */
		c = data_file->format;
		if( 'p' == c ){
			char pr_filter[SMALLBUFFER];
			char banner_temp[SMALLBUFFER];
			if( Pr_program == 0 || *Pr_program  == 0 ){
				setstatus(cfp,"no 'p' format filter available" );
				Errorcode = JREMOVE;
				fatal( LOG_ERR, "Print_job: no '-p' formatter for '%s'",
					c, data_file->openname );
			}
			pr_filter[0] = 0;
			safestrncat( pr_filter, Pr_program );
			safestrncat( pr_filter, " $w $l" );
			if( cfp->PRTITLE ) safestrncat( pr_filter, " -h $-T" );
			/* we need to open a file and process the job */
			/* make a temp file */
			pfd = Make_temp_fd( banner_temp, sizeof(banner_temp) );
			DEBUG3( "Print_job: pr temp file '%s', fd %d", banner_temp, pfd );
			if( ftruncate( pfd, 0 )  < 0 ){
				Errorcode = JABORT;
				logerr_die( LOG_ERR, "Print_job: ftruncate of temp failed",
					banner_temp);
			}
			/* make the pr program */
			if( Make_filter( 'f', cfp, &Pr_fd_info, pr_filter, 1, 0,
				pfd, printcap_entry, data_file, Accounting_port,
				Logger_destination != 0, 0 ) ){
				setstatus( cfp, "%s", cfp->error );
				cleanup(0);
			}
			/* filter the data file */
			err = Print_copy(cfp, fd, &data_file->statb, &Pr_fd_info,
				transfer_timeout,
				1, file_name);
			n = Close_filter( 0, &Pr_fd_info, 
				transfer_timeout, "pr" );
			DEBUG3( "Print_job: pr filter status  %d", n );
			if( n ){
				Errorcode = n;
				cleanup(0);
			}
			if( dup2( pfd, fd ) < 0 ){
				Errorcode = JABORT;
				logerr_die( LOG_ERR, "Print_job: dup2 failed" );
			}
			if( lseek( fd, 0, SEEK_SET ) == (off_t)(-1) ){
				Errorcode = JABORT;
				logerr_die( LOG_ERR, "Print_job: lseek failed" );
			}
		}
		/*
		 * now we check to see if there is an input filter
		 */
		filter = 0;
		switch( c ){
		case 'p': case 'f': case 'l':
			filter = IF_Filter;
			c = 'f';
			break;
		default:
			filter = Find_filter( c, printcap_entry );
			if( filter == 0 ){
				Errorcode = JREMOVE;
				setstatus(cfp,"no '%c' format filter available", c );
				fatal( LOG_ERR, "Print_job: cannot find filter '%c' for '%s'",
					c, data_file->openname );
			}
		}
		/* hook filter up to the device file descriptor */
		if( filter ){
			if( Make_filter( c, cfp, &XF_fd_info, filter, 0, 0,
				Device_fd_info.input, printcap_entry, data_file,
				Accounting_port, Logger_destination != 0,
				Direct_read? fd : 0 ) ){
				setstatus( cfp, "%s", cfp->error );
				cleanup(0);
			}
		} else {
			XF_fd_info.input = Device_fd_info.input;
			XF_fd_info.pid = 0;
		}
		/* send job to printer - we get the error status of filter if any */
		err = 0;
		if( Direct_read == 0 || filter == 0 ){
			err = Print_copy(cfp, fd, &data_file->statb, &XF_fd_info,
				transfer_timeout,
				1,file_name);
			if( err ){
				plp_snprintf( cfp->error, sizeof(cfp->error),
					"IO error '%s'", Errormsg(errno) );
			}
		}
		(void)close(fd);
		fd = -1;
		/* close Xf */
 		if( err == 0 && (XF_fd_info.input != Device_fd_info.input) ){
 		    err = Close_filter( cfp, &XF_fd_info, 
				transfer_timeout,
				"printer (IF)");
			DEBUG2( "Print_job: close Xf status %d", n );
 		}
		/* exit with either the filter status or the copy status */
  		if( err ){
			Set_job_control( cfp, (void *)0 );
  			Errorcode = err;
			cleanup(0);
		}
	}

	/* 
	 *  start up the of filter
	 * if( of_fd != dev_fd ){
	 * 	wait up of_filter;
	 * }
	 */

	if( OF_fd_info.input != Device_fd_info.input ){
		DEBUG3( "Print_job: restarting OF filter" );
		i = of_start( &OF_fd_info );
		if( i ){
			DEBUG3( "Print_job: restarting OF filter FAILED, result %d", i );
			n = Close_filter( 0, &OF_fd_info, 
				transfer_timeout,
				"printer (OF)" );
			if( n ) i = n;
			Errorcode = i;
			cleanup( 0 );
		}
	}

	/* 
	 *  put out the termination stuff
	 * if( ( Always_banner || !Suppress_bannner) && Banner_last ){
	 * 	bannner -> of_fd;
	 * 	kill off banner printer;
	 * 	wait for banner printer to exit;
	 * }
	 */ 


	if( do_banner && ( Banner_end || Banner_last ) ){
		if( !No_FF_separator && FF_len ){
			/* FF befor banner     -> of_fd; */
			setstatus(cfp,"printing '%s', FF separator ",id);
			DEBUG3( "Print_job: printing FF separator" );
			i = Print_string( &OF_fd_info, FF_str, FF_len,
				transfer_timeout);
			if( i ){
				DEBUG3( "Print_job: printing FF separator FAILED, status 0x%x", i );
				Errorcode = i;
				cleanup( 0 );
			}
		}
		setstatus(cfp,"printing '%s', printing banner on close",id);
		s = 0;
		DEBUG2( "Print_job: Banner_end '%s', Banner_printer '%s'",
			Banner_end, Banner_printer );
		if( Banner_printer && *Banner_printer ) s = Banner_printer;
		if( Banner_end && *Banner_end ) s = Banner_end;
		DEBUG3( "Print_job: printing banner" );
		i = Print_banner( s, cfp, 
			transfer_timeout,
			OF_fd_info.input, FF_len, FF_str, printcap_entry );
		if( i ){
			DEBUG3( "Print_job: printing banner FAILED, status 0x%x", i );
			Errorcode = i;
			cleanup( 0 );
		}
	}

	/* 
	 * FF_on_close     -> of_fd;
	 */ 
	if( FF_on_close && FF_len ){
		setstatus(cfp,"printing '%s', sending FF on close",id);
		DEBUG3( "Print_job: printing FF on close" );
		i = Print_string( &OF_fd_info, FF_str, FF_len, 
			transfer_timeout );
		if( i ){
			DEBUG3( "Print_job: printing FF on close FAILED, status 0x%x", i );
			Errorcode = i;
			cleanup( 0 );
		}
	}

	/* 
	 * Trailer_on_close -> of_fd;
	 */ 
	if( trailer_len ){
		setstatus(cfp,"printing '%s', sending trailer",id);
		DEBUG3( "Print_job: printing trailer" );
		i = Print_string( &OF_fd_info, trailer_str,trailer_len,
			transfer_timeout );
		if( i ){
			DEBUG3( "Print_job: printing trailer FAILED, status 0x%x", i );
			Errorcode = i;
			cleanup( 0 );
		}
	}


	/*
	 * do accounting at end
	 */
	if( Accounting_end && Local_accounting ){
		setstatus(cfp,"accounting at end '%s'", id);
		DEBUG3( "Print_job: accounting at end" );
		i = Do_accounting( 1, Accounting_end, cfp, 
			transfer_timeout,
			printcap_entry, OF_fd_info.input );
		if( i ){
			setstatus(cfp,"accounting failed at end '%s'", id);
			DEBUG3( "Print_job: accounting at end FAILED, status 0x%x", i );
			if( i != JFAIL && i != JABORT && i != JREMOVE ){
				i = JFAIL;
			}
			Errorcode = i;
			cleanup(0);
		}
	}

	/*
	 * close( of_fd );
	 */
	setstatus(cfp,"printing '%s', closing device",id);
	n = Close_filter( cfp, &OF_fd_info, 
		transfer_timeout,
		"printer (OF)" );
	if( n ){
		DEBUG3( "Print_job: close OF filter FAILED, status 0x%x", n );
		Set_job_control( cfp, (void *)0 );
		Errorcode = n;
		cleanup( 0 );
	}
	if( Device_fd_info.pid ){
		n = Close_filter( cfp, &Device_fd_info, 
			transfer_timeout,
			"printer (LP)" );
		if( n ){
			Set_job_control( cfp, (void *)0 );
			DEBUG3( "Print_job: close OF filter FAILED, status 0x%x", n );
			Errorcode = n;
			cleanup( 0 );
		}
	}

	/* close them out */
	Print_close( cfp, -1 );

	Errorcode = JSUCC;

	setstatus(cfp,"printing '%s', finished ",id);
	return( Errorcode );
}

/***************************************************************************
 * int Fix_str( char **copy, *str );
 * - make a copy of the original string
 * - substitute all the escape characters
 * \f, \n, \r, \t, and \nnn
 ***************************************************************************/
static int Fix_str( char **copy, char *str )
{
	int len, c;
	char *s, *t, *end;

	if( copy && *copy ){
		free( *copy );
		*copy = 0;
	}
	DEBUG3("Fix_str: '%s'", str );
	s = str;
	if( str && copy ){
		*copy = s = safestrdup(str);
	}
	for( len = 0, t = str; t && *t && (c = (s[len] = *t++)); ++len ){
		/* DEBUG3("Fix_str: char '%c', len %d", c, len ); */
		if( c == '\\' && (c = *t) != '\\' ){
			/* check for \nnn */
			if( isdigit( c ) ){
				end = t;
				s[len] = strtol( t, &end, 8 );
				if( (end - t ) != 3 ){
					return( -1 );
				}
				t = end;
			} else {
				switch( c ){
					default: s[len] = c; break;
					case 'f': s[len] = '\f'; break;
					case 'r': s[len] = '\r'; break;
					case 'n': s[len] = '\n'; break;
					case 't': s[len] = '\t'; break;
					case 0: return(-1);
				}
				++t;
			}
		}
	}
	if( s ) s[len] = 0;
	DEBUG3( "Fix_str: str '%s' -> '%s', len %d", str, s, len );
	return( len );
}

/*
 * Print a banner
 * check for a small or large banner as necessary
 */

int Print_banner(
	char *banner_printer, struct control_file *cfp, int timeout, int input,
	int ff_len, char *ff_str, struct printcap_entry *printcap_entry )
{
	char bline[SMALLBUFFER];
	char *ep;
	int i;
	char *id;				/* id for job */

	id = cfp->identifier+1;
	if( *id == 0 ) id = cfp->transfername;


	/*
	 * print the banner
	 */
	DEBUG2( "Printer_banner: banner_printer '%s'", banner_printer );

	/* make short banner look like BSD-style( Sun, etc.) short banner.
	 * 
	 * This is necessary for use with an OF that parses
	 * the banner( as lprps does).
	 */
	
	if( Banner_line == 0 ){
		Banner_line = "";
	}
	DEBUG2( "Printer_banner: raw banner '%s'", Banner_line );
	ep = bline + sizeof(bline) - 2;
	ep = Expand_command( cfp, bline, ep, Banner_line, 'f', (void *)0 );
	if( strchr( bline, '\\' ) ){
		Fix_str( 0, bline );
	} else {
		safestrncat(bline, "\n");
	}
	DEBUG2( "Printer_banner: expanded banner '%s'", bline );


 	if( !Short_banner && banner_printer && *banner_printer ){
		if( Make_filter( 'f', cfp, &Pr_fd_info, banner_printer, 0, 0,
			input, printcap_entry, (void *)0,
			Accounting_port, Logger_destination != 0, 0 ) ){
				setstatus( cfp, "%s", cfp->error );
				cleanup(0);
			}
	} else {
		Pr_fd_info.input = input;
		Pr_fd_info.pid = 0;
	}

	DEBUG2( "Printer_banner: writing short banner '%s'", bline );
	if( Write_fd_str( Pr_fd_info.input, bline ) < 0 ){
		logerr( LOG_INFO, "banner: write to banner printer failed");
		Errorcode = JFAIL;
		cleanup( 0 );
	}
	if( Pr_fd_info.input != input ){
		i = Close_filter( cfp, &Pr_fd_info, timeout, "banner (IF)" );
		if( i ){
			Errorcode = i;
			cleanup( 0 );
		}
	}
	if( !No_FF_separator && ff_len ){
		/* FF after banner     -> of_fd; */
		setstatus(cfp,"printing '%s', sending FF after banner",id);
		i = Print_string( &OF_fd_info, ff_str, ff_len, timeout);
		if( i ){
			Errorcode = i;
			cleanup( 0 );
		}
	}
	return( JSUCC );
}

void Setup_accounting( struct control_file *cfp, struct printcap_entry *printcap_entry )
{
	int oldport, i, n;
	struct stat statb;
	char *s;
	char buffer[LINEBUFFER];
	int err;

	DEBUG2("Setup_accounting: '%s'", Accounting_file);
	if( Is_server == 0 || Accounting_file == 0 || *Accounting_file == 0 ) return;
	safestrncpy( buffer, Accounting_file );
	s = buffer;
	if( s[0] == '|' ){
		/* first try to make the filter */
		if( Make_filter( 'f', cfp, &Af_fd_info, s, 0, 0,
			0, printcap_entry, (void *)0, 0, Logger_destination != 0, 0 ) ){
			setstatus( cfp, "%s", cfp->error );
			cleanup(0);
		}
		err = errno;
		Accounting_file = 0;
		Accounting_port = Af_fd_info.input;
	} else if( (s = strchr( s, '%' )) ){
		/* now try to open a connection to a server */
		*s++ = 0;
		i = atoi( s );
		if( i <= 0 ){
			logerr( LOG_ERR,
				"Setup_accounting: bad accounting server info '%s'", Accounting_file );
			plp_snprintf( cfp->error, sizeof(cfp->error),
				"bad accounting server '%s'", Accounting_file );
			Set_job_control( cfp, (void *)0 );
			Errorcode = JFAIL;
			cleanup(0);
		}
		oldport = Destination_port;
		DEBUG2("Setup_accounting: opening connection to %s port %d", buffer, i );
		Destination_port = i;
		while( (n = Link_open(buffer,Connect_timeout,0)) < 0
			&& Retry_NOLINK ){
			err = errno;
			setstatus(cfp,"cannot open connection to accounting server '%s' - '%s', sleeping %d",
				Accounting_file, Errormsg(err), Connect_interval );
			if( Connect_interval > 0 ){
				plp_sleep( Connect_interval );
			}
		}
		err = errno;
		if( n < 0 ){
			plp_snprintf( cfp->error, sizeof(cfp->error),
			_("connection to accounting server '%s' failed '%s'"),
				Accounting_file, Errormsg(err) );
			setstatus( cfp, cfp->error );
			Set_job_control( cfp, (void *)0 );
			Errorcode= JABORT;
			cleanup(0);
		}
		Destination_port = oldport;
		DEBUG2("Setup_accounting: socket %d", n );
		Accounting_file = 0;
		Af_fd_info.input = Accounting_port = n;
	} else {
		n = Checkwrite( Accounting_file, &statb, O_RDWR, 0, 0 );
		err = errno;
		if( n > 0 ){
			Af_fd_info.input = Accounting_port = n;
		}
	}
	errno = err;
}

int Do_accounting( int end, char *command, struct control_file *cfp,
	int timeout, struct printcap_entry *printcap_entry, int filter_out )
{
	int i, err;
	char *ep;
	char buffer[SMALLBUFFER];
	char *id;				/* id for job */

	DEBUG2("Do_accounting: %s", command );
	if( command ) while( isspace( *command ) ) ++command;
	if( Is_server == 0 || command == 0 || *command == 0 ){
		return(0);
	}

	id = cfp->identifier+1;
	if( *id == 0 ) id = cfp->transfername;

	err = JSUCC;
	if( *command == '|' ){
		if( As_fd_info.pid > 0 ){
			plp_snprintf( cfp->error, sizeof(cfp->error),
				"conflict in accounting - af and additional accounting required" );
			err = JABORT;
		} else if( Make_filter( 'f', cfp, &As_fd_info, command, 0, 0,
			filter_out, printcap_entry, (void *)0, Accounting_port,
			Logger_destination != 0, 0 ) ){
			err = JABORT;
		} else {
			err = Close_filter( 0, &As_fd_info, timeout, "accounting" );
			DEBUG2("Do_accounting: filter exit status %s", Server_status(err) );
			if( err ){
				plp_snprintf( cfp->error, sizeof(cfp->error),
					"accounting check at %s failed - status %s", end?"end":"start",
					Server_status( err ) );
			}
		}
	} else if( Accounting_port > 0 ){
		/* now we have to expand the string */
		ep = buffer + sizeof(buffer) - 2;
		ep = Expand_command( cfp, buffer, ep, command, 'f', (void *)0 );
		i = strlen(buffer);
		buffer[i++] = '\n';
		buffer[i++] = 0;
		DEBUG2("Do_accounting: job '%s' '%s'", id, buffer );
		if( Write_fd_str( Accounting_port, buffer ) <  0 ){
			err = JFAIL;
			logerr( LOG_ERR, "Do_accounting: write failed" );
			plp_snprintf( cfp->error, sizeof(cfp->error),
				"accounting write failed" );
			err = JFAIL;
		} else if( end == 0 && Accounting_check ){
			static char accept[] = "accept";
			i = sizeof(buffer) - 1;
			buffer[0] = 0;
			err = Link_line_read( "ACCOUNTING SERVER", &Accounting_port,
				timeout, buffer, &i );
			buffer[i] = 0;
			if( err ){
				plp_snprintf( cfp->error, sizeof( cfp->error ),
					"read failed from accounting server" );
				err = JFAIL;
			} else if( strncasecmp( buffer, accept, sizeof(accept)-1 ) ){
				logerr( LOG_ERR,
					"Do_accounting: accounting check failed '%s'", buffer );
				plp_snprintf( cfp->error, sizeof(cfp->error),
					"accounting check failed '%s'", buffer );
				err = JFAIL;
			}
		}
	}
	if( err ){
		Errorcode = err;
		setstatus( cfp,  cfp->error );
		Set_job_control( cfp, (void *)0 );
	}
	return( err );
}
