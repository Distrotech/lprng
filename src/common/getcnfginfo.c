/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: getconfiginfo.c
 * PURPOSE: get configuration information
 **************************************************************************/

static char *const _id =
"getcnfginfo.c,v 3.7 1998/03/24 02:43:22 papowell Exp";

#include "lp.h"
#include "printcap.h"
#include "getcnfginfo.h"
#include "printcap.h"
#include "dump.h"
#include "fileopen.h"
#include "malloclist.h"
#include "merge.h"
/**** ENDINCLUDE ****/

/***************************************************************************
 * Getconfig( char * filename, struct keywords **keys )
 * Getconfig will get configuration information from a configuration file
 *  This configuration file will be read exactly once,  at startup time.
 *  The method used is simple:
 *  1. we stat the file,  and get the file size
 *  2. we malloc a buffer for the file of filesize+2
 *  3. we read the file into the buffer;
 *  4. we scan the file line by line,  looking for options
 *  5. we set the option variables
 *
 * Configuration file format:
 *  # introduces a comment to the end of line
 *  \<endofline> is replaced by a set of spaces
 *  entry has the form
 *  keyword<whitespace>entry entry entry<endofline>
 *
 *  Some options take 1 value,  some none,  some a list of keywords
 *  Parsing is controlled by values in the struct keywords{}
 *
 * Configuration information is supplied as an array of pointers to
 * arrays of keyword structures:
 *
 * keys-> &keylist1, &keylist2, ... , 0
 *
 * keylists are arrays of struct keywords entries, terminate by 0 valued entry
 *    keylist1 = { {entry0}, {entry1},  ... , {0} };
 *    keylist2 = { {entry0}, {entry1},  ... , {0} };
 * 
 * We scan the configuration information,  looking for lines that match.
 *
 * NOTE: if we use another method,  all we need to do is allocate a buffer
 *  for the information,  read it in, and then call the parse code.
 *
 ***************************************************************************/

void Set_config_var_list( char *title, struct keywords *vars, char **values );

/***************************************************************************
 * void Clear_config( void )
 *  clears all of the configuration information, resets to void
 *  also frees memory allocated for modified strings
 ***************************************************************************/

void Clear_config( void )
{
	DEBUGF(DDB1)("Clear_config: setting to Default");
	Free_file_entry( &Config_info );
	Clear_var_list( Pc_var_list );
	clear_malloc_list( &Config_info.expanded_str, 1 );
	clear_malloc_list( &Raw_printcap_files.expanded_str, 1 );
	Expand_value( Pc_var_list, &Config_info );
}


/***************************************************************************
 * Reset_config() - resets variables to their
 *  original value before printcap overrode values.
 ***************************************************************************/
void Reset_config( void )
{
	DEBUGF(DDB1)( "Reset_config: clearing and resetting values");

	/* clear the allocated memory and then expand strings */
	Clear_var_list( Pc_var_list );
	clear_malloc_list( &Config_info.expanded_str, 1 );
	clear_malloc_list( &Raw_printcap_files.expanded_str, 1 );
	Set_config_var_list( "VALUES", Pc_var_list, Config_info.entries.list );
	Expand_value( Pc_var_list, &Config_info );
}

/***************************************************************************
 * void Get_config( char *names )
 *  gets the configuration information from a list of files
 ***************************************************************************/

void Get_config( char *names )
{
	char entry[MAXPATHLEN];
	char *end, *path;
	int c, fd, err;
	struct stat statb;

	Expand_percent( names, entry, entry+sizeof(entry)-2 );
	entry[sizeof(entry)-1] = 0;
	DEBUGF(DDB1)("Get_config: expanded '%s'", entry);
	for(path = entry; path && *path; path = end ){
		end = strpbrk( path, ";:," );
		if( end ){
			*end++ = 0;
		}
		trunc_str( path );
		while( (c = *path) && isspace(c) ) ++path;
		if( c == 0 ) continue;
		DEBUGF(DDB1)( "Get_config: file '%s'", path );
		if( c != '/' ){
			fatal( LOG_ERR,
				"Get_config: entry not absolute pathname '%s'",
				path );
		}
		fd =  Checkread( path, &statb );
		err = errno;
		if( fd < 0 ){
			DEBUGF(DDB1)( "Get_config: cannot open '%s' - %s",
				path, Errormsg(err) );
		} else if( fd >= 0 ){
			char *s;
			DEBUGF(DDB2)("Get_config: file '%s', size %d",
				path, statb.st_size );
			add_str( &Config_info.files, path,__FILE__,__LINE__ );
			if( (s = Readprintcap( path, fd, &statb, &Config_info)) == 0 ){
				log( LOG_ERR, "Error reading %s", path );
			} else {
				Parse_pc_buffer( s, path, &Config_info, 1 );
			}
		}
		close(fd);
	}
	Set_config_var_list( "VALUES", Pc_var_list, Config_info.entries.list );
	Expand_value( Pc_var_list, &Config_info );
}
/***************************************************************************
 * valuecmp: compare two value entries, excluding the actual value
 * - for Mergesort
 * real_valuecmp: does the actual comparison
 ***************************************************************************/

int real_valuecmp( char *left, char *right );
int valuecmp( const void *l, const void *r )
{
	return( real_valuecmp( *(char **)l, *(char **)r ) );
}

int real_valuecmp( char *left, char *right )
{
	char leftval[LINEBUFFER];
	char rightval[LINEBUFFER];
	char *end;
	int cmp;

	leftval[0] = 0;
	rightval[0] = 0;
	if( left ) safestrncpy( leftval, left );
	if( right ) safestrncpy( rightval, right );
	if( (end = strpbrk( leftval, " \t=#@" )) ) *end = 0;
	if( (end = strpbrk( rightval, " \t=#@" )) ) *end = 0;
	for( end = strchr( leftval, '-' ); end; end = strchr( end+1, '-' ) ){
		*end = '_';
	}
	for( end = strchr( rightval, '-' ); end; end = strchr( end+1, '-' ) ){
		*end = '_';
	}
	cmp = strcmp( leftval, rightval );
	/*DEBUGF(DDB4)("real_valuecmp: %s to %s = %d", leftval, rightval, cmp ); */
	return( cmp );
}

/***************************************************************************
 * Set_config_var_list( char *name, struct keywords *vars, char **values );
 *  1. the pc_var_list and initialization list must be sorted alphabetically
 *  2. we scan each of the lines, one by one.
 *  3.  for each of the entries in the keylist, we search for a match
 *  4.  when we find a match we set the value
 ***************************************************************************/


void Set_config_var_list( char *name, struct keywords *keys, char **values )
{
	struct keywords *vars = keys;
	char *key, *value;
	char *end;
	int c, n, diff;

	/* we sort the values first */
	for( n = 0; values && values[n]; ++n );
	if( n == 0 ){
		DEBUGF(DDB2)("Set_config_var_list: no values");
		return;
	}
	if( Mergesort( values, n, sizeof(values[0]), valuecmp ) ){
		fatal( LOG_ERR, "Set_config_var_list: Mergesort failed" );
	}
	DEBUGFC(DDB3){
		logDebug("Set_config_var_list: '%s' %d values after sorting", name, n );
		for( c = 0; c < n; ++c ){
			logDebug( " [%d] '%s'", c, values[c] );
		}
	}

	while( (key = vars->keyword) && (value = *values) ){
		diff = real_valuecmp( key, value ); 
		DEBUGF(DDB4)("Set_config_var_list: compare '%s' to '%s' = %d",
			key, value,diff );
		if( diff < 0 ){
			++vars;
			continue;
		} else if( diff > 0 ){
			++values;
			continue;
		}
		/* check for a separator in value */
		end = strpbrk( value, " \t=#@" );
		DEBUGF(DDB4)("Set_config_var_list: found '%s', value '%s' ",
			key, end );
		/* we have found the keyword.  Now set the value */
		Config_value_conversion( vars, end );
		++values;
	}
	DEBUGFC(DDB1){
		char title[64];
		plp_snprintf( title, sizeof(title), "Set_config_var_list: %s", name );
		dump_parms( title, keys );
	}
}



/***************************************************************************
 * int Check_str_keyword( char *name, int *value )
 * - check a string for a simple keyword name
 ***************************************************************************/

#define FIXV(S,V) { S, INTEGER_K, (void *)0, V }
static struct keywords simple_words[] = {
 FIXV( "all", 1 ), FIXV( "yes", 1 ), FIXV( "allow", 1 ), FIXV( "true", 1 ),
 FIXV( "no", 0 ), FIXV( "deny", 0 ), FIXV( "false", 0 ),
 FIXV( "none", 0 ), FIXV( (char *)0, 0 ) } ;

int Check_str_keyword( char *name, int *value )
{
	struct keywords *keys;
	for( keys = simple_words; keys->keyword; ++keys ){
		if( !strcasecmp( name, keys->keyword ) ){
			*value = keys->maxval;
			return( 1 );
		}
	}
	return( 0 );
}

/***************************************************************************
 * void Config_value_conversion( struct keyword *key, char *value )
 *  set the value of the variable as required
 ***************************************************************************/
void Config_value_conversion( struct keywords *key, char *s )
{
	int i = 0;
	char *end;		/* end of conversion */
	int value;

	trunc_str( s );
	switch( key->type ){
	case FLAG_K:
		((int *)key->variable)[0] = 1;
	case INTEGER_K:
		if( s && *s ){
			while( *s == '#' || isspace( *s ) ) ++s;
			if( Check_str_keyword( s, &value ) ){
				((int *)key->variable)[0] = value;
			} else if( *s == '@' ){
				((int *)key->variable)[0] = 0;
			} else {
				end = s;
				i = strtol( s, &end, 0 );
				((int *)key->variable)[0] = i;
				if( end == s || *end ){
					log( LOG_ERR,
					"Config_value_conversion: '%s' bad integer value '%s'",
						key->keyword,s);
				}
			}
		}
		DEBUGF(DDB3)("Config_value_conversion: key '%s' INTEGER value %d",
			key->keyword, ((int *)key->variable)[0]);
		break;
	case STRING_K:
		if(s){
			while( (*s == '=') || isspace( *s ) ) ++s;
			trunc_str( s );
			if( *s == 0) s = 0;
		}
		((char **)key->variable)[0] = s;
		DEBUGF(DDB3)("Config_value_conversion: key '%s' STRING value '%s'",
			key->keyword, ((char **)key->variable)[0] );
		break;
	default:
		fatal( LOG_ERR, "variable '%s', unknown type", key->keyword );
		break;
	}
}
