/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lpr_copyfiles.c
 * PURPOSE: check the input files and make a copy if STDIN is specified
 **************************************************************************/

static char *const _id =
"$Id: lpr_cpyfiles.c,v 3.12 1997/10/27 00:14:19 papowell Exp $";

#include "lp.h"
#include "errorcodes.h"
#include "fileopen.h"
#include "killchild.h"
#include "malloclist.h"
#include "pathname.h"
/**** ENDINCLUDE ****/

int Check_lpr_printable(char *file, int fd, struct stat *statb, int format );
/***************************************************************************
 * off_t Copy_stdin()
 * 1. we get the name of a temporary file
 * 2. we open the temporary file and read from STDIN until we get
 *    no more.
 * 3. stat the  temporary file to prevent games
 ***************************************************************************/

static int did_stdin;

off_t Copy_stdin( struct control_file *cfp )
{
	int fd, count;
	off_t size = 0;
	int i, c;			/* The all-seeing i */
	struct data_file *df, *dfp;
	char buffer[LARGEBUFFER];
	char *str;


	/* we get the temporary directory for these operations */
	DEBUG2( "Copy_stdin: file count %d", cfp->data_file_list.count );
	if( did_stdin ){
		Diemsg( _("You have already specified STDIN in the job list") );
	}
	did_stdin = 1;

	/* OK, now we can set up the information */
	i = 3+Copies; /* overestimate just incase, but 1 less than other places */
	if( cfp->data_file_list.count+ i > cfp->data_file_list.max ){
		extend_malloc_list( &cfp->data_file_list, sizeof( df[0] ), i );
	}
	dfp = (void *)cfp->data_file_list.list;
	df = &dfp[cfp->data_file_list.count];
	if( cfp->data_file_list.count < 26 ){
		c = 'A'+cfp->data_file_list.count;
	} else {
		c = 'a'+cfp->data_file_list.count-26;
	}
	for( i = 0; i < Copies; ++i ){
		df = &dfp[cfp->data_file_list.count++];
		memset( df, 0, sizeof(df[0]) );
		/* each data file entry is:
           FdfNNNhost\n    (namelen+1)
           UdfNNNhost\n    (namelen+1)
		   Nfile\n         (strlen(datafile->file)+2
		 */
		df->format = *Format;
		plp_snprintf( df->transfername, sizeof(df->transfername),
			"%cdf%c%0*d%s", *Format, c, cfp->number_len, cfp->number, FQDNHost );
		str = Add_job_line( cfp, df->transfername, 1 );
		DEBUG3("Make_job:line [%d] '%s'", cfp->control_file_lines.count, str );
		plp_snprintf( df->Ninfo, sizeof( df->Ninfo), "N%s", "(stdin)" );
		str = Add_job_line( cfp, df->Ninfo, 1 );
		DEBUG3("Make_job:line [%d] '%s'", cfp->control_file_lines.count, str );
	}

	/* we simply set up the filter and return */
	df->copies = Copies;
	safestrncpy( df->Uinfo, df->transfername );
	df->Uinfo[0] = 'U';
	str = Add_job_line( cfp, df->Uinfo, 1 );
	DEBUG3("Make_job:line [%d] '%s'", cfp->control_file_lines.count, str );

	if( Secure != 0 ){
		df->d_flags |= PIPE_FLAG;
		df->fd = dup( 0 );
		size = -1;
		DEBUG2( "Copy_stdin: setting up pipe" );
		return( size );
	}

	fd = Make_temp_fd( buffer, sizeof(buffer) );

	if( fd < 0 ){
		logerr_die( LOG_INFO, _("Make_temp_fd failed") );
	} else if( fd == 0 ){
		Diemsg( _("You have closed STDIN! cannot pipe from a closed connection"));
	}
	safestrncpy( df->openname, buffer );
	DEBUG1("Temporary file '%s', fd %d", df->openname, fd );
	/* now we copy standard input into the file until we get EOF */
	size = 0;
	while( (count = read( 0, buffer, sizeof(buffer))) > 0 ){
		if( Write_fd_len( fd, buffer, count ) < 0 ){
			Errorcode = JABORT;
			logerr_die( LOG_INFO, _("Copy_stdin: write to temp file failed"));
		}
		size += count;
	}
	if( fstat( fd, &df->statb ) < 0 ){
		logerr_die( LOG_INFO, _("Copy_stdin: fstat '%s' failed"), df->openname);
	}
	if( !Check_lpr_printable(df->Ninfo+1, fd, &df[0].statb, *Format)){
		return( 0 );
	}
	close(fd);
	DEBUG3( "Copy_Stdin: Tempfile '%s' size %d, copies %d", Tempfile, size, Copies );
	return( size*Copies );
}

/***************************************************************************
 * off_t Check_files( char **files, int filecount )
 * 1. malloc the space for struct data_file{} array
 * 2. check each of the input files for access
 * 3. stat the files and get the size
 * 4. Check for printability
 * 5. Put information in the data_file{} entry
 ***************************************************************************/

off_t Check_files( struct control_file *cfp, char **files, int filecount )
{
	off_t size = 0;
	int i, j, c, fd, printable;
	struct data_file *df, *dfp;
	struct stat statb;
	int err;
	char *str;

	if( filecount == 0 ){
		return( size );
	}

	/* preallocate enough space so that things are not moved around by realloc */
	if( Copies == 0 ) Copies = 1;
	i = filecount*Copies + 4;	/* overestimate just in case */
	if( filecount > 0 && cfp->data_file_list.count+i > cfp->data_file_list.max ){
		extend_malloc_list( &cfp->data_file_list, sizeof( df[0] ), i );
	}

	/* OK, now we can set up the information */
	for( i = 0; i < filecount; ++i){
		DEBUG2( "Check_files: doing '%s'", files[i] );
		if( strcmp( files[i], "-" ) == 0 ){
			Copy_stdin( cfp );		
			continue;
		}
		fd = Checkread( files[i], &statb );
		err = errno;
		if( fd < 0 ){
			Warnmsg( _("Cannot open file '%s', %s"), files[i], Errormsg( err ) );
			continue;
		}
		printable = Check_lpr_printable( files[i], fd, &statb, *Format );
		close( fd );
		if( printable > 0 ){
			DEBUG3( "Check_files: printing '%s'", files[i] );
			if( cfp->data_file_list.count+Copies >= cfp->data_file_list.max ){
				Diemsg(_("Check_files: you did not allocate enough space for files"));
			}
			size += statb.st_size*Copies;
			dfp = (void *)cfp->data_file_list.list;
			df = &dfp[cfp->data_file_list.count];
			if( cfp->data_file_list.count < 26 ){
				c = 'A'+cfp->data_file_list.count;
			} else {
				c = 'a'+cfp->data_file_list.count-26;
			}
			for( j = 0; j < Copies; ++j ){
				df = &dfp[cfp->data_file_list.count++];
				memset( df, 0, sizeof(df[0]) );
				/* each data file entry is:
				   FdfNNNhost\n    (namelen+1)
				   UdfNNNhost\n    (namelen+1)
				   Nfile\n         (strlen(datafile->file)+2
				 */
				df->format = *Format;
				plp_snprintf( df->transfername, sizeof(df->transfername),
					"%cdf%c%0*d%s", *Format, c, cfp->number_len, cfp->number, FQDNHost );
				str = Add_job_line( cfp, df->transfername, 1 );
				DEBUG3("Make_job:line [%d] '%s'", cfp->control_file_lines.count, str );
				df->statb = statb;
				strncpy( df->openname,files[i],sizeof(df->openname));
				plp_snprintf( df->Ninfo, sizeof( df->Ninfo), "N%s", files[i] );
				str = Add_job_line( cfp, df->Ninfo, 1 );
				DEBUG3("Make_job:line [%d] '%s'", cfp->control_file_lines.count, str );
			}
			df->copies = Copies;
			safestrncpy( df->Uinfo, df->transfername );
			df->Uinfo[0] = 'U';
			str = Add_job_line( cfp, df->Uinfo, 1 );
			DEBUG3("Make_job:line [%d] '%s'", cfp->control_file_lines.count, str );
		}
	}
	DEBUG3( "Check_files: size %d", size );
	return( size );
}

/***************************************************************************
 * int Check_lpr_printable(char *file, int fd, struct stat *statb, int format )
 * 1. Check to make sure it is a regular file.
 * 2. Check to make sure that it is not 'binary data' file
 * 3. If a text file,  check to see if it has some control characters
 *
 ***************************************************************************/

static int is_arch(char *buf, int n);
static int is_exec( char *buf, int n);

int Check_lpr_printable(char *file, int fd, struct stat *statb, int format )
{
    char buf[LINEBUFFER];
    int n, i, c;                /* Acme Integers, Inc. */
    int printable = 0;
	char *err = _("cannot print '%s': %s");

	/*
	 * Do an LSEEK on the file, i.e.- see to the start
	 * Ignore any error return
	 */
	lseek( fd, 0, 0 );
    if(!S_ISREG( statb->st_mode )) {
		Diemsg(err, file, _("not a regular file"));
    } if(statb->st_size == 0) {
		/* empty file */
		printable = -1;
    } else if ((n = read (fd, buf, sizeof(buf))) <= 0) {
        Diemsg (err, file, _("cannot read it"));
    } else if (format != 'p' && format != 'f' ){
        printable = 1;
    } else if( Check_for_nonprintable == 0 ) {
        /*
         * We don't have to do the following checks, applicable to text files.
         */
        printable = 1;
    } else if (is_exec ( buf, n)) {
        Diemsg (err, file, _("executable program"));
    } else if (is_arch ( buf, n)) {
        Diemsg (err, file, _("archive file"));
    } else {
        printable = 1;
		for (i = 0; printable && i < n; ++i) {
			c = ((unsigned char *)buf)[i];
			/* we allow backspace */
			if( !isprint( c ) && !isspace( c )
				&& c != 0x08 && c != 0x1B ) printable = 0;
		}
		if( !printable ) Diemsg (err, file, _("unprintable file"));
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
