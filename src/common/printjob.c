/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: printjob.c,v 5.1 1999/09/12 21:32:50 papowell Exp papowell $";


#include "lp.h"
#include "errorcodes.h"
#include "printjob.h"
#include "getqueue.h"
#include "child.h"
#include "fileopen.h"
/**** ENDINCLUDE ****/
#if defined(HAVE_TCDRAIN)
#  if defined(HAVE_TERMIOS_H)
#    include <termios.h>
#  endif
#endif

/***************************************************************************
 * Commentary:
 * Patrick Powell Sat May 13 08:24:43 PDT 1995
 * 
 * The following algorithm is used to print a job
 * 
 * if( OF ){
 * 	of_fd = open( OF filter -> dev_fd );
 * } else {
 *     of_fd = dev_fd;
 * }
 * 
 *   now we put out the various initialization strings
 * 
 * Leader_on_open_DYN -> buffer;
 * FF_on_open_DYN     -> buffer;
 * if( ( Always_banner_DYN || !Suppress_banner) && !Banner_last_DYN ){
 * 	banner -> buffer
 * }
 * 
 *   now we suspend the of filter
 * if( OF_FILTER ){
 *     pipe for OF errors
 *     fork process to read OF stderr
 *     fork and exec OF filter
 *     buffer -> OF Filter
 * 	Suspend_string -> OF Filter
 * 	wait for suspend;
 * } else {
 *     buffer -> output
 * }
 * 
 *  print out the data files
 * for( i = 0; i < data_files; ++i ){
 *     if( i > 0 && FF between files && OF Filter ){
 * 		wake up of_filter;
 * 		FF -> of_filter;
 * 		Suspend_string -> OF Filter
 * 		wait for suspend;
 * 	}
 * 	if( IF ){
 *         pipe for IF errors;
 * 		fork and exec IF filter, stdin = datafile
 *         wait for IF to exit;
 * 	} else {
 * 		datafile -> output;
 *     }
 * }
 * 
 * if( (Always_banner_DYN || !Suppress_banner) && Banner_last_DYN ){
 * 	banner -> buffer;
 * }
 * Trailer_on_close_DYN -> buffer;
 * FF_on_close_DYN     -> buffer;
 * 
 * if( OF Filter ){
 * 	wake up of_filter;
 * 	buffer -> OF Filter
 * 	close OF Filter input;
 * 	wait for OF to exit
 * } else {
 * 	buffer->output
 * }
 * 
 ****************************************************************************/

#define FILTER_STOP "\031\001"

int Wait_for_pid( int of_pid, char *name, int suspend, int timeout,
	plp_status_t *ps_status)
{
	int pid, n = 0, err;
	DEBUG2("Wait_for_pid: name '%s', pid %d, suspend %d, timeout %d",
		name, of_pid, suspend, timeout );
	errno = 0;
	do{
		memset(ps_status,0,sizeof(ps_status[0]));
		if( timeout > 0 ){
			pid = plp_waitpid_timeout(timeout,of_pid,ps_status,WUNTRACED);
		} else if( timeout == 0 ){
			pid = plp_waitpid(of_pid,ps_status,WUNTRACED);
		} else {
			pid = plp_waitpid(of_pid,ps_status,WUNTRACED|WNOHANG);
		}
		err = errno;
		DEBUG2("Wait_for_pid: pid %d exit status '%s'",
			pid, Decode_status(ps_status));
	} while( pid == -1 && err != ECHILD && err != EINTR );
	if( pid > 0 ){
		if( suspend && WIFSTOPPED(*ps_status) ){
			n = 0;
			DEBUG1("Wait_for_pid: %s filter suspended", name );
		} else {
			if( WIFEXITED(*ps_status) ){
				n = WEXITSTATUS(*ps_status);
				DEBUG3( "Wait_for_pid: %s filter exited with status %d",
					name, n);
			} else if( WIFSIGNALED(*ps_status) ){
				n = WTERMSIG(*ps_status);
				Errorcode = JABORT;
				fatal(LOG_INFO,
					"Wait_for_pid: %s filter died with signal '%s'",name,
					Sigstr(n));
			} else if( suspend && !WIFSTOPPED(*ps_status) ){
				Errorcode = JABORT;
				fatal(LOG_INFO,
					"Wait_for_pid: %s filter did not suspend", name );
			}
			if( n && n < 32 ){
				n += 31;
			}
		}
	} else if( pid <= 0 ){
		/* you got an error, and it was ECHILD or EINTR
		 * if it was EINTR, you want to know 
		 */
		n = -1;
		if( err == EINTR ) n = -2;
	}
	DEBUG1("Wait_for_pid: returning '%s', exit status '%s'",
		Server_status(n), Decode_status(ps_status) );
	errno = err;
	return( n );
}

void Print_job( int output, struct job *job, int timeout )
{
	char *FF_str, *leader_str, *trailer_str, *filter;
	int i, c, of_fd[2], of_error[2], if_error[2],
		of_pid = 0, copy, copies,
		do_banner, n, pid, count, size, fd, tempfd,
		elapsed, left, files_printed;

	char buffer[LARGEBUFFER];
	char msg[SMALLBUFFER];
	char filter_name[8], filter_title[8];
	char *id, *s, *banner_name, *transfername, *openname, *format;
	struct line_list *datafile, files;
	struct stat statb;
	time_t start_time, current_time;
	plp_status_t ps_status;
	int exit_status;

	Init_line_list(&files);
	of_fd[0] = of_fd[1] = of_error[0] = of_error[1] = -1;
	files_printed = 0;
	FF_str = leader_str = trailer_str = 0;
	/* we record the start time */
	start_time = time((void *) 0);
	current_time = time((void *)0);
	elapsed = current_time - start_time;
	left = timeout;
	if( timeout > 0 ){
		left = timeout - elapsed;
	}

	DEBUG2( "Print_job: output fd %d", output );
	if(DEBUGL3){
		logDebug("Print_job: at start open fd's");
		for( i = 0; i < 20; ++i ){
			if( fstat(i,&statb) == 0 ){
				logDebug("  fd %d (0%o)", i, statb.st_mode&S_IFMT);
			}
		}
	}
	if(DEBUGL2) Dump_job( "Print_job", job );
	id = Find_str_value(&job->info,IDENTIFIER,Value_sep);
	if( id == 0 ) id = Find_str_value(&job->info,TRANSFERNAME,Value_sep);

	/* clear output buffer */
	Init_buf(&Outbuf, &Outmax, &Outlen );

	FF_str = Fix_str( Form_feed_DYN );
	leader_str = Fix_str( Leader_on_open_DYN );
	trailer_str = Fix_str( Trailer_on_close_DYN );

	/* Leader_on_open_DYN -> output; */
	if( leader_str ) Put_buf_str( leader_str, &Outbuf, &Outmax, &Outlen );

	/* FF_on_open_DYN -> output; */
	if( FF_on_open_DYN ) Put_buf_str( FF_str, &Outbuf, &Outmax, &Outlen );

	/*
	 * if( ( Always_banner_DYN || !Suppress_banner) && !Banner_last_DYN ){
	 *  we need to have a banner and a banner name
	 * 	banner -> of_fd;
	 * 	kill off banner printer;
	 * 	wait for banner printer to exit;
	 * }
	 */

	/* we are always going to do a banner; get the user name */

	banner_name = Find_str_value(&job->info, BNRNAME, Value_sep );
	/* check for the banner printing */
	do_banner = Always_banner_DYN ||
		(!Suppress_header_DYN && banner_name);
	if( do_banner && banner_name == 0 ){
		banner_name = Find_str_value( &job->info,LOGNAME,Value_sep);
		if( banner_name == 0 ) banner_name = "ANONYMOUS";
		Set_str_value(&job->info,BNRNAME,banner_name);
	}

	/* now we have a banner, is it at start or end? */
	DEBUG2("Print_job: do_banner %d, Banner_last_DYN %d, banner_name '%s', Banner_start_DYN '%s'",
			do_banner, Banner_last_DYN, banner_name, Banner_start_DYN );
	if( do_banner && !Banner_last_DYN ){
		Print_banner( banner_name, Banner_start_DYN, job );
	}

	DEBUG2("Print_job: setup %d bytes '%s'", Outlen, Outbuf ); 

	DEBUG2("Print_job: OF_Filter_DYN '%s'", OF_Filter_DYN );
	setstatus(job,"printing '%s' starting OF", id );
	if( OF_Filter_DYN ){
		Put_buf_str( FILTER_STOP, &Outbuf, &Outmax, &Outlen );
		if( pipe( of_fd ) == -1 || pipe( of_error ) == -1 ){
			Errorcode = JFAIL;
			logerr_die( LOG_INFO,"Print_job: pipe() failed");
		}
		DEBUG3("Print_job: fd of_fd[%d,%d], of_error[%d,%d]",
			of_fd[0], of_fd[1], of_error[0], of_error[1] );

		/* set format */
		Set_str_value(&job->info,FORMAT,"o");
		/* set up file descriptors */

		s = 0;
		if( Backwards_compatible_filter_DYN ) s = BK_of_filter_options_DYN;
		if( s == 0 ) s = OF_filter_options_DYN;
		if( s == 0 ) s = Filter_options_DYN;

		Check_max(&files,10);
		files.list[files.count++] = Cast_int_to_voidstar(of_fd[0]);	/* stdin */
		files.list[files.count++] = Cast_int_to_voidstar(output);	/* stdout */
		files.list[files.count++] = Cast_int_to_voidstar(of_error[1]);	/* stderr */
		if( Accounting_port > 0 ){; /* accounting */
			files.list[files.count++] = Cast_int_to_voidstar(Accounting_port);
		}
        if( (of_pid = Make_passthrough( OF_Filter_DYN, s,&files, job, 0 ))<0){
            Errorcode = JFAIL;
            logerr_die(LOG_INFO,"Print_job: could not create OF process");
        }
		files.count = 0;
		Free_line_list(&files);

		DEBUG3("Print_job: OF pid %d", of_pid );
		if( (close( of_fd[0] ) == -1 ) ){
			logerr_die( LOG_INFO,"Print_job: close(%d) failed", of_fd[0]);
		}
		if( (close( of_error[1] ) == -1 ) ){
			logerr_die( LOG_INFO,"Print_job: close(%d) failed", of_error[1]);
		}
		DEBUG3("Print_job: writing init to OF pid '%d', count %d", of_pid, Outlen );
		/* we write the output buffer to the filter */
		msg[0] = 0;
		n = Write_outbuf_to_OF(job,"OF",of_pid,of_fd[1],of_error[0],
			Outbuf, Outlen,
			msg, sizeof(msg)-1, left, 1, Filter_poll_interval_DYN,
			&exit_status, &ps_status );
		if( n ){
			Errorcode = JFAIL;
			if( exit_status ) Errorcode = exit_status;
			setstatus(job,"OF filter problems, error '%s'", Server_status(n));
			cleanup(0);
		}
		setstatus(job,"OF filter suspended" );
	} else {
		Write_fd_len( output, Outbuf, Outlen );
	}
	Init_buf(&Outbuf, &Outmax, &Outlen );


	/* 
	 *  print out the data files
	 */

	for( count = 0; count < job->datafiles.count; ++count ){
		datafile = (void *)job->datafiles.list[count];
		if(DEBUGL4)Dump_line_list("Print_job - datafile", datafile );

		transfername = Find_str_value(datafile,TRANSFERNAME,Value_sep);
		openname = Find_str_value(datafile,OPENNAME,Value_sep);
		format = Find_str_value(datafile,FORMAT,Value_sep);
		size = Find_flag_value(datafile,SIZE,Value_sep);
		copies = Find_flag_value(datafile,COPIES,Value_sep);
		if( copies == 0 ) copies = 1;

		Set_str_value(&job->info,FORMAT,format);
		Set_str_value(&job->info,DF_NAME,transfername);

		s = Find_str_value(datafile,"N",Value_sep);
		Set_str_value(&job->info,"N",s);

		/*
		 * now we check to see if there is an input filter
		 */
		plp_snprintf(filter_name,sizeof(filter_name),"%s","if");
		c = *format;
		filter_name[0] = c;
		switch( c ){
		case 'p': case 'f': case 'l':
			filter = IF_Filter_DYN;
			filter_name[0] = 'i';
			if( c == 'p' ){
				DEBUG3("Print_job: using 'p' formatter '%s'", Pr_program_DYN );
				if( Pr_program_DYN == 0 ){
					setstatus(job,"no 'p' format filter available" );
					Errorcode = JABORT;
					fatal( LOG_ERR, "Print_job: no '-p' formatter for '%s'",
						c, id );
				}
			}
			break;
		default:
			filter = Find_str_value(&PC_entry_line_list,
				filter_name,Value_sep);
			if( !filter){
				filter = Find_str_value(&Config_line_list,filter_name,
					Value_sep);
			}
			if( filter == 0 ) filter = Filter_DYN;

			if( filter == 0 ){
				setstatus(job,"no '%s' format filter available", filter_name );
				Errorcode = JABORT;
				fatal( LOG_ERR, "Print_job: cannot find '%s' filter",
					filter_name );
			}
		}
		DEBUG3("Print_job: format '%s', filter '%s'", format, filter );

		safestrncpy(filter_title,filter_name);
		uppercase(filter_title);
		for( copy = 0; copy < copies; ++copy ){
			current_time = time((void *)0);
			elapsed = current_time - start_time;
			left = timeout;
			if( timeout > 0 ){
				left = timeout - elapsed;
			}
			DEBUG3(
	"Print_job - openname '%s', format '%s', copy %d, elapsed %d, left %d",
				openname, format, copy, elapsed, left );
	if(DEBUGL3){
		logDebug("Print_job: doing '%s' open fd's", openname);
		for( i = 0; i < 20; ++i ){
			if( fstat(i,&statb) == 0 ){
				logDebug("  fd %d (0%o)", i, statb.st_mode&S_IFMT);
			}
		}
	}
			if( timeout > 0 && left <= 0 ){
				setstatus(job,"excess elapsed time %d seconds", elapsed);
				Errorcode = JFAIL;
				cleanup(0);
			}
			if( files_printed++ && !No_FF_separator_DYN && FF_str ){
				/* FF separator -> of_fd; */
				setstatus(job,"printing '%s' FF separator ",id);
				Init_buf(&Outbuf, &Outmax, &Outlen );
				Put_buf_str( FF_str, &Outbuf, &Outmax, &Outlen );
				if( of_pid ){
					Put_buf_str( FILTER_STOP, &Outbuf, &Outmax, &Outlen );
					kill(of_pid,SIGCONT);
					DEBUG3("Print_job: writing FF sep to OF pid '%d', count %d",
						of_pid, Outlen );
					msg[0] = 0;
					n = Write_outbuf_to_OF(job,"OF",
						of_pid,of_fd[1],of_error[0],
						Outbuf, Outlen,
						msg, sizeof(msg)-1, left, 1,
						Filter_poll_interval_DYN, &exit_status, &ps_status );
					if( n ){
						Errorcode = JFAIL;
						if( exit_status ) Errorcode = exit_status;
						setstatus(job,"OF filter problems, error '%s'",
							Server_status(n));
						cleanup(0);
					}
					setstatus(job,"OF filter suspended" );
				} else {
					Write_fd_len( output, Outbuf, Outlen );
				}
				Init_buf(&Outbuf, &Outmax, &Outlen );
			}

			if( (fd = Checkread( openname, &statb )) < 0 ){
				Errorcode = JFAIL;
				fatal( LOG_ERR, "Print_job: job '%s', cannot open data file '%s'",
					id, openname );
			}
			setstatus(job,"printing data file '%s', size %0.0f",
				transfername, (double)statb.st_size );

			DEBUG2( "Print_job: data file format '%s', IF_Filter_DYN '%s'",
				format, IF_Filter_DYN );
			c = *format;
			if( 'p' == c ){
				tempfd = Make_temp_fd(0);
				if( pipe( if_error ) == -1 ){
					Errorcode = JFAIL;
					logerr_die( LOG_INFO,"Print_job: pipe() failed");
				}
				DEBUG3("Print_job: PR fd if_error[%d,%d]",
					 if_error[0], if_error[1] );

				Free_line_list(&files);
				Check_max(&files,10);
				files.list[files.count++] = Cast_int_to_voidstar(fd);		/* stdin */
				files.list[files.count++] = Cast_int_to_voidstar(tempfd);	/* stdout */
				files.list[files.count++] = Cast_int_to_voidstar(if_error[1]);	/* stderr */
				if( Accounting_port > 0 ){; /* accounting */
					files.list[files.count++] = Cast_int_to_voidstar(Accounting_port);
				}
				if( (pid = Make_passthrough( Pr_program_DYN, 0, &files,
					job, 0 )) < 0 ){
					Errorcode = JFAIL;
					logerr_die(LOG_INFO,
						"Print_job: could not create PR process");
				}
				files.count = 0;
				Free_line_list(&files);

				if( (close(if_error[1]) == -1 ) ){
					Errorcode = JFAIL;
					logerr_die( LOG_INFO,"Print_job: close(%d) failed",
						if_error[1]);
				}
				if( (close(fd) == -1 ) ){
					Errorcode = JFAIL;
					logerr_die( LOG_INFO,"Print_job: close(%d) failed",
						fd);
				}
				msg[0] = 0;
				n = Write_outbuf_to_OF(job,"PR",pid,-1,if_error[0],
					0, 0,
					msg, sizeof(msg)-1, left, 0, Filter_poll_interval_DYN,
					&exit_status, &ps_status );
				if( n ){
					Errorcode = JFAIL;
					if( exit_status ) Errorcode = exit_status;
					setstatus(job,"OF filter problems, error '%s'",
							Server_status(n));
					cleanup(0);
				}
				/* this should be closed already */
				close(if_error[0]);
				Init_buf(&Outbuf, &Outmax, &Outlen );
				fd = tempfd;
				if( lseek(fd,0,SEEK_SET) < 0 ){
					Errorcode = JFAIL;
					logerr_die( LOG_INFO,"Print_job: fseek(%d) failed", fd);
				}
			}
			if( filter ){
				DEBUG3("Print_job: format '%s' starting filter '%s'",
					format, filter );
				if( pipe( if_error ) == -1 ){
					Errorcode = JFAIL;
					logerr_die( LOG_INFO,"Print_job: pipe() failed");
				}
				DEBUG3("Print_job: %s fd if_error[%d,%d]", filter_title,
					 if_error[0], if_error[1] );
				s = 0;
				if( Backwards_compatible_filter_DYN ) s = BK_filter_options_DYN;
				if( s == 0 ) s = Filter_options_DYN;

				Free_line_list(&files);
				Check_max(&files, 10 );
				files.list[files.count++] = Cast_int_to_voidstar(fd);		/* stdin */
				files.list[files.count++] = Cast_int_to_voidstar(output);	/* stdout */
				files.list[files.count++] = Cast_int_to_voidstar(if_error[1]);	/* stderr */
				if( Accounting_port > 0 ){; /* accounting */
					files.list[files.count++] = Cast_int_to_voidstar(Accounting_port);
				}
				if( (pid = Make_passthrough( filter, s, &files, job, 0 )) < 0 ){
					Errorcode = JFAIL;
					logerr_die(LOG_INFO,"Print_job:  could not make %s process",
						filter_title );
				}
				files.count = 0;
				Free_line_list(&files);

				if( (close(fd) == -1 ) ){
					Errorcode = JFAIL;
					logerr_die( LOG_INFO,"Print_job: close(%d) failed", fd);
				}
				if( (close(if_error[1]) == -1 ) ){
					Errorcode = JFAIL;
					logerr_die( LOG_INFO,"Print_job: close(%d) failed",
						if_error[1]);
				}
				Init_buf(&Outbuf, &Outmax, &Outlen );
				msg[0] = 0;
				n = Write_outbuf_to_OF(job,filter_title,pid,-1,if_error[0],
					0, 0,
					msg, sizeof(msg)-1, timeout, 0, Filter_poll_interval_DYN,
					&exit_status, &ps_status );
				if( n ){
					Errorcode = JFAIL;
					if( exit_status ) Errorcode = exit_status;
					setstatus(job,"%s filter problems, error '%s'",
						filter_title, Server_status(n));
					cleanup(0);
				}
				setstatus(job, "%s filter finished", filter_title );
				if( (close(if_error[0]) == -1 ) ){
					Errorcode = JFAIL;
					logerr_die( LOG_INFO,"Print_job: close(%d) failed",
						if_error[1]);
				}
			} else {
				DEBUG3("Print_job: format '%s' no filter", format );
				while( (n = read(fd,buffer,sizeof(buffer))) > 0 ){
					if( Write_fd_len(output, buffer, n ) < 0 ){
						Errorcode = JFAIL;
						logerr_die(LOG_INFO,"Print_job: write to output failed");
					}
				}
				if( (close(fd) == -1 ) ){
					Errorcode = JFAIL;
					logerr_die( LOG_INFO,"Print_job: close(%d) failed", fd);
				}
				DEBUG3("Print_job: format '%s' finished writing", format );
			}
		}
	}

	/* 
	 * now we do the end
	 */

	Init_buf(&Outbuf, &Outmax, &Outlen );

	/* check for the banner at the end */

	if( do_banner && Banner_last_DYN ){
		Print_banner( banner_name, Banner_end_DYN, job );
	}

	/* 
	 * FF_on_close_DYN     -> of_fd;
	 */ 
	if( FF_on_close_DYN ) Put_buf_str( FF_str, &Outbuf, &Outmax, &Outlen );

	/* 
	 * Trailer_on_close_DYN -> of_fd;
	 */ 
	if( trailer_str ) Put_buf_str( trailer_str, &Outbuf, &Outmax, &Outlen );

	/*
	 * close the OF Filters
	 */

	if( of_pid ){
		current_time = time((void *)0);
		elapsed = current_time - start_time;
		left = timeout;
		if( timeout > 0 ){
			left = timeout - elapsed;
		}
		kill(of_pid,SIGCONT);
		DEBUG3("Print_job: writing trailer to OF pid '%d', count %d",
			of_pid, Outlen );
		msg[0] = 0;
		n = Write_outbuf_to_OF(job,"OF",
			of_pid,of_fd[1],of_error[0],
			Outbuf, Outlen,
			msg, sizeof(msg)-1, left, 0, Filter_poll_interval_DYN,
			&exit_status, &ps_status );
		if( n ){
			Errorcode = JFAIL;
			if( exit_status ) Errorcode = exit_status;
			setstatus(job,"OF filter problems, error '%s'",
				Server_status(n));
			cleanup(0);
		}
		close(of_error[0]);
		setstatus(job,"OF filter finished" );
	} else {
		Write_fd_len( output, Outbuf, Outlen );
	}
#ifdef HAVE_TCDRAIN
	if( isatty( output ) && tcdrain( output ) == -1 ){
		logerr_die( LOG_INFO,"Print_job: tcdrain failed");
	}
#endif
	if(DEBUGL3){
		logDebug("Print_job: at end open fd's");
		for( i = 0; i < 20; ++i ){
			if( fstat(i,&statb) == 0 ){
				logDebug("  fd %d (0%o)", i, statb.st_mode&S_IFMT);
			}
		}
	}
	Init_buf(&Outbuf, &Outmax, &Outlen );
	if( Outbuf ) free(Outbuf); Outbuf = 0;
	if(FF_str) free(FF_str);
	if(leader_str) free(leader_str);
	if(trailer_str) free(trailer_str);
	Errorcode = JSUCC;
	setstatus(job,"printing done '%s'",id);
}

/*
 * Print a banner
 * check for a small or large banner as necessary
 */

void Print_banner( char *name, char *pgm, struct job *job )
{
	char buffer[LARGEBUFFER];
	int len, pid, n;
	struct line_list l;
	char *bl = 0, *s;
	int i, tempfd, of_error[2], nullfd;
	struct stat statb;
	struct line_list files;
	plp_status_t status;

	/*
	 * print the banner
	 */
	Init_line_list(&l);
	Init_line_list(&files);
	if(DEBUGL3){
		logDebug("Print_banner: at start open fd's");
		for( i = 0; i < 20; ++i ){
			if( fstat(i,&statb) == 0 ){
				logDebug("  fd %d (0%o)", i, statb.st_mode&S_IFMT);
			}
		}
	}
	if( !pgm ) pgm = Banner_printer_DYN;

	DEBUG2( "Print_banner: name '%s', pgm '%s', sb=%d, Banner_line_DYN '%s'",
		name, pgm, Short_banner_DYN, Banner_line_DYN );

	if( !pgm && !Short_banner_DYN ){
		setstatus(job,"no banner");
		return;
	}
	Split(&l,Banner_line_DYN,Whitespace,0,0,0,0,0);
	if( l.count ){
		Fix_dollars(&l,job);
		s = Join_line_list(&l," ");
		bl = safestrdup2(s,"\n",__FILE__,__LINE__);
		Free_line_list(&l);
		if(s) free(s); s = 0;
	}

 	if( pgm ){
		/* we now need to create a banner */
		setstatus(job,"creating banner");

		tempfd = Make_temp_fd(0);
		if( (nullfd = open("/dev/null", O_RDONLY )) < 0 ){
			Errorcode = JFAIL;
			logerr_die( LOG_INFO,"Print_banner: open /dev/null failed");
		}
		if( pipe( of_error ) == -1 ){
			Errorcode = JFAIL;
			logerr_die( LOG_INFO,"Print_banner: pipe() failed");
		}
		DEBUG3("Print_banner: fd of_error[%d,%d]",
			of_error[0], of_error[1] );

		Free_line_list(&files);
		Check_max(&files, 10 );
		files.list[files.count++] = Cast_int_to_voidstar(nullfd);		/* stdin */
		files.list[files.count++] = Cast_int_to_voidstar(tempfd);	/* stdout */
		files.list[files.count++] = Cast_int_to_voidstar(of_error[1]);	/* stderr */
		if( Accounting_port > 0 ){; /* accounting */
			files.list[files.count++] = Cast_int_to_voidstar(Accounting_port);
		}
		if( (pid = Make_passthrough( pgm, Filter_options_DYN, &files, job, 0 )) < 0 ){
			Errorcode = JFAIL;
			logerr_die(LOG_INFO,"Print_banner:  could not OF process");
		}
		files.count = 0;
		Free_line_list(&files);

		if( (close(of_error[1]) == -1 ) ){
			Errorcode = JFAIL;
			logerr_die( LOG_INFO,"Print_banner: close(%d) failed",
				of_error[1]);
		}
		if( (close(nullfd) == -1 ) ){
			Errorcode = JFAIL;
			logerr_die( LOG_INFO,"Print_banner: close(%d) failed",
				nullfd);
		}
		buffer[0] = 0;
		len = 0;
		while( len < sizeof(buffer)-1
			&& (n = read(of_error[0],buffer+len,sizeof(buffer)-len-1)) >0 ){
			buffer[n+len] = 0;
			while( (s = safestrchr(buffer,'\n')) ){
				*s++ = 0;
				setstatus(job,"BANNER: %s", buffer );
				memmove(buffer,s,strlen(s)+1);
			}
			len = strlen(buffer);
		}
		while( (n = plp_waitpid(pid,&status,0)) != pid );
		DEBUG1("Print_banner: pid %d, exit status '%s'", pid,
			Decode_status(&status) );
		if( WIFEXITED(status) && (n = WEXITSTATUS(status)) ){
			setstatus(job,"Print_banner: banner printer '%s' exited with status %d", pgm, n);
		} else if( WIFSIGNALED(status) ){
			setstatus(job,"Print_banner: banner printer '%s' died with signal %d, '%s'",
				pgm, n, Sigstr(n));
		}

		if( lseek(tempfd,0,SEEK_SET) < 0 ){
			Errorcode = JFAIL;
			logerr_die( LOG_INFO,"Print_banner: fseek(%d) failed", tempfd);
		}
		len = Outlen;
		while( (n = read(tempfd, buffer, sizeof(buffer))) > 0 ){
			Put_buf_len(buffer, n, &Outbuf, &Outmax, &Outlen );
		}
		if( (close(tempfd) == -1 ) ){
			Errorcode = JFAIL;
			logerr_die( LOG_INFO,"Print_banner: close(%d) failed",
				tempfd);
		}
		DEBUG4("Print_banner: BANNER '%s'", Outbuf+len);
	} else {
		Put_buf_str( bl, &Outbuf, &Outmax, &Outlen );
	}
	if( bl ) free(bl); bl = 0;
	if(DEBUGL3){
		logDebug("Print_banner: at end open fd's");
		for( i = 0; i < 20; ++i ){
			if( fstat(i,&statb) == 0 ){
				logDebug("  fd %d (0%o)", i, statb.st_mode&S_IFMT);
			}
		}
	}
}

/*
 * Write_outbuf_to_OF(
 * int of_pid    - pid of filter
 * int of_fd     - write to this
 * char *msg, int msgmax - message storage area
 * int of_error  - read from this
 * int timeout   - timeout
 *                 nnn - wait this long
 *                 0   - wait indefinately
 *                 -1  - do not wait
 * int suspend
 *	   0 - wait for exit status
 *     1 - wait for suspend status
 *     (if pid <= 0, do not wait)
 * )
 *   RETURN: exit code from process
 *     nn - process exit code
 *     0  if suspended or exited correctly
 *     -1 if IO error on of_fd
 *     -2 if timeout
 *     -3 if IO error on of_error
 *     -4 if eof on of_error
 */

int Write_outbuf_to_OF( struct job *job, char *title,
	int of_pid, int of_fd, int of_error,
	char *buffer, int outlen,
	char *msg, int msgmax,
	int timeout, int suspend, int max_wait, int *exit_status,
	plp_status_t *ps_status)
{
	time_t start_time, current_time;
	int msglen, n, m, count, elapsed, left;
	char *s;

	*exit_status = 0;
	memset(ps_status, 0, sizeof(ps_status[0]));
	start_time = time((void *)0);
	msglen = strlen(msg);
	DEBUG3(
"Write_outbuf_to_OF: pid %d, of_fd %d, of_error %d, timeout %d, suspend %d",
		of_pid, of_fd, of_error, timeout, suspend );

	n = 0;
	while( n >=0 && outlen > 0 ){
		left = timeout;
		if( timeout > 0 ){
			current_time = time((void *)0);
			elapsed = current_time - start_time;
			left = timeout - elapsed;
			if( left <= 0 ){
				n = -2;
				break;
			}
		}

		msglen = strlen(msg);
		DEBUG4("Write_outbuf_to_OF: writing %d", outlen );
		DEBUG5("Write_outbuf_to_OF: writing '%s'", buffer );
		count = -1;	/* number written */
		n = Read_write_timeout( of_error, msg+msglen, msgmax-msglen, &count,
			of_fd, &buffer, &outlen, left );
		DEBUG4("Write_outbuf_to_OF: write returned %d, read %d, '%s'",
			n, count, msg);
		if( count > 0 ){
			msglen += count;
			msg[msglen] = 0;
			s = msg;
			while( (s = safestrchr(msg,'\n')) ){
				*s++ = 0;
				setstatus(job, "%s filter msg - '%s'", title, msg );
				memmove(msg,s,strlen(s)+1);
			}
			msglen = strlen(msg);
		} else if( count == 0 ){
			DEBUG5("Write_outbuf_to_OF: no more reading");
			of_error = -1;
		}
	}
	DEBUG3("Write_outbuf_to_OF: after writing status %d", n);
	while( n == 0 && of_pid > 0 ){
		if( !suspend && of_fd >= 0 ){
			DEBUG3("Write_outbuf_to_OF: closing of_fd %d", of_fd);
			close(of_fd);
			of_fd = -1;
		}
		left = timeout;
		if( timeout > 0 ){
			current_time = time((void *)0);
			elapsed = current_time - start_time;
			left = timeout - elapsed;
			if( left <= 0 ){
				n = -2;
				continue;
			}
		}
		DEBUG3("Write_outbuf_to_OF: waiting %d secs for pid %d, suspend %d",
			left, of_pid, suspend );

		if( !suspend ){
			count = -1;
			if( of_error >= 0 ){
				/* we see if we have output */
				count = -1;
				/* poll for any errors */
				m = Read_write_timeout( of_error, msg+msglen, msgmax-msglen,
					&count, -1, 0, 0, left );
				DEBUG4("Write_outbuf_to_OF: read status %d, read %d, '%s'",
					m,count,msg);
				if( count > 0 ){
					msglen += count;
					msg[msglen] = 0;
					s = msg;
					while( (s = safestrchr(msg,'\n')) ){
						*s++ = 0;
						setstatus(job, "%s filter msg - '%s'", title, msg );
						memmove(msg,s,strlen(s)+1);
					}
					msglen = strlen(msg);
					continue;
				} else if( count == 0 ){
					of_error = -1;
					DEBUG3("Write_outbuf_to_OF: no more reading");
					continue;
				}
			}
			n = Wait_for_pid( of_pid, title, suspend, left, ps_status );
			/* we got process exiting */
			*exit_status = n;
			of_pid = 0;
		} else {
			if( left == 0 || left > max_wait ){
				left = max_wait;
			}
			/* we wait until it returns or we get a timeout */
			n = Wait_for_pid( of_pid, title, suspend, left, ps_status );
			*exit_status = 0;
			DEBUG4("Write_outbuf_to_OF: wait returned %d", n);
			if( n != -2 ){
				/* we got process exiting */
				of_pid = 0;
			} else {
				/* we timed out, try again */
				n = 0;
			}
			left = -1;
			do{
				count = -1;
				/* poll for any errors */
				DEBUG4("Write_outbuf_to_OF: doing read for %d sec", left);
				m = Read_write_timeout( of_error, msg+msglen, msgmax-msglen,
					&count,
					-1, 0, 0, left );
				DEBUG4("Write_outbuf_to_OF: read status %d, read %d, '%s'",
					m,count,msg);
				left = -1;
				if( count > 0 ){
					msglen += count;
					msg[msglen] = 0;
					s = msg;
					while( (s = safestrchr(msg,'\n')) ){
						*s++ = 0;
						setstatus(job, "%s filter msg - '%s'", title, msg );
						memmove(msg,s,strlen(s)+1);
					}
					msglen = strlen(msg);
					left = 1;
				} else if( count == 0 ){
					of_error = -1;
				}
			}while( left > 0 );
		}
	}
	return(n);
}
