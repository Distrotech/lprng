/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpr.c
 * PURPOSE:
 **************************************************************************/

/***************************************************************************
 * SYNOPSIS
 *      lpr [ -PPrinter ] [-Q] [ -Knum ] [ -C class ] [ -J job ] [
 *      -RremoteAccount ] [ -m[mailTo] ] [ -T title ] [-i[numcols]]
 *      [ -1234 font ] [ -wnum ] [ -Zzoptions ] [ -Uuser ] [ -HHost
 *      ] [ -Ffilter ] [ -bhrs ] [ -Dn ] [ -X ] [ filename ...  ]
 * DESCRIPTION
 *      Lpr uses a spooling server to print the named files when
 *      facilities become available.  If no Names appear, the stan-
 *      dard input is assumed.
 *      -C class or priority (A - Z)
 *      -D[n] debug level
 *      -F?  Filter or format specification
 *      -HHost Used by root process to specify a Host
 *      -J jobname  Specify the job name to print on the burst page
 *      -Q include the queue name in the control file
 *      -PPrinter  Output to the specified Printer
 *      -R remoteAccount
 *      -T title specify the title used by pr(1);
 *      -Uuser Used by root process to specify a user
 *      -Z output printer filter options
 *      -b The files are assumed to contain binary data
 *      -c data produced by cifplot(l).
 *      -d output from tex(l) (DVI format from Stanford).
 *      -f same as -Fr, FORTAN carriage control
 *      -g standard plot data as produced by the plot(3X) routines
 *      -i[numcols] Cause the output to be indented
 *      -l (literal) text with control characters to be printed
 *      -n output from DITROFF
 *      -m[mailTo] Send mail upon completion
 *      -t output from troff(1)
 *      -v a raster image for devices like the Benson Varian.
 *      -wwidth  specify the page width for pr.
 *      -#num number of copies of each file to be printed.
 ****************************************************************************
 * Implementation:
 * 	Each time lpr is invoked it creates a "job" entry in the appropriate
 * spool directory.  Each job consists of a control file and zero or more
 * data files.  The control file contains lines that specify the job
 * parameters, such as job Name, etc., and the Names of the data files.
 *      Control file format
 *      First character in the line flags kind of entry, remainder of line is
 *          the arguemnt.
 *
 * The control file has the flag lines in the following order:
 *
 * HPJCNMDTL <- Standard SUN/BSD order
 *   ABEFGIKOQRSVWXYZ <- PLP additional ones
 * The following are one per data file:
 *   N      -  file 'name'
 *   a-z    -  data file format and file
 *   U      -  unlink
 *
 * Flag Lines:
 *		C -- "class name" on banner page
 *		H -- "Hostname" of machine where lpr was done (Fully Qualified Domain)
 *		I -- "indent" amount to indent output   - Number
 *		J -- "job Name" on banner page          - String
 *		L -- "literal" user's Name to print on banner - STring
 *		M -- "mail" to user when done printing
 *		N -- "Name" of file (used by lpq)
 *		P -- "Person" user's login Name
 *		Q -- "Queue" name
 *		R -- account id  for charging
 *		U -- "unlink" Name of file to remove after we print it
 *		W -- "width" page width for PR
 *		T -- Title for use with 'p' formatting
 *		X -- "header" header title for PR
 *		Z -- xtra options to filters
 *
 *	Lower case letters are formats, and are together with the Name
 *      of the file to print
 *		f -- "file Name" Name of text file to print
 *		l -- "file Name" text file with control chars
 *		p -- "file Name" text file to print with pr(1)
 *		t -- "file Name" troff(1) file to print
 *		n -- "file Name" ditroff(1) file to print
 *		d -- "file Name" dvi file to print
 *		g -- "file Name" plot(1G) file to print
 *		v -- "file Name" plain raster file to print
 *		c -- "file Name" cifplot file to print
 ***************************************************************************/
/*
Additional Implementation Notes

Wed Apr  5 05:10:45 PDT 1995 Patrick Powell
The control file is the most difficult part of making a portable front end,
as it must be compatible with several existing systems.  There are historical
restrictions on the order and contents of the various fields.

A - Job Id (LPRng)
  Identifier for the job.  This should be unique for each job
  that is generated.
H - Hostname     (RFC1179 - 31 or fewer octets)
  The Hostname field is used to identify the host originating the job.
  However,  RFC1179 does not specify if this is a 'short' or 'fully
  qualified domain name'.  In fact,  on some systems it is impossible to
  provide a fully qualified domain name.  So we have to do the following:
  1. If the control file format string has an S flag, we use the short
     name (overrides all other considerations).
  2. We get the fully qualified domain name and use this.
  3. If we cannot get the fully qualified domain name,  we get the
     short host name.
  4. If we cannot get the short host name,  we use IP address with IP prefixed
	 i.e. - IP130.191.163.56

C - Class Name     (RFC1179 - 31 or fewer octets)
  The Class can be specified on the command line by the -C option
  in almost all LPR implementions,  and is placed in the C flag line.
  If no job class is specified,  the SUN and older Berkeley implementations
  put in the originating host name; PLP puts in the job priority.
  NOTE: some implementations INSIST on the C field being present and non-empty.

J - Job name     (RFC1179 - 99 or fewer octets)
  The Job name can be specified on the command line with the -J option;
  if not specified,  defaults to the list of files to be printed.
  If no file,  the name is '(stdin)'
  NOTE: some implementations INSIST on the J field being present and non-empty.

L - Banner User Name (why L?) (RFC1179 - silent on length)
  If this field is specified,  a banner will be printed.  Note that
  in some LPD implementations that L field triggers banner printing;
  the J field must appear before the L field.  The L field value
  should be 
  1. Set by the -U option (overrides)
  2. The 'P' field (user login name)

	Note: Class and Jobname must precede in control file
	which means that if you specify -U, you must force J and C entries
	in control file.

T - Title for use with 'p' formatting
  If 'p' format (print with 'pr') is specified,  then this is the title 
  to be supplied to 'pr'.

P - "Person" or user's login name
  This is the login name of the user.  It is obtained by trying the
  following in order:
	1. user name entry returned by 'getpwuid( user ID )' 
	2. On PCs or others without password files,  $USER environment variable
	3. On PCs or others without password files,  "nobody"

M - mail on completetion
  This is set by the -m option,  or the LPR tries to infer the user
  and host.  Note that this may not work correctly in systems where
  you work on workstations but have your mail delivered to a central
  site.  The following is used:
  1. -m option value (if any)
  2. Person@fully qualified domainname (note: this may not be the same as
      the H or 'host' information, as the H value may be the short host
      name.

Data Files

Data files are specified in two ways: as a list of names on the command
line OR as the output from another program piped to STDIN. We use an
array of struct datafile{} records to record information about the
data files.

We can use two possible methods for sending the data files.
Method 1:
Pass1: we open each file, get file descriptor fd, stat the fd, close fd.
Pass2: reopen each file, get file descriptor fd, stat the fd,
   compare old and new LENGTH for change, copy the file

Method 2:
Pass1: we open each file, get file descriptor fd, stat the fd
Pass2: restat the file, check for length change, copy the file

Method 1 requires a larger number of open and close operations;
Method 2 requires a larger number of open file descriptors being available;

We will use Method 1, even though it has a slightly higher overhead.

STDIN Spooling:
When we need to create a spool file for STDIN input,  we will use
the following method:
1. Get the temporary directory name:
	$(TMP) environment variable,
	$(TEMP) environment variable,
	"/tmp"
2. Open a file in the temporary directory with a name of the form
	lpr_$PID
3. Write the contents of stdin to this file.
4. After transferring the file to the remote host,  remove the file.

Control File Name

Each control file has the format cfXNNNhost, where NNN is a job number
identifier.  On UNIX systems, NNN is PID%1000, ie.- the process ID
of the LPR process modulo 1000.  If you are using a PC or some other
system that does not have PID's,  then you need another method:
the current time of day % 1000 is pretty good,  but may run
into difficulties if you are printing files quickly. A difficulty
with the PC is that there is no simple way to do a 'lock' operation
to prevent multiple updates.  The solution that appears to
be fairly reasonable is to choose a random number based on the
current following information:
1. time of day to the finest resolution.
2. names of files to be printed.
3. generate a random number using this as a seed.

Note that there is still a 1/1000 chance of getting a duplicate job name;
if you rapidly print the same files multiple numbers of times you most
certainly will get a duplicate.

*/

static char *const _id =
"lpr.c,v 3.20 1998/03/24 02:43:22 papowell Exp";

#include "lp.h"
#include "dump.h"
#include "initialize.h"
#include "killchild.h"
#include "pathname.h"
#include "sendjob.h"
#include "setuid.h"
#include "printcap.h"
/**** ENDINCLUDE ****/

/***************************************************************************
 * main()
 * - top level of LPR Lite.  This is a cannonical method of handling
 *   input.  Note that we assume that the LPD daemon will handle all
 *   of the dirty work associated with formatting, printing, etc.
 * 
 * 1. get the debug level from command line arguments
 * 2. set signal handlers for cleanup
 * 3. get the Host computer Name and user Name
 * 4. scan command line arguments
 * 5. check command line arguments for consistency
 * 5. if we are spooling from stdin, copy stdin to a file.
 * 6. if we have a list of files,  check each for access
 * 7. create a control file
 * 8. send control file to server
 *
 ****************************************************************************/
extern void	Check_parms( struct printcap_entry **printcap_entry );

int main(int argc, char *argv[], char *envp[])
{
	off_t job_size;
	struct dpathname dpath;
	struct printcap_entry *printcap_entry = 0;


	/*
	 * set up the uid
	 */
	Errorcode = 1;
	Interactive = 1;
	Initialize(argc, argv, envp);

	/* set signal handlers */
	(void) plp_signal (SIGHUP, cleanup_HUP);
	(void) plp_signal (SIGINT, cleanup_INT);
	(void) plp_signal (SIGQUIT, cleanup_QUIT);
	(void) plp_signal (SIGTERM, cleanup_TERM);

	Setup_configuration();

	/* scan the input arguments, setting up values */
	Get_parms(argc, argv);      /* scan input args */

	/* Note: we may need the open connection to the remote printer
		to get our IP address if it is not available */

	Check_parms( &printcap_entry );

	if(DEBUGL4 ) dump_parms("LPR Vars after checking parms",Lpr_parms);

	if( Check_for_rg_group( Logname ) ){
		fprintf( stderr, "cannot use printer - not in privileged group\n" );
		cleanup(0);
	}
	/*
	 * Fix the rest of the control File
	 */
	job_size = Make_job( Cfp_static );
	if( job_size == 0 ){
		Errorcode = 1;
		Diemsg (_("nothing to print"));
	}

	/* Send job to the LPD server for the printer */

	Init_path( &dpath, (char *)0 );

	if( RemotePrinter == 0 || RemotePrinter[0] == 0 ) RemotePrinter = Printer;
	if( Remote_support
		&& strchr( Remote_support, 'r' ) == 0
		&& strchr( Remote_support, 'R' ) == 0 ){
		Errorcode = 1;
		Warnmsg( _("no remote support for %s@%s"), RemotePrinter,RemoteHost );
	} else {
		Errorcode =
		Send_job( RemotePrinter?RemotePrinter:Printer, RemoteHost, Cfp_static,
		&dpath, Connect_timeout, Connect_interval, Max_connect_interval,
		Send_job_rw_timeout, printcap_entry );
	}

	/* the dreaded -r (remove files) option */
	if( Removefiles && !Errorcode ){
		int i;

		/* eliminate any possible game playing */
		To_user();
		for( i = 0; i < Filecount; ++i ){
			if( unlink( Files[i] ) == -1 ){
				Warnmsg(_("Error unlinking '%s' - %s"),
					Files[i], Errormsg( errno ) );

			}
		}
	}
	cleanup(0);
	return(0);
}
