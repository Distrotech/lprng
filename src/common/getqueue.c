/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: getqueue.c
 * PURPOSE: read the entries in the the spool directory
 **************************************************************************/

static char *const _id =
"$Id: getqueue.c,v 3.4 1996/08/31 21:11:58 papowell Exp papowell $";

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
#include "printcap.h"
#include "jobcontrol.h"
#include "globmatch.h"

static struct malloc_list c_files;
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
	if( rcf->priority_time ) rt = rcf->priority_time;
	if( lcf->priority_time ) lt = lcf->priority_time;

	i = ((c = (lcf->error[0] - rcf->error[0]))
		|| (c = (lcf->flags - rcf->flags))
		|| (c = (lcf->remove_time - rcf->remove_time))	/* oldest first */
		|| (c = (lcf->move_time - rcf->move_time))	/* oldest first */
		|| (c = (lcf->held_class - rcf->held_class))	/* oldest first */
		|| (c = (lcf->hold_time - rcf->hold_time))	/* oldest first */
		|| (c = - (lt - rt)) /* youngest first */
		|| (c = (lcf->statb.st_ctime - rcf->statb.st_ctime)) /* oldest first */
		|| (c = (lcf->number - rcf->number )));
	/*
	DEBUG9( "fcmp: "
		"lcf = '%s', err '%d', flags '%d', priority_time 0x%x, "
		" time 0x%x, t 0x%x, number %d",
		lcf->name,lcf->error[0],lcf->flags,lcf->priority_time,
		lcf->statb.st_ctime,lt,lcf->number);
	DEBUG9( "fcmp: "
		"rcf = '%s', err '%d', flags '%d', priority_time 0x%x, "
		"time 0x%x, t 0x%x, number %d",
		rcf->name,rcf->error[0],rcf->flags,rcf->priority_time,
		rcf->statb.st_ctime,rt,rcf->number);
	DEBUG9( "fcmp: result %d", c );
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

void Getcontrolfile( char *file, struct dpathname *dpath,
	int fd, struct stat *statb, int check_df, struct control_file *cf )
{
	int len, i;				/* ACME Integers */
	char *s;				/* ACME Pointers */
	static char CF_name[] = "CONTROL_FILE=";

	/* copy the status */
	DEBUG7("Getcontrolfile: file '%s' size %d, dirname '%s'",
		file, statb->st_size, dpath->pathname );

	/* free up memory for the control file */
	Clear_control_file( cf );

	cf->statb = *statb;
	if( cf->statb.st_size == 0 ){
		time_t t;
		int age;

		/* ignore zero length control files */
		strcpy( cf->error, "zero length control file" );
		/* check to see if the file is older than 1 hour */

		time( &t );
		age = t - statb->st_ctime;
		DEBUG8("Getcontrolfile: age %d", age );
		if( age > 60 ){
			plp_snprintf( cf->error, sizeof( cf->error),"empty control file %d mins old",
			age/60 );
			cf->flags |= BAD_FLAG;
			cf->flags |= OLD_FLAG;
		}
	}

	/* copy the control file name */
	cf->name = add_buffer( &cf->control_file, strlen( file ) );
	strcpy( cf->name, file );

	/*
	 * check the control file name format
	 */
	s = strrchr(cf->name, '/' );
	if( s ){
		++s;
	} else {
		s = cf->name;
	}

	/* read the control file into buffer */
	cf->cf_info = add_buffer( &cf->control_file, cf->statb.st_size );
	for( i = 1, len = cf->statb.st_size, s = cf->cf_info;
		len > 0 && (i = read( fd, s, len )) > 0;
		len -= i, s += i );
	*s++ = 0;
	cf->cf_copy = add_buffer( &cf->control_file,
		cf->statb.st_size + sizeof( CF_name ) + 1 );
	strcpy( cf->cf_copy, CF_name );
	strcat( cf->cf_copy, cf->cf_info );
	DEBUG8("Get_control_file: '%s'", cf->cf_copy );
	if( i <= 0 ){
		plp_snprintf( cf->error, sizeof( cf->error),"cannot read '%s'", file );
		cf->flags |= BAD_FLAG;
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
	char *Nline = 0;			/* first N line in control file */

	/* get the job number */

	DEBUG8("Parse_cf: job '%s' '%s'", cf->name, cf->cf_info );
	/* set up control file checks */
	if( Check_format( CONTROL_FILE, cf->name, cf ) ){
		plp_snprintf( cf->error, sizeof( cf->error),
			"bad job filename format '%s'", cf->name );
		goto error;
	}

	DEBUG8("Parse_cf: job number '%d', filehostname '%s'",
		cf->number, cf->filehostname );

	/* count the numbers of lines */
	for( s = cf->cf_info, datafiles = 0;
		s && *s && (end = strchr( s, '\n' ));
		s = end+1 ){
		*end = 0;
		if( cf->info_lines.count >= cf->info_lines.max ){
			extend_malloc_list( &cf->info_lines, sizeof( char *), 100 );
		}
		lines = (char **)cf->info_lines.list;
		lines[ cf->info_lines.count++] = s;
		if( islower( s[0] ) ) ++datafiles;
	}
	if( cf->info_lines.count >= cf->info_lines.max ){
		extend_malloc_list( &cf->info_lines, sizeof( char *), 100 );
	}
	lines = (char **)cf->info_lines.list;
	lines[ cf->info_lines.count] = 0;

	/* check for last line not ending in New Line */
	if( s && *s ){
		plp_snprintf( cf->error, sizeof( cf->error),
			"last line missing NL '%s'", s, cf->name );
		goto error;
	}

	DEBUG8("Parse_cf: '%s', lines %d, datafiles %d",
		cf->name, cf->info_lines.count, datafiles );


	/* set up the data files */
	if( cf->data_file.count >= cf->data_file.max ){
		extend_malloc_list(&cf->data_file, sizeof(df[0]), 10);
	}
	/* df = (void *)cf->data_file.list; */

	for( i = 0; (s = lines[i]); ++i ){
		DEBUG8("Parse_cf: line[%d] '%s'", i, s );
		c = s[0];
		if( isupper( c ) ){
			if( c == 'N' ){
				if( df && df->Ninfo == 0 ){
					df->Ninfo = s;
				} else {
					Nline = s;
				}
				continue;
			}
			if( c == 'U' ){
				if( df == 0 ){
					plp_snprintf( cf->error, sizeof( cf->error),
						"'%s' before data info in '%s'",
						s, cf->name );
					goto error;
				} else if( df->Uinfo ){
					plp_snprintf( cf->error, sizeof( cf->error),
						"multiple U (unlink) for same data file, line '%s' in %s",
						s, cf->name );
					goto error;
				}
				if( Check_format( DATA_FILE, s+1, cf ) ){
					plp_snprintf( cf->error, sizeof( cf->error),
						"U (unlink) data file name format bad, line '%s' in %s",
						s, cf->name );
					goto error;
				}
				if( strcmp( s+1, df->openname ) ){
					plp_snprintf( cf->error, sizeof( cf->error),
						"U (unlink) does not match data file name, line '%s' in %s",
						s, cf->name );
					goto error;
				}
				df->uline = i;	/* unlink line */
				df->Uinfo = s;
				continue;
			}
			if( cf->capoptions[ c - 'A' ] ){
				plp_snprintf( cf->error, sizeof( cf->error),
					"duplicate option '%s' in '%s'",
					s, cf->name );
				goto error;
			}
			cf->capoptions[ c - 'A' ] = s;
		} else if( isdigit( c ) ){
			if( cf->digitoptions[ c - '0' ] ){
				plp_snprintf( cf->error, sizeof( cf->error),
					"duplicate option '%s' in '%s'",
					s, cf->name );
				goto error;
			}
			cf->digitoptions[ c - '0' ] = s;
		} else if( islower( c ) ){
			DEBUG4("Parse_cf: format '%c' data file '%s'", c, s+1 );
			if( strchr( "aios", c )
				/* || ( Formats_allowed && !strchr( Formats_allowed, c )) */ ){
				plp_snprintf( cf->error, sizeof( cf->error),
					"illegal data file format '%c' in '%s'", c, cf->name );
				goto error;
			}

			/* check to see that the name has the format FdfXnnnHost*/
			if( Check_format( DATA_FILE, s+1, cf ) ){
				plp_snprintf( cf->error, sizeof( cf->error),
					"bad data file name format '%s' in %s", s, cf->name );
				goto error;
			}
			/* check for blank control file */
			if( cf->control_info == 0 ){
				if( i == 0 ){
					plp_snprintf( cf->error, sizeof( cf->error),
						"no control information before '%s' in '%s'",
						s, cf->name );
					goto error;
				}
				cf->control_info = i;
			}
			/* if you have a previous data file entry, then
			 *    adjust the line count
			 *    check for copies of the same file
			 */
			if( df ){
				df->linecount = i - df->line;
				df->copies = !strcmp( df->openname, s+1 );
			}
			/* get a new data file entry */
			if( cf->data_file.count >= cf->data_file.max ){
				extend_malloc_list(&cf->data_file, sizeof(df[0]), 10);
			}
			df = (void *)cf->data_file.list;
			df = &df[cf->data_file.count++];
			/* get the format and name of file */
			memset( df, 0, sizeof( df[0] ) );
			df->format = s[0];
			df->openname = s+1;
			df->line = i;
			df->Ninfo = Nline;
			/* check to see if there is a data file */
			if( check_df ){
				s = Add_path( dpath, df->openname );
				DEBUG4("Parse_cf: checking data file '%s'", s );
				fd = Checkread( s, &df->statb );
				err = errno;
				if( fd < 0 ){
					plp_snprintf( cf->error, sizeof( cf->error),
						"cannot open '%s' - '%s'", df->openname, Errormsg(err) );
					goto error;
				}
				cf->jobsize += df->statb.st_size;
				close( fd );
			}
		}
	}
	/* adjust the data file line count information in last data file */
	if( df ){
		df->linecount = i - df->line;
	}

	if( Make_identifier( cf ) ){
		goto error;
	}

	DEBUG8("Parse_cf: before dump job number '%d', filehostname '%s'",
		cf->number, cf->filehostname );

	if( Debug > 4 ){
		dump_control_file( cf->name, cf );
	}
	return( 0 );

error:
	DEBUG4("Parse_cf: error '%s'", cf->error );
	return( 1 );
}

/************************************************************************
 * Make_identifier - add an identifier field to the job
 *  the identifier has the format name@host%id
 ************************************************************************/
int Make_identifier( struct control_file *cf )
{
	char *user = "nobody";
	char *host = "unknown";
	int len;

	if( cf->IDENTIFIER ){
		if( cf->IDENTIFIER[1] == 0 ){
			plp_snprintf( cf->error, sizeof( cf->error),
				"bad IDENTIFIER line in '%s'", cf->name );
			return( 1 );
		}
		cf->identifier = cf->IDENTIFIER+1;
		return( 0 );
	}
	cf->identifier = cf->ident_buffer + 1;
	if( cf->LOGNAME ){
		if( cf->LOGNAME[1] == 0 ){
			plp_snprintf( cf->error, sizeof( cf->error),
				"bad LOGNAME line in '%s'", cf->name );
			return( 1 );
		}
		user = cf->LOGNAME+1;
	}
	plp_snprintf( cf->ident_buffer, sizeof(cf->ident_buffer),
		"A%s@", user );
	len = strlen( cf->ident_buffer );
	if( cf->FROMHOST ){
		if( cf->FROMHOST[1] == 0 ){
			plp_snprintf( cf->error, sizeof( cf->error),
				"bad FROMHOST line in '%s'", cf->name );
			return( 1 );
		}
		host = cf->FROMHOST+1;
	}
	strncat( cf->ident_buffer+len, host, sizeof(cf->ident_buffer)-len );
	if( (host = strchr( cf->ident_buffer+len, '.' )) ) *host = 0;
	len = strlen( cf->ident_buffer );
	plp_snprintf( cf->ident_buffer+len, sizeof(cf->ident_buffer)-len,
		"+%d", cf->number );
	return( 0 );
}

/***************************************************************************
 * Scan_qeueue( char *pathname )
 * - scan the directory, checking for new or changed control files.
 * - insert them, append them
 * - check for unused entries and remove them
 * - sort the list
 ***************************************************************************/

void Scan_queue( int check_df )
{
	DIR *dir;						/* directory */
	int fd, i, j;			/* ACME integers */
	struct stat statb;				/* statb information */
	struct dirent *d;				/* directory entry */
	int found;						/* found in list */
	struct control_file *cfp, *cf, **cfpp;	/* control file pointers */
	char *pathname;

	pathname = Clear_path( SDpathname );
	DEBUG4("Scan_qeueue: pathname '%s'", pathname );
	if( c_files.count >= c_files.max ){
		extend_malloc_list(&c_files, sizeof(cfp[0]), 10);
	}
	if(Debug>4){
		logDebug( "c_files.count %d", c_files.count );
		cfp = (void *)c_files.list;
		for( i = 0; i < c_files.count; ++i ){
			logDebug( "[%d] '%s'", i, cfp[i].name );
		}
	}

	dir = opendir( pathname );
	if( dir == 0 ){
		logerr_die( LOG_ERR, "Scan_qeueue: opendir '%s' failed", pathname );
	}

	/* now we read the files */

	cf = 0;
	while( (d = readdir(dir)) ){
		found = 0;


		if( d->d_name[0] != 'c' || d->d_name[1] != 'f' ) continue;
		pathname = Add_path( SDpathname, d->d_name );
		DEBUG4("Scan_qeueue: entry '%s', pathname '%s'",
			d->d_name, pathname );
		fd = Checkread( pathname, &statb );
		if( fd < 0 ){
			DEBUG7("Scan_qeueue: cannot open file '%s'", pathname );
			continue;
		}
		/* search through the list of control files and see if we have it */
		cfp = (void *)c_files.list;
		for( i = 0; i < c_files.count; ++i ){
			/* we now check the control file name */
			cf = &cfp[i];
			DEBUG8("Scan_qeueue: checking cf name '%s' against '%s'",
				cf->name, d->d_name );
			if( cf->name && strcmp( cf->name, d->d_name ) == 0 ){
				DEBUG8(
				"Scan_qeueue: '%s' old ctime %d, new ctime %d", cfp->name,
					cf->statb.st_ctime, statb.st_ctime );
				/* now we check the modification time */
				if( cf->statb.st_ctime != statb.st_ctime ){
					cf->name = 0;
				} else if( Get_job_control( cf ) ){
					cf->name = 0;
				} else {
					found = 1;	/* we don't need to process it */
				}
				break;
			}
		}
		if( !found ){
			/* we need an entry */
			/* see if we can reuse the old one */
			if( i >= c_files.count || cf->name ){
				for( i = 0 ; i < c_files.count && cfp[i].name; ++i );
			}
			if( i >= c_files.count ){
				if( c_files.count >= c_files.max ){
					extend_malloc_list(&c_files, sizeof(cfp[0]), 10);
				}
				++c_files.count;
			}
			cfp = (void *)c_files.list;
			cf = &cfp[i];
			Getcontrolfile( d->d_name, SDpathname, fd, &statb, check_df, cf );
			Get_job_control( cf );
		}
		close(fd);

		/* now we fix up the class information */
		cf->held_class = 0;
		if( Classes ){
			cf->held_class = Fix_class_info( cf, Classes );
		}
		cf->flags |= FOUND_FLAG;
	}
	closedir(dir);

	/* we now create the sorted list */
	while( c_files.count+1 >= C_files_list.max ){
		extend_malloc_list( &C_files_list, sizeof( cfpp[0] ), 100 );
	}
	cfp = (void *)c_files.list;
	cfpp = (void *)C_files_list.list;
	for( i = 0, j = 0; i < c_files.count; ++i ){
		cf = &cfp[i];
		if( cf->name && (cf->flags & FOUND_FLAG) == 0 ){
			DEBUG6("Scan_qeueue: deleting '%s'", cf->name );
			/* we can delete the file */
			cf->name = 0;
		}
		if( cf->name ){
			cfpp[j++] = cf;
		}
		/* clear the flag */
		cf->flags &= ~FOUND_FLAG;
	}

	C_files_list.count = j;

	/* now we sort them */
	if( Debug > 9 ){
		logDebug( "Unsorted Jobs" );
		for( i = 0; i < C_files_list.count; ++i ){
			logDebug( "[%d] '%s'", i, cfpp[i]->name );
		}
	}
	qsort( cfpp, C_files_list.count, sizeof( cf ), fcmp );
	if( Debug > 9 ){
		logDebug( "Sorted Jobs" );
		for( i = 0; i < C_files_list.count; ++i ){
			cf = cfpp[i];
			logDebug( "[%d] '%s'", i, cf->name );
			dump_control_file( cf->name, cf );
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
	static char *line;
	static unsigned linelen;

	if( classes ){
		result = 1;
		DEBUG6("Fix_class_info: class '%s'", classes );
		if( strlen(classes)+1 > linelen ){
			if( line ) free(line);
			line = 0;
			linelen = strlen(classes) + 128;
			malloc_or_die( line, linelen );
		}
		strcpy( line, classes );
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
