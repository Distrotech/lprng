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
"getqueue.c,v 3.16 1998/03/29 18:32:49 papowell Exp";

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
#include "decodestatus.h"
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
	int c, i, pr, pl;
	long rt = -1;
	long lt = -1;
	if( rcf->hold_info.priority_time ) rt = rcf->hold_info.priority_time;
	if( lcf->hold_info.priority_time ) lt = lcf->hold_info.priority_time;
	pr = rcf->priority; pl = lcf->priority;
	if( Ignore_requested_user_priority ) pr = pl;

	i =    (c = ((rcf->hold_info.redirect[0] != 0) - (lcf->hold_info.redirect[0] != 0 )))
		|| (c = ((lcf->error[0] != 0) - (rcf->error[0] != 0)))
		|| (c = (lcf->flags - rcf->flags))
		|| (c = (lcf->hold_info.remove_time - rcf->hold_info.remove_time))	/* oldest first */
		|| (c = (lcf->hold_info.done_time - rcf->hold_info.done_time))	/* oldest first */
		|| (c = (lcf->hold_info.held_class - rcf->hold_info.held_class))	/* oldest first */
		|| (c = (lcf->hold_info.hold_time - rcf->hold_info.hold_time))	/* oldest first */
		|| (c = (pl - pr))	/* lowest first */
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
	cf->cf_info = add_buffer( &cf->control_file_image, len+1,__FILE__,__LINE__ );
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

static struct malloc_list Lclines;

static int Add_line_to_control_file( char *s, struct control_file *cf )
{
	int i, c;
	if( *s == 0 )return(0);
	for( i = 0; (c = s[i]); ++i ){
		if( (!isprint(c) && !isspace(c)) || Is_meta(c) ){
			if( Fix_bad_job ){
				s[i] = '_';
			} else {
				plp_snprintf( cf->error, sizeof( cf->error),
					"bad job line '%s'", s );
				return(1);
			}
		}
		if( isspace(c) ) s[i] = ' ';
	}
	c = s[0];
	if( c == 'U' || c == 'N' || islower(c) ){
		if(Lclines.count+1 >= Lclines.max ){
			extend_malloc_list( &Lclines, sizeof( char *), 10,__FILE__,__LINE__ );
		}
		DEBUG4("Add_line_to_control_file: Lclines[%d] '%s'",Lclines.count, s );
		Lclines.list[Lclines.count++] = s;
		Lclines.list[Lclines.count] = 0;
	} else {
		if( cf->control_file_lines.count+1 >= cf->control_file_lines.max ){
			extend_malloc_list( &cf->control_file_lines, sizeof( char *), 10,__FILE__,__LINE__ );
		}
		DEBUG4("Add_line_to_control_file: control[%d] '%s'",cf->control_file_lines.count, s );
		cf->control_file_lines.list[cf->control_file_lines.count++] = s;
		cf->control_file_lines.list[cf->control_file_lines.count] = 0;
	}
	return(0);
}

int Parse_cf( struct dpathname *dpath, struct control_file *cf, int check_df )
{
	char *s, *end;				/* ACME Pointers */
	char **lines;				/* pointer to lines in file */
	int datafiles;				/* number of datafiles in file */
	struct data_file *df = 0;	/* data file array */
	int i, c, err;					/* ACME Integers */
	int fd;						/* file descriptor */
	/* get the job number */
	DEBUG4("Parse_cf: job '%s' '%s'", cf->original, cf->cf_info );
	/* set up control file checks */
	if( Check_format( CONTROL_FILE, cf->original, cf ) ){
		char buffer[LINEBUFFER];
		safestrncpy(buffer,cf->error);
		plp_snprintf( cf->error, sizeof(cf->error),
			_("file '%s' name problems - %s"),
				cf->original, buffer );
		goto error;
	}

	DEBUG4("Parse_cf: job number '%d', filehostname '%s'",
		cf->number, cf->filehostname );

	Lclines.count = 0;
	cf->control_file_lines.count = 0;
	cf->error[0] = 0;
	memset(cf->capoptions,0,sizeof(cf->capoptions));
	memset(cf->digitoptions,0,sizeof(cf->digitoptions));

	/* split up the file into lines */
	for( s = cf->cf_info; s && *s && (end = strchr( s, '\n' )); s = end ){
		if( end ) *end++ = 0;
		if( Add_line_to_control_file( s, cf ) ) goto error;
	}
	if( s && *s ){
		/* last line did not have \n */
		if( Add_line_to_control_file( s, cf ) ) goto error;
	}

	/* allocate storage for line and data files */
	DEBUG4("Parse_cf: line count '%d', data files lines %d",
		cf->control_file_lines.count, Lclines.count );
	extend_malloc_list( &cf->control_file_lines, sizeof( char *), Lclines.count+1,__FILE__,__LINE__ );
	if(DEBUGL3){
		lines = cf->control_file_lines.list;
		logDebug("Parse_cf: control %d", cf->control_file_lines.count );
		for(i = 0; i < cf->control_file_lines.count; ++i ){
			logDebug("  [%d] '%s'", i , lines[i] );
		}
		lines = Lclines.list;
		logDebug("Parse_cf: data %d", Lclines.count );
		for(i = 0; i < Lclines.count; ++i ){
			logDebug("  [%d] '%s'", i , lines[i] );
		}
	}

	/* rescan the control file, parsing values */
	lines = cf->control_file_lines.list;
	for( i = 0; i < cf->control_file_lines.count; ++i ){
		/* now parse the data file */
		s = lines[i];
		c = s[0];
		if( isupper( c ) ){
			if( cf->capoptions[ c - 'A' ] && !Fix_bad_job ){
				plp_snprintf( cf->error, sizeof( cf->error),
					"duplicate option '%s' in '%s'",
					s, cf->transfername );
				goto error;
			}
			DEBUG4("Parse_cf: cap option '%c'='%s'",c,s);
			cf->capoptions[ c - 'A' ] = s;
			if( c == 'A' ){
				if( s[1] == 0 ){
					plp_snprintf( cf->error, sizeof( cf->error),
						"bad identifier line in '%s'",
						cf->transfername );
					goto error;
				}
				cf->orig_identifier = s;
				safestrncpy( cf->identifier, s);
				cf->capoptions[ c - 'A' ] = cf->identifier;
				lines[i] = cf->identifier;
			}
		} else if( isdigit( c ) ){
			if( cf->digitoptions[ c - '0' ] && !Fix_bad_job ){
				plp_snprintf( cf->error, sizeof( cf->error),
					"duplicate option '%s' in '%s'",
					s, cf->transfername );
				goto error;
			}
			cf->digitoptions[ c - '0' ] = s;
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

	cf->control_info = cf->control_file_lines.count;
	
	/* set up the data files */
	cf->data_file_list.count = 0;
	if( Lclines.count >= cf->data_file_list.max ){
		extend_malloc_list(&cf->data_file_list, sizeof(df[0]),
			Lclines.count+1-cf->data_file_list.count,__FILE__,__LINE__  );
	}
	memset(cf->data_file_list.list, 0, sizeof(df[0])*cf->data_file_list.max );

	/* rescan the data files lines, parsing values */
	df = (void *)cf->data_file_list.list;
	datafiles = 0;
	lines = Lclines.list;
	for( i = 0; i < Lclines.count; ++i ){
		s = lines[i];
		c = *s;
		DEBUG3("Parse_cf: data '%c' '%s'", c, s );
		if( c == 'N' ){
			if( datafiles && df[datafiles-1].Ninfo[0] == 0 ){
				DEBUG3("Parse_cf: Ninfo[%d] '%s'",datafiles-1, s );
				safestrncpy( df[datafiles-1].Ninfo, s );
			} else {
				DEBUG3("Parse_cf: Ninfo[%d] '%s'",datafiles, s );
				safestrncpy( df[datafiles].Ninfo, s );
			}
		} else if( c == 'U' ){
			if( datafiles && df[datafiles-1].Uinfo[0] == 0 ){
				DEBUG3("Parse_cf: Uinfo[%d] '%s'", datafiles-1,s);
				safestrncpy( df[datafiles-1].Uinfo, s);
			} else {
				DEBUG3("Parse_cf: Uinfo[%d] '%s'", datafiles,s );
				safestrncpy( df[datafiles].Uinfo, s);
			}
			if( Check_format( DATA_FILE, s+1, cf ) ){
				char buffer[LINEBUFFER];
				safestrncpy(buffer,cf->error);
				plp_snprintf( cf->error, sizeof( cf->error),
					"bad data file name format '%s' in %s - %s",
					s+1, cf->original, cf->error );
				goto error;
			}
		} else if( islower( c ) ){
			DEBUG3("Parse_cf: format '%c' data file '%s'", c, s+1 );
			safestrncpy( df[datafiles].cfline, s );
			if( strchr( "aios", c )
				/* || ( Formats_allowed && !strchr( Formats_allowed, c )) */ ){
				plp_snprintf( cf->error, sizeof( cf->error),
					"illegal data file format '%c' in '%s'", c, cf->transfername );
				goto error;
			}
			/* check to see that the name has the format FdfXnnnHost*/
			if( Check_format( DATA_FILE, s+1, cf ) ){
				char buffer[LINEBUFFER];
				safestrncpy(buffer,cf->error);
				plp_snprintf( cf->error, sizeof( cf->error),
					"bad data file name format '%s' in %s - %s",
					s+1, cf->original, cf->error );
				goto error;
			}
			/* if you have a previous data file entry, then
			 *    adjust the line count
			 *    check for copies of the same file
			 */
			df[datafiles].format = c;
			strncpy(df[datafiles].cfline, s, sizeof(df[datafiles].cfline) );
			strncpy(df[datafiles].original,s+1,sizeof(df[datafiles].original) );
			if( dpath ){
				s = Add_path( dpath, s+1 );
			} else {
				s = s+1;
			}
			strncpy(df[datafiles].openname,s,sizeof(df[datafiles].openname));
			if( check_df ){
				DEBUG3("Parse_cf: checking data file '%s'", df[datafiles].openname );
				fd = Checkread( df[datafiles].openname, &df[datafiles].statb );
				err = errno;
				if( fd < 0 ){
					plp_snprintf( cf->error, sizeof( cf->error),
						"cannot open '%s' - '%s'", df[datafiles].openname, Errormsg(err) );
					goto error;
				}
				cf->jobsize += df[datafiles].statb.st_size;
				close( fd );
			}
			++datafiles;
		}
	}
	cf->data_file_list.count = datafiles;

	/* indicate which entry a job is a copy of.  Note that we assume
	 * that the LAST entry in the file is used to record information.
	 */
	for( i = datafiles-1; i > 0; --i ){
		int j;
		if( !df[i].is_a_copy ){
			s = df[i].cfline+1;
			for( j = i-1; j >= 0; --j ){
				if( !strcmp( df[j].cfline+1, s ) ){
					df[j].is_a_copy = i;
				}
			}
		}
	}

	if(DEBUGL3){
		logDebug( "Parse_cf: data file count %d", datafiles);
		for( i = 0; i < datafiles; ++i ){
			logDebug( "  [%d] file '%s'", i, df[i].cfline );
			logDebug( "  [%d] N '%s'", i, df[i].Ninfo );
			logDebug( "  [%d] U '%s'", i, df[i].Uinfo );
			logDebug( "  [%d] is_a_copy '%d'", i, df[i].is_a_copy );
		}
	}
	
	/* we need to fix up the control file with the right information.
	 * parsing will simply restore it in a fairly simple manner
	 */
	for( i = 0;  i < datafiles; ++i ){
		if( df[i].Ninfo[0] ){
			cf->control_file_lines.list[cf->control_file_lines.count++] = df[i].Ninfo;
		}
		cf->control_file_lines.list[cf->control_file_lines.count++] = df[i].cfline;
		if( df[i].Uinfo[0] ){
			cf->control_file_lines.list[cf->control_file_lines.count++] = df[i].Uinfo;
		}
	}
	cf->control_file_lines.list[cf->control_file_lines.count] = 0;

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
	cfp = (void *)add_buffer( &c_xfiles, size,__FILE__,__LINE__  );
	/* now we expand the list */
	max = C_files_list.max;
	DEBUG3("Extend_file_list: C_files_list.max %d", max );
	extend_malloc_list( &C_files_list, sizeof( cfpp[0] ), count,__FILE__,__LINE__  );
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

int Fix_data_file_info( struct control_file *cfp )
{
	int i, j, c, status;
	char **lines;
	struct data_file *jobfile;
	int jobfilecount;

	jobfile = (void *)cfp->data_file_list.list;
	jobfilecount = cfp->data_file_list.count;

	status = 0;
	/* reformat job names if they are totally screwed up */
	if( cfp->name_format_error ){
		/* we will brutally remake all of the names of the files */
		/* first, we deal with the control file */
		safestrncpy(cfp->filehostname, cfp->FROMHOST+1);
		cfp->name_format_error = 0;
	}

	plp_snprintf( cfp->transfername, sizeof(cfp->transfername),
		"cf%c%0*d%s",cfp->priority,
		cfp->number_len, cfp->number,cfp->filehostname);
	DEBUGF(DRECV3)("Fix_data_file_info: control file '%s'",
		cfp->transfername );

	c = 'A';
	
	cfp->number_of_unique_data_files = 0;
	for( i = 0; i < jobfilecount; ++i ){
		if(!jobfile[i].is_a_copy){
			++cfp->number_of_unique_data_files;
			jobfile[i].cfline[3] = c;
			plp_snprintf( jobfile[i].cfline+1,
				sizeof(jobfile[i].cfline)-1,
				"df%c%0*d%s",c,cfp->number_len,cfp->number,cfp->filehostname );
			plp_snprintf( jobfile[i].Uinfo, sizeof(jobfile[i].Uinfo),
				"U%s", jobfile[i].cfline+1 );
			if( c == 'z' ){
				plp_snprintf(cfp->error,sizeof(cfp->error),
						"too many data files");
				status = JFAIL;
				goto error;
			} else if( c == 'Z' ){
				c = 'a';
			} else {
				++c;
			}
		}
	}
	/* now we make sure that the job information is propagated to
	 * the right places */
	for( i = jobfilecount; i >= 0 ; --i ){
		if( (j = jobfile[i].is_a_copy) ){
			safestrncpy( jobfile[i].cfline, jobfile[j].cfline );
			jobfile[i].Uinfo[0] = 0;
		}
	}
	/*
	 * reformat the control file
	 */

	i = cfp->control_info;
	for( j = 0; j < jobfilecount; ++j ){
		if( jobfile[j].Uinfo[0] ) ++i;
		i += 1; /* cfline */
		if( jobfile[j].Ninfo[0] ) ++i;
	}
	/* find out how many extra lines you need */
	i =  i + 1 - cfp->control_file_lines.max;
	if( i > 0 ){
		extend_malloc_list( &cfp->control_file_lines,
			sizeof( char *), i,__FILE__,__LINE__  );
	}

	/* put the lines in */
	i = cfp->control_info;
	lines = (char **)cfp->control_file_lines.list;
	for( j = 0; j < jobfilecount; ++j ){
		lines[i++] = jobfile[j].cfline;
		if( jobfile[j].Ninfo[0] ) lines[i++] = jobfile[j].Ninfo;
		if( jobfile[j].Uinfo[0] ) lines[i++] = jobfile[j].Uinfo;
	}
	lines[i] = 0;
	cfp->control_file_lines.count = i;

error:
	return( status );
}

struct destination *Destination_cfp( struct control_file *cfp, int i )
{
	struct destination *dest = 0;
	if( i > 0 && i <= cfp->destination_list.count ){
		dest = (void *)cfp->destination_list.list;
		dest = &dest[i-1];
	}
	return( dest );
}

/***************************************************************************
 * Check_printable()
 * Check to see if the job is printable
 ***************************************************************************/

int Check_printable( struct control_file *cfp, struct destination *dest,
	char *buffer, int len )
{
	int permission, not_printable;
	struct perm_check perm_check;
	char *s;

	DEBUG0("Check_printable: '%s'", cfp->transfername );
	memset( &perm_check, 0, sizeof(perm_check) );
	buffer[0] = 0;
	not_printable = 0;
	/* check to see if it is being held or not printed */
	/* cfp->hold_info.not_printable = 0; */
	if( stat(cfp->transfername, &cfp->statb ) == -1 ){
		plp_snprintf( buffer, len, _("job removed from queue") );
	} else if( cfp->statb.st_size == 0 ){
		plp_snprintf( buffer, len, _("zero length control file") );
	} else if( cfp->flags ){
		plp_snprintf( buffer, len, _("flag %d"), cfp->flags );
	} else if( cfp->error[0] ){
		plp_snprintf( buffer, len, _("error '%s'"), cfp->error );
	} else if( cfp->hold_info.server > 0 ){
		plp_snprintf( buffer, len, _("hold_info.server %d"), cfp->hold_info.server );
	} else if( cfp->hold_info.hold_time ){
		plp_snprintf( buffer, len, _("hold_info.hold_time %d"), cfp->hold_info.hold_time );
	} else if( cfp->hold_info.remove_time ){
		plp_snprintf( buffer, len, _("hold_info.remove_time %d"), cfp->hold_info.remove_time );
	} else if( cfp->hold_info.done_time ){
		plp_snprintf( buffer, len, _("hold_info.done_time %d"), cfp->hold_info.done_time );
	} else if( cfp->hold_info.held_class ){
		plp_snprintf( buffer, len, "hold_info.held_class %d",
			cfp->hold_info.held_class );
	} else if( dest ){
		if( dest->error[0] ){
			plp_snprintf( buffer, len, _("dest error '%s'"), dest->error );
		} else if( dest->subserver >0 ){
			plp_snprintf( buffer, len, _("dest subserver '%d'"), dest->subserver );
		}
	}
	if( buffer[0] ) not_printable = JIGNORE;
	if( not_printable == 0 ){
		/*
		 * check to see if you have permissions to print/process
		 * the job
		 */
		memset( &perm_check, 0, sizeof( perm_check ) );
		/* use the P or print code */
		perm_check.service = 'P';
		perm_check.printer = Printer;
		if( cfp->LOGNAME && cfp->LOGNAME[1] ){
			perm_check.user = cfp->LOGNAME+1;
			perm_check.remoteuser = perm_check.user;
		}

		s = 0;
		if( cfp->FROMHOST && cfp->FROMHOST[1] ){
			s = Find_fqdn( &PermcheckHostIP, &cfp->FROMHOST[1], 0 );
			DEBUG0("Check_printable: looking for '%s', found '%s'",
				&cfp->FROMHOST[1], s );
		} else if( cfp->filehostname[0] ){
			s = Find_fqdn( &PermcheckHostIP, cfp->filehostname, 0 );
			DEBUG0("Check_printable: looking for '%s', found '%s'",
				cfp->filehostname, s );
		}
		if( s ){
			perm_check.host = &PermcheckHostIP;
			perm_check.remotehost = &PermcheckHostIP;
		}

		Init_perms_check();
		if( (permission = Perms_check( &Perm_file, &perm_check, cfp )) == REJECT
			|| (permission == 0
				&& (permission = Perms_check(
						&Local_perm_file, &perm_check, cfp )) == REJECT)
			|| (permission == 0 && Last_default_perm == REJECT) ){
			plp_snprintf( buffer, len,
				_("no permission to print job %s"), cfp->identifier+1 );
			/* cfp->hold_info.not_printable = */
			not_printable = JREMOVE;
		}
	}
	/*
	if( dest == 0 ){
		cfp->hold_info.not_printable = not_printable;
	} else {
		dest->not_printable = not_printable;
	}
	*/
	DEBUG0("Check_printable: job '%s', dest '%s', status '%s' reason '%s'",
		cfp->transfername, dest?dest->destination:"",
			Server_status(not_printable), buffer );
	return( not_printable );
}

/***************************************************************************
 * int = JobPrintableStatus( cfp, dest )
 *   Return the job printable status, setting the dest as a
 *   side effect
 ***************************************************************************/

int Job_printable_status( struct control_file *cfp, struct destination **dp,
	char *msg, int len)
{
	struct destination *dpp, *destination;	/* current destination */
	int j, status, can_remove;

	DEBUG0("Job_printable_status: checking '%s'%s, for printable",
		cfp->transfername, (cfp->hold_info.routed_time ? " (routed)" : "") );

	*dp = 0;
	status = 0;
	can_remove = 1;
	dpp = (void *)cfp->destination_list.list;
	destination = 0;

	if( cfp->hold_info.routed_time == 0 ){
		/* first we do non-routed jobs */
		status = Check_printable(cfp, 0, msg, len );
		DEBUG0("Job_printable_status: check printable '%s', result %s, '%s'",
			cfp->transfername, Server_status(status), msg );
	}  else {
		/* now we check for routed jobs */
		/* check to see if anything left */
		for( j = 0;
			destination == 0 && j < cfp->destination_list.count;
			++j ){
			destination = &dpp[j];
			/* status will be JREMOVE or JIGNORE */
			status = Check_printable( cfp, destination, msg, len );
			DEBUG0(
				"Job_printable_status: routed check printable '%s', status %s",
				   cfp->transfername, Server_status(status) );
			if( status == 0 ){
				if( destination->done_time == 0 && destination->copies
					&& destination->copy_done >= destination->copies ){
					destination->done_time = time( (void *) 0 );
				}
				if( destination->done_time ){
					destination = 0;
				}
			} else if( status == JREMOVE ){
				/* the job is unprintable */
				DEBUG0( "Job_printable_status: routed job unprintable '%s'",
					cfp->transfername );
				destination = 0;
				break;
			} else {
				/* we ignore this destination - simply not printable */
				DEBUG0("Job_printable_status: ignoring destination '%s'",
					destination->destination );
				destination = 0;
				can_remove = 0;
			}
		}
		/* if no destination, then we may be done or have an error */
		if( destination == 0 ){
			if( can_remove ){
				if( cfp->orig_identifier[0] ){
					strncpy( cfp->identifier, cfp->orig_identifier,
						sizeof(cfp->identifier) );
				}
				DEBUG0( "Job_printable_status: job finished '%s' ID now '%s'",
					cfp->transfername, cfp->identifier );
				cfp->hold_info.done_time = time( (void *)0 );
				status = JSUCC;
			} else {
				status = JIGNORE;
			}
		}
	}
	*dp = destination;
	return( status );
}
