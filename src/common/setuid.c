/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: setuid.c
 * PURPOSE: set up the setuid.c information
 **************************************************************************/

static char *const _id =
"setuid.c,v 3.4 1997/09/18 19:46:05 papowell Exp";
/*
 * setuid.c:
 * routines to manipulate user-ids securely (and hopefully, portably).
 * The * internals of this are very hairy, because
 * (a) there's lots of sanity checking
 * (b) there's at least three different setuid-swapping
 * semantics to support :(
 * 
 *
 * Note that the various functions saves errno then restores it afterwards;
 * this means it's safe to do "root_to_user();some_syscall();user_to_root();"
 * and errno will be from the system call.
 * 
 * "root" is the user who owns the setuid executable (privileged).
 * "user" is the user who runs it.
 * "daemon" owns data files used by the PLP utilities (spool directories, etc).
 *   and is set by the 'user' entry in the configuration file.
 * 
 * To_user();	-- set euid to user
 * To_root();	-- set euid to root
 * To_daemon();	-- set euid to daemon
 * To_root();	-- set euid to root
 * Full_daemon_perms() -- set both UID and EUID, one way, no return
 * 
 * - Justin Mason <jmason@iona.ie> May '94.
 * - Patrick Powell Sat Jul  8 07:01:19 PDT 1995
 *   This code has been greatly simplified, at the cost of some 
 *   extra function calls. See commentary below.
 */

/***************************************************************************
Commentary:
Patrick Powell Sat Apr 15 07:56:30 PDT 1995

This has to be one of the ugliest parts of any portability suite.
The following models are available:
1. process has <uid, euid>  (old SYSV, BSD)
2. process has <uid, euid, saved uid, saved euid> (new SYSV, BSD)

There are several possibilites:
1. need euid root   to do some operations
2. need euid user   to do some operations
3. need euid daemon to do some operations

Group permissions are almost useless for a server;
usually you are running as a specified group ID and do not
need to change.  Client programs are slightly different.
You need to worry about permissions when creating a file;
for this reason most client programs do a u mask(0277) before any
file creation to ensure that nobody can read the file, and create
it with only user access permissions.

> int setuid(uid) uid_t uid;
> int seteuid(euid) uid_t euid;
> int setruid(ruid) uid_t ruid;
> 
> DESCRIPTION
>      setuid() (setgid()) sets both the real and effective user ID
>      (group  ID) of the current process as specified by uid (gid)
>      (see NOTES).
> 
>      seteuid() (setegid()) sets the effective user ID (group  ID)
>      of the current process.
> 
>      setruid() (setrgid()) sets the real user ID  (group  ID)  of
>      the current process.
> 
>      These calls are only permitted to the super-user or  if  the
>      argument  is  the  real  or effective user (group) ID of the
>      calling process.
> 
> SYSTEM V DESCRIPTION
>      If the effective user ID  of  the  calling  process  is  not
>      super-user,  but if its real user (group) ID is equal to uid
>      (gid), or if the saved set-user (group) ID  from  execve(2V)
>      is equal to uid (gid), then the effective user (group) ID is
>      set to uid (gid).
>      .......  etc etc

Conclusions:
1. if EUID == ROOT or RUID == ROOT then you can set EUID, UID to anything
3. if EUID is root, you can set EUID 

General technique:
Initialization
  - use setuid() system call to force EUID/RUID = ROOT

Change
  - assumes that initialization has been carried out and
	EUID == ROOT or RUID = ROOT
  - Use the seteuid() system call to set EUID

 ***************************************************************************/

#include "lp.h"
#include "setuid.h"
/**** ENDINCLUDE ****/

#if !defined(HAVE_SETREUID) && !defined(HAVE_SETEUID) && !defined(HAVE_SETRESUID)
#error You need one of setreuid(), seteuid(), setresuid()
#endif

/***************************************************************************
 * Commentary
 * setuid(), setreuid(), and now setresuid()
 *  This is probably the easiest road.
 *  Note: we will use the most feature ridden one first, as it probably
 *  is necessary on some wierd system.
 *   Patrick Powell Fri Aug 11 22:46:39 PDT 1995
 ***************************************************************************/
#if !defined(HAVE_SETEUID) && !defined(HAVE_SETREUID) && defined(HAVE_SETRESUID)
# define setreuid(x,y) (setresuid( (x), (y), -1))
# define HAVE_SETREUID
#endif

/***************************************************************************
 * setup_info()
 * 1. checks for the correct daemon uid
 * 2. checks to see if called (only needs to do this once)
 * 3. if UID 0 or EUID 0 forces both UID and EUID to 0 (test)
 * 4. Sets UID_root flag to indicate that we can change
 ***************************************************************************/

static void setup_info(void)
{
	int err = errno;
	static int SetRootUID;	/* did we set UID to root yet? */

	DaemonUID = Getdaemon(); /* do this each time in case we change it */
	if( SetRootUID == 0 ){
		OriginalEUID = geteuid();	
		OriginalRUID = getuid();	
		/* we now make sure that we are able to use setuid() */
		/* notice that setuid() will work if EUID or RUID is 0 */
		if( OriginalEUID == 0 || OriginalRUID == 0 ){
			/* set RUID/EUID to ROOT - possible if EUID or UID is 0 */
			if(
#				ifdef HAVE_SETEUID
					setuid( (uid_t)0 ) || seteuid( (uid_t)0 )
#				else
					setuid( (uid_t)0 ) || setreuid( 0, 0 )
#				endif
				){
				fatal( LOG_ERR,
					"setup_info: RUID/EUID Start %d/%d seteuid failed",
					OriginalRUID, OriginalEUID);
			}
			if( getuid() || geteuid() ){
				fatal( LOG_ERR,
				"setup_info: IMPOSSIBLE! RUID/EUID Start %d/%d, now %d/%d",
					OriginalRUID, OriginalEUID, 
					getuid(), geteuid() );
			}
			UID_root = 1;
		}
		SetRootUID = 1;
	}
	errno = err;
}

/***************************************************************************
 * seteuid_wrapper()
 * 1. you must have done the initialization
 * 2. check to see if you need to do anything
 * 3. check to make sure you can
 ***************************************************************************/
static int seteuid_wrapper( int to )
{
	int err = errno;
	uid_t euid;


	DEBUG0(
		"seteuid_wrapper: Before RUID/EUID %d/%d, DaemonUID %d, UID_root %d",
		OriginalRUID, OriginalEUID, DaemonUID, UID_root );
	if( UID_root ){
		/* be brutal: set both to root */
		if( setuid( 0 ) ){
			logerr_die( LOG_ERR,
			"seteuid_wrapper: setuid() failed!!");
		}
#if defined(HAVE_SETEUID)
		if( seteuid( to ) ){
			logerr_die( LOG_ERR,
			"seteuid_wrapper: seteuid() failed!!");
		}
#else
		if( setreuid( 0, to) ){
			logerr_die( LOG_ERR,
			"seteuid_wrapper: setreuid() failed!!");
		}
#endif
	}
	euid = geteuid();
	DEBUG0( "seteuid_wrapper: After uid/euid %d/%d", getuid(), euid );
	errno = err;
	return( to != euid );
}


/*
 * Superhero functions - change the EUID to the requested one
 *  - these are really idiot level,  as all of the tough work is done
 * in setup_info() and seteuid_wrapper() 
 */
int To_root(void) 	{ setup_info(); return( seteuid_wrapper( 0 )	); }
int To_daemon(void)	{ setup_info(); return( seteuid_wrapper( DaemonUID )	); }
int To_user(void)	{ setup_info(); return( seteuid_wrapper( OriginalRUID )	); }
int To_uid( int uid ) { setup_info(); return( seteuid_wrapper( uid ) ); }

/*
 * set both uid and euid to the same value, using setuid().
 * This is unrecoverable!
 */

int setuid_wrapper(int to)
{
	int err = errno;
	if( UID_root ){
		/* Note: you MUST use setuid() to force saved_setuid correctly */
		if( setuid( (uid_t)0 ) ){
			logerr_die( LOG_ERR, "setuid_wrapper: setuid(0) failed!!");
		}
		if( setuid( (uid_t)to ) ){
			logerr_die( LOG_ERR, "setuid_wrapper: setuid(%d) failed!!", to);
		}
	}
    DEBUG3("after setuid: (%d, %d)", getuid(),geteuid());
	errno = err;
	return( to != getuid() || to != geteuid() );
}

int Full_daemon_perms(void)	{ setup_info(); return(setuid_wrapper(DaemonUID)); }
int Full_root_perms(void)	{ setup_info(); return(setuid_wrapper( 0 )); }
int Full_user_perms(void)	{ setup_info(); return(setuid_wrapper(OriginalRUID)); }


/***************************************************************************
 * Getdaemon()
 *  get daemon uid
 *
 ***************************************************************************/

int Getdaemon(void)
{
	char *str = 0;
	char *t;
	struct passwd *pw;
	int uid;

	if( Daemon_user ){
		str = Daemon_user;
	}
	if( str == 0 ){
		str = "daemon";
	}
	while( isspace( *str ) ) ++str;
	if( (t = strpbrk( str, " \t:;")) )  *t = 0;
	DEBUG3( "Getdaemon: Daemon_user '%s', daemon '%s'", Daemon_user, str );
	t = str;
	uid = strtol( str, &t, 10 );
	if( str == t || *t ){
		/* try getpasswd */
		pw = getpwnam( str );
		if( pw ){
			uid = pw->pw_uid;
		}
	}
	if( uid == 0 ) uid = getuid();
	DEBUG3( "Getdaemon: uid '%d'", uid );
	return( uid );
}

/***************************************************************************
 * Getdaemon_group()
 *  get daemon gid
 *
 ***************************************************************************/

int Getdaemon_group(void)
{
	char *str = 0;
	char *t;
	struct group *gr;
	gid_t gid;

	if( Daemon_group ){
		str = Daemon_group;
	}
	DEBUG3( "Getdaemon_group: Daemon_group '%s'", str?str:"<NULL>" );
	if( str == 0 ){
		str = "daemon";
	}
	DEBUG3( "Getdaemon_group: name '%s'", str );
	t = str;
	gid = strtol( str, &t, 10 );
	if( str == t ){
		/* try getpasswd */
		gr = getgrnam( str );
		if( gr ){
			gid = gr->gr_gid;
		}
	}
	if( gid == 0 ) gid = getgid();
	DEBUG3( "Getdaemon_group: gid '%d'", gid );
	return( gid );
}

/***************************************************************************
 * set daemon uid and group
 * 1. get the current EUID
 * 2. set up the permissions changing
 * 3. set the RGID/EGID
 ***************************************************************************/

int Setdaemon_group(void)
{
	uid_t euid;
	int status;
	int err;

	DaemonGID = Getdaemon_group();
	DEBUG3( "Setdaemon_group: set '%d'", DaemonGID );
	if( UID_root ){
		euid = geteuid();
		To_root();	/* set RUID/EUID to root */
		status = setgid( DaemonGID );
		err = errno;
		if( To_uid( euid ) ){
			err = errno;
			logerr_die( LOG_ERR, "setdaemon_group: To_uid '%d' failed '%s'",
				euid, Errormsg( err ) );
		}
		if( status < 0 || DaemonGID != getegid() ){
			logerr_die( LOG_ERR, "setdaemon_group: setgid '%d' failed '%s'",
			DaemonGID, Errormsg( err ) );
		}
	}
	return( 0 );
}


/*
 * Testing magic:
 * if we are running SUID
 *   We have set our RUID to root and EUID daemon
 * However,  we may want to run as another UID for testing.
 * The config file allows us to do this, but we set the SUID values
 * from the hardwired defaults before we read the configuration file.
 * After reading the configuration file,  we check the current
 * DaemonUID and the requested Daemon UID.  If the requested
 * Daemon UID == 0, then we run as the user which started LPD.
 */

void Reset_daemonuid(void)
{
	uid_t uid;
    uid = Getdaemon();  /* get the config file daemon id */
    if( uid != DaemonUID ){
        if( uid == 0 ){
            DaemonUID = OriginalRUID;   /* special case for testing */
        } else {
            DaemonUID = uid;
        }
    }
	To_daemon();        /* now we are running with desired UID */
    DEBUG3( "DaemonUID %d", DaemonUID );
}
