/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: getqueue.c
 * PURPOSE: read the entries in the the spool directory
 **************************************************************************/

static char *const _id =
"$Id: getqueue.c,v 3.14 1997/10/27 00:14:19 papowell Exp $";

/***************************************************************************
Commentary
Patrick Powell Thu Apr 27 21:48:38 PDT 1995

The spool directory holds files and other information associated
with jobs.  Job files have names of the form cfXNNNhostname.

The Scan_queue routine will scan a spool directory, looking for new
or missing control files.  If one is found,  it will add it to
the control file list.  It will then sort the list of file names.

In order to prevent strange things with pointers,  you should not store
pointers to this list,  but use indexes instead.

 ***************************************************************************/

#include "lp.h"
#include "getqueue.h"
#include "cleantext.h"
#include "dump.h"
#include "errorcodes.h"
#include "fileopen.h"
#include "gethostinfo.h"
#include "globmatch.h"
#include "jobcontrol.h"
#include "malloclist.h"
#include "merge.h"
#include "pathname.h"
#include "permission.h"
/**** ENDINCLUDE ****/

static struct malloc_list c_xfiles;
int Fix_class_info( struct control_file *cf, char *classes );


/***************************************************************************
 * fcmp()
 * Job order comparison:
 * 1. error message-
 * 2. flags
 * 3. priority (youngest)
 * 3. modification time 
 * 4. job number
 *  - if there is a priority time,  then the youngest goes first
 ***************************************************************************/
static int fcmp( const void *l, const void *r )
{
	struct control_file *lcf = *(struct control_file **)l,
		*rcf = *(struct control_file **)r;
	int c, i;
	long rt = -1;
	long lt = -1;
	if( rcf->hold_info.priority_time ) rt = rcf->hold_info.priority_time;
	if( lcf->hold_info.priority_time ) lt = lcf->hold_info.priority_time;

	i =    (c = ((rcf->transfername[0] != 0) - (lcf->transfername[0] != 0 )))
		|| (c = ((lcf->error[0] != 0) - (rcf->error[0] != 0)))
		|| (c = (lcf->flags - rcf->flags))
		|| (c = (lcf->hold_info.remove_time - rcf->hold_info.remove_time))	/* oldest first */
		|| (c = (lcf->hold_info.done_time - rcf->hold_info.done_time))	/* oldest first */
		|| (c = (lcf->hold_info.held_class - rcf->hold_info.held_class))	/* oldest first */
		|| (c = (lcf->hold_info.hold_time - rcf->hold_info.hold_time))	/* oldest first */
		|| (c = (lcf->priority - rcf->priority))	/* lowest first */
		|| (c = - (lt - rt)) /* youngest first */
		|| (c = (lcf->statb.st_ctime - rcf->statb.st_ctime)) /* oldest first */
		|| (c = (lcf->number - rcf->number ));
	/*
	DEBUG4( "fcmp: "
		"lcf = '%s', err '%d', flags '%d', priority_time 0x%x, "
		" time 0x%x, t 0x%x, number %d",
		lcf->name,lcf->error[0],lcf->flags,lcf->hold_info.priority_time,
		lcf->statb.st_ctime,lt,lcf->number);
	DEBUG4( "fcmp: "
		"rcf = '%s', err '%d', flags '%d', priority_time 0x%x, "
		"time 0x%x, t 0x%x, number %d",
		rcf->name,rcf->error[0],rcf->flags,rcf->hold_info.priority_time,
		rcf->statb.st_ctime,rt,rcf->number);
	DEBUG4( "fcmp: result %d", c );
	/ **/
	return(c);
}

/***************************************************************************
 * Getcontrolfile( char *file, struct dpathname *dpath,
 *	int fd, struct stat *statb, int check_df, struct control_file *cf )
 *
 * 1. read the file into a buffer
 * 2. parse the control file
 *
 ***************************************************************************/

void Getcontrolfile( char *pathname, char *file, struct dpathname *dpath,
	int fd, struct stat *statb, int check_df, struct control_file *cf )
{
	int len, i;				/* ACME Integers */
	char *s;				/* ACME Pointers */

	/* copy the status */
	DEBUG4("Getcontrolfile: path '%s', file '%s' size %d, dirname '%s'",
		pathname, file, statb->st_size, dpath->pathname );

	/* free up memory for the control file */
	Clear_control_file( cf );

	cf->statb = *statb;
	if( cf->statb.st_size == 0 ){
		time_t t;
		int age;

		/* ignore zero length control files */
		/* strcpy( cf->error, "zero length control file" ); */
		/* check to see if the file is older than 1 hour */

		t = time( (time_t *)0 );
		age = t - statb->st_ctime;
		DEBUG4("Getcontrolfile: age %d", age );
		if( age > 60 ){
			plp_snprintf( cf->error, sizeof( cf->error),"empty control file %d mins old",
			age/60 );
			cf->flags |= BAD_FLAG;
			cf->flags |= OLD_FLAG;
		}
	}

	/* copy the control file name */
	strncpy( cf->openname, pathname, sizeof( cf->openname ) );
	strncpy( cf->original, file, sizeof( cf->original ) );
	strncpy( cf->transfername, file, sizeof( cf->transfername ) );

	/* read the control file into buffer */
	len = cf->statb.st_size;
	DEBUG4("Getcontrolfile: allocate control file buffer len %d", len );
	cf->cf_info = add_buffer( &cf->control_file_image, len+1 );
	for( i = 1, s = cf->cf_info;
		len > 0 && (i = read( fd, s, len )) > 0;
		len -= i, s += i );
	*s++ = 0;
	if( i < 0 ){
		plp_snprintf( cf->error, sizeof( cf->error),"cannot read '%s'", file );
		cf->flags |= BAD_FLAG;
		return;
	}

	/* parse the control file */
	if( Parse_cf( dpath, cf, check_df ) ){
		cf->flags |= BAD_FLAG;
	}
}

/***************************************************************************
 * int Parse_cf( struct dpathname *dpath, struct control_file *cf, int check_df )
 *
 *
 * dirname- pathname of the directory
 * cf -     struct control file {} address
 * check_df - open and check data files if non-zero
 *
 *  This routine will parse the control file in a buffer.
 *  It will REALLOC control file information,  putting all data
 *  into the dynamically allocated buffer.
 *
 * 
 * Returns: 0 no error, non-zero - error
 ***************************************************************************/
int Parse_cf( struct dpathname *dpath, struct control_file *cf, int check_df )
{
	char *s, *end;				/* ACME Pointers */
	char **lines;				/* pointer to lines in file */
	int datafiles;				/* number of datafiles in file */
	struct data_file *df = 0;	/* data file array */
	int i, c, err;					/* ACME Integers */
	int fd;						/* file descriptor */
	int linecount;				/* number of lines in file */
	int Nline = 0;			/* first N line in control file */

	/* get the job number */

	DEBUG4("Parse_cf: job '%s' '%s'", cf->original, cf->cf_info );
	/* set up control file checks */
	if( Check_format( CONTROL_FILE, cf->original, cf ) ){
		plp_snprintf( cf->error, sizeof( cf->error),
			"bad job filename format '%s'", cf->original );
		goto error;
	}

	DEBUG4("Parse_cf: job number '%d', filehostname '%s'",
		cf->number, cf->filehostname );

	if( check_df ){
		cf->jobsize = 0;
	}
	memset( cf->capoptions, 0, sizeof( cf->capoptions ) );
	memset( cf->digitoptions, 0, sizeof( cf->digitoptions ) );
	cf->data_file_list.count = 0;

	/* count the numbers of lines and datafile lines */
	linecount = 0;
	datafiles = 0;
	s = cf->cf_info;
	/* this is violent, but it will catch the metacharacters */
	/* now we break up the file */
	while( s && *s ){
		if( islower( s[0] ) ) ++datafiles;
		++linecount;
		if( (s = strchr( s, '\n' )) ){
			++s;
		}
	}
	++linecount; /* at end */

	/* allocate storage for line and data files */
	DEBUG4("Parse_cf: line count '%d', data files %d",
		linecount, datafiles );
	cf->control_file_lines.count = 0;
	if( linecount >= cf->control_file_lines.max ){
		extend_malloc_list( &cf->control_file_lines, sizeof( char *), linecount+1);
	}
	/* set up the data files - allow for some extras */
	cf->data_file_list.count = 0;
	if( datafiles+4 >= cf->data_file_list.max ){
		extend_malloc_list(&cf->data_file_list, sizeof(df[0]), datafiles+4 );
	}

	/* check the line for bad characters */
	lines = (char **)cf->control_file_lines.list;
	for( s = cf->cf_info; s && *s && (end = strchr( s, '\n' ));
		s = end, ++cf->control_file_lines.count ){
		*end++ = 0;
		/* check for printable chars */
		lines[ cf->control_file_lines.count ] = s;
		for( i = 0; (c = s[i]); ++i ){
			if( !isprint(c) && !isspace(c) ){
				if( Fix_bad_job ){
					s[i] = ' ';
				} else {
					plp_snprintf( cf->error, sizeof( cf->error),
						"bad job line '%s'", s );
					goto error;
				}
			}
		}

		/* now parse the data file */
		c = s[0];
		if( isupper( c ) ){
			if( c == 'N' ){
				if( df && df->Ninfo[0] == 0 ){
					safestrncpy( df->Ninfo, s );
					lines[ cf->control_file_lines.count ] = df->Ninfo;
					Nline = 0;
				} else {
					Nline = cf->control_file_lines.count;
				}
				continue;
			}
			if( c == 'U' ){
				/* see if there is a data file list */
				if( df == 0 ){
					plp_snprintf( cf->error, sizeof( cf->error),
						"'%s' Unlink before data info in '%s'",
						s, cf->transfername );
					goto error;
				}
				if( Check_format( DATA_FILE, s+1, cf ) ){
					/* vintage and broken LPR servers put in bad 'U' lines */
					DEBUG3( "Parse_cf: Bad U line '%s", s );
					if( Fix_bad_job ){
						lines[ cf->control_file_lines.count ] = 0;
						continue;
					}
					plp_snprintf( cf->error, sizeof( cf->error),
						"U (unlink) data file name format bad, line '%s' in %s",
						s, cf->transfername );
					goto error;
				}
				if( strcmp( s+1, df->original ) ){
					DEBUG3( "Parse_cf: U line does not match '%s", s );
					if( Fix_bad_job ){
						lines[ cf->control_file_lines.count ] = 0;
						continue;
					}
					plp_snprintf( cf->error, sizeof( cf->error),
						"U (unlink) does not match data file name, line '%s' in %s",
						s, cf->transfername );
					goto error;
				}
				if( df->Uinfo[0] ){
					DEBUG3( "Parse_cf: multiple U (unlink) for same data file, line '%s' in %s",
						s, cf->transfername );
					lines[ cf->control_file_lines.count ] = 0;
				} else {
					lines[ cf->control_file_lines.count ] = df->Uinfo;
					strncpy( df->Uinfo, s, sizeof( df->Uinfo ) );
				}
				continue;
			}
			if( cf->capoptions[ c - 'A' ] ){
				if( Fix_bad_job ){
					/* remove the duplicate line */
					lines[ cf->control_file_lines.count ] = 0;
					continue;
				} else {
					plp_snprintf( cf->error, sizeof( cf->error),
						"duplicate option '%s' in '%s'",
						s, cf->transfername );
					goto error;
				}
			}
			cf->capoptions[ c - 'A' ] = s;
			if( c == 'A' ){
				if( s[1] == 0 ){
					plp_snprintf( cf->error, sizeof( cf->error),
						"bad identifier line in '%s'",
						cf->transfername );
					goto error;
				}
				strncpy( cf->identifier, s, sizeof( cf->identifier ) );
				cf->capoptions[ c - 'A' ] = cf->identifier;
				lines[ cf->control_file_lines.count ] = cf->identifier;
				cf->orig_identifier = s;
			}
		} else if( isdigit( c ) ){
			if( cf->digitoptions[ c - '0' ] ){
				plp_snprintf( cf->error, sizeof( cf->error),
					"duplicate option '%s' in '%s'",
					s, cf->transfername );
				goto error;
			}
			cf->digitoptions[ c - '0' ] = s;
		} else if( islower( c ) ){
			DEBUG3("Parse_cf: format '%c' data file '%s'", c, s+1 );
			if( strchr( "aios", c )
				/* || ( Formats_allowed && !strchr( Formats_allowed, c )) */ ){
				plp_snprintf( cf->error, sizeof( cf->error),
					"illegal data file format '%c' in '%s'", c, cf->transfername );
				goto error;
			}

			/* check to see that the name has the format FdfXnnnHost*/
			if( Check_format( DATA_FILE, s+1, cf ) ){
				plp_snprintf( cf->error, sizeof( cf->error),
					"bad data file name format '%s' in %s", s+1, cf->original );
				goto error;
			}
			/* check for blank control file */
			if( cf->control_info == 0 ){
				if( cf->control_file_lines.count == 0 ){
					plp_snprintf( cf->error, sizeof( cf->error),
						"no control information before '%s' in '%s'",
						s, cf->transfername );
					goto error;
				}
				cf->control_info = cf->control_file_lines.count;
			}
			/* if you have a previous data file entry, then
			 *    adjust the line count
			 *    check for copies of the same file
			 */
			if( df ){
				df->copies = strcmp( df->transfername, s+1 ) != 0;
			}
			df = (void *)cf->data_file_list.list;
			df = &df[cf->data_file_list.count++];
			/* get the format and name of file */
			memset( (void *)df, 0, sizeof( df[0] ) );
			df->format = c;
			df->copies = 1;
			strncpy(df->transfername, s, sizeof(df->transfername) );
			strncpy(df->original, s+1, sizeof(df->original) );
			if( dpath ){
				s = Add_path( dpath, s+1 );
			} else {
				s = s+1;
			}
			strncpy(df->openname, s, sizeof(df->openname) );
			/* now you do not need to worry */
			lines[ cf->control_file_lines.count ] = df->transfername;
			if( Nline ){
				safestrncpy( df->Ninfo, lines[ Nline ] );
				lines[ Nline ] = df->Ninfo;
			}
			Nline = 0;
			/* check to see if there is a data file */
			if( Check_format( DATA_FILE, df->original, cf ) ){
				plp_snprintf( cf->error, sizeof( cf->error),
					"bad job datafile name format '%s'", df->original );
				goto error;
			}

			if( check_df ){
				DEBUG3("Parse_cf: checking data file '%s'", df->openname );
				fd = Checkread( df->openname, &df->statb );
				err = errno;
				if( fd < 0 ){
					plp_snprintf( cf->error, sizeof( cf->error),
						"cannot open '%s' - '%s'", df->openname, Errormsg(err) );
					goto error;
				}
				cf->jobsize += df->statb.st_size;
				close( fd );
			}
		} else if( c == '_' ){
			/* we have authentication information - put it
			 * into the control file
			 */
			if( cf->auth_id[0] ){
				plp_snprintf( cf->error, sizeof( cf->error),
					"duplicate authentication information '%s' - '%s'",
						cf->auth_id+1, s+1 );
				goto error;
			}
			/* copy the line to the authentication place */
			safestrncpy( cf->auth_id, s );
			/* remove line from control file line list */
			lines[ cf->control_file_lines.count ] = cf->auth_id;
		}
	}
	lines[ cf->control_file_lines.count ] = 0;

	/* check for last line not ending in New Line */
	if( s && *s ){
		plp_snprintf( cf->error, sizeof( cf->error),
			"last line missing NL '%s'", s, cf->transfername );
		goto error;
	}
	/* clean out metacharacters */
	for( i = 0; i < cf->control_file_lines.count; ++i ){
		Clean_meta( cf->control_file_lines.list[i] );
	}


	DEBUG4("Parse_cf: '%s', lines %d, datafiles %d",
		cf->transfername, cf->control_file_lines.count, datafiles );


	if( Make_identifier( cf ) ){
		goto error;
	}


	if(DEBUGL4 ){
		char buffer[LINEBUFFER];
		plp_snprintf( buffer, sizeof(buffer), "Parse_cf: %s",
			cf->transfername );
		dump_control_file( buffer, cf );
	}
	return( 0 );

error:
	DEBUG3("Parse_cf: error '%s'", cf->error );
	return( 1 );
}

/***************************************************************************
 * Extend_file_list( int count )
 * 1. Allocate a block of cnt*sizeof( struct control_file ) size
 * 2. Set up pointers to it in the C_files_list array.
 ***************************************************************************/

void Extend_file_list( int count )
{
	/* first we allocate a buffer */
	struct control_file *cfp, **cfpp;	/* control file pointers */
	int size;
	int i, max;

	DEBUG3("Extend_file_list: count %d, max %d, additional %d",
		C_files_list.count, C_files_list.max, count );
	DEBUG3("Extend_file_list: buffers count %d, max %d",
		c_xfiles.count, c_xfiles.max );
	if( count == 0 ) count = 1;
	size = sizeof( cfp[0] ) * count;
	cfpp = (void *)C_files_list.list;
	if(DEBUGL3 ){
		for(i = 0; i < C_files_list.max; ++i ){
			logDebug( "Extend_file: before [%d] = 0x%x", i, cfpp[i] );
		}
	}
	cfp = (void *)add_buffer( &c_xfiles, size );
	memset( cfp, 0, size );
	/* now we expand the list */
	max = C_files_list.max;
	DEBUG3("Extend_file_list: C_files_list.max %d", max );
	extend_malloc_list( &C_files_list, sizeof( cfpp[0] ), count );
	cfpp = (void *)C_files_list.list;
	for( i = 0; i < count ; ++i ){
		cfpp[i+max] = &cfp[i];
	}
	if(DEBUGL3 ){
		for(i = 0; i < C_files_list.max; ++i ){
			logDebug( "Extend_file: after [%d] = 0x%x", i, cfpp[i] );
		}
	}
}

/***************************************************************************
 * Scan_qeueue( char *pathname )
 * - scan the directory, checking for new or changed control files.
 * - insert them, append them
 * - check for unused entries and remove them
 * - sort the list
 ***************************************************************************/

void Scan_queue( int check_df, int new_queue )
{
	DIR *dir;						/* directory */
	int fd, i;						/* ACME integers */
	struct stat statb;				/* statb information */
	struct dirent *d;				/* directory entry */
	int found;						/* found in list */
	int free_entry;					/* free entry */
	struct control_file *cfp, **cfpp;	/* control file pointers */
	char *pathname;

	/* first we make sure we have some space allocated */
	if( new_queue ) C_files_list.count = 0;
	if( C_files_list.count >= C_files_list.max ){
		Extend_file_list( 10 );
	}
	cfpp = (void *)C_files_list.list;

	pathname = Clear_path( SDpathname );
	DEBUG3("Scan_queue: pathname '%s', count %d",
		pathname, C_files_list.count );
	for( i = 0; i < C_files_list.count; ++i ){
		/* we now set to not found */
		DEBUG3("Scan_queue: present pathname [%d] '%s'",
			i, cfpp[i]->transfername );
		cfpp[i]->found = 0;
	}

	dir = opendir( pathname );
	if( dir == 0 ){
		logerr_die( LOG_ERR, "Scan_queue: opendir '%s' failed", pathname );
	}

	/* now we read the files */

	while( (d = readdir(dir)) ){
		if( d->d_name[0] != 'c' || d->d_name[1] != 'f' ) continue;
		pathname = Add_path( SDpathname, d->d_name );
		DEBUG3("Scan_queue: entry '%s', pathname '%s'",
			d->d_name, pathname );
		fd = Checkread( pathname, &statb );
		if( fd < 0 ){
			DEBUG4("Scan_qeueue: cannot open file '%s'", pathname );
			continue;
		}
		/* search through the list of control files and see if we have it */
		found = 0;
		free_entry = -1;
		/* we only search if an old queue */
		cfp = 0;
		for( i = 0; i < C_files_list.count; ++i ){
			/* we now check the control file name */
			cfp = cfpp[i];
			DEBUG4("Scan_qeueue: checking cf name '%s' against '%s'",
				cfp->transfername, d->d_name );
			if( cfp->transfername[0] ){
				if( strcmp( cfp->transfername, d->d_name ) == 0 ){
					DEBUG4(
					"Scan_qeueue: '%s' old ctime %d, new ctime %d", cfp->transfername,
						cfp->statb.st_ctime, statb.st_ctime );

					found = 1;
					/* now we check the size and modification time */
					if( cfp->statb.st_size != statb.st_size
						|| cfp->statb.st_ctime != statb.st_ctime
						|| cfp->statb.st_dev != statb.st_dev
						|| cfp->statb.st_ino != statb.st_ino ){
						/* we need to reprocess the control file */
						Getcontrolfile( pathname, d->d_name, SDpathname, fd,
							&statb, check_df, cfp );
					}
					/* get the job control information */
					Get_job_control( cfp, 0 );
					break;
				}
			} else if( free_entry < 0 ){
				free_entry = i;
			} 
		}
		if( !found ){
			/* we need an entry */
			/* see if we can reuse the old one */
			if( free_entry < 0 ){
				if( C_files_list.count >= C_files_list.max ){
					Extend_file_list( 10 );
					cfpp = (void *)C_files_list.list;
				}
				free_entry = C_files_list.count++;
			}
			cfp = cfpp[free_entry];
			Getcontrolfile( pathname, d->d_name, SDpathname, fd,
				&statb, check_df, cfp );
			Get_job_control( cfp, 0 );
		}
		close(fd);

		/* now we fix up the class information */
		cfp->hold_info.held_class = 0;
		if( Classes ){
			cfp->hold_info.held_class = Fix_class_info( cfp, Classes );
		}
		cfp->found = 1;
	}
	closedir(dir);

	/* remove the entries which do not exist any more */

	for( i = 0; i < C_files_list.count; ++i ){
		if( cfpp[i]->found == 0 ){
			DEBUG4( "Scan_queue: not found [%d] '%s'",
				i, cfpp[i]->transfername );
			cfpp[i]->transfername[0] = 0;
		}
	}
	/* we now create the sorted list */
	if(DEBUGL3 ){
		logDebug( "Scan_queue: unsorted Jobs, count %d", C_files_list.count );
		for( i = 0; i < C_files_list.count; ++i ){
			logDebug( "[%d] 0x%x '%s'", i, cfpp[i], cfpp[i]->transfername );
		}
	}
	if( Mergesort( cfpp, C_files_list.count, sizeof( cfp ), fcmp ) ){
		fatal( LOG_ERR, "Scan_queue: Mergesort failed" );
	}
	for( i = 0; i < C_files_list.count; ++i ){
		if( cfpp[i]->transfername[0] == 0 ) break;
	}
	C_files_list.count = i;
	if(DEBUGL3 ){
		logDebug( "Scan_queue: sorted Jobs, count %d", C_files_list.count );
		for( i = 0; i < C_files_list.count; ++i ){
			char buffer[LINEBUFFER];
			logDebug( "[%d] 0x%x '%s'", i, cfpp[i], cfpp[i]->transfername );
			plp_snprintf( buffer,sizeof(buffer),"Scan_queue: %s",
				cfpp[i]->transfername );
			dump_control_file( buffer, cfpp[i] );
		}
	}
}

/***************************************************************************
 * Fix_class_info( control file *, char * )
 *
 * Decide which class to let through
 *  - we use the permissions matching algorithm
 *  - class list is a list of permissions
 *  - we match this to one of the control file lines
 *
 ***************************************************************************/
int Fix_class_info( struct control_file *cf, char *classes )
{
	int result = 0;
	char *entry;
	char *s, *end;
	char line[LINEBUFFER];

	if( classes && *classes ){
		result = 1;
		DEBUG3("Fix_class_info: class '%s'", classes );
		safestrncpy( line, classes );
		entry = cf->CLASSNAME;
		s = line;
		if( isupper(s[0]) && s[1] == '=' ){
			entry = cf->capoptions[ s[0] - 'A'];
			s += 2;
		}
		if( entry ) ++entry;
		for( ; result && s && *s; s = end ){
			end = strpbrk( s, ":,; \t" );
			if( end ){
				*end++ = 0;
			}
			while( isspace( *s ) ) ++s;
			if( *s == 0 ) continue;
			result = Globmatch( s, entry );
		}
	}
	return( result != 0);
}

/***************************************************************************
 * Job_count( int *held_files, int *printable jobs )
 *  Report the job statistics
 ***************************************************************************/
void Job_count( int *hc, int *cnt )
{
	struct control_file *cfp, **cfpp;	/* pointer to control file */
	int i, count, hold_count;

	count = 0; hold_count = 0;
	cfpp = (void *)C_files_list.list;
	for( i = 0; i < C_files_list.count; ++i ){
		cfp = cfpp[i];
		if( cfp->hold_info.hold_time ){
			++hold_count;
			continue;
		}
		if( cfp->hold_info.not_printable ) continue;
		++count;
	}
	if( hc ) *hc = hold_count;
	if( cnt ) *cnt = count;
}
