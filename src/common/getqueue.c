/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: getqueue.c,v 5.7 1999/10/28 01:28:14 papowell Exp papowell $";


/***************************************************************************
 * Commentary
 * Patrick Powell Thu Apr 27 21:48:38 PDT 1995
 * 
 * The spool directory holds files and other information associated
 * with jobs.  Job files have names of the form cfXNNNhostname.
 * 
 * The Scan_queue routine will scan a spool directory, looking for new
 * or missing control files.  If one is found,  it will add it to
 * the control file list.  It will then sort the list of file names.
 * 
 * In order to prevent strange things with pointers,  you should not store
 * pointers to this list,  but use indexes instead.
 ***************************************************************************/

#include "lp.h"
#include "child.h"
#include "errorcodes.h"
#include "fileopen.h"
#include "getprinter.h"
#include "getqueue.h"
#include "globmatch.h"
#include "permission.h"
#include "merge.h"

/**** ENDINCLUDE ****/

/*
 * The cf_names, hfinfo, and sort_order lists are organized
 * as follows:
 * 
 * cf_names: contents are control file names and corresponding
 *   hold files.  For example:
 *     cfXXXX=hfXXXX
 *   would indicate that cfXXXX was already found,  and had
 *   hold file hfXXXX entry.
 * 
 * hfinfo: contents are hold file information entries.  These
 *   have the form:
 *     hfXXXX=key=value^Akey=value^A
 *   That is,  the values are separated by ^A (CTRL-A) characters.
 *   The following keys are used:
 *   cf_name=cfXXXX
 *   hf_name=hfXXXX
 *   cf_image=line\nline\nline\n
 * 	  The image of the control file.
 *   hf_image=line\nline\nline\n
 * 	  The image of the hold file.
 *   key=xxxxx:hfXXXX
 *       The key value to be used for sorting this file
 * sort_order: contents are keys and hold files in sorted order.
 *   For example:
 * 	  xxxx1:hfXXX1
 * 	  xxxx2:hfXXX2
 *   The first (highest?) priority entry would be xxxx1 with hfXXXX1
 *   
 */


int Scan_queue( const char *dirpath, struct line_list *spool_control,
	struct line_list *sort_order, int *pprintable, int *pheld, int *pmove,
		int only_pr_and_move )
{
	DIR *dir;						/* directory */
	struct dirent *d;				/* directory entry */
	struct line_list info_lines, cf_line_list, directory_files;
	char *cf_name, *hf_name, *sort_key, *s;
	int c, printable, held, move, p, h, m, df_count;
	struct job job;

	cf_name = hf_name = sort_key = s = 0;
	c = printable = held = move = 0;
	Init_job( &job );
	Init_line_list(&directory_files);
	Init_line_list(&info_lines);
	Init_line_list(&cf_line_list);
	if( pprintable ) *pprintable = printable;
	if( pheld ) *pheld = held;
	if( pmove ) *pmove = move;

	Free_line_list(sort_order);

	DEBUG4("Scan_queue: START dir '%s'", dirpath );
	if( !(dir = opendir( dirpath )) ){
		return(1);
	}

	while( (d = readdir(dir)) ){
		cf_name = d->d_name;
		DEBUG5("Scan_queue: found file '%s'", cf_name );
		if( safestrncmp( cf_name,"cf",2 )
			|| !isalpha(cval(cf_name+2))
			|| !isdigit(cval(cf_name+3))
			){
			DEBUG5("Scan_queue: not a control file '%s'", cf_name );
			continue;
		}
		Add_line_list( &directory_files, cf_name, 0, 0, 0);
	}
	closedir(dir);

	for( df_count = 0; df_count < directory_files.count; ++df_count ){
		cf_name = directory_files.list[df_count];
		DEBUG5("Scan_queue: processing file '%s'", cf_name );

		if( hf_name ) free(hf_name); hf_name = 0;
		if( sort_key ) free(sort_key); sort_key = 0;
		Free_line_list(&info_lines);
		Free_line_list(&cf_line_list);
		Free_job( &job );

		DEBUG4("Scan_queue: control file '%s'", cf_name );

		/* now we get the corresonding hold file name */
		hf_name = safestrdup( cf_name, __FILE__,__LINE__ );
		hf_name[0]='h';
		hf_name[1]='f';
		hf_name[2]='A';
		s = hf_name+3;
		while( isdigit( cval(s) ) ) ++s;
		*s = 0;
		DEBUG4("Scan_queue: hold file '%s'", hf_name );

		/* we have the control file name and the status file name */
		Get_file_image_and_split( dirpath, hf_name, 0, 0,
			&job.info,     Line_ends, 1, Value_sep,1,1,1);
		Get_file_image_and_split( dirpath, cf_name, 0, 1,
			&cf_line_list, Line_ends, 0,         0,0,0,0);
		if(DEBUGL5)Dump_line_list("Scan_queue: hf", &job.info );
		if(DEBUGL5)Dump_line_list("Scan_queue: cf", &cf_line_list );

		if( cf_line_list.count == 0 ){
			continue;
		}

		/* get the information from the control and status files */
		Setup_job( &job, spool_control, dirpath, cf_name, hf_name, &cf_line_list );
		Job_printable(&job,spool_control, &p,&h,&m);
		if( p ) ++printable;
		if( h ) ++held;
		if( m ) ++move;
		DEBUG4("Scan_queue: p %d, m %d, only_pr_and_move %d",
			p, m, only_pr_and_move );
		if( only_pr_and_move && !( p || m ) ) continue;
		if(DEBUGL4)Dump_job("Scan_queue - before Make_sort_key",&job);

		/* now generate the sort key */
		sort_key = Make_sort_key( &job );
		DEBUG5("Scan_queue: sort key '%s'",sort_key);
		Add_line_list(sort_order,sort_key,";",1,0);
		if(sort_key) free(sort_key); sort_key = 0;
	}
	if( hf_name ) free(hf_name); hf_name = 0;
	if( sort_key ) free(sort_key); sort_key = 0;
	Free_line_list(&directory_files);
	Free_line_list(&info_lines);
	Free_line_list(&cf_line_list);
	Free_job(&job);

	if(DEBUGL5){
		logDebug("Scan_queue: final values for '%s'", dirpath);
		Dump_line_list_sub(SORT_KEY,sort_order);
	}
	if( pprintable ) *pprintable = printable;
	if( pheld ) *pheld = held;
	if( pmove ) *pmove = move;
	DEBUG3("Scan_queue: printable %d, held %d, move %d", printable,
		held, move );
	return(0);
}

/*
 * char *Get_file_image( char *dir, char *file )
 *  Get an image of a file
 */

char *Get_file_image( const char *dir, const char *file, int maxsize )
{
	char *path;
	char *s = 0;
	struct stat statb;
	char buffer[LARGEBUFFER];
	int fd, n, len;

	if( file == 0 ) return(0);
	path = Make_pathname( dir, file );
	DEBUG3("Get_file_image: '%s'", path );
	if( (fd = Checkread( path, &statb )) >= 0 ){
		n = len = 0;
		if( maxsize && maxsize< statb.st_size/1024 ){
			lseek(fd, -maxsize*1024, SEEK_END);
		}
		while( (n = read(fd,buffer,sizeof(buffer))) > 0 ){
			s = realloc_or_die(s,len+n+1,__FILE__,__LINE__);
			memcpy(s+len,buffer,n);
			len += n;
			s[len] = 0;
		}
		close(fd);
	}
	free(path);
	return(s);
}

/*
 * char *Get_file_image_and_split
 *  Get an image of a file
 */

int Get_file_image_and_split( const char *dir, const char *file,
	int maxsize, int clean,
	struct line_list *l, const char *sep,
	int sort, const char *keysep, int uniq, int trim, int nocomments )
{
	char *s = 0;
	if( file ){
		s = Get_file_image( dir, file, maxsize );
	}
	if( s == 0 ) return 1;
	if( clean ) Clean_meta(s);
	Split( l, s, sep, sort, keysep, uniq, trim, nocomments );
	if( s ) free(s); s = 0;
	return(0);
}

void Is_set_str_value( struct line_list *l, const char *key, char *s)
{
	if( s && *s == 0 ) s = 0;
	if( !Find_str_value(l,key,Value_sep) ){
		Set_str_value(l,key,s);
	}
}

int Setup_cf_info( const char *dir, struct line_list *cf_line_list,
	struct job *job)
{
	int i, c;
	char *s, *t;
	struct line_list info;

	Init_line_list(&info);
	if( job->controlfile.count == 0 ){
		if( cf_line_list == 0 ){
			s = Find_str_value(&job->info,OPENNAME,Value_sep);
			i = Get_file_image_and_split(0,s,0,0,
				&info, Line_ends,0,0,0,0,0);
			if( i ){
				DEBUG3("Setup_cf_info: missing control file '%s'", s );
				return(1);
			}
			cf_line_list = &info;
		}
		Get_datafile_info( dir, cf_line_list, job );
		if( cf_line_list && cf_line_list->count ){
			s = Join_line_list(cf_line_list,"\n");
			t = Escape(s,0,1);
			Set_str_value(&job->info,CF_ESC_IMAGE,t);
			if(s)free(s); s = 0;
			if(t)free(t); t = 0;
		}
		for( i = 0; i < job->controlfile.count; ++i ){
			s = job->controlfile.list[i];
			if(s && (c = s[0]) ) switch(c){
			case '_': Is_set_str_value(&job->info,AUTHINFO,s+1); break;
			case 'A': Is_set_str_value(&job->info,IDENTIFIER,s+1); break;
			case 'C': Is_set_str_value(&job->info,CLASS,s+1); break;
			case 'D': Is_set_str_value(&job->info,DATE,s+1); break;
			case 'H': Is_set_str_value(&job->info,FROMHOST,s+1); break;
			case 'J': Is_set_str_value(&job->info,JOBNAME,s+1); break;
			case 'L': Is_set_str_value(&job->info,BNRNAME,s+1); break;
			case 'M': Is_set_str_value(&job->info,MAILNAME,s+1); break;
			case 'P': Is_set_str_value(&job->info,LOGNAME,s+1); break;
			case 'Q': Is_set_str_value(&job->info,QUEUENAME,s+1); break;
			}
		}
	}
	Free_line_list(&info);
	return(0);
}

/*
 * Set up a job data structure with information from the
 *   file images
 * 
 */

void Setup_job( struct job *job, struct line_list *spool_control, const char *dir,
	const char *cf_name, const char *hf_name,
	struct line_list *cf_line_list )
{
	struct stat statb;
	char *path, *s;
	struct line_list *lp;
	int i, j, size = 0, held;


	/* add the hold file information directly */ 
	DEBUG3("Setup_job: hf_name '%s', cf_name '%s'", hf_name, cf_name );
	if( cf_name ){
		if( !Find_str_value(&job->info,TRANSFERNAME,Value_sep) ){
			Set_str_value(&job->info,TRANSFERNAME,cf_name);
		}
		if( !Find_str_value(&job->info,OPENNAME,Value_sep) ){
			path = Make_pathname( dir, cf_name );
			Set_str_value(&job->info,OPENNAME,path);
			free(path);
		}
	}
	cf_name = Find_str_value(&job->info,TRANSFERNAME,Value_sep);

	if( hf_name && !Find_str_value(&job->info,HF_NAME,Value_sep) ){
		path = Make_pathname( dir, hf_name );
		Set_str_value(&job->info,HF_NAME,path);
		free(path);
	}

	if( cf_name && !Find_str_value(&job->info,NUMBER,Value_sep) ){
		Check_format( CONTROL_FILE, cf_name, job );
	}

	if( !Find_str_value(&job->info,JOB_TIME,Value_sep)
		&& (path = Find_str_value(&job->info,OPENNAME,Value_sep)) ){
		j = 0;
		if( stat(path,&statb) ){
			i = time((void *)0);
		} else {
			i = statb.st_mtime;
#ifdef ST_MTIMESPEC_TV_NSEC
			j = statb.st_mtimespec.tv_nsec/1000;
#endif
#ifdef ST_MTIMENSEC
			j = statb.st_mtimensec/1000;
#endif
		}
		Set_flag_value(&job->info,JOB_TIME,i);
		Set_flag_value(&job->info,JOB_TIME_USEC,j);
	}

	/* set up the control file information */
	Setup_cf_info( dir, cf_line_list, job );

	if( !Find_str_value(&job->info,CLASS,Value_sep)
		&& (s = Find_str_value(&job->info,PRIORITY,Value_sep)) ){
		Set_str_value(&job->info,CLASS,s);
	}
	if( !Find_flag_value(&job->info,SIZE,Value_sep) ){
		size = 0;
		for( i = 0; i < job->datafiles.count; ++i ){
			lp = (void *)job->datafiles.list[i];
			size +=  Find_flag_value(lp,SIZE,Value_sep);
		}
		Set_decimal_value(&job->info,SIZE,size);
	}

	held = Get_hold_class(&job->info,spool_control);
	Set_flag_value(&job->info,HOLD_CLASS,held);

	if( !Find_exists_value(&job->info,HOLD_TIME,Value_sep) ){
		if( Find_flag_value(spool_control,HOLD_TIME,Value_sep) ){
			i = time((void *)0);
		} else {
			i = 0;
		}
		Set_flag_value( &job->info, HOLD_TIME, i );
	}
	held = Find_flag_value(&job->info,HOLD_TIME,Value_sep);

	Set_flag_value(&job->info,HELD,held);
	if(DEBUGL3)Dump_job("Setup_job",job);
}

/* Get_hold_class( spool_control, job )
 *  check to see if the spool class and the job class are compatible
 *  returns:  non-zero if held, 0 if not held
 *   i.e.- cmpare(spoolclass,jobclass)
 */

int Get_hold_class( struct line_list *info, struct line_list *sq )
{
	int held, i;
	char *s, *t;
	struct line_list l;

	Init_line_list(&l);
	held = 0;
	if( (s = Clsses(sq))
		&& (t = Find_str_value(info,CLASS,Value_sep)) ){
		held = 1;
		Free_line_list(&l);
		Split(&l,s,File_sep,0,0,0,0,0);
		for( i = 0; held && i < l.count; ++i ){
			held = Globmatch( l.list[i], t );
		}
		Free_line_list(&l);
	}
	return(held);
}

/*
 * Extract the control file and data file information from the
 * control file image
 */

void Get_datafile_info( const char *dir, struct line_list *cf_line_list, struct job *job )
{
	struct line_list *datafile = 0;
	struct stat statb;
	char *s, buffer[SMALLBUFFER], *t;
	int i, c, copies = 0, last_format = 0;
	char *file_found;
	char *names = 0;

	if( cf_line_list == 0 ) return;

	file_found = 0;
	datafile = malloc_or_die(sizeof(datafile[0]),__FILE__,__LINE__);
	memset(datafile,0,sizeof(datafile[0]));

	for( i = 0; i < cf_line_list->count; ++i ){
		s = cf_line_list->list[i];
		c = cval(s);
		if( islower(c) ){
			t = s;
			while( (t = strpbrk(t," \t")) ) *t++ = '_';
			if( file_found && (safestrcmp(file_found,s+1) || last_format != c)){
				Check_max(&job->datafiles,1);
				job->datafiles.list[job->datafiles.count++] = (void *)datafile;
				copies = 0;
				file_found = 0;
				datafile = malloc_or_die(sizeof(datafile[0]),__FILE__,__LINE__);
				memset(datafile,0,sizeof(datafile[0]));
			}
			last_format = c;
			++copies;
			c = s[1]; s[1] = 0;
			Set_str_value(datafile,FORMAT,s);
			s[1] = c;
			Set_str_value(datafile,TRANSFERNAME,s+1);
			file_found = Find_str_value(datafile,TRANSFERNAME,Value_sep);
			Set_flag_value(datafile,COPIES,copies);

			s = Make_pathname( dir, s+1 );
			Set_str_value(datafile,OPENNAME,s);
			/* now we check for the status */
			if( stat(s, &statb) == 0 ){
				double size;
				size = statb.st_size;
				DEBUG4("Get_datafile_info: '%s' - size %ld", s, size );
				Set_double_value(datafile,SIZE,size );
			} else {
				plp_snprintf(buffer,sizeof(buffer),
					"missing data file %s - %s", s, Errormsg(errno) );
				Set_str_value(&job->info,ERROR,buffer);
			}
			if(s) free(s); s = 0;
		} else if( c == 'N' ){
			Clean_meta(s);
			if( file_found && (t = Find_str_value(datafile,"N",Value_sep))
				/* && safestrcmp(t,s+1) */ ){
				Check_max(&job->datafiles,1);
				job->datafiles.list[job->datafiles.count++] = (void *)datafile;
				copies = 0;
				file_found = 0;
				datafile = malloc_or_die(sizeof(datafile[0]),__FILE__,__LINE__);
				memset(datafile,0,sizeof(datafile[0]));
			}
			Set_str_value(datafile,"N",s+1);
			if( !names ){
				names = safestrdup(s+1,__FILE__,__LINE__);
			} else {
				names =  safeextend3(names,",",s+1,__FILE__,__LINE__);
			}
		} else if( c == 'U' ){
			Set_str_value(datafile,"U",s+1);
		} else {
			Add_line_list(&job->controlfile,s,0,0,0);
		}
	}
	if( Find_str_value(datafile,TRANSFERNAME,Value_sep) ){
		Check_max(&job->datafiles,1);
		job->datafiles.list[job->datafiles.count++] = (void *)datafile;
	} else {
		free(datafile); datafile = 0;
	}
	if( names ){
		Set_str_value(&job->info,FILENAMES,names);
		free(names); names=0;
	}
}

/*
 * hold file manipulation
 */
 const char * const *contents[] = {

&ACTIVE_TIME,
&ATTEMPT,
&AUTHINFO,
&BNRNAME,
&CLASS,
&COPIES,
&COPY_DONE,
&DATE,
&DESTINATION,
&DESTINATIONS,
&DONE_TIME,
&ERROR,
&FORWARD_ID,
&FROMHOST,
&HF_NAME,
&HOLD_TIME,
&IDENTIFIER,
&JOBNAME,
&JOB_TIME,
&LOGNAME,
&MAILNAME,
&NUMBER,
&OPENNAME,
&PRIORITY,
&PRIORITY_TIME,
&QUEUENAME,
&REDIRECT,
&REMOVE_TIME,
&SEQUENCE,
&SERVER,
&SERVER_ORDER,
&SIZE,
&START_TIME,
&SUBSERVER,
&TRANSFERNAME,

 0
};

/*
 * Write a hold file
 */

int Set_hold_file( struct job *job, struct line_list *perm_check )
{
	struct line_list vals;
	char *s, *t, *hf_name, *tempfile;
	int i, fd, status;

	status = 0;
	Init_line_list(&vals);
	Set_str_value(&job->info,UPDATE_TIME,Time_str(0,0));
	if( !(hf_name = Find_str_value(&job->info,HF_NAME,Value_sep)) ){
		DEBUG3( "Set_hold_file: no hf_name value in job->info" );
		status = 1;
		return( status );
	}
	DEBUG3( "Set_hold_file: '%s'", hf_name );
	if(DEBUGL5)Dump_job( "Set_hold_file - before", job );

	for( i = 0; i < job->info.count; ++i ){
		s = job->info.list[i];
		if( !safestrpbrk(s,Line_ends) ){
			Add_line_list(&vals,s,Value_sep,1,1);
		}
	}
	if(DEBUGL5)Dump_line_list("Set_hold_file - output",&vals);

	s = Join_line_list(&vals,"\n");
	Set_str_value(&job->info,HF_IMAGE,s);
	Free_line_list(&vals);
	fd = Make_temp_fd( &tempfile );
	if( Write_fd_str(fd, s) < 0 ){
		logerr(LOG_INFO,"Set_hold_file: write to '%s' failed", tempfile );
		status = 1;
	}
	close(fd);
	if( status == 0 && rename( tempfile, hf_name ) == -1 ){
		logerr(LOG_INFO,"Set_hold_file: rename '%s' to '%s' failed",
			tempfile, hf_name );
		status = 1;
	}
	t = Escape(s,0,1);
	Set_str_value(&vals,HF_IMAGE,t);
	if( s ) free(s); s = 0;
	if( t ) free(t); t = 0;

	if( perm_check ){
		s = Join_line_list( perm_check, "\n" );
		t = Escape(s,0,1);
		Set_str_value(&vals,LPC,t);
		if( s ) free(s); s = 0;
		if( t ) free(t); t = 0;
	}
	s = Join_line_list( &vals, "\n" );
	send_to_logger(-1, -1, job,UPDATE,s);
	if( s ) free(s); s = 0;
	if( t ) free(t); t = 0;
	return( status );
}

/*
 * Get Spool Control Information
 *  - simply read the file
 */

void Get_spool_control( const char *dir, const char *cf,
	const char *printer, struct line_list *info )
{
	char *path;

	Free_line_list(info);
	path = Make_pathname( dir, cf );
	DEBUG2("Get_spool_control: dir '%s', control file '%s', path '%s'",
		dir, cf, path );
	Get_file_image_and_split( dir, path, 0, 0,
			info,Line_ends,1,Value_sep,1,1,1);
	if(DEBUGL4)Dump_line_list("Get_spool_control- info", info );
	if( path ) free(path); path = 0;
}

/*
 * Set Spool Control Information
 *  - simply write the file
 */

void Set_spool_control(
	struct line_list *perm_check, const char *dir, const char *scf,
	const char *printer, struct line_list *info )
{
	char *path, *s, *t, *tempfile;
	struct line_list l;
	int fd;

	path = s = t = tempfile = 0;
	Init_line_list(&l);
	path = Make_pathname( dir, scf );
	fd = Make_temp_fd( &tempfile );
	DEBUG2("Set_spool_control: path '%s', tempfile '%s'",
		path, tempfile );
	if(DEBUGL4)Dump_line_list("Set_spool_control- info", info );
	s = Join_line_list(info,"\n");
	if( Write_fd_str(fd, s) < 0 ){
		Errorcode = JFAIL;
		logerr_die(LOG_INFO,"Set_spool_control: cannot write tempfile '%s'",
			tempfile );
	}
	close(fd);
	if( rename( tempfile, path ) == -1 ){
		Errorcode = JFAIL;
		logerr_die(LOG_INFO,"Set_spool_control: rename of '%s' to '%s' failed",
			tempfile, path );
	}
	if( path ) free(path); path = 0;

	/* log the spool control file changes */

	t = Escape(s,0,1);
	Set_str_value(&l,QUEUE,t);
	if(s) free(s); s = 0;
	if(t) free(t); t = 0;

	if( perm_check ){
		s = Join_line_list( perm_check, "\n" );
		t = Escape(s,0,1);
		Set_str_value(&l,LPC,t);
		if(s) free(s); s = 0;
		if(t) free(t); t = 0;
	}
	t = Join_line_list( &l, "\n");

	send_to_logger(-1,-1,0,QUEUE,t);

	Free_line_list(&l);
	if(s) free(s); s = 0;
	if(t) free(t); t = 0;
}

void intval( const char *key, struct line_list *list, struct line_list *info )
{
	char buffer[SMALLBUFFER];
	int i = Find_flag_value(list,key,Value_sep);
	plp_snprintf(buffer,sizeof(buffer),"%s=0x%08x",key,i&0xffffffff);
	DEBUG5("intval: '%s'", buffer );
	Add_line_list(info,buffer,0,0,0);
}

void revintval( const char *key, struct line_list *list, struct line_list *info )
{
	char buffer[SMALLBUFFER];
	int i = Find_flag_value(list,key,Value_sep);
	plp_snprintf(buffer,sizeof(buffer),"%s=0x%08x",key,(~i)&0xffffffff);
	DEBUG5("intval: '%s'", buffer );
	Add_line_list(info,buffer,0,0,0);
}

void strzval( const char *key, struct line_list *list, struct line_list *info )
{
	char buffer[SMALLBUFFER];
	char *s = Find_str_value(list,key,Value_sep);
	plp_snprintf(buffer,sizeof(buffer),"%s=%d",key,s!=0);
	DEBUG5("strzval: '%s'", buffer );
	Add_line_list(info,buffer,0,0,0);
}

void strnzval( const char *key, struct line_list *list, struct line_list *info )
{
	char buffer[SMALLBUFFER];
	char *s = Find_str_value(list,key,Value_sep);
	plp_snprintf(buffer,sizeof(buffer),"%s=%d",key,(s==0 || *s == 0));
	DEBUG5("strzval: '%s'", buffer );
	Add_line_list(info,buffer,0,0,0);
}

void strval( const char *key, struct line_list *list, struct line_list *info,
	int reverse )
{
	char buffer[SMALLBUFFER];
	char *s = Find_str_value(list,key,Value_sep);
	int c = 0;

	if(s) c = cval(s);
	if( reverse ) c = -c;
	c = 0xFF & (-c);
	plp_snprintf(buffer,sizeof(buffer),"%s=%02x",key,c);
	DEBUG5("strval: '%s'", buffer );
	Add_line_list(info,buffer,0,0,0);
}


/*
 * Make_sort_key
 *   Make a sort key from the image information
 */
char *Make_sort_key( struct job *job )
{
	char *cmpstr = 0, *s, *t;
	struct line_list fields;

	Init_line_list(&fields);

	intval(DONE_TIME,&job->info,&fields);
	intval(REMOVE_TIME,&job->info,&fields);
	strzval(ERROR,&job->info,&fields);
	intval(HOLD_CLASS,&job->info,&fields);
	intval(HOLD_TIME,&job->info,&fields);
	strnzval(MOVE,&job->info,&fields);
	revintval(PRIORITY_TIME,&job->info,&fields);
	if( Ignore_requested_user_priority_DYN == 0 ){
		strval(PRIORITY,&job->info,&fields,Reverse_priority_order_DYN);
	}
	intval(JOB_TIME,&job->info,&fields);
	intval(JOB_TIME_USEC,&job->info,&fields);
	intval(NUMBER,&job->info,&fields);
	cmpstr = Join_line_list(&fields,"|");

	DEBUG4("Make_sort_key: cmpstr '%s'", cmpstr );

	s = Join_line_list(&job->info,";");
	t = safestrdup3(cmpstr,";",s,__FILE__,__LINE__);

	DEBUG4("Make_sort_key: '%s'",t);

	Free_line_list(&fields);
	if(s) free(s); s = 0;
	if(cmpstr) free(cmpstr); cmpstr = 0;
	return(t);
}

/*
 * Set up printer
 *  1. reset configuration information
 *  2. check the printer name
 *  3. get the printcap information
 *  4. set the configuration variables
 *  5. If run on the server,  then check for the Lp_device_DYN
 *     being set.  If it is set, then clear the RemotePrinter_DYN
 *     and RemoteHost_DYN.
 */

int Setup_printer( char *name, char *error, int errlen )
{
	char *s;
	int status = 0;

	DEBUG3( "Setup_printer: checking printer '%s'", name );

	/* reset the configuration information, just in case
	 * this is a subserver or being used to get status
	 */
	name = safestrdup(name,__FILE__,__LINE__);
	if( error ) error[0] = 0;
	Reset_config();
	if( Is_server && Status_fd > 0 ){
		close( Status_fd );
		Status_fd = -1;
	}

	if( (s = Clean_name( name )) ){
		plp_snprintf( error, errlen,
			"printer '%s' has illegal char at '%s' in name", name, s );
		status = 1;
		goto error;
	}
	lowercase(name);
	Set_DYN(&Printer_DYN,name);
	Fix_Rm_Rp_info();
	if(DEBUGL3){
		logDebug("Setup_printer: printer '%s'", Printer_DYN );
		Dump_line_list_sub("entry", &PC_entry_line_list);
		Dump_line_list_sub("aliases", &PC_alias_line_list);
	}

	/* set printer name and printcap variables */
	if(DEBUGL3){
		logDebug("Setup_printer: printer '%s'", Printer_DYN );
		Dump_parms("Setup_printer", Pc_var_list);
	}

	if( Spool_dir_DYN == 0 || *Spool_dir_DYN == 0 ){
		plp_snprintf( error, errlen,
			"no spool directory for printer '%s'", name );
		status = 2;
		goto error;
	}

	if( chdir( Spool_dir_DYN ) < 0 ){
		plp_snprintf( error, errlen,
			"printer '%s', chdir to '%s' failed '%s'",
				name, Spool_dir_DYN, Errormsg( errno ) );
		status = 2;
		goto error;
	}

	/*
	 * get the override information from the control/spooling
	 * directory
	 */

	Get_spool_control( Spool_dir_DYN, Queue_control_file_DYN, Printer_DYN, &Spool_control );

	DEBUG1("Setup_printer: printer now '%s', spool dir '%s'",
		Printer_DYN, Spool_dir_DYN );
	if(DEBUGL3){
		Dump_parms("Setup_printer - vars",Pc_var_list);
		Dump_line_list("Setup_printer - spool control", &Spool_control );
	}

 error:
	DEBUG3("Setup_printer: status '%d', error '%s'", status, error );
	if( name ) free(name); name = 0;
	return( status );
}

/**************************************************************************
 * Read_pid( int fd, char *str, int len )
 *   - Read the pid from a file
 **************************************************************************/
int Read_pid( int fd, char *str, int len )
{
	char line[LINEBUFFER];
	int n;

	if( lseek( fd, 0, SEEK_SET ) < 0 ){
		logerr_die( LOG_ERR, "Read_pid: lseek failed" );
	}

	if( str == 0 ){
		str = line;
		len = sizeof( line );
	}
	str[0] = 0;
	if( (n = read( fd, str, len-1 ) ) < 0 ){
		logerr_die( LOG_ERR, "Read_pid: read failed" );
	}
	str[n] = 0;
	n = atoi( str );
	DEBUG3( "Read_pid: %d", n );
	return( n );
}

/**************************************************************************
 * Write_pid( int fd )
 *   - Write the pid to a file
 **************************************************************************/
void Write_pid( int fd, int pid, char *str )
{
	char line[LINEBUFFER];

	if( ftruncate( fd, 0 ) ){
		logerr_die( LOG_ERR, "Write_pid: ftruncate failed" );
	}
	if( lseek( fd, 0, SEEK_SET ) < 0 ){
		logerr_die( LOG_ERR, "Write_pid: lseek failed" );
	}

	if( str == 0 ){
		plp_snprintf( line, sizeof(line), "%d\n", pid );
	} else {
		plp_snprintf( line, sizeof(line), "%s\n", str );
	}
	DEBUG3( "Write_pid: pid %d, str '%s'", pid, str );
	if( Write_fd_str( fd, line ) < 0 ){
		logerr_die( LOG_ERR, "Write_pid: write failed" );
	}
}

/***************************************************************************
 * int Patselect( struct line_list *tokens, struct line_list *cf );
 *    check to see that the token value matches one of the following
 *    in the control file:
 *  token is INTEGER: then matches the job number
 *  token is string: then matches either the user name or host name
 *    then try glob matching job ID
 *  return:
 *   0 if match found
 *   nonzero if not match found
 ***************************************************************************/

int Patselect( struct line_list *token, struct line_list *cf, int starting )
{
	int match = 1;
	int i, n, val;
	char *key, *s, *end;
	
	if(DEBUGL3)Dump_line_list("Patselect- tokens", token );
	if(DEBUGL3)Dump_line_list("Patselect- info", cf );
	for( i = starting; match && i < token->count; ++i ){
		key = token->list[i];
		DEBUG3("Patselect: key '%s'", key );
		/* handle wildcard match */
		if( !(match = safestrcasecmp( key, "all" ))){
			break;
		}
		end = key;
		val = strtol( key, &end, 10 );
		if( *end == 0 ){
			n = Find_decimal_value(cf,NUMBER,Value_sep);
			/* we check job number */
			DEBUG3("Patselect: job number check '%d' to job %d",
				val, n );
			match = (val != n);
		} else {
			/* now we check to see if we have a name match */
			if( (s = Find_str_value(cf,LOGNAME,Value_sep))
				&& !(match = Globmatch(key,s)) ){
				break;
			}
			if( (s = Find_str_value(cf,IDENTIFIER,Value_sep))
				&& !(match = Globmatch(key,s)) ){
				break;
			}
		}
	}
	DEBUG3("Patselect: returning %d");
	return(match);
}

/***************************************************************************
 * char * Check_format( int type, char *name, struct control_file *job )
 * Check to see that the file name has the correct format
 * name[0] == 'c' or 'd' (type)
 * name[1] = 'f'
 * name[2] = A-Za-z
 * name[3-5] = NNN
 * name[6-end] = only alphanumeric and ., _, or - chars
 * RETURNS: 0 if OK, error message (string) if not
 ***************************************************************************/
int Check_format( int type, const char *name, struct job *job )
{
	int n, c;
	const char *s;
	char *t;
	char msg[SMALLBUFFER];

	DEBUG4("Check_format: type %d, name '%s'", type, name ); 
	msg[0] = 0;
	n = cval(name);
	switch( type ){
	case DATA_FILE:
		if( n != 'd' ){
			plp_snprintf(msg, sizeof(msg),
				"data file does not start with 'd' - '%s'",
				name );
			goto error;
		}
		break;
	case CONTROL_FILE:
		if( n != 'c' ){
			plp_snprintf(msg, sizeof(msg),
				"control file does not start with 'c' - '%s'",
				name );
			goto error;
		}
		break;
	default:
		plp_snprintf(msg, sizeof(msg),
			"bad file type '%c' - '%s' ", type,
			name );
		goto error;
	}
	/* check for second letter */
	n = cval(name+1);
	if( n != 'f' ){
		plp_snprintf(msg, sizeof(msg),
			"second letter must be f not '%c' - '%s' ", n, name );
		goto error;
	}
	n = cval(name+2);
	if( !isalpha( n ) ){
		plp_snprintf(msg, sizeof(msg),
			"third letter must be letter not '%c' - '%s' ", n, name );
		goto error;
	}
	if( type == CONTROL_FILE ){
		plp_snprintf(msg,sizeof(msg),"%c",n);
		Set_str_value(&job->info,PRIORITY,msg);
		msg[0] = 0;
	}
	s = &name[3];
	n = strtol(s,&t,10);
	c = 0;
	if( t ) c = t - s;
	/* check on length of number */
	if( c != 3 && c != 6 ){
		plp_snprintf(msg, sizeof(msg),
			"id number length %d out of bounds '%s' ", c, name );
		goto error;
	}
	if( (c = Find_decimal_value( &job->info,NUMBER,Value_sep)) ){
		if( c != n ){
			plp_snprintf(msg, sizeof(msg),
				"job numbers differ '%s', old %d and new %d",
					name, c, n );
			goto error;
		}
	} else {
		Fix_job_number( job, n );
	}
	if( Clean_name(t) ){
		plp_snprintf(msg, sizeof(msg),
			"bad hostname '%s' - '%s' ", t, name );
		goto error;
	}
	if( (s = Find_str_value( &job->info,FILE_HOSTNAME,Value_sep)) ){
		if( safestrcasecmp(s,t) ){
			plp_snprintf(msg, sizeof(msg),
				"bad hostname '%s' - '%s' ", t, name );
			goto error;
		}
	} else {
		Set_str_value(&job->info,FILE_HOSTNAME,t);
	}
	/* clear out error message */
	msg[0] = 0;

 error:
	if( msg[0] ){
		DEBUG1("Check_format: %s", msg ); 
		Set_str_value(&job->info,FORMAT_ERROR,msg);
	}
	return( msg[0] != 0 );
}

char *Find_start(char *str, const char *key )
{
	int n = strlen(key);
	while( (str = strstr(str,key)) && str[n] != '=' );
	if( str ) str += (n+1);
	return( str );
}

char *Frwarding(struct line_list *l)
{
	return( Find_str_value(l,FORWARDING,Value_sep) );
}
int Pr_disabled(struct line_list *l)
{
	return( Find_flag_value(l,PRINTING_DISABLED,Value_sep) );
}
int Sp_disabled(struct line_list *l)
{
	return( Find_flag_value(l,SPOOLING_DISABLED,Value_sep) );
}
int Pr_aborted(struct line_list *l)
{
	return( Find_flag_value(l,PRINTING_ABORTED,Value_sep) );
}
int Hld_all(struct line_list *l)
{
	return( Find_flag_value(l,HOLD_ALL,Value_sep) );
}
char *Clsses(struct line_list *l)
{
	return( Find_str_value(l,CLASS,Value_sep) );
}
char *Cntrol_debug(struct line_list *l)
{
	return( Find_str_value(l,DEBUG,Value_sep) );
}
char *Srver_order(struct line_list *l)
{
	return( Find_str_value(l,SERVER_ORDER,Value_sep) );
}

/*
 * Job datastructure management
 */

void Init_job( struct job *job )
{
	memset(job,0,sizeof(job[0]) );
}

void Free_job( struct job *job )
{
	Free_line_list( &job->info );
	Free_line_list( &job->controlfile );
	Free_listof_line_list( &job->datafiles );
	Free_line_list( &job->destination );
}

void Copy_job( struct job *dest, struct job *src )
{
	Merge_line_list( &dest->info, &src->info, 0,0,0 );
	Merge_line_list( &dest->controlfile, &src->controlfile, 0,0,0 );
	Merge_listof_line_list( &dest->datafiles, &src->datafiles, 0,0,0 );
	Merge_line_list( &dest->destination, &src->destination, 0,0,0 );
}

/**************************************************************************
 * static int Fix_job_number();
 * - fixes the job number range and value
 **************************************************************************/

char *Fix_job_number( struct job *job, int n )
{
	char buffer[SMALLBUFFER];
	int len = 3, max = 1000;

	if( n == 0 ){
		n = Find_decimal_value( &job->info, NUMBER, Value_sep );
	}
	if( Long_number_DYN && !Backwards_compatible_DYN ){
		len = 6;
		max = 1000000;
	}
	plp_snprintf(buffer,sizeof(buffer),"%0*d",len, n % max );
	Set_str_value(&job->info,NUMBER,buffer);
	return( Find_str_value(&job->info,NUMBER,Value_sep) );
}

/************************************************************************
 * Make_identifier - add an identifier field to the job
 *  the identifier has the format Aname@host%id
 *  Since the identifier is put in the cfp->identifier field,
 *  and you may want to use it as the job identifier,  we put the
 *  leading 'A' character on the name.
 * 
 ************************************************************************/

char *Make_identifier( struct job *job )
{
	char *user, *host, *s, *id;
	char number[32];
	int n;

	if( !(s = Find_str_value( &job->info,IDENTIFIER,Value_sep )) ){
		if( !(user = Find_first_letter( &job->controlfile,'P',0 ))){
			user = "nobody";
		}
		if( !(host= Find_first_letter( &job->controlfile,'H',0 ))){
			host = "unknown";
		}
		n = Find_decimal_value( &job->info,NUMBER,Value_sep );
		plp_snprintf(number,sizeof(number),"%d",n);
		if( (s = safestrchr( host, '.' )) ) *s = 0;
		id = safestrdup5(user,"@",host,"+",number,__FILE__,__LINE__);
		if( s ) *s = '.';
		Set_str_value(&job->info,IDENTIFIER,id);
		if( id ) free(id); id = 0;
		s = Find_str_value(&job->info,IDENTIFIER,Value_sep);
	}
	return(s);
}

void Dump_job( char *title, struct job *job )
{
	int i;
	struct line_list *lp;
	if( title ) logDebug( "*** Job %s *** - 0x%lx", title, Cast_ptr_to_long(job));
	Dump_line_list_sub( "info",&job->info);
	Dump_line_list_sub( "jobfile",&job->controlfile);
	logDebug("  datafiles - count %d", job->datafiles.count );
	for( i = 0; i < job->datafiles.count; ++i ){
		char buffer[SMALLBUFFER];
		plp_snprintf(buffer,sizeof(buffer),"  datafile[%d]", i );
		lp = (void *)job->datafiles.list[i];
		Dump_line_list_sub(buffer,lp);
	}
	Dump_line_list_sub( "destination",&job->destination);
	if( title ) logDebug( "*** end ***" );
}


void Job_printable( struct job *job, struct line_list *spool_control,
	int *pprintable, int *pheld, int *pmove )
{
	char *s;
	char buffer[SMALLBUFFER];
	char destbuffer[SMALLBUFFER];
	int n, printable = 0, held = 0, move = 0, destination, destinations;

	if(DEBUGL4)Dump_job("Job_printable - job info",job);
	if(DEBUGL4)Dump_line_list("Job_printable - spool control",spool_control);

	buffer[0] = 0;
	if( job->info.count == 0 ){
		plp_snprintf(buffer,sizeof(buffer), "removed" );
	} else if( Find_str_value(&job->info,ERROR,Value_sep) ){
		plp_snprintf(buffer,sizeof(buffer), "error" );
	} else if( Find_flag_value(&job->info,HOLD_TIME,Value_sep) ){
		plp_snprintf(buffer,sizeof(buffer), "hold" );
		held = 1;
	} else if( Find_flag_value(&job->info,REMOVE_TIME,Value_sep) ){
		plp_snprintf(buffer,sizeof(buffer), "remove" );
	} else if( Find_flag_value(&job->info,DONE_TIME,Value_sep) ){
		plp_snprintf(buffer,sizeof(buffer), "done" );
	} else if( (n = Find_flag_value(&job->info,SERVER,Value_sep))
		&& kill( n, 0 ) == 0 ){
		int delta;
		n = Find_flag_value(&job->info,START_TIME,Value_sep);
		delta = time((void *)0) - n;
		if( Stalled_time_DYN && delta > Stalled_time_DYN ){
			plp_snprintf( buffer, sizeof(buffer),
				"stalled(%dsec)", delta );
		} else {
			n = Find_flag_value(&job->info,ATTEMPT,Value_sep);
			plp_snprintf(buffer,sizeof(buffer), "active" );
			if( n > 0 ){
				plp_snprintf( buffer, sizeof(buffer),
					"active(attempt-%d)", n+1 );
			}
		}
		printable = 1;
	} else if((s = Find_str_value(&job->info,MOVE,Value_sep)) ){
		plp_snprintf(buffer,sizeof(buffer), "moved->%s", s );
		move = 1;
	} else if( Get_hold_class(&job->info, spool_control ) ){
		plp_snprintf(buffer,sizeof(buffer), "holdclass" );
		held = 1;
	} else {
		printable = 1;
	}
	if( (destinations = Find_flag_value(&job->info,DESTINATIONS,Value_sep)) ){
		printable = 0;
		for( destination = 0; destination < destinations; ++destination ){
			Get_destination(job,destination);
			if(DEBUGL4)Dump_job("Job_destination_printable - job",job);
			destbuffer[0] = 0;
			if( Find_str_value(&job->destination,ERROR,Value_sep) ){
				plp_snprintf(destbuffer,sizeof(destbuffer), "error" );
			} else if( Find_flag_value(&job->destination,HOLD_TIME,Value_sep) ){
				plp_snprintf(destbuffer,sizeof(destbuffer), "hold" );
				held += 1;
			} else if( Find_flag_value(&job->destination,REMOVE_TIME,Value_sep) ){
				plp_snprintf(destbuffer,sizeof(destbuffer), "remove" );
			} else if( Find_flag_value(&job->destination,DONE_TIME,Value_sep) ){
				plp_snprintf(destbuffer,sizeof(destbuffer), "done" );
			} else if( (n = Find_flag_value(&job->destination,SERVER,Value_sep))
				&& kill( n, 0 ) == 0 ){
				int delta;
				n = Find_flag_value(&job->destination,START_TIME,Value_sep);
				delta = time((void *)0) - n;
				if( Stalled_time_DYN && delta > Stalled_time_DYN ){
					plp_snprintf( destbuffer, sizeof(destbuffer),
						"stalled(%dsec)", delta );
				} else {
					n = Find_flag_value(&job->destination,ATTEMPT,Value_sep);
					plp_snprintf(destbuffer,sizeof(destbuffer), "active" );
					if( n > 0 ){
						plp_snprintf( destbuffer, sizeof(destbuffer),
							"active(attempt-%d)", n+1 );
					}
				}
				printable += 1;
			} else if((s = Find_str_value(&job->destination,MOVE,Value_sep)) ){
				plp_snprintf(destbuffer,sizeof(destbuffer), "moved->%s", s );
				move += 1;
			} else if( Get_hold_class(&job->destination, spool_control ) ){
				plp_snprintf(destbuffer,sizeof(destbuffer), "holdclass" );
				held += 1;
			} else {
				printable += 1;
			}
			Set_str_value(&job->destination,PRSTATUS,destbuffer);
			Set_flag_value(&job->destination,PRINTABLE,printable);
			Set_flag_value(&job->destination,HELD,held);
			Update_destination(job);
		}
	}

	Set_str_value(&job->info,PRSTATUS,buffer);
	Set_flag_value(&job->info,PRINTABLE,printable);
	Set_flag_value(&job->info,HELD,held);
	if( pprintable ) *pprintable = printable;
	if( pheld ) *pheld = held;
	if( pmove ) *pmove = move;
	DEBUG3("Job_printable: printable %d, held %d, move '%d', status '%s'",
		printable, held, move, buffer );

}

int Server_active( char *dir, char *file )
{
	struct stat statb;
	int serverpid = 0;
	char *path = Make_pathname( dir, file );
	int fd = Checkread( path, &statb );
	if( fd >= 0 ){
		serverpid = Read_pid( fd, 0, 0 );
		close(fd);
		DEBUG5("Server_active: checking path %s, serverpid %d", path, serverpid );
		if( serverpid && kill(serverpid,0) ){
			serverpid = 0;
		}
	}
	DEBUG3("Server_active: path %s, serverpid %d", path, serverpid );
	if(path) free(path); path = 0;
	return( serverpid );
}

/*
 * Destination Information
 *   The destination information is stored in the control file
 * as lines of the form:
 * NNN=.....   where NNN is the destination number
 *                   .... is the escaped destination information
 * During normal printing or other activity,  the destination information
 * is unpacked into the job->destination line list.
 */

/*
 * Update_destination updates the information with the values in the
 *  job->destination line list.
 */
void Update_destination( struct job *job )
{
	char *s, *t, buffer[SMALLBUFFER];
	int id;
	id = Find_flag_value(&job->destination,DESTINATION,Value_sep);
	plp_snprintf(buffer,sizeof(buffer),"DEST%d",id);
	s = Join_line_list(&job->destination,"\n");
	t = Escape(s,0,1);
	Set_str_value(&job->info,buffer,t);
	free(s);
	free(t);
	if(DEBUGL4)Dump_job("Update_destination",job);
}

/*
 * Get_destination puts the requested information into the
 *  job->destination structure if it is available.
 *  returns: 1 if not found, 0 if found;
 */

int Get_destination( struct job *job, int n )
{
	char buffer[SMALLBUFFER];
	char *s;
	int result = 1;

	plp_snprintf(buffer,sizeof(buffer), "DEST%d", n );

	Free_line_list(&job->destination);
	if( (s = Find_str_value(&job->info,buffer,Value_sep)) ){
		s = safestrdup(s,__FILE__,__LINE__);
		Unescape(s);
		Split(&job->destination,s,Line_ends,1,Value_sep,1,1,1);
		if(s) free( s ); s = 0;
		result = 0;
	}
	return( result );
}

/*
 * Get_destination_by_name puts the requested information into the
 *  job->destination structure if it is available.
 *  returns: 1 if not found, 0 if found;
 */

int Get_destination_by_name( struct job *job, char *name )
{
	int result = 1;
	char *s;

	Free_line_list(&job->destination);
	if( name && (s = Find_str_value(&job->info,name,Value_sep)) ){
		s = safestrdup(s,__FILE__,__LINE__);
		Unescape(s);
		Split(&job->destination,s,Line_ends,1,Value_sep,1,1,1);
		if(s) free( s ); s = 0;
		result = 0;
	}
	return( result );
}

/*
 * Trim_status_file - trim a status file to an acceptible length
 */

int Trim_status_file( char *path, int max, int min )
{
	int status_fd = -1, tempfd = -1, status = -1;
	char buffer[LARGEBUFFER];
	struct stat statb;
	char *tempfile, *s;
	int count;

	memset(&statb,0,sizeof(statb));
	if(path) status = stat(path,&statb);

	DEBUG1("Trim_status_file: '%s' max %d, min %d, size %ld", path, max, min, 
		(long)(statb.st_size) );

	if( status != -1 && max > 0 && statb.st_size/1024 > max
		&& (status_fd = open(path,O_RDONLY,0)) >= 0
		&& (tempfd = Make_temp_fd(&tempfile)) >= 0 ){
		if( min > max ){
			min = max/4;
		}
		if( min == 0 ) min = 1;
		DEBUG1("Trim_status_file: trimming to %d K", min);
		if( lseek( status_fd, -min*1024, SEEK_END ) < 0 ){
			Errorcode = JABORT;
			logerr_die( LOG_ERR, "Trim_status_file: cannot seek '%s'", path );
		}
		while( (count = read( status_fd, buffer, sizeof(buffer) - 1 ) ) > 0 ){
			buffer[count] = 0;
			if( (s = safestrchr(buffer,'\n')) ){
				*s++ = 0;
				Write_fd_str( tempfd, s );
				break;
			}
		}
		while( (count = read( status_fd, buffer, sizeof(buffer) ) ) > 0 ){
			Errorcode = JABORT;
			if( write( tempfd, buffer, count) < 0 ){
				logerr_die( LOG_ERR, "Trim_status_file: cannot write tempfile" );
			}
		}
		close( tempfd ); tempfd = -1;
		close( status_fd ); status_fd = -1;
		if( rename( tempfile, path ) == -1 ){
			Errorcode = JABORT;
			logerr_die( LOG_ERR, "Trim_status_file: rename '%s' to '%s' failed",
				tempfile, path );
		}
	}
	if( status_fd >= 0 ) close(status_fd);
	status_fd = Checkwrite( path, &statb,0,Create_files_DYN,0);
	return( status_fd );
}

/********************************************************************
 * int Fix_control( struct job *job, char *order )
 *   fix the order of lines in the control file so that they
 *   are in the order of the letters in the order string.
 * Lines are checked for metacharacters and other trashy stuff
 *   that might have crept in by user efforts
 *
 * job - control file area in memory
 * order - order of options
 *
 *  order string: Letter - relative position in file
 *                * matches any character not in string
 *                  can have only one wildcard in string
 *   Berkeley-            HPJCLIMWT1234
 *   PLP-                 HPJCLIMWT1234*
 *
 * RETURNS: 0 if fixed correctly
 *          non-zero if there is something wrong with this file and it should
 *          be rejected out of hand
 ********************************************************************/

/********************************************************************
 * BSD and LPRng order
 * We use these values to determine the order of jobs in the file
 * The order of the characters determines the order of the options
 *  in the control file.  A * puts all unspecified options there
 ********************************************************************/

 static char BSD_order[] = "HPJCLIMWT1234" ;

 static char LPRng_order[] = "HPJCLIMWT1234*" ;



char *Fix_datafile_info( struct job *job, char *number, char *suffix )
{
	int i, c, copies, linecount, count, jobcopies, copy;
	char *s, *Nline, *transfername, *dataline;
	struct line_list *lp, outfiles;
	char prefix[8];
	char fmt[2];
	
	Init_line_list(&outfiles);
	transfername = dataline = Nline = 0;
	if(DEBUGL4)Dump_job("Fix_datafile_info - starting", job );

	/* now we find the number of different data files */

	count = 0;
	/* we look through the data file list, looking for jobs with the name
	 * TRANSFERNAME.  If we find a new one, we create the correct form
	 * of the job datafile
	 */
	for( linecount = 0; linecount < job->datafiles.count; ++linecount ){
		lp = (void *)job->datafiles.list[linecount];
		transfername = Find_str_value(lp,TRANSFERNAME,Value_sep);
		if( !(s = Find_casekey_str_value(&outfiles,transfername,Value_sep)) ){
			/* we add the entry */
			c = 'A'+count;
			if( c > 'Z' ) c += 'a' - 'Z' - 1;
			++count;
			plp_snprintf(prefix,sizeof(prefix),"df%c",c);
			s = safestrdup3(prefix,number,suffix,__FILE__,__LINE__);
			if( transfername ) Set_casekey_str_value(&outfiles,transfername,s);
			Set_str_value(lp,TRANSFERNAME,s);
			if(s) free(s); s = 0;
		} else {
			Set_str_value(lp,TRANSFERNAME,s);
		}
	}
	Free_line_list(&outfiles);

	if( count > 52 ){
		fatal(LOG_INFO,"Fix_datafile_info: too many job files");
	}
	if(DEBUGL4)Dump_job("Fix_datafile_info - after finding duplicates", job );

	jobcopies = Find_flag_value(&job->info,COPIES,Value_sep);
	if( !jobcopies ) jobcopies = 1;
	fmt[0] = 'f'; fmt[1] = 0;
	DEBUG4("Fix_datafile_info: jobcopies %d", jobcopies );
	for(copy = 0; copy < jobcopies; ++copy ){
		for( linecount = 0; linecount < job->datafiles.count; ++linecount ){
			lp = (void *)job->datafiles.list[linecount];
			if(DEBUGL5)Dump_line_list("Fix_datafile_info - info", lp  );
			transfername = Find_str_value(lp,TRANSFERNAME,Value_sep);
			Nline = Find_str_value(lp,"N",Value_sep);
			fmt[0] = 'f';
			if( (s = Find_str_value(lp,FORMAT,Value_sep)) ){
				fmt[0] = *s;
			}
			if( (Is_server || Lpr_bounce_DYN) && Xlate_format_DYN
				&& (s = safestrchr( Xlate_format_DYN, fmt[0] )) && islower(cval(s+1)) ){
				fmt[0] = cval(s+1);
			}
			copies = Find_flag_value(lp,COPIES,Value_sep);
			if( copies == 0 ) copies = 1;
			for(i = 0; i < copies; ++i ){
				if( Nline && !Nline_after_file_DYN ){
					dataline = safeextend4(dataline,"N",Nline,"\n",__FILE__,__LINE__);
				}
				dataline = safeextend4(dataline,fmt,transfername,"\n",__FILE__,__LINE__);
				if( Nline && Nline_after_file_DYN ){
					dataline = safeextend4(dataline,"N",Nline,"\n",__FILE__,__LINE__);
				}
			}
			DEBUG4("Fix_datafile_info: file [%d], dataline '%s'",
				linecount, dataline);
		}
	}
	DEBUG4("Fix_datafile_info: adding remove lines" );
	for( linecount = 0; linecount < job->datafiles.count; ++linecount ){
		lp = (void *)job->datafiles.list[linecount];
		if(DEBUGL4)Dump_line_list("Fix_datafile_info - info", lp );
		transfername = Find_str_value(lp,TRANSFERNAME,Value_sep);
		if( !Find_casekey_str_value(&outfiles,transfername,Value_sep) ){
			dataline = safeextend4(dataline,"U",transfername,"\n",__FILE__,__LINE__);
			Set_casekey_str_value(&outfiles,transfername,"YES");
		}
		DEBUG4("Fix_datafile_info: file [%d], dataline '%s'",
			linecount, dataline);
	}
	Free_line_list(&outfiles);

	Set_str_value(&job->info,DATALINES,dataline);
	if(dataline) free(dataline); dataline = 0;
	if(DEBUGL3)Dump_job("Fix_datafile_info - finished",job);
	s = Find_str_value(&job->info,DATALINES,Value_sep);
	return(s);
}

int ordercomp( char *order, const void *left, const void *right )
{
	const char *lpos, *rpos, *wildcard;
	int cmp;

	/* blank lines always come last */
	if( (wildcard = safestrchr( order, '*' )) ){
		wildcard = order + strlen(order);
	}
	lpos = *((const char **)left);
	if( lpos == 0 || *lpos == 0 ){
		lpos = order+strlen(order);
	} else if( !(lpos = safestrchr( order, *lpos )) ){
		lpos = wildcard;
	}
	rpos = *((const char **)right);
	if( rpos == 0 || *rpos == 0 ){
		rpos = order+strlen(order);
	} else if( !(rpos = safestrchr( order, *rpos )) ){
		rpos = wildcard;
	}
	cmp = lpos - rpos;
	DEBUG4("ordercomp '%s' to '%s' -> %d",
		*((const char **)left), *((const char **)right), cmp );
	return( cmp );
}

int BSD_sort( const void *left, const void *right )
{
	return( ordercomp( BSD_order, left, right) );
}
int LPRng_sort( const void *left, const void *right )
{
	return( ordercomp( LPRng_order, left, right) );
}


/************************************************************************
 * Fix_control:
 *  Fix up the control file,  setting the various entries
 *  to be compatible with transfer to the remote location
 * 1. info will have fromhost, priority, and number information
 *   if not, you will need to add it.
 *
 ************************************************************************/

 struct maxlen{
	int c, len;
 } maxclen[] = {
	{ 'A', 131 }, { 'C', 31 }, { 'D', 1024 }, { 'H', 31 }, { 'I', 31 },
	{ 'J', 99 }, { 'L', 31 }, { 'N', 131 }, { 'M', 131 }, { 'N', 131 },
	{ 'P', 31 }, { 'Q', 131 }, { 'R', 131 }, { 'S', 131 }, { 'T', 79 },
	{ 'U', 131 }, { 'W', 31 }, { 'Z', 1024 }, { '1', 131 }, { '2', 131 },
	{ '3', 131 }, { '4', 131 },
	{0,0}
	};

int Fix_control( struct job *job, char *filter )
{
	char *s, *t, *file_hostname, *number, *priority,
		*datainfo, *order;
	int i, n, j, c, wildcard, pid, len;
	plp_status_t status;
	struct line_list l;
	int error = 0;

	Init_line_list(&l);


	n = j = 0;
	n = Find_decimal_value( &job->info,NUMBER,Value_sep);
	j = Find_decimal_value( &job->info,SEQUENCE,Value_sep);

	number = Fix_job_number(job, n+j);
	
	if( !(priority = Find_str_value( &job->info,PRIORITY,Value_sep))
		&& !(priority = Default_priority_DYN) ){
		priority = "A";
	}
	if( (s = Find_str_value( &job->destination,PRIORITY,Value_sep)) ){
		priority = s;
	}

	file_hostname = Find_str_value(&job->info,FILE_HOSTNAME,Value_sep);

	if( !file_hostname ){
		file_hostname = Find_first_letter(&job->controlfile,'H',0);
		if( file_hostname == 0 || *file_hostname == 0 ){
			file_hostname = FQDNHost_FQDN;
		}
		Set_str_value(&job->info,FILE_HOSTNAME,file_hostname);
		file_hostname = Find_str_value(&job->info,FILE_HOSTNAME,Value_sep);
	}

	if( (Backwards_compatible_DYN || Use_shorthost_DYN)
		&& (s = safestrchr( file_hostname, '.' )) ){
		*s = 0;
	}

	if(DEBUGL3) Dump_job( "Fix_control: before fixing", job );


	/* fix control file name */

	s = safestrdup4("cf",priority,number,file_hostname,__FILE__,__LINE__);
	Set_str_value(&job->info,TRANSFERNAME,s);
	if(s) free(s); s = 0;

	/* fix control file contents */

	if( (Is_server && Auth_forward_DYN == 0) ){
		/* clobber the authentication information */
		if( Find_first_letter(&job->controlfile,'_',&n) ){
			Remove_line_list(&job->controlfile,n);
		}
	}
	if( Use_identifier_DYN ){
		if( job->destination.count == 0 ){
			s = Find_str_value(&job->info,IDENTIFIER,Value_sep);
			if( !s ){
				Make_identifier( job );
				s = Find_str_value(&job->info,IDENTIFIER,Value_sep);
			}
			Set_letter_str(&job->controlfile,'A',s);
		} else {
			char buffer[16];
			s = Find_str_value(&job->destination,IDENTIFIER,Value_sep);
			c = Find_flag_value(&job->destination,COPIES,Value_sep);
			n = Find_flag_value(&job->destination,COPY_DONE,Value_sep);
			if( c > 1 ){
				plp_snprintf(buffer,sizeof(buffer),"C%d",n+1);
				s = safestrdup2(s,buffer,__FILE__,__LINE__);
				Set_letter_str(&job->controlfile,'A',s);
				if(s) free(s); s = 0;
			} else {
				Set_letter_str(&job->controlfile,'A',s);
			}
		}
	}
	if( Use_date_DYN &&
		!Find_first_letter(&job->controlfile,'D',0) ){
		Set_letter_str(&job->controlfile,'D', Time_str( 0, 0 ) );
	}
	if( (Use_queuename_DYN || Force_queuename_DYN) &&
		!Find_first_letter(&job->controlfile,'Q',0) ){
		s = Force_queuename_DYN;
		if( s == 0 ) s = Queue_name_DYN;
		if( s == 0 ) s = Printer_DYN;
		Set_letter_str(&job->controlfile,'Q', s );
	}

	/* fix up the control file lines overrided by routing */
	for( i = 0; i < job->destination.count; ++i ){
		s = job->destination.list[i];
		c = cval(s);
		if( isupper(c) && Find_first_letter(&job->controlfile,c,&n) ){
			free(job->controlfile.list[n]);
			job->controlfile.list[n] = safestrdup(s,__FILE__,__LINE__);
		}
	}

	order = LPRng_order;
    if( Backwards_compatible_DYN ){
        order = BSD_order;
	}
	wildcard = (safestrchr( order,'*') != 0);

	/*
	 * remove any line not required and fix up line metacharacters
	 */

	for( i = 0; i < job->controlfile.count; ){
		/* get line and first character on line */
		c = 0;
		if( (s = job->controlfile.list[i]) ){
			c = cval(s);
			Clean_meta(s);
		}
		/* remove any non-listed options */
		if( (!isupper(c) && !isdigit(c)) || (!safestrchr(order, c) && !wildcard) ){
			Remove_line_list(&job->controlfile,i);
		} else {
			if( Backwards_compatible_DYN ){
				for( j = 0; maxclen[j].c && c != maxclen[j].c ; ++j );
				if( (len = maxclen[j].len) && strlen(s) > len ){
					s[len] = 0;
				}
			}
			++i;
		}
	}

	/*
	 * we check to see if order is correct - we need to check to
	 * see if allowed options in file first.
	 */

	if(DEBUGL3)Dump_job( "Fix_control: before sorting", job );
    if( Backwards_compatible_DYN ){
		n = Mergesort( job->controlfile.list,
			job->controlfile.count, sizeof( char *), BSD_sort );
	} else {
		n = Mergesort( job->controlfile.list,
			job->controlfile.count, sizeof( char *), LPRng_sort );
	}
	if( n ){
		Errorcode = JABORT;
		logerr_die( LOG_ERR, "Fix_control: Mergesort failed" );
	}

	if(DEBUGL3) Dump_job( "Fix_control: after sorting", job );
	datainfo = Fix_datafile_info( job, number, file_hostname );
	DEBUG3( "Fix_control: data info '%s'", datainfo );
	s = Join_line_list(&job->controlfile,"\n");
	DEBUG3( "Fix_control: control info '%s'", s );
	s = safeextend2(s,datainfo,__FILE__,__LINE__);
	Set_str_value(&job->info,CF_OUT_IMAGE,s);
	if( s ) free(s); s = 0;
	
	if( filter ){
		int tempfd, tempcf, error_fd[2];
		struct line_list files;
		char buffer[LARGEBUFFER];
		DEBUG3("Fix_control: filter '%s'", filter );

		Init_line_list(&files);
		/* make copies of the data file information */

		tempfd = Make_temp_fd( 0 );
		tempcf = Make_temp_fd( 0 );

		if( pipe(error_fd) == -1 ){
			Errorcode = JFAIL;
			logerr_die( LOG_INFO, "Fix_control: pipe() failed" );
		}

		t = Find_str_value(&job->info,CF_OUT_IMAGE,Value_sep );
		if( Write_fd_str( tempcf, t ) < 0 ){
			Errorcode = JFAIL;
			logerr_die( LOG_INFO, "Fix_control: write to tempfile failed" );

		}
		if( lseek( tempcf, 0, SEEK_SET ) < 0 ){
			Errorcode = JFAIL;
			logerr_die( LOG_INFO, "Fix_control: lseek failed" );
		}

		Free_line_list(&files);
		Check_max(&files,10);
		files.list[files.count++] = Cast_int_to_voidstar(tempcf); 	/* input */
		files.list[files.count++] = Cast_int_to_voidstar(tempfd);	/* output */
		files.list[files.count++] = Cast_int_to_voidstar(error_fd[1]);			/* stderr */
		if( (pid = Make_passthrough( filter, Filter_options_DYN, &files, job, 0 )) < 0 ){
			Errorcode = JFAIL;
			logerr_die(LOG_INFO,"Fix_control:  could not create process");
		}
		files.count = 0;
		Free_line_list(&files);

		close(error_fd[1]); error_fd[1] = -1;
		len = 0;
		buffer[0] = 0;
		while( len < sizeof(buffer)-1
			&& (n = read(error_fd[0],buffer+len, sizeof(buffer)-1-len )) > 0 ){
			buffer[len+n] = 0;
			while( (s = safestrchr(buffer,'\n')) ){
				*s++ = 0;
				setstatus(job,"control filter error '%s'", buffer );
				memmove(buffer,s,strlen(s)+1);
			}
		}
		close(error_fd[0]); error_fd[0] = -1;

		
		while( (n = plp_waitpid(pid,&status,0)) != pid );
		if( WIFEXITED(status) && (n = WEXITSTATUS(status)) ){
			error = Errorcode = n;
			logmsg(LOG_INFO,
			"Fix_control: control filter process exited with status %d",
				status);
			goto error;
		} else if( WIFSIGNALED(status) ){
			n = WTERMSIG(status);
			logerr_die(LOG_INFO,
			"Fix_control: control filter process died with signal %d, '%s'",
				n, Sigstr(n));
		}

		close( tempcf );
		if( lseek( tempfd, 0, SEEK_SET ) < 0 ){
			Errorcode = JFAIL;
			logerr_die( LOG_INFO, "Fix_control: lseek failed" );
		}
		s = 0;
		c = 0;
		while( (n = read(tempfd,buffer,sizeof(buffer))) > 0 ){
			s = realloc_or_die(s,c+n+1,__FILE__,__LINE__);
			memcpy(s+c,buffer,n);
			c += n;
			s[c] = 0;
		}
		if( n < 0 ){
			Errorcode = JFAIL;
			logerr_die( LOG_INFO, "Fix_control: read from tempfd failed" );
		}
		if( s == 0 || *s == 0 ){
			Errorcode = JFAIL;
			logerr_die( LOG_INFO, "Fix_control: zero length control filter output" );
		}
		DEBUG4("Fix_control: control filter output '%s'", s);
		Set_str_value(&job->info,CF_OUT_IMAGE,s);
		if(s) free(s); s = 0;
	}
 error:
	Free_line_list(&l);
	return( error );
}

/************************************************************************
 * Create_control:
 *  Create the control file,  setting the various entries
 *  to be compatible with transfer to the remote location
 * 1. info will have fromhost, priority, and number information
 *   if not, you will need to add it.
 *
 ************************************************************************/

int Create_control( struct job *job, char *error, int errlen )
{
	char *s, *t, *file_hostname, *number, *priority, *datainfo, *openname;
	int status = 0, fd;

	if(DEBUGL3) Dump_job( "Create_control: before fixing", job );

	/* deal with authentication */
	if( Auth_client_id_DYN ){
		Set_str_value(&job->info,AUTHINFO,Auth_client_id_DYN);
		Set_letter_str(&job->controlfile,'_',Auth_client_id_DYN);
	}
	if( !(s = Find_str_value(&job->info,FROMHOST,Value_sep)) || Clean_name(s) ){
		Set_str_value(&job->info,FROMHOST,FQDNRemote_FQDN);
		Set_letter_str(&job->controlfile,'H',FQDNRemote_FQDN);
		s = FQDNRemote_FQDN;
	}
	if( Force_FQDN_hostname_DYN && !safestrchr(s,'.')
		&& (t = safestrchr(FQDNRemote_FQDN,'.')) ){
		s = safestrdup2(s, t, __FILE__,__LINE__ );
		Set_str_value(&job->info,FROMHOST,s);
		Set_letter_str(&job->controlfile,'H',s);
		if( s ) free(s); s = 0;
	}
	/* fix control file contents */
	if( Use_identifier_DYN &&
		!Find_str_value(&job->info,IDENTIFIER,Value_sep) ){
		s = Make_identifier( job );
		Set_letter_str(&job->controlfile,'A',s);
	}

	if( Use_date_DYN && Find_str_value(&job->info,DATE,Value_sep) ){
		s = Time_str(0,0);
		Set_letter_str(&job->controlfile,'D',s);
		Set_str_value(&job->info,DATE,s);
	}
	if( (Use_queuename_DYN || Force_queuename_DYN)
		&& !Find_str_value(&job->info,QUEUENAME,Value_sep) ){
		s = Force_queuename_DYN;
		if( s == 0 ) s = Queue_name_DYN;
		if( s == 0 ) s = Printer_DYN;
		Set_letter_str(&job->controlfile,'Q',s);
		Set_str_value(&job->info,QUEUENAME,s);
		Set_DYN(&Queue_name_DYN,s);
	}
	if( Hld_all(&Spool_control) || Auto_hold_DYN ){
		Set_flag_value( &job->info,HOLD_TIME,time((void *)0) );
	} else {
		Set_flag_value( &job->info,HOLD_TIME,0);
	}

	number = Find_str_value( &job->info,NUMBER,Value_sep);

	priority = Find_str_value( &job->info,PRIORITY,Value_sep);
	if( !priority ){
		priority = Default_priority_DYN;
		if( !priority ) priority = "A";
		Set_str_value(&job->info,PRIORITY,priority);
		priority = Find_str_value(&job->info,PRIORITY,Value_sep);
	}

	file_hostname = Find_first_letter(&job->controlfile,'H',0);
	if( file_hostname == 0 || file_hostname[0] == 0 ){
		file_hostname = FQDNHost_FQDN;
	}
	Set_str_value(&job->info,FILE_HOSTNAME,file_hostname);
	file_hostname = Find_str_value(&job->info,FILE_HOSTNAME,Value_sep);

	/* fix control file name */

	s = safestrdup4("cf",priority,number,file_hostname,__FILE__,__LINE__);
	Set_str_value(&job->info,TRANSFERNAME,s);
	if(s) free(s); s = 0;

	s = Join_line_list(&job->controlfile,"\n");
	DEBUG4("Create_control: first part '%s'", s );
	datainfo = Fix_datafile_info( job, number, file_hostname );
	DEBUG4("Create_control: data info '%s'", datainfo );
	s = safeextend2(s,datainfo,__FILE__,__LINE__);
	DEBUG4("Create_control: joined '%s'", s );
	t = Escape(s,0,1);
	DEBUG4("Create_control: escaped '%s'", t );
	Set_str_value(&job->info,CF_ESC_IMAGE,t);
	if( t ) free(t); t = 0;

	openname = Find_str_value(&job->info,OPENNAME,Value_sep); 
	if( (fd = open(openname,O_WRONLY,0)) < 0
		|| ftruncate(fd,0) || Write_fd_str(fd,s) < 0 ){
		plp_snprintf(error,errlen,"Create_control: cannot write '%s' - '%s'",
			openname, Errormsg(errno) );
		status = 1;
	}
	if( s ) free(s); s = 0;
	if( fd > 0 ) close(fd); fd = -1;
	if(DEBUGL3) Dump_job( "Create_control: after fixing", job );
	return( status );
}

/*
 * Buffer management
 *  Set up and put values into an output buffer for
 *  transmission at a later time
 */
void Init_buf(char **buf, int *max, int *len)
{
	DEBUG4("Init_buf: buf 0x%lx, max %d, len %d",
		Cast_ptr_to_long(*buf), *max, *len );
	if( *max <= 0 ) *max = LARGEBUFFER;
	if( *buf == 0 ) *buf = realloc_or_die( *buf, *max+1,__FILE__,__LINE__);
	*len = 0;
	(*buf)[0] = 0;
}

void Put_buf_len( char *s, int cnt, char **buf, int *max, int *len )
{
	DEBUG4("Put_buf_len: starting- buf 0x%lx, max %d, len %d, adding %d",
		Cast_ptr_to_long(*buf), *max, *len, cnt );
	if( s == 0 || cnt <= 0 ) return;
	if( *max - *len <= cnt ){
		*max += ((LARGEBUFFER + cnt )/1024)*1024;
		*buf = realloc_or_die( *buf, *max+1,__FILE__,__LINE__);
		DEBUG4("Put_buf_len: update- buf 0x%lx, max %d, len %d",
		Cast_ptr_to_long(*buf), *max, *len);
		if( !*buf ){
			Errorcode = JFAIL;
			logerr_die( LOG_INFO,"Put_buf_len: realloc %d failed", *len );
		}
	}
	memcpy( *buf+*len, s, cnt );
	*len += cnt;
	(*buf)[*len] = 0;
}

void Put_buf_str( char *s, char **buf, int *max, int *len )
{
	if( s && *s ) Put_buf_len( s, strlen(s), buf, max, len );
}

void Free_buf(char **buf, int *max, int *len)
{
	if( *buf ) free(*buf); *buf = 0;
	*len = 0;
	*max = 0;
}
