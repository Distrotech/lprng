/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpr_copyfiles.c
 * PURPOSE: check the input files and make a copy if STDIN is specified
 **************************************************************************/

static char *const _id =
"$Id: lpr_cpyfiles.c,v 3.1 1996/08/31 21:11:58 papowell Exp papowell $";

#include "lpr.h"
#include "lp_config.h"

int Check_printable(char *file, int fd, struct stat *statb, int format );
/***************************************************************************
 * off_t Copy_stdin()
 * 1. we get the name of a temporary file
 * 2. we open the temporary file and read from STDIN until we get
 *    no more.
 * 3. stat the  temporary file to prevent games
 ***************************************************************************/
void remove_tmp( void *p )
{
	if( Tempfile ){
		unlink( Tempfile );
	}
}


off_t Copy_stdin( struct control_file *cf )
{
	struct dpathname dpath;
	char *dir = 0;
	char *file = 0;
	int fd, count;
	off_t size = 0;
	int i;			/* The all-seeing i */
	struct data_file *df;
	int err;
	char buffer[4096];

	/* we get the temporary directory for these operations */

	/* OK, now we can set up the information */
	if( cf->data_file.count >= cf->data_file.max ){
		extend_malloc_list( &cf->data_file, sizeof( df[0] ), 10 );
	}
	df = (void *)cf->data_file.list;

	df->Ninfo = "(stdin)";
	df->fd = 0;
	df->copies = Copies;
	cf->data_file.count++;

	/* we simply set up the filter and return */
	if( Secure != 0 ){
		df->flags |= PIPE_FLAG;
		df->fd = dup( 0 );
		size = -1;
		return( size );
	}

	if( dir == 0 || *dir == 0 ){
		dir = getenv( "LPR_TMP" );
	}
	if( dir == 0 || *dir == 0 ){
		dir = Default_tmp_dir;
	}
	if( dir == 0 || *dir == 0 ){
		dir = "/tmp";
	}

	/* build up the string */
	Init_path( &dpath, dir );
	file = Add_path( &dpath, "LPRXXXXXX" );

	/* open the file for reading and writing */
	/* there is a race condition here - we block all signals */

	plp_block_signals();

	/* Note: we set up the mask value to protect the user */
	register_exit( remove_tmp, 0 );
#if defined(HAVE_MKSTEMP)
	fd = mkstemp( file );
#else
# if defined(HAVE_MKTEMP)
	mktemp( file );
# else
#   error missing mkstemp and mktemp functions
# endif
	fd = -1;
	{
	struct stat statb;
	if( file[0] ) fd = Checkwrite( file, &statb, O_RDWR, 1, 0 );
	}
#endif
	err = errno;
	Tempfile = safestrdup( file );
	plp_unblock_signals();
	if( fd < 0 ){
		logerr_die( LOG_INFO, "Copy_stdin: cannot open temp file");
	}

	df->openname = Tempfile;

	DEBUG2("Temporary file '%s', fd %d", Tempfile, fd );

	if( fd < 0 ){
		logerr_die( LOG_INFO, "mkstemp '%s' failed", Tempfile );
	} else if( fd == 0 ){
		Diemsg( "You have closed STDIN! cannot pipe from a closed connection");
	}
	/* now we copy standard input into the file until we get EOF */
	i = 1;
	size = 0;
	while( (count = read( 0, buffer, sizeof(buffer))) > 0 ){
		if( Write_fd_len( fd, buffer, count ) < 0 ){
			Errorcode = JABORT;
			logerr_die( LOG_INFO, "Copy_stdin: write to temp file failed");
		}
		size += count;
	}
	/*
	if( count < 0 ){
		logerr_die( LOG_INFO, "Copy_stdin: read failed!!");
	}
	*/
	if( fstat( fd, &df->statb ) < 0 ){
		logerr_die( LOG_INFO, "Copy_stdin: fstat '%s' failed", Tempfile);
	}
	if( !Check_printable(df->Ninfo, fd, &df[0].statb, *Format)){
		return( 0 );
	}
	DEBUG5( "Copy_Stdin: Tempfile '%s' size %d ", Tempfile, size );
	return( size );
}

/***************************************************************************
 * off_t Check_files( char **files, int filecount )
 * 1. malloc the space for struct data_file{} array
 * 2. check each of the input files for access
 * 3. stat the files and get the size
 * 4. Check for printability
 * 5. Put information in the data_file{} entry
 ***************************************************************************/

off_t Check_files( struct control_file *cf, char **files, int filecount )
{
	off_t size = 0;
	int i, fd, printable;
	struct data_file *df;
	struct stat statb;
	int err;

	if( filecount == 0 ){
		return( size );
	}

	/* OK, now we can set up the information */
	for( i = 0; i < filecount; ++i){
		DEBUG5( "Check_files: doing '%s'", files[i] );
		fd = Checkread( files[i], &statb );
		err = errno;
		if( fd < 0 ){
			Warnmsg( "Cannot open file '%s', %s", files[i], Errormsg( err ) );
			continue;
		}
		printable = Check_printable( files[i], fd, &statb, *Format );
		close( fd );
		if( printable > 0 ){
			DEBUG5( "Check_files: printing '%s'", files[i] );
			if( cf->data_file.count >= cf->data_file.max ){
				extend_malloc_list( &cf->data_file, sizeof( df[0] ), 10 );
			}
			df = (void *)cf->data_file.list;
			df = &df[cf->data_file.count++];
			size += statb.st_size*Copies;
			df->statb = statb;
			df->openname = files[i];
			df->Ninfo = files[i];
			df->copies = Copies;
		}
	}
	DEBUG5( "Check_files: size %d", size );
	return( size );
}

/***************************************************************************
 * int Check_printable(char *file, int fd, struct stat *statb, int format )
 * 1. Check to make sure it is a regular file.
 * 2. Check to make sure that it is not 'binary data' file
 * 3. If a text file,  check to see if it has some control characters
 *
 ***************************************************************************/

static int is_arch(char *buf, int n);
static int is_exec( char *buf, int n);
static int is_mmdf_mail(char *buf);

int Check_printable(char *file, int fd, struct stat *statb, int format )
{
    char buf[LINEBUFFER];
    int n, i, c;                /* Acme Integers, Inc. */
    int printable = 0;
	char *err = "cannot print '%s': %s";

	/*
	 * Do an LSEEK on the file, i.e.- see to the start
	 * Ignore any error return
	 */
	lseek( fd, (off_t)0, 0 );
    if(!S_ISREG( statb->st_mode )) {
		Diemsg(err, file, "not a regular file");
    } if(statb->st_size == 0) {
		/* empty file */
		printable = -1;
    } else if ((n = read (fd, buf, sizeof(buf))) <= 0) {
        Diemsg (err, file, "cannot read it");
    } else if (format != 'p' && format != 'f' ){
        printable = 1;
    } else if( Check_for_nonprintable == 0 ) {
        /*
         * We don't have to do the following checks, applicable to text files.
         */
        printable = 1;
    } else if (is_exec ( buf, n)) {
        Diemsg (err, file, "executable program");
    } else if (is_arch ( buf, n)) {
        Diemsg (err, file, "archive file");
    } else if (is_mmdf_mail (buf)) {
        printable = 1;
    } else {
        printable = 1;
		for (i = 0; printable && i < n; ++i) {
			c = ((unsigned char *)buf)[i];
			if( !isprint( c ) && !isspace( c ) ) printable = 0;
		}
		if( !printable ) Diemsg (err, file, "unprintable file");
    }
    return(printable);
}

/***************************************************************************
 * This following code, to put it mildly, is obnoxious.
 * There is little if any justification for this outside of generating
 * interesting error messages for the users.
 *
 * Patrick Powell Wed Apr 12 19:58:58 PDT 1995
 ***************************************************************************/

/***************************************************************************
 * Why do we want to treat MMDF files as printable?  They have funny
 * control characters in them... sigh.
 * Patrick Powell Wed Apr 12 19:58:58 PDT 1995
 ***************************************************************************/

#define MMDF_SEPARATOR "\001\001\001\001"
static char mmdf[] = { 1, 1, 1, 1 };
static int is_mmdf_mail(char *buf)
{
    return (!memcmp (buf, mmdf, sizeof(mmdf )));
}

/***************************************************************************
 * The is_exec and is_arch are system dependent functions which
 * check if a file is an executable or archive file, based on the
 * information in the header.  Note that most of the time we will end
 * up with a non-printable character in the first 100 characters,  so
 * this test is moot.
 *
 * I swear I must have been out of my mind when I put these tests in.
 * In fact,  why bother with them?
 *
 * Patrick Powell Wed Apr 12 19:58:58 PDT 1995
 ***************************************************************************/

#ifdef HAVE_A_OUT_H
#include <a.out.h>
#endif

#ifdef HAVE_EXECHDR_H
#include <sys/exechdr.h>
#endif

/* this causes trouble, eg. on SunOS. */
#ifdef IS_NEXT
#  ifdef HAVE_SYS_LOADER_H
#    include <sys/loader.h>
#  endif
#  ifdef HAVE_NLIST_H
#    include <nlist.h>
#  endif
#  ifdef HAVE_STAB_H
#    include <stab.h>
#  endif
#  ifdef HAVE_RELOC_H
#   include <reloc.h>
#  endif
#endif /* IS_NEXT */

#if defined(HAVE_FILEHDR_H) && !defined(HAVE_A_OUT_H)
#include <filehdr.h>
#endif

#if defined(HAVE_AOUTHDR_H) && !defined(HAVE_A_OUT_H)
#include <aouthdr.h>
#endif

#ifdef HAVE_SGS_H
#include <sgs.h>
#endif

/***************************************************************************
 * I really don't want to know.  This alone tempts me to rip the code out
 * Patrick Powell Wed Apr 12 19:58:58 PDT 1995
 ***************************************************************************/
#ifndef XYZZQ_
#define XYZZQ_ 1		/* ugh! antediluvian BSDism, I think */
#endif

/***************************************************************************
 * FROM: PLP 4.0- lpr_canprint.c code
 * The following comments were extracted from the above code.
 * Note carefully that there was no author attribution.
 *   Patrick Powell Wed Apr 12 19:58:58 PDT 1995
 *
 * The N_BADMAG macro isn't present on some OS'es.
 * 
 * If it isn't defined on yours, just edit your system's
 * "system/os.h" file and #undef both USE_A_OUT_H and USE_EXECHDR_H.
 * 
 * This function will allow files through if it can't
 * find a working magic-interpretation macro.
 *
 ***************************************************************************
 * ARGH, ACH ... I did not read that, surely?  Edit a system include file?
 * Just to print fancy messages?   Sigh...
 *
 * A safer way to do this is to put the following lines in the
 * portable.h file:
 * #ifdef MYSYSTEM / * MYSYTEM is whatever name you give to your system * /
 * #undef USE_A_OUT_H
 * #undef USE_EXEHDR_H
 * #endif
 *
 * Patrick Powell Wed Apr 12 20:07:38 PDT 1995
 ***************************************************************************/

#ifndef N_BADMAG
#  ifdef NMAGIC
#    define N_BADMAG(x) \
	   ((x).a_magic!=OMAGIC && (x).a_magic!=NMAGIC && (x).a_magic!=ZMAGIC)
#  else				/* no NMAGIC */
#    ifdef MAG_OVERLAY		/* AIX */
#      define N_BADMAG(x) (x.a_magic == MAG_OVERLAY)
#    endif				/* MAG_OVERLAY */
#  endif				/* NMAGIC */
#endif				/* N_BADMAG */

static int is_exec( char *buf, int n)
{
    int i = 0;

#ifdef N_BADMAG		/* BSD, non-mips Ultrix */
#  ifdef HAVE_STRUCT_EXEC
    if (n >= sizeof (struct exec)){
		i |= !(N_BADMAG ((*(struct exec *) buf)));
	}
#  else
    if (n >= sizeof (struct aouthdr)){
		i |= !(N_BADMAG ((*(struct aouthdr *) buf)));
	}
#  endif
#endif

#ifdef ISCOFF		/* SVR4, mips Ultrix */
    if (n >= sizeof (struct filehdr)){
		i |= (ISCOFF (((struct filehdr *) buf)->f_magic));
	}
#endif

#ifdef MH_MAGIC		/* NeXT */
    if (n >= sizeof (struct mach_header)){
		i |= (((struct mach_header *) buf)->magic == MH_MAGIC);
	}
#endif

#ifdef IS_DATAGEN	/* Data General (forget it! ;) */
    {
		if( n > sizeof (struct header)){
			i |= ISMAGIC (((struct header *)buff->magic_number));
		}
    }
#endif

    return (i);
}

#include <ar.h>

static int is_arch(char *buf, int n)
{
	int i = 0;
#ifdef ARMAG
	if( n >= SARMAG ){
		i = !memcmp( buf, ARMAG, SARMAG);
	}
#endif				/* ARMAG */
    return(i);
}
