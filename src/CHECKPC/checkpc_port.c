/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: testport.c
 * PURPOSE: test portabile functionality
 **************************************************************************/

static char *const _id = "checkpc_port.c,v 3.8 1998/03/24 02:43:22 papowell Exp";

#include "lp.h"
#include "fileopen.h"
#include "freespace.h"
#include "killchild.h"
#include "lockfile.h"
#include "pathname.h"
#include "rw_pipe.h"
#include "setuid.h"
#include "stty.h"
#include "timeout.h"
#include "waitchild.h"
/**** ENDINCLUDE ****/

/***************************************************************************
 * We have put a slew of portatbility tests in here.
 * 1. setuid
 * 2. RW/pipes, and as a side effect, waitpid()
 * 3. get file system size (/tmp)
 * 4. try nonblocking open
 * 5. try locking test
 * 6. getpid() test
 * 7. try serial line locking
 * 8. try file locking
 ***************************************************************************/

void Test_port(int ruid, int euid, char *serial_line )
{
	FILE *tf;
	int fds[2];
	char line[LINEBUFFER];
	char cmd[LINEBUFFER];
	char t1[LINEBUFFER];
	char t2[LINEBUFFER];
	char stty[LINEBUFFER];
	char diff[LINEBUFFER];
	char *sttycmd;
	char *diffcmd;
	int ttyfd;
	static pid_t pid, result;
	plp_status_t status;
	char *mother = "From Mother";
	char *child = "From Child";
	struct dpathname dpath;
	unsigned long freespace;
	static int fd;
	char *s;
	static int i, err;
	struct stat statb;

	status = 0;
	fd = -1;

	/*
	 * SETUID
	 * - try to go to user and then back
	 */
	fflush(stderr);
	fflush(stdout);

	Spool_file_perms = 000600;
	Spool_dir_perms =  042700;
	if( ( ruid == 0 && euid == 0 ) || (ruid != 0 && euid != 0 ) ){
			fprintf( stderr,
				"*******************************************************\n" );
			fprintf( stderr, "***** not SETUID, skipping setuid checks\n" );
			fprintf( stderr,
				"*******************************************************\n" );
			goto rw_test;
	} else if( ( ruid == 0 || euid == 0 ) ){
		if( UID_root == 0 ){
		fprintf( stderr,
			"checkpc: setuid code failed!! Portability problems\n" );
			exit(1);
		}
		if( To_uid(1) ){
			fprintf( stderr,
			"checkpc: To_uid() seteuid code failed!! Portability problems\n" );
			exit(1);
		}
		if( To_user() ){
			fprintf( stderr,
			"checkpc: To_usr() seteuid code failed!! Portability problems\n" );
			exit(1);
		}
		fprintf( stderr, "***** SETUID code works\n" );
	}


rw_test:
	fflush(stderr);
	/*
	 * rw_pipe
	 * see if you can open a set of read/write pipes
	 */
	status = rw_pipe( fds );
	if( status < 0 ){
		fprintf( stderr, "rw_pipe failed - %s", Errormsg(errno) );
	} else {
		if( (pid = fork()) < 0 ){
			fprintf( stderr, "fork failed - %s", Errormsg(errno) );
		} else if( pid ){
			plp_usleep(1000);
			/* Mother */
			fprintf( stderr, "Mother writing to %d\n", fds[0] );
			fflush(stderr);
			s = mother;
			if( write( fds[0], s, strlen(s) ) < 0 ){
				fprintf( stderr, "write failed fd %d - %s\n",
					fds[0], Errormsg(errno) );
				status = 1;
			}
			fprintf( stderr, "Mother reading from %d\n", fds[0] );
			fflush(stderr);
			i = read( fds[0], line, sizeof(line) );
			if( i < 0 ){
				fprintf( stderr, "read failed fd %d - %s\n",
					fds[0], Errormsg(errno) );
				status = 1;
			} else {
				line[i] = 0;
				fprintf( stderr, "Mother got '%s'\n", line );
				fflush(stderr);
				if( strcmp( line, child ) ){
					fprintf( stderr, "ERROR!!! wrong answer\n" );
					status = 1;
				}
			}
			fflush(stderr);
		} else if( pid == 0 ){
			/* Child */
			fprintf( stderr, "Daughter reading from %d\n", fds[1] );
			fflush(stderr);
			i = read( fds[1], line, sizeof(line) );
			if( i < 0 ){
				fprintf( stderr, "read failed fd %d - %s\n",
					fds[1], Errormsg(errno) );
				fflush(stderr);
				exit(1);
			} else {
				line[i] = 0;
				fprintf( stderr, "Child got '%s'\n", line );
				fflush(stderr);
				if( strcmp( line, mother ) ){
					fprintf( stderr, "ERROR!!! wrong answer\n" );
					fflush(stderr);
					exit(1);
				}
			}
			fflush(stderr);
			plp_usleep(1000);
			fprintf( stderr, "Daughter writing to %d\n", fds[1] );
			fflush(stderr);
			s = child;
			if( write( fds[1], s, strlen(s) ) < 0 ){
				fprintf( stderr, "write failed - fd %d %s\n",
					fds[1], Errormsg(errno) );
				fflush(stderr);
				exit(1);
			}
			fflush(stderr);
			exit(0);
		}

		fflush(stderr);
		plp_usleep(1000);
		while(1){
			status = 0;
			result = plp_waitpid( -1, &status, 0 );
			err = errno;
			fprintf( stderr, "waitpid result %d, status %d, errno '%s'\n",
				(int)result, status, Errormsg(err) );
			if( result == pid ){
				fprintf( stderr, "Daughter exit status %d\n", status );
				if( status != 0 ){
					fprintf( stderr, "rw_test failed\n");
				}
				break;
			} else if( (result == -1 && errno == ECHILD) || result == 0 ){
				break;
			} else if( result == -1 && errno != EINTR ){
				fprintf( stderr,
					"plp_waitpid() failed!  This should not happen!");
			}
			fflush(stderr);
		}
		fflush(stderr);
		if( status == 0 ){
			fprintf( stderr, "***** waitpid() works\n" );
			fflush(stderr);
		}
	}
	if( status == 0 ){
		fprintf( stderr, "***** Bidirectional pipes work\n" );
		fflush(stderr);
	} else {
		fprintf( stderr,
			"rw_pipe code failed- see Makefile and setup_filter.c\n"
			"Good luck! Sun Aug  6 00:07:19 PDT 1995 Patrick Powell\n" );
		fflush(stderr);
	}

	Init_path( &dpath, "/tmp" );
	freespace = Space_avail( &dpath );

	fprintf( stderr, "***** Free space '/tmp' = %d Kbytes \n"
		"   (check using df command)\n", (int)freespace );

	/*
	 * check serial line
	 */
	if( serial_line == 0 ){
		fprintf( stderr,
			"*******************************************************\n" );
		fprintf( stderr, "********** Missing serial line\n" );
		fprintf( stderr,
			"*******************************************************\n" );
		goto test_lockfd;
	} else {
		fprintf( stderr, "Trying to open '%s'\n",
			serial_line );
		fd = Checkwrite_timeout( 2, serial_line, &statb, O_RDWR, 0, 1 );
		err = errno;
		if( Alarm_timed_out ){
			fprintf( stderr,
				"ERROR: open of '%s'timed out\n"
				" Check to see that the attached device is online\n",
				serial_line );
			goto test_stty;
		} else if( fd < 0 ){
			fprintf( stderr, "Error opening line '%s'\n", Errormsg(err));
			goto test_stty;
		} else if( !isatty( fd ) ){
			fprintf( stderr,
				"*******************************************************\n" );
			fprintf( stderr, "***** '%s' is not a serial line!\n",
				serial_line );
			fprintf( stderr,
				"*******************************************************\n" );
			goto test_stty;
		} else {
			fprintf( stderr, "\nTrying read with timeout\n" );
			i = Read_fd_len_timeout( 1, fd, cmd, sizeof(cmd) );
			err = errno;
			if( Alarm_timed_out ){
				fprintf( stderr, "***** Read with Timeout successful\n" );
			} else {
				 if( i < 0 ){
					fprintf( stderr,
					"***** Read with Timeout FAILED!! Error '%s'\n",
						Errormsg( err ) );
				} else {
					fprintf( stderr,
						"***** Read with Timeout FAILED!! read() returned %d\n",
							i );
		fprintf( stderr,
"***** On BSD derived systems CARRIER DETECT (CD) = OFF indicates EOF condition.\n" );
		fprintf( stderr,
"*****  Check that CD = ON and repeat test with idle input port.\n" );
		fprintf( stderr,
"*****  If the test STILL fails,  then you have problems.\n" );
				}
			}
		}
		/*
		 * now we try locking the serial line
		 */
		/* we try to lock the serial line */
		fprintf( stderr, "\nChecking for serial line locking\n" );
		fflush(stderr);
		fflush(stdout);
#if defined(LOCK_DEVS) && LOCK_DEVS == 0
		fprintf( stderr,
			"*******************************************************\n" );
		fprintf( stderr,
			"******** Device Locking Disabled by compile time options" );
		fprintf( stderr, "\n" );
		fprintf( stderr,
			"*******************************************************\n" );
		fflush(stderr);
		goto test_stty;
#endif

		if( Set_timeout() ){
			Set_timeout_alarm( 1, 0);
			i =  LockDevice( fd, serial_line );
		}
		Clear_timeout();
		err = errno;
		if( Alarm_timed_out || i < 0 ){
			if( Alarm_timed_out ){
				fprintf( stderr, "LockDevice timed out - %s", Errormsg(err) );
			}
			fprintf( stderr,
				"*******************************************************\n" );
				fprintf( stderr, "********* LockDevice failed -  %s\n",
					Errormsg(err) );
				fprintf( stderr, "********* Try an alternate lock routine\n" );
			fprintf( stderr,
				"*******************************************************\n" );
			fflush(stderr);
			goto test_stty;
		}
		
		fprintf( stderr, "***** LockDevice with no contention successful\n" );
		fflush(stderr);
		/*
		 * now we fork a child with tries to reopen the file and lock it
		 */
		if( (pid = fork()) < 0 ){
			fprintf( stderr, "fork failed - %s", Errormsg(errno) );
		} else if( pid == 0 ){
			close(fd);
			fd = -1;
			i = -1;
			fprintf( stderr, "Daughter re-opening line '%s'\n", serial_line );
			fflush(stderr);
			if( Set_timeout() ){
				Set_timeout_alarm( 1, 0);
				fd = Checkwrite( serial_line, &statb, O_RDWR, 0, 0 );
				if( fd >= 0 ) i = LockDevice( fd, serial_line );
			}
			Clear_timeout();
			err = errno;
			fprintf( stderr, "Daughter open completed- fd '%d', lock %d\n",
				 fd, i );
			fflush(stderr);
			if( Alarm_timed_out ){
				fprintf( stderr, "Timeout opening line '%s'\n",
					serial_line );
			} else if( fd < 0 ){
				fprintf( stderr, "Error opening line '%s' - %s\n",
				serial_line, Errormsg(err));
			} else if( i > 0 ){
				fprintf( stderr, "Lock '%s' succeeded! wrong result\n",
					serial_line);
			} else {
				fprintf( stderr, "**** Lock '%s' failed, desired result\n",
					serial_line);
			}
			fflush(stderr);
			if( fd >= 0 ){
				fprintf( stderr,"Daughter closing '%d'\n", fd );
				fflush(stderr);
				close( fd );
			}
			fflush(stderr);
			fprintf( stderr,"Daughter exit with '%d'\n", (i >= 0)  );
			fflush(stderr);
			exit(i >= 0);
		} else {
			status = 0;
			fflush(stderr);
			fprintf( stderr, "Mother starting sleep\n" );
			fflush(stderr);
			plp_usleep(2000);
			fprintf( stderr, "Mother sleep done\n" );
			fflush(stderr);
			while(1){
				result = plp_waitpid( -1, &status, 0 );
				err = errno;
				fprintf( stderr, "waitpid result %d, status %d, errno '%s'\n",
					(int)result, status, Errormsg(err) );
				if( result == pid ){
					fprintf( stderr, "Daughter exit status %d\n", status );
					fflush(stderr);
					if( status != 0 ){
						fprintf( stderr, "LockDevice failed\n");
					}
					break;
				} else if( (result == -1 && errno == ECHILD) || result == 0 ){
					break;
				} else if( result == -1 && errno != EINTR ){
					fprintf( stderr,
						"plp_waitpid() failed!  This should not happen!");
					status = -1;
					break;
				}
				fflush(stderr);
			}
			fflush(stderr);
			if( status == 0 ){
				fprintf( stderr, "***** LockDevice() works\n" );
			}
			fflush(stderr);
		}
test_stty:
		/*
		 * do an STTY operation, then print the status.
		 * we cheat and use a shell script; check the output
		 */
		if( fd <= 0 ) goto test_lockfd;
		fprintf( stderr, "\n\n" );
		fprintf( stderr, "Checking stty functions, fd %d\n\n", fd );
		fflush(stderr);
		if( (pid = fork()) < 0 ){
			fprintf( stderr, "fork failed - %s", Errormsg(errno) );
		} else if( pid == 0 ){
			/* default for status */
			plp_snprintf( t1, sizeof(t1), "/tmp/t1XXX%d", getpid() );
			plp_snprintf( t2, sizeof(t2), "/tmp/t2XXX%d", getpid() );
			diffcmd = "diff -c %s %s 1>&2";
			ttyfd = 1;	/*stdout is reported */
			sttycmd = "stty -a 2>%s";	/* on stderr */
#if defined(SUNOS4_1_4)
			ttyfd = 1;	/*stdout is reported */
			sttycmd = "/bin/stty -a 2>%s";	/* on stderr */
#elif defined(SOLARIS) || defined(SVR4) || defined(linux)
			ttyfd = 0;	/* stdin is reported */
			sttycmd = "/bin/stty -a >%s";	/* on stdout */
#elif (defined(BSD) && (BSD >= 199103))	/* HMS: Might have to be 199306 */
			ttyfd = 0;	/* stdin is reported */
			sttycmd = "stty -a >%s";	/* on stdout */
#elif defined(BSD) /* old style BSD4.[23] */
			sttycmd = "stty everything 2>%s";
#endif
			if( fd != ttyfd ){
				i = dup2(fd, ttyfd );
				if( i != ttyfd ){
					fprintf( stderr, "dup2() failed - %s\n", Errormsg(errno) );
					exit(-1);
				}
				close( fd );
			}
			plp_snprintf( stty, sizeof(stty), sttycmd, t1 );
			plp_snprintf( diff, sizeof(diff), diffcmd, t1, t2 );
			plp_snprintf( cmd, sizeof(cmd), "%s; cat %s 1>&2", stty, t1 );
			fprintf( stderr,
			"Status before stty, using '%s', on fd %d->%d\n",
				cmd, fd, ttyfd );
			fflush(stderr); fflush(stdout); i = system( cmd ); fflush(stdout);
			fprintf( stderr, "\n\n" );
			Stty_command = "9600 -even odd echo";
			fprintf( stderr, "Trying 'stty %s'\n", Stty_command );
			fflush(stderr);
			Do_stty( ttyfd );
			plp_snprintf( stty, sizeof(stty), sttycmd, t2 );
			plp_snprintf( cmd, sizeof(cmd),
				"%s; %s", stty, diff );
			fprintf( stderr, "Doing '%s'\n", cmd );
			fflush(stderr); i = system( cmd ); fflush(stdout);
			fprintf( stderr, "\n\n" );
			Stty_command = "1200 -odd even";
			fprintf( stderr, "Trying 'stty %s'\n", Stty_command );
			fflush(stderr);
			Do_stty( ttyfd );
			fprintf( stderr, "Doing '%s'\n", cmd );
			fflush(stderr); i = system( cmd ); fflush(stdout);
			fprintf( stderr, "\n\n" );
			Stty_command = "300 -even -odd -echo cbreak";
			fprintf( stderr, "Trying 'stty %s'\n", Stty_command );
			fflush(stderr);
			Do_stty( ttyfd );
			plp_snprintf( stty, sizeof(stty), sttycmd, serial_line, t2 );
			fprintf( stderr, "Doing '%s'\n", cmd );
			fflush(stderr); i = system( cmd ); fflush(stdout);
			fprintf( stderr, "\n\n" );
			fprintf( stderr, "Check the above for parity, speed and echo\n" );
			fprintf( stderr, "\n\n" );
			unlink(t1);
			unlink(t2);
			fflush(stderr);
			exit(0);
		} else {
			close(fd);
			fd = -1;
			status = 0;
			while(1){
				result = plp_waitpid( -1, &status, 0 );
				if( result == pid ){
					fprintf( stderr, "Daughter exit status %d\n", status );
					fflush(stderr);
					if( status != 0 ){
						fprintf( stderr, "STTY operation failed\n");
					}
					break;
				} else if( (result == -1 && errno == ECHILD) || result == 0 ){
					break;
				} else if( result == -1 && errno == EINTR ){
					fprintf( stderr,
						"plp_waitpid() failed!  This should not happen!");
					status = -1;
					break;
				}
				fflush(stderr);
			}
			fflush(stderr);
			if( status == 0 ){
				fprintf( stderr, "***** STTY works\n" );
			}
			fflush(stderr);
		}
	}
test_lockfd:
	fflush(stderr);
	if( fd >= 0 ) close(fd);
	fd = -1;

	fprintf( stderr, "\n\n" );
	fflush(stderr);
	/*
	 * check out Lockf
	 */
	plp_snprintf( line, sizeof(line), "/tmp/XX%dXX", getpid );
	fprintf( stderr, "Checking Lockf '%s'\n", line );
	fflush(stderr);
	fd = Lockf( line, &statb );
	fflush(stderr);
	sprintf( cmd, "ls -l %s", line );
	fflush(stderr); i = system( cmd ); fflush(stdout);
	if( fd < 0 ){
		fprintf( stderr, "file open '%s' failed - '%s'\n", line,
			Errormsg( errno ) );
	} else if( (pid = fork()) < 0 ){
		fprintf( stderr, "fork failed!\n");
	} else if ( pid == 0 ){
		fprintf( stderr, "Daughter re-opening and locking '%s'\n", line );
		close( fd );
		if( (fd = Checkwrite(line, &statb, O_RDWR, 1, 0 )) < 0) {
			err = errno;
			fprintf( stderr,
				"Daughter re-open '%s' failed: wrong result - '%s'\n",
				line, Errormsg(errno)  );
			exit(1);
		}
		if( Do_lock( fd, line, 0 ) < 0) {
			fprintf( stderr,
				"Daughter could not lock '%s', correct result\n", line );
			exit(0);
		}
		fprintf( stderr,
			"Daughter locked '%s', incorrect result\n", line );
		exit(1);
	}
	fflush(stderr);
	plp_usleep(1000);
	fflush(stderr);
	status = 0;
	while(1){
		result = plp_waitpid( -1, &status, 0 );
		if( result == pid ){
			fprintf( stderr, "Daughter exit status %d\n", status );
			break;
		} else if( (result == -1 && errno == ECHILD) || result == 0 ){
			break;
		} else if( result == -1 && errno != EINTR ){
			fprintf( stderr,
				"plp_waitpid() failed!  This should not happen!");
			status = -1;
			break;
		}
		fflush(stderr);
	}
	if( status == 0 ){
		fprintf( stderr, "***** Lockf() works\n" );
	}
	fflush(stderr);

	if( (pid = fork()) < 0 ){
		fprintf( stderr, "fork failed!\n");
		fflush(stderr);
	} else if ( pid == 0 ){
		int lock = 0;
		fprintf( stderr, "Daughter re-opening '%s'\n", line );
		fflush(stderr);
		close( fd );
		if( (fd = Checkwrite(line, &statb, O_RDWR, 1, 0 )) < 0) {
			err = errno;
			fprintf( stderr,
				"Daughter re-open '%s' failed: wrong result - '%s'\n",
				line, Errormsg(errno)  );
			exit(1);
		}
		fprintf( stderr, "Daughter blocking for lock\n" );
		fflush(stderr);
		lock = Do_lock( fd, line, 1 );
		if( lock < 0 ){
			fprintf( stderr, "Daughter lock '%s' failed! wrong result\n",
				line );
			fflush(stderr);
			exit( 1 );
		}
		fprintf( stderr, "Daughter lock '%s' succeeded, correct result\n",
			line );
		fflush(stderr);
		exit(0);
	}
	plp_usleep(1000);
	fflush(stderr);

	fprintf( stderr, "Mother closing '%s', releasing lock on fd %d\n",
		line, fd );
	close( fd );
	fflush(stderr);
	fd = -1;
	status = 0;
	while(1){
		result = plp_waitpid( -1, &status, 0 );
		if( result == pid ){
			fprintf( stderr, "Daughter exit status %d\n", status );
			break;
		} else if( (result == -1 && errno == ECHILD) || result == 0 ){
			break;
		} else if( result == -1 && errno != EINTR ){
			fprintf( stderr,
				"plp_waitpid() failed!  This should not happen!");
			status = -1;
			break;
		}
		fflush(stderr);
	}
	fflush(stderr);
	if( status == 0 ){
		fprintf( stdout, "***** Lockf() with unlocking works\n" );
	}
	fflush(stderr);

	if( fd >= 0 ) close(fd);
	fd = - 1;
	unlink( line );


/***************************************************************************
 * check out the process title
 ***************************************************************************/

	fprintf( stdout, "checking if setting process info to 'XXYYZZ' works\n" );
	proctitle( "XXYYZZ" );
	/* try simple test first */
	i = 0;
	if( (tf = popen( "ps | grep XXYYZZ | grep -v grep", "r" )) ){
		while( fgets( line, sizeof(line), tf ) ){
			printf( line );
			++i;
		}
		fclose(tf);
	}
	
	if( i == 0 && (tf = popen( "ps | grep XXYYZZ | grep -v grep", "r" )) ){
		while( fgets( line, sizeof(line), tf ) ){
			printf( line );
			++i;
		}
		fclose(tf);
	}
	if( i ){
		fprintf( stdout, "***** setproctitle works\n" );
	} else {
		fprintf( stdout, "***** setproctitle debugging aid unavailable (not a problem)\n" );
	}
	exit(0);
}
