/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2001, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: lpd_logger.c,v 1.14 2001/09/02 20:42:11 papowell Exp $";


#include "lp.h"
#include "lpd.h"

#include "child.h"
#include "errorcodes.h"
#include "fileopen.h"
#include "getopt.h"
#include "getprinter.h"
#include "getqueue.h"
#include "linksupport.h"
#include "proctitle.h"

#include "lpd_logger.h"

/***************************************************************************
 * Setup_logger()
 * 
 * We will have a process that sits and listens for log data, and then
 * forwards it to the destination.  This process will have some odd properities.
 * 
 * 1.  It will never update its destination.  This means you will have to
 *     kill the logger to get it to accept a new destination.
 ***************************************************************************/


void Free_file_info( struct file_info *io )
{
	if( io->fd > 0 ) close(io->fd); io->fd = -1;
	if( io->outbuffer ) free( io->outbuffer ); io->outbuffer = 0;
	if( io->inbuffer ) free( io->inbuffer ); io->inbuffer = 0;
	memset( io, 0, sizeof(io[0]));
}


void Hex_dump( void *p, int len )
{
	unsigned char *s = (unsigned char *)p;
	int i, m;
	char buffer[SMALLBUFFER];
	for( i = 0; i < len; ++i ){
		if( !(i % 16) ){
			if( i ) LOGDEBUG("%s", buffer);
			SNPRINTF(buffer, sizeof(buffer)) "  [0x%03x] ", i );
		}
		m = strlen(buffer);
		SNPRINTF(buffer+m, sizeof(buffer)-m)" %02x", s[i] );
	}
	if( !(i % 16) ){
		 LOGDEBUG("%s", buffer);
	}
}

void Dump_file_info_sub( char *title, struct file_info *io )
{
	char buffer[32];
	if( io ){
		LOGDEBUG(" %s fd %d", title, io->fd );
		SNPRINTF(buffer,sizeof(buffer)-4)"%s", io->outbuffer );
		if( io->outbuffer && strlen(io->outbuffer) > sizeof(buffer)-4){
			safestrncat(buffer,"...");
		}
		LOGDEBUG("  outbuffer 0x%lx, outmax 0x%x, '%s'",
			Cast_ptr_to_long(io->outbuffer), io->outmax, buffer );
		SNPRINTF(buffer,sizeof(buffer)-4)"%s", io->inbuffer );
		if( io->inbuffer && strlen(io->inbuffer) > sizeof(buffer)-4 ){
			safestrncat(buffer,"...");
		}
		LOGDEBUG("  inbuffer 0x%lx, inmax 0x%x, '%s'",
			Cast_ptr_to_long(io->inbuffer), io->inmax, buffer );
		LOGDEBUG("  start %d, count %d",
			io->start, io->count );
	}
}

void Dump_file_info( char *title, struct file_info *io )
{
	LOGDEBUG("Dump_io: %s - 0x%lx", title, Cast_ptr_to_long(io) );
	Dump_file_info_sub( "", io );
}

void Dump_file_info_contents( char *title, struct file_info *io )
{
	int i, len;
	char *s;
	LOGDEBUG("*** Dump_file_info_contents: %s - 0x%lx", title, Cast_ptr_to_long(io) );
	Dump_file_info_sub( "", io );
	i = 0;
	while( i < io->count ){
		s = Get_record( io, io->start+i, &len );
		LOGDEBUG(" [%d] len %d - '%s'", i, len, s );
		i += len;
	}
	LOGDEBUG("*** end");
}

void Init_file_info( struct file_info *io, char *path, int max_size )
{
	int fd;

	DEBUGFC(DLOG2)Dump_file_info("Init_file_info - start", io );
	Free_file_info( io );

	if( max_size == 0 ) max_size = 1024;
	io ->max_size = max_size;
	if( path == 0 ){
		fd = Make_temp_fd( &path );
	} else {
		fd = open( path, O_RDWR|O_CREAT,Is_server?Spool_file_perms_DYN:0600 );
		Max_open(fd);
	}
	io->fd = fd;
	if( fd < 0 ){
		Errorcode = JABORT;
		LOGERR_DIE(LOG_INFO) "Init_file_info: cannot open '%s'", path );
	}
	if( ftruncate( fd, 0 ) == -1 ){
		Errorcode = JABORT;
		LOGERR_DIE(LOG_INFO) "Init_file_info: cannot truncate '%s'", path );
	}
	if( lseek( fd, 0, SEEK_SET ) == -1 ){
		Errorcode = JABORT;
		LOGERR_DIE(LOG_INFO) "Init_file_info: cannot seek '%s' to offset %d",
			path, max_size );
	}
	DEBUGFC(DLOG1)Dump_file_info("Init_file_info - end",io);
}

void Read_rec( struct file_info *io, char *s, int start, int reccount )
{
	int n, cnt;
	while( reccount > 0 ){
		start %= io->max_size;
		if( lseek( io->fd, start, SEEK_SET ) == -1 ){
			Errorcode = JABORT;
			LOGERR_DIE(LOG_INFO) "Get_record: lseek offset %d failed", start );
		}
		cnt = reccount;
		if( cnt + start >= io->max_size ){
			DEBUGF(DLOG2)("Read_rec: wrap around");
			cnt = io->max_size - start;
		}
		if( (n = read( io->fd, s, cnt )) != cnt ){
			Errorcode = JABORT;
			LOGERR_DIE(LOG_INFO) "Get_record: read %d failed - ret %d", cnt, n );
		}
		s += cnt;
		reccount -= cnt;
	}
}

void Write_rec( struct file_info *io, char *s, int start, int reccount )
{
	int n, cnt;
	while( reccount > 0 ){
		start %= io->max_size;
		if( lseek( io->fd, start, SEEK_SET ) == -1 ){
			Errorcode = JABORT;
			LOGERR_DIE(LOG_INFO) "Get_record: lseek offset %d failed", start );
		}
		cnt = reccount;
		if( cnt + start >= io->max_size ){
			DEBUGF(DLOG2)("Write_rec: wrap around at '%s'", s);
			cnt = io->max_size - start;
		}
		if( (n = write( io->fd, s, cnt )) != cnt ){
			Errorcode = JABORT;
			LOGERR_DIE(LOG_INFO) "Get_record: write %d failed - ret %d", cnt, n );
		}
		s += cnt;
		reccount -= cnt;
	}
}

char *Get_record( struct file_info *io, int start, int *len )
{
	union val val;
	int reccount;

	DEBUGF(DLOG4)("Seek_record: start %d", start );
	reccount = sizeof(val);
	Read_rec( io, (char *)&val, start, sizeof(val));
	reccount = val.v;
	DEBUGF(DLOG1)("Get_record: start %d, record size %d", start, reccount);
	if( len ) *len = reccount + sizeof(val);
	if( reccount >= io->outmax ){
		io->outmax = reccount;
		io->outbuffer = realloc_or_die(
			io->outbuffer, io->outmax+1, __FILE__,__LINE__);
	}
	Read_rec( io, io->outbuffer, start+sizeof(val), reccount );
	if( io->outbuffer ) io->outbuffer[reccount] = 0;
	DEBUGF(DLOG4)("Get_record: '%s'", io->outbuffer );
	return( io->outbuffer );
}

int Put_record( struct file_info *io, int start, char *buf )
{
	union val val;
	int reccount;

	reccount = 0;
	if( buf ) reccount = strlen(buf);
	DEBUGF(DLOG1)("Put_record: start %d, record size %d, avail %d",
		start, reccount, io->max_size - io->count);
	if( reccount ){
		if( reccount + sizeof(val) > io->max_size - io->count ){
			reccount = -1;
		} else {
			val.v = reccount;
			Write_rec( io, val.s, start, sizeof(val));
			Write_rec( io, buf, start+sizeof(val), reccount );
			reccount += sizeof(val);
		}
	}
	return(reccount);
}

void Remove_first_record( struct file_info *io )
{
	int n;
	if( io->count>0 ){
		Get_record( io, io->start, &n);
		io->count -= n;
		io->start = (io->start+n) % io->max_size;
	}
	DEBUGFC(DLOG1)Dump_file_info("Remove_first_record - after", io );
}

void Add_record( struct file_info *io, char *buf )
{
	int reccount = 0;

	while( (reccount = Put_record( io, io->start+io->count, buf )) < 0 ){
		if( io->count > 0 ){
			Remove_first_record( io );
		} else {
			Errorcode = JABORT;
			FATAL(LOG_ERR)"Add_record: message len %d too long (max %d)",
				strlen(buf), io->max_size );
		}
	}
	io->count += reccount;
	DEBUGFC(DLOG2)Dump_file_info_contents("Add_record - end", io );
}

int Dump_queue_status(int outfd)
{
	int i, count, fd;
	char *s, *sp, *pr;
	struct line_list info;
	struct job job;
	char buffer[SMALLBUFFER];
	/* char *esc_lf_2 = Escape("\n", 2); */
	/* char *esc_lf_2 = "%25250a"; */
	char *esc_lf_1 = "%250a";
	struct stat statb;

	s = sp = 0;
	Init_job(&job);
	Init_line_list(&info);
	if(All_line_list.count == 0 ){
		Get_all_printcap_entries();
	}
	DEBUGF(DLOG2)("Dump_queue_status: writing to fd %d", outfd );
	for( i = 0; i < All_line_list.count; ++i ){
		Set_DYN(&Printer_DYN,0);
		pr = All_line_list.list[i];
		DEBUGF(DLOG2)("Dump_queue_status: checking '%s'", pr );
		if( Setup_printer( pr, buffer, sizeof(buffer), 0 ) ) continue;
		Free_line_list( &Sort_order );
		if( Scan_queue( &Spool_control, &Sort_order, 0,0,0, 0, 0, 0 ) ){
			continue;
		}
		Free_line_list(&info);
		Set_str_value(&info,PRINTER,Printer_DYN);
		Set_str_value(&info,HOST,FQDNHost_FQDN);
		Set_decimal_value(&info,PROCESS,getpid());
		Set_str_value(&info,UPDATE_TIME,Time_str(0,0));

		if( Write_fd_str( outfd, "DUMP=" ) < 0 ){ return(1); }
		s = Join_line_list(&info,"\n");
		sp = Escape(s, 1);
		if( Write_fd_str( outfd, sp ) < 0 ){ return(1); }

		if( s ) free(s); s = 0;
		if( sp ) free(sp); s = 0;

		if( Write_fd_str( outfd, "VALUE=" ) < 0 ){ return(1); }

		if( Write_fd_str( outfd, "QUEUE%3d" ) < 0 ){ return(1); }
		if( (fd = Checkread( Queue_control_file_DYN, &statb )) > 0 ){
			while( (count = read(fd, buffer, sizeof(buffer)-1)) > 0 ){
				buffer[count] = 0;
				s = Escape(buffer,3);
				if( Write_fd_str( outfd, s ) < 0 ){ return(1); }
				free(s);
			}
			close(fd);
		}
		if( Write_fd_str( outfd, esc_lf_1 ) < 0 ){ return(1); }

		if( Write_fd_str( outfd, "PRSTATUS%3d" ) < 0 ){ return(1); }
		if( (fd = Checkread( Queue_status_file_DYN, &statb )) > 0 ){
			while( (count = read(fd, buffer, sizeof(buffer)-1)) > 0 ){
				buffer[count] = 0;
				s = Escape(buffer,3);
				if( Write_fd_str( outfd, s ) < 0 ){ return(1); }
				free(s);
			}
			close(fd);
		}
		if( Write_fd_str( outfd, esc_lf_1 ) < 0 ){ return(1); }

		for( count = 0; count < Sort_order.count; ++count ){
			Free_job(&job);
			Get_hold_file( &job, Sort_order.list[count] );
			
			if( job.info.count == 0 ) continue;
			if( Write_fd_str( outfd, "UPDATE%3d" ) < 0 ){ return(1); }
			s = Join_line_list(&job.info,"\n");
			sp = Escape(s, 3);
			if( Write_fd_str( outfd, sp ) < 0 ){ return(1); }
			if( s ) free(s); s = 0;
			if( sp ) free(sp); s = 0;
			if( Write_fd_str( outfd, esc_lf_1 ) < 0 ){ return(1); }
		}
		if( Write_fd_str( outfd, "\n" ) < 0 ){ return(1); }
	}

	if( Write_fd_str( outfd, "END\n" ) < 0 ){ return(1); }
	Set_DYN(&Printer_DYN,0);

	Free_line_list( &Sort_order );
	Free_line_list(&info);
	Free_job(&job);
	if( s ) free(s); s = 0;
	if( sp ) free(sp); sp = 0;
	return(0);
}

void Logger( struct line_list *args )
{
	char *port, *s, *path, *host, *tempfile;
	int max_size, writefd,m, c, timeout, readfd;
	time_t start_time, current_time;
	int elapsed, left, err;
	struct timeval timeval, *tp;
	fd_set readfds, writefds; /* for select() */
	char inbuffer[LARGEBUFFER];
	static struct file_info ioval;
	int status_fd;
	struct stat statb;

	Errorcode = JABORT;


	Name = "LOG2";
	setproctitle( "lpd %s", Name );
	Register_exit("Free_file_info", (exit_ret)Free_file_info, &ioval );

	DEBUGFC(DLOG2)Dump_line_list("Logger - args", args );

	timeout = Logger_timeout_DYN;
	max_size = Logger_max_size_DYN;
	if( max_size == 0 ) max_size = 1024;
	max_size *= 1024;
	path = Logger_path_DYN;

	host = safestrdup(Logger_destination_DYN,__FILE__,__LINE__);
	port = 0;
	/* OK, we try to open a connection to the logger */
	if( host && (port = safestrchr( host, '%')) ){
		*port++ = 0;
	}

	readfd = Find_flag_value(args,INPUT,Value_sep);
	Free_line_list(args);

	writefd = -2;
	/* now we set up the IO file */
	Init_file_info(&ioval,path,max_size);
	Set_nonblock_io(readfd);
	
	DEBUGF(DLOG2)("Logger: host '%s', port %s", host, port );

	time( &start_time );
	Init_buf(&Inbuf,&Inmax,&Inlen);
	Init_buf(&Outbuf,&Outmax,&Outlen);
	status_fd = Make_temp_fd( &tempfile );
	close( status_fd );
	status_fd = -1;

	while( 1 ){
		tp = 0;
		left = 0;
		if( readfd < 0 && ioval.count == 0 && Outlen == 0 ){
			DEBUGF(DLOG2)("Logger: exiting - no work to do");
			Errorcode = 0;
			break;
		}
		if( writefd < 0 ){
			time( &current_time );
			elapsed = current_time - start_time;
			left = timeout - elapsed;
			DEBUGF(DLOG2)("Logger: writefd fd %d, max timeout %d, left %d",
					writefd, timeout, left );
			if( left <= 0 || writefd == -2 ){
				writefd = Link_open(host, port, Connect_timeout_DYN, 0 );
				DEBUGF(DLOG2)("Logger: open fd %d, host '%s', port '%s'",
						writefd, host, port );
				if( writefd >= 0 ){
					Set_nonblock_io( writefd );
					ioval.start = 0;
					ioval.count = 0;
					if( (status_fd = Checkwrite(tempfile, &statb, O_RDWR, 1, 0)) < 0 ){
						LOGERR_DIE(LOG_INFO) "Logger: cannot open file '%s'", tempfile);
					}
					if( lseek( status_fd, 0, SEEK_SET) == -1 ){
						Errorcode = JABORT;
						LOGERR_DIE(LOG_INFO) "Logger: lseek failed file '%s'", tempfile);
					}
					if( ftruncate( status_fd, 0 ) ){
						Errorcode = JABORT;
						LOGERR_DIE(LOG_INFO) "Logger: ftruncate failed file '%s'", tempfile);
					}
					Init_buf(&Outbuf,&Outmax,&Outlen);
					if( Dump_queue_status(status_fd) ){
						DEBUGF(DLOG2)("Logger: Dump_queue_status failed - %s", Errormsg(errno) );
						Errorcode = JABORT;
						LOGERR_DIE(LOG_INFO) "Logger: cannot write file '%s'", tempfile);
					}
					if( lseek( status_fd, 0, SEEK_SET) == -1 ){
						Errorcode = JABORT;
						LOGERR_DIE(LOG_INFO) "Logger: lseek failed file '%s'", tempfile);
					}
					if( (m = read( status_fd, inbuffer, sizeof(inbuffer) )) > 0 ){
						Put_buf_len( inbuffer, m, &Outbuf, &Outmax, &Outlen );
						DEBUGF(DLOG2)("Logger: queue status '%s'", Outbuf );
					} else if( m == 0 ){
						close( status_fd );
						status_fd = -1;
					} else {
						Errorcode = JABORT;
						LOGERR_DIE(LOG_INFO)"Logger: read error %s", tempfile);
					}
				} else {
					writefd = -1;
				}
				time( &start_time );
				time( &current_time );
				DEBUGF(DLOG2)("Logger: writefd now fd %d", writefd );
			}
			if( writefd < 0 && timeout > 0 ){
				memset( &timeval, 0, sizeof(timeval) );
				elapsed = current_time - start_time;
				left = timeout - elapsed;
				timeval.tv_sec = left;
				tp = &timeval;
				DEBUGF(DLOG2)("Logger: timeout now %d", left );
			}
		}
		FD_ZERO( &writefds );
		FD_ZERO( &readfds );
		m = 0;
		if( writefd >= 0 ){
			if( ioval.count || Outlen || status_fd >= 0 ){
				FD_SET( writefd, &writefds );
				if( m <= writefd ) m = writefd+1;
			}
			FD_SET( writefd, &readfds );
			if( m <= writefd ) m = writefd+1;
		}
		if( readfd >= 0 ){
			FD_SET( readfd, &readfds );
			if( m <= readfd ) m = readfd+1;
		}
		errno = 0;
		DEBUGF(DLOG2)("Logger: starting select, timeout '%s', left %d",
			tp?"yes":"no", left );
        m = select( m,
            FD_SET_FIX((fd_set *))&readfds,
            FD_SET_FIX((fd_set *))&writefds,
            FD_SET_FIX((fd_set *))0, tp );
		err = errno;
		DEBUGF(DLOG2)("Logger: select returned %d, errno '%s'",
			m, Errormsg(err) );
		if( m < 0 ){
			if( err != EINTR ){
				Errorcode = JABORT;
				LOGERR_DIE(LOG_INFO)"Logger: select error");
			}
		} else if( m > 0 ){
			if( writefd >=0 && FD_ISSET( writefd, &readfds ) ){
				/* we have EOF on the file descriptor */
				DEBUGF(DLOG2)("Logger: eof on writefd fd %d", writefd );
				close( writefd );
				writefd = -2;
			}
			if( readfd >=0 && FD_ISSET( readfd, &readfds ) ){
				DEBUGF(DLOG2)("Logger: read possible on fd %d", readfd );
				inbuffer[0] = 0;
				m = read( readfd, inbuffer, sizeof(inbuffer)-1 );
				if( m >= 0) inbuffer[m] = 0;
				DEBUGF(DLOG2)("Logger: read count %d '%s'", m, inbuffer );
				if( m > 0 ){
					inbuffer[m] = 0;
					Put_buf_len( inbuffer, m, &Inbuf, &Inmax, &Inlen );
					while( (s = safestrchr(Inbuf,'\n')) ){
						c = s[1];
						s[1] = 0;
						DEBUGF(DLOG2)("Logger: found '%s'", Inbuf );
						if( writefd >= 0 ) Add_record( &ioval, Inbuf );
						s[1] = c;
						memmove(Inbuf,s+1,strlen(s+1)+1);
					}
					Inlen = strlen(Inbuf);
				} else if( m == 0 ) {
					/* we have a 0 length read - this is EOF */
					Errorcode = 0;
					DEBUGF(DLOG1)("Logger: eof on input fd %d", readfd);
					close(readfd);
					readfd = -1;
				} else {
					Errorcode = JABORT;
					LOGERR_DIE(LOG_INFO)"Logger: read error on input fd %d", readfd);
				}
			}
			if( writefd >=0 && FD_ISSET( writefd, &writefds ) ){
				DEBUGF(DLOG2)("Logger: write possible on fd %d, Outlen %d, ioval.count %d",
					writefd, Outlen, ioval.count );
				if( Outlen == 0 && status_fd >= 0 ){
					if( (m = read( status_fd, inbuffer, sizeof(inbuffer) )) > 0 ){
						Put_buf_len( inbuffer, m, &Outbuf, &Outmax, &Outlen );
					} else if( m == 0 ){
						close( status_fd );
						status_fd = -1;
					} else {
						Errorcode = JABORT;
						LOGERR_DIE(LOG_INFO)"Logger: read error %s", tempfile);
					}
				}
				if( Outlen == 0 && ioval.count ){
					Init_buf( &Outbuf, &Outmax, &Outlen );
					if( (s = Get_record( &ioval, ioval.start, 0 )) ){
						Put_buf_str( s, &Outbuf, &Outmax, &Outlen );
						Remove_first_record(&ioval);
						DEBUGF(DLOG2)("Logger: new record Outlen %d, '%s'",
						Outlen, Outbuf );
					}
				}
				if( Outlen ){
					m = write( writefd, Outbuf, Outlen);
					DEBUGF(DLOG2)("Logger: last write %d", m );
					if( m < 0 ){
						/* we have EOF on the file descriptor */
						close( writefd );
						writefd = -2;
					} else {
						memmove(Outbuf, Outbuf+m, strlen(Outbuf+m)+1 );
						Outlen = strlen( Outbuf );
					}
				}
			}
		}
	}
	if(host) free(host); host = 0;
	cleanup(0);
}
