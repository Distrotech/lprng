/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: getconfiginfo.c
 * PURPOSE: get configuration information
 **************************************************************************/

static char *const _id =
"$Id: getcnfginfo.c,v 3.7 1996/08/25 22:20:05 papowell Exp papowell $";

#include "lp.h"
#include "lp_config.h"
#include "printcap.h"

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

static char * Readconfig( char * filename, struct malloc_list *buffers );
	/* read configuration files */
void Parsebuffer( char *filename,
	 char *buffer, struct keywords **keys,
	struct malloc_list *buffers ); /* Parse */
static char *getline( char **next );			/* get next line from buffer */
static void doinclude( char *filename, int count,
	struct token *token, struct keywords **keys, struct malloc_list *buffers );

void Freeconfig( struct keywords **list, struct malloc_list *buffers)
{
	void *p;
	struct keywords *keys;
	for( ; list && *list; ++list ){
		for( keys = *list; keys->keyword; ++keys ){
			p = keys->variable;
			switch( keys->type ){
			case FLAG_K: case INTEGER_K:
				((int *)p)[0] = 0; break;
			case STRING_K:
				((char **)p)[0] = 0; break;
			default:
				fatal( LOG_ERR,"Freeconfig: variable '%s' unknown type",
					keys->keyword );
				break;
			}
		}
	}
	clear_malloc_list(buffers, 1);
}

void Getconfig( char * names, struct keywords **keys,
	struct malloc_list *buffers )
{
	char *s, *end;
	char *buffer;
	int c;
	static char *dupfilename;

	if( dupfilename ){
		free( dupfilename );
		dupfilename = 0;
	}
	if( names ){
		dupfilename = safestrdup( names );
		for( s=dupfilename ; s && *s; s = end ){
			end = strpbrk( s, ":, \t" );
			if( end ){
				*end++ = 0;
			}
			while( (c = *s) && isspace( c ) ) ++s;
			if( c == 0 ) continue;
			DEBUG4("Getconfig: reading '%s'", s );
			buffer = Readconfig( s, buffers );
			if( buffer ){
				Parsebuffer( s, buffer, keys, buffers );
			}
		}
	}
}

/***************************************************************************
 * char *Readconfig( char * filename, struct malloc_list *buffers )
 *  1. we stat the file,  and get the file size
 *  2. we malloc a buffer for the file of filesize+2
 *  3. we read the file into the buffer
 ***************************************************************************/

static char *Readconfig_fd( char *filename, int fd, struct malloc_list *buffers );

static char * Readconfig( char * filename, struct malloc_list *buffers )
{
	int fd;			/* config file */
	struct stat statb;

	fd = Checkread( filename, &statb );
	if( fd < 0 ){
		DEBUG0("Readconfig: can't open config file '%s'", filename);
		return( (char *)0 );
	}
	return( Readconfig_fd( filename, fd, buffers ) );
}

static char *Readconfig_fd( char *filename, int fd, struct malloc_list *buffers )
{
	int i, size, count;	/* number of bytes in config file */
	struct stat statb;	/* stat information */
	char *buffer;		/* buffer to read it into */
	char *s;

	/* now we stat the file and get its size */
	if( fstat( fd, &statb ) != 0 ){
		logerr_die (LOG_NOTICE, "Readconfig_fd: can't stat config file \"%s\"!!",
			filename);
	}

	/* malloc the config file space + 1 */
	size = statb.st_size;
	DEBUG8("Readconfig_fd: file '%s' size %d", filename, size );
	buffer = add_buffer( buffers, size );

	/* read the file into the buffer */
	for( count = 1, i = size, s = buffer;
			count >= 0 && i > 0 ;
			i -= count, s += count ){
		count = read( fd, buffer, i );
	}
	if( count < 0 ){
		logerr_die (LOG_NOTICE,
			"Readconfig_fd_fd: read failed from file \"%s\"!!!",
			filename );
	}
	/* Add a 0 on the end */
	s[0] = 0;
	close( fd );
	return( buffer );
}

/***************************************************************************
 * void Parsebuffer( char *filename, char *buffer, struct keywords **keys )
 * 1. We scan through the buffer, getting a line
 * 2. We crack each line into tokens
 * 3. We then look up the tokens in the keys array for a value
 *    We ignore any we do not find in the keys array
 * 4. We set the value of the variable to the integer/string/list we find
 ***************************************************************************/


#define MAXTOKENS 50

static void setvalue( char *filename,
	int count, struct token *token, struct keywords **keys,
	struct malloc_list *buffers );

void Parsebuffer( char *filename, char *buffer, struct keywords **keys,
	struct malloc_list *buffers )
{
	int tokencount;
	char *startline, *nextline;
	struct token token[MAXTOKENS];	/* very long line */

	nextline = buffer;
	while( (startline = getline( &nextline )) ){
		DEBUG8("Parsebuffer: line '%s'", startline );
		tokencount = Crackline( startline, token, sizeof(token)/sizeof(token[0]) );
		DEBUG8("Parsebuffer: tokencount %d [0]'%s', [1]'%s'",
			tokencount,
			(tokencount>0)?token[0].start:"<NONE>",
			(tokencount>1)?token[1].start:"<NONE>");
		if( tokencount > 0 && token[0].start[0] != '#' ){
			setvalue( filename, tokencount, token, keys, buffers );
		}
	}
}

/***************************************************************************
 * char *getline( char **next )
 *  finds the next line in buffer, starting at position 'next'
 *  updates 'next' with start of next line
 *  checks for continuation character \ at end of line
 *  checks for unescaped # in line and truncates
 *  strips off whitespace at start of line
 *  if line is empty, tries again
 *
 *  RETURNS: pointer to start of line
 *  WARNING: modifies the buffer by placing 0 values at ends of lines
 ***************************************************************************/

static char *getline( char **next )
{
	char *endline;
	char *startline, *nextline = *next;

	do{
		startline = 0;
		while( *nextline ){
			/* find the start of the line */
			if( startline == 0 ) startline = nextline;
			endline = strchr( nextline, '\n' );
			if( endline ){
				*endline = 0;
				nextline = endline+1;
			} else {
				endline = nextline + strlen( nextline );
				nextline = endline;
			}
			/* check to see if the end of the line what \<endofline> */ 
			/* note that you have to check for possibility of first line
				in buffer being a \n and not access outside of buffer */
			if( endline - startline > 1 && endline[-1] == '\\' ){
				endline[-1] = ' ';
				endline[0]  = ' ';
			} else {
				break;
			}
		}
		if( startline ){
			/* check for a comment character in line */
			endline = strchr( startline, '#' );
			if( endline - startline > 1 && endline[-1] != '\\' ){
				*endline = 0;
			}
			/* skip the whitespace at beginning of line */
			while( *startline && isspace( *startline ) ) ++startline;
			/* remove the whitespace at end of line */
			trunc_str( startline );
		}
	} while( startline && *startline == 0 );
	*next = nextline;
	return( startline );
}

/***************************************************************************
 * int *Crackline( char *line, struct token *token )
 *  cracks the input line into tokens,  separating them at whitespace
 *  Note: for most practical purposes,  you have either
 *  1 token:  keyword
 *  2 tokens: keyword value-to-<endofline>
 *  many tokens: keyword value1 value2 value3
 *    - single word tokens separated by whitespace
 ***************************************************************************/

int Crackline( char *line, struct token *token, int max )
{
	int i;
	char *end;
	for(i=0; *line && i < max; ){
		/* strip off whitespace */
		while( *line && isspace( *line ) ) ++line;
		if( *line ){
			token[i].start = line;
			for( end = line; *end && !isspace( *end ); ++end );
			token[i].length = end-line;
			line = end;
			++i;
		}
	}
	return( i );
}

/***************************************************************************
 * static void setvalue( char *filename,
	int count, struct token *token, struct keywords *keys )
 * 1. scans the keys array for a matching keywork entry.
 * 2. if it does not find it in the array, it ignores it
 * 3. if it finds it,  it updates the variable
 * 4. if it finds an include file,  it will parse the include file
 *
 * Note: keywordcmp( char *key, char *name )
 * compares keywords in caseinsensitive form,  allows _ or - in keyword
 ***************************************************************************/

static int keywordcmp( char *key, char *name )
{
	int c1, c2;
	do{
		while( (*key == *name) && (c1 = *key) ){
			++key; ++name;
		}
		c1 = *key++;
		c2 = *name++;
		if( c1 == '-' ) c1 = '_';
		if( c2 == '-' ) c2 = '_';
		if( isupper(c1) ) c1 = tolower(c1);
		if( isupper(c2) ) c2 = tolower(c2);
	} while( (c1 == c2) && c1 );
	return( c1 );
}

static void setvalue( char *filename,	/* file name for error reporting */
	int count,		/* number of tokens on line */
	struct token *token,	/* tokens on line */
	struct keywords **keylist, 	/* keywords to search for */
	struct malloc_list *buffers )
{
	int n;
	char *name, *end, *value;
	struct keywords *keys;

	name = token[0].start;
	end = name+token[0].length;

	n = *end;
	*end = 0;

	DEBUG8("setvalue: name '%s'", name );

	if( 0 == strcmp( name, "include" ) ){
		*end = n;
		doinclude( filename, count, token, keylist, buffers );
		return;
	}
	keys = 0;
	while( *keylist ){
		for( keys = *keylist;
			keys->keyword && keywordcmp( name, keys->keyword );
			++keys
		);
		if( keys->keyword ) break;
		++keylist;
	}
	*end = n;
	if( keys && keys->keyword ){
		/* update the variable */
		DEBUG8("setvalue: found name '%s', type %d",
			keys->keyword, keys->type );
		switch( keys->type ){

		/* flag keyword: set value to 1 */
		case FLAG_K:
		case INTEGER_K:
			if( count != 2 ){
				goto bad;
			}
			value = token[1].start;
			end = value;
			n = strtol( value, &end, 0 );
			DEBUG8("setvalue: integer '%d' value '%s', len %d, converted %d",
				n, value, token[1].length, end-value );
			if( end-value != token[1].length ){
				if( Check_token_keyword( &token[1], &n ) == 0 ){
					logerr_die (LOG_NOTICE,
						"setvalue: file '%s': bad integer value \"%s\"",
						filename, name );
				}
				DEBUG8("setvalue: translated token value '%d'", n );
			}
			*(int *)(keys->variable) = n;
			break;
			
		/* string keyword - assign string value */
		case STRING_K:
			if( count == 1 ){
				*(char **)(keys->variable) = 0;
			} else {
				*(char **)(keys->variable) = token[1].start;
			}
			break;

		/* default */
		default:
			fatal( LOG_ERR,"setvalue: variable '%s' unknown type",
				keys->keyword );
			break;
		}
	}
	return;

bad:
	logerr_die (LOG_NOTICE,
	"setvalue: file '%s': bad number of argument for option \"%s\"",
		filename, name );
}

/***************************************************************************
 * Expandconfig( struct keywords **list )
 *  Scan throught the list of configuration entries and expand them
 *  by doing the following substitutions
 *  '%h' -> ShortHost
 *  '%H' -> FQDNHost
 *  '%a' -> Architecuture
 *  '%x' -> %x (no change)
 * expand_key( char **value )
 * - does the actual expansion
 ***************************************************************************/

static void expand_key( char **value, struct malloc_list *buffers );

void Expandconfig( struct keywords **keylist, struct malloc_list *buffers )
{
	struct keywords *keys;
	while( *keylist ){
		for( keys = *keylist; keys->keyword; ++keys ){
			switch( keys->type ){
			case STRING_K:
				DEBUG8("Expandconfig: before '%s' = '%s'",
					keys->keyword, *(char **)keys->variable );
				expand_key( keys->variable, buffers );
				break;
			default: break;
			}
		}
		++keylist;
	}
}

static struct keywords keyletter[] = {
	{ "h", STRING_K, &ShortHost },
	{ "H", STRING_K, &FQDNHost },
	{ "a", STRING_K, &Architecture },
	{ 0 }
};


 
/***************************************************************************
 * expand_key:
 *  we expand the string into a fixed size buffer, and then we allocate
 *  a dynamic copy if needed
 ***************************************************************************/
 
static void expand_key( char **value, struct malloc_list *buffer )
{
	char *s, *next, *end, *str;
	char copy[LARGEBUFFER];
	struct keywords *keys;
	int c;

	/* check to see if you need to expand */
	if( value == 0 || *value == 0 || strchr( *value, '%' ) == 0 ){
		return;
	}
	next = copy;
	end = copy+sizeof(copy)-1;

	for( s = *value; next < end && s && (c = *s); ++s ){
		if( c != '%' ){
			*next++ = c;
			continue;
		}
		/* we have a '%' character */
		/* get the next character */
		c = *++s;
		if( c == 0 ) break;
		if( c == '%' ){
			*next++ = c;
			continue;
		}
		for( keys = keyletter; keys->keyword && keys->keyword[0] != c; ++keys );
		if( keys->keyword ){
			str = *(char **)keys->variable;
			DEBUG9("expand_key: key '%c' '%s'", c, str );
			if( str ){
				while( next < end && (*next = *str++) ) ++next;
			}
		} else {
			DEBUG9("expand_key: key '%c' not found", c );
			/* we have a unknown configuration expansion value */
			/* we leave it in for later efforts */
			*next++ = '%';
			if( next < end ) *next++ = c;
		}
	}
	*next = 0;
	/* now we allocate a buffer entry */
	str = add_buffer( buffer, strlen( copy )+1 );
	strcpy( str, copy );
	DEBUG8("expand_key: input '%s' result '%s'", *value, str );
	*value = str;
}

/***************************************************************************
 * int Check_str_keyword( char *name, int *value )
 * - check a string for a simple keyword name
 * int Check_token_keyword( struct token *token, int *value )
 * - check a token for a simple keyword name
 *  - strings such as 'yes', 'no'
 *  value is the address of the variable;
 *  returns 0 on failure, 1 on success
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

int Check_token_keyword( struct token *token, int *value )
{
	char *s;
	int n, success;
	s = token->start+token->length;
	n = *s;
	*s = 0;
	success = Check_str_keyword( token->start, value );
	*s = n;
	return( success );
}


/***************************************************************************
 * doinclude(
 *	int count,				 number of tokens on line
 *	struct token *token,	 tokens on line
 *	struct keywords **keylist )	keywords to search for
 * Open and parse the include file
 ***************************************************************************/
 
static int depth;
static void doinclude(
	char *filename,
	int count,		/* number of tokens on line */
	struct token *token,	/* tokens on line */
	struct keywords **keys,
	struct malloc_list *buffers )	/* keywords to search for */
{
	char path[MAXPATHLEN];	/* copy of the include specification */
	char *file, *end;		/* actual file working on */
	int fd, c;				/* file descriptor */
	struct stat statb;
	char *buffer;			/* copy of the file */

	
	if( depth > 10 ){
		fatal (LOG_NOTICE,
			"doinclude: file '%s', nesting too deep (%d)", filename,depth);
	}
	++depth;

	/* now get the file */
	if( count != 2 ){
		fatal (LOG_NOTICE,
			"doinclude: file '%s', bad include line", filename );
	}

	file = token[1].start;
	file[token[1].length] = 0;
	if( token[1].length > sizeof(path)-1 ){
		fatal( LOG_ERR,
			"doinclude: file '%s' included file pathname too long '%s'",
			filename, file );
	}
	strncpy( path, file, sizeof(path) );
	for( file = path; file && *file; file = end ){
		end = strchr( file, ':' );
		if( end ){
			*end++ = 0;
		}
		while( (c = *file) && isspace( *file ) ) ++file;
		if( c == 0 ) continue;
		if( c != '/' ){
			fatal( LOG_ERR,
				"doinclude: file '%s' included file pathname not absolute '%s'",
				filename, file );
		}
		if( (fd = Checkread( file, &statb ) ) >= 0 ){
			DEBUG8("doinclude: STARTING '%s'", file );
			buffer = Readconfig_fd( file, fd, buffers );
			Parsebuffer( file, buffer, keys, buffers ); /* Parse */
			DEBUG8("doinclude: ENDING '%s'", file );
		} else {
			fatal (LOG_NOTICE,
				"doinclude: cannot find include file '%s'", file );
		}
	}
	--depth;
}


/***************************************************************************
 * configuration file keywords and associated variables
 ***************************************************************************/

struct keywords lpd_config[]
 = {
 { "allow_getenv", INTEGER_K, &Allow_getenv },
 { "architecture", STRING_K, &Architecture },
 { "bk_filter_options", STRING_K, &BK_filter_options },
 { "bk_of_filter_options", STRING_K, &BK_of_filter_options },
 { "check_for_nonprintable", INTEGER_K, &Check_for_nonprintable },
 { "client_config_file", STRING_K, &Client_config_file },
 { "connect_grace", INTEGER_K, &Connect_grace },
 { "connect_interval", INTEGER_K, &Connect_interval },
 { "connect_retry", INTEGER_K, &Connect_try },
 { "connect_timeout", INTEGER_K, &Connect_timeout },
 { "default_banner_printer", STRING_K, &Default_banner_printer },
 { "default_format", STRING_K, &Default_format },
 { "default_logger_port", STRING_K, &Default_logger_port },
 { "default_logger_protocol", STRING_K, &Default_logger_protocol },
 { "default_permission", STRING_K, &Default_permission },
 { "default_printer", STRING_K, &Default_printer },
 { "default_priority", STRING_K, &Default_priority },
 { "default_remote_host", STRING_K, &Default_remote_host },
 { "domain_name", STRING_K, &Domain_name },
 { "filter_ld_path", STRING_K, &Filter_ld_path },
 { "filter_options", STRING_K, &Filter_options },
 { "filter_path", STRING_K, &Filter_path },
 { "group", STRING_K, &Server_group },
 { "lockfile", STRING_K, &Lockfile },
 { "logfile", STRING_K, &Logfile },
 { "logger_destination", STRING_K, &Logger_destination },
 { "longnumber", INTEGER_K, &Long_number },
 { "lpd_port", STRING_K, &Lpd_port },
 { "lpd_printcap_path", STRING_K, &Lpd_printcap_path },
 { "mail_operator_on_error", STRING_K, &Mail_operator_on_error },
 { "max_status_size", INTEGER_K, &Max_status_size },
 { "min_status_size", INTEGER_K, &Min_status_size },
 { "minfree", STRING_K, &Minfree }, /**/
 { "of_filter_options", STRING_K, &OF_filter_options },
 { "originate_port", STRING_K, &Originate_port },
 { "printcap_path", STRING_K, &Printcap_path },
 { "printer_perms_path", STRING_K, &Printer_perms_path },
 { "router", STRING_K, &Routing_filter },
 { "save_when_done", INTEGER_K, &Save_when_done },
 { "send_data_first", INTEGER_K, &Send_data_first },
 { "send_failure_action", STRING_K, &Send_failure_action },
 { "send_timeout", INTEGER_K, &Send_timeout },
 { "send_try", INTEGER_K, &Send_try },
 { "sendmail", STRING_K, &Sendmail },
 { "server_config_file", STRING_K, &Server_config_file },
 { "spool_dir_perms", INTEGER_K, &Spool_dir_perms },
 { "spool_file_perms", INTEGER_K, &Spool_file_perms },
 { "syslog_device", STRING_K, &Syslog_device },
 { "use_identifier", INTEGER_K, &Use_identifier },
 { "use_info_cache", INTEGER_K, &Use_info_cache },
 { "use_queuename", INTEGER_K, &Use_queuename },
 { "use_shorthost", INTEGER_K, &Use_shorthost },
 { "user", STRING_K, &Server_user },
 { (char *)0 }
} ;

void Check_lpd_config()
{
	int i;
	char *s, *t;
	for( i = 1; lpd_config[i].keyword; ++i ){
		s = lpd_config[i-1].keyword;
		t = lpd_config[i].keyword;
		DEBUG8("Check_lpd_config: '%s' ?? '%s'", s, t );
		if( strcmp( s, t )  > 0 ){
			fatal( LOG_ERR, "Check_lpd_config: '%s' and '%s' out of order",
				s, t );
		}
	}
}
