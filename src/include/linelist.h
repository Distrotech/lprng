/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@astart.com
 * See LICENSE for conditions of use.
 * $Id: linelist.h,v 5.4 1999/10/23 04:20:36 papowell Exp papowell $
 ***************************************************************************/



#ifndef _LINELIST_H_
#define _LINELIST_H_ 1

/*
 * arrays of pointers to lines
 */

#define cval(x) (int)(*(unsigned const char *)(x))

struct line_list {
	char **list;	/* array of pointers to lines */
	int count;		/* number of entries */
	int max;		/* maximum number of entries */
};

/*
 * data structure for job
 */

struct job{
	/* information about job in key=value format */
	struct line_list info;

	/* control file lines up to data file info
		Axxx
		Bxxx
	 */
	struct line_list controlfile;

	/* data file lines in raw order
		Nxxx  - name information
		fxxx  - format and file name information
	 */
	struct line_list datafiles;
	struct line_list destination;
};

/*
 * Types of options that we can initialize or set values of
 */
#define FLAG_K		0
#define INTEGER_K	1
#define	STRING_K	2
#define LIST_K		3

/*
 * datastructure for initialization
 */

struct keywords{
    char *keyword;		/* name of keyword */
    int type;			/* type of entry */
    void *variable;		/* address of variable */
	int  maxval;		/* value of token */
	int  flag;			/* flag for variable */
	char *default_value;		/* default value */
};

struct jobwords{
    const char **keyword;		/* name of keyword */
    int type;			/* type of entry */
    void *variable;		/* address of variable */
	int  maxlen;		/* value of token */
	int  key;			/* value of token */
};

/*
 * Variables
 */
extern struct keywords Pc_var_list[], DYN_var_list[];
/* we need to free these when we initialize */

EXTERN struct line_list
	Config_line_list, PC_filters_line_list,
	PC_names_line_list, PC_order_line_list,
	PC_info_line_list, PC_entry_line_list, PC_alias_line_list,
	All_line_list, Spool_control, Sort_order,
	RawPerm_line_list, Perm_line_list, Perm_filters_line_list,
	Process_list, Exit_list, Tempfiles, Servers_line_list, Printer_list,
	Files, Status_lines, Logger_line_list, RemoteHost_line_list;
EXTERN struct line_list *Allocs[]
#ifdef DEFS
	 ={
	 &Config_line_list, &PC_filters_line_list,
	 &PC_names_line_list, &PC_order_line_list,
	 &PC_info_line_list, &PC_entry_line_list, &PC_alias_line_list,
	 &All_line_list, &Spool_control, &Sort_order,
	 &RawPerm_line_list, &Perm_line_list, &Perm_filters_line_list,
	 &Process_list, &Exit_list, &Tempfiles, &Servers_line_list,
	 &Printer_list, &Files, &Status_lines, &Logger_line_list, &RemoteHost_line_list,
	0 }
#endif
	;


/*
 * These record tempfiles for a process, on a per process basis
 */
EXTERN char *Tempfile;

/*
 * Constants
 */
EXTERN char *Value_sep DEFINE( = " \t=#@" );
EXTERN char *Whitespace DEFINE( = " \t\n\f" );
EXTERN char *List_sep DEFINE( = "[] \t\n\f" );
EXTERN char *Linespace DEFINE( = " \t" );
EXTERN char *File_sep DEFINE( = " \t,;:" );
EXTERN char *Strict_file_sep DEFINE( = ";:" );
EXTERN char *Perm_sep DEFINE( = "=,;" );
EXTERN char *Arg_sep DEFINE( = ",;" );
EXTERN char *Name_sep DEFINE( = "|:" );
EXTERN char *Line_ends DEFINE( = "\n\014\004\024" );
EXTERN char *Printcap_sep DEFINE( = "|:" );
EXTERN char *Host_sep DEFINE( = "{} \t," );

/* PROTOTYPES */
void lowercase( char *s );
void uppercase( char *s );
char *trunc_str( char *s);
int Lastchar( char *s );
void *malloc_or_die( size_t size, const char *file, int line );
void *realloc_or_die( void *p, size_t size, const char *file, int line );
char *safestrdup (const char *p, const char *file, int line);
char *safestrdup2( const char *s1, const char *s2, const char *file, int line );
char *safeextend2( char *s1, const char *s2, const char *file, int line );
char *safestrdup3( const char *s1, const char *s2, const char *s3,
	const char *file, int line );
char *safeextend3( char *s1, const char *s2, const char *s3,
	const char *file, int line );
char *safeextend4( char *s1, const char *s2, const char *s3, const char *s4,
	const char *file, int line );
char *safestrdup4( const char *s1, const char *s2,
	const char *s3, const char *s4,
	const char *file, int line );
char *safestrdup5( const char *s1, const char *s2,
	const char *s3, const char *s4, const char *s5,
	const char *file, int line );
void Init_line_list( struct line_list *l );
void Free_line_list( struct line_list *l );
void Free_listof_line_list( struct line_list *l );
void Check_max( struct line_list *l, int incr );
void Add_line_list( struct line_list *l, char *str,
		const char *sep, int sort, int uniq );
void Add_casekey_line_list( struct line_list *l, char *str,
		const char *sep, int sort, int uniq );
void Merge_line_list( struct line_list *dest, struct line_list *src,
	char *sep, int sort, int uniq );
void Merge_listof_line_list( struct line_list *dest, struct line_list *src,
	char *sep, int sort, int uniq );
void Move_line_list( struct line_list *dest, struct line_list *src );
void Split( struct line_list *l, char *str, const char *sep,
	int sort, const char *keysep, int uniq, int trim, int nocomments );
char *Join_line_list( struct line_list *l, char *sep );
char *Join_line_list_with_sep( struct line_list *l, char *sep );
char *Join_line_list_with_quotes( struct line_list *l, char *sep );
void Dump_line_list( const char *title, struct line_list *l );
void Dump_line_list_sub( const char *title, struct line_list *l );
char *Find_str_in_flat( char *str, const char *key, const char *sep );
int Find_last_key( struct line_list *l, const char *key, const char *sep, int *m );
int Find_last_casekey( struct line_list *l, const char *key, const char *sep, int *m );
int Find_first_key( struct line_list *l, const char *key, const char *sep, int *m );
int Find_first_casekey( struct line_list *l, const char *key, const char *sep, int *m );
const char *Find_value( struct line_list *l, const char *key, const char *sep );
char *Find_first_letter( struct line_list *l, const char letter, int *mid );
const char *Find_exists_value( struct line_list *l, const char *key, const char *sep );
char *Find_str_value( struct line_list *l, const char *key, const char *sep );
char *Find_casekey_str_value( struct line_list *l, const char *key, const char *sep );
void Set_str_value( struct line_list *l, const char *key, const char *value );
void Set_casekey_str_value( struct line_list *l, const char *key, const char *value );
void Set_flag_value( struct line_list *l, const char *key, long value );
void Set_double_value( struct line_list *l, const char *key, double value );
void Set_decimal_value( struct line_list *l, const char *key, long value );
void Set_letter_str( struct line_list *l, const char key, const char *value );
void Set_letter_int( struct line_list *l, const char key, long value );
void Remove_line_list( struct line_list *l, int mid );
void Remove_duplicates_line_list( struct line_list *l );
int Find_flag_value( struct line_list *l, const char *key, const char *sep );
int Find_decimal_value( struct line_list *l, const char *key, const char *sep );
double Find_double_value( struct line_list *l, const char *key, const char *sep );
const char *Fix_val( const char *s );
void Read_file_list( int required, struct line_list *model, char *str,
	const char *linesep, int sort, const char *keysep, int uniq, int trim,
	int marker, int doinclude, int nocomment );
void Read_fd_and_split( struct line_list *list, int fd,
	const char *linesep, int sort, const char *keysep, int uniq, int trim, int nocomment );
void Read_file_and_split( struct line_list *list, char *file,
	const char *linesep, int sort, const char *keysep, int uniq, int trim, int nocomment );
int  Build_pc_names( struct line_list *names, struct line_list *order,
	char *str, struct host_information *hostname  );
void Build_printcap_info( 
	struct line_list *names, struct line_list *order,
	struct line_list *list, struct line_list *raw,
	struct host_information *hostname  );
char *Select_pc_info( const char *id, struct line_list *aliases,
	struct line_list *info,
	struct line_list *names,
	struct line_list *order,
	struct line_list *input, int depth );
void Clear_var_list( struct keywords *v, int setv );
void Set_var_list( struct keywords *keys, struct line_list *values );
int Check_str_keyword( const char *name, int *value );
void Config_value_conversion( struct keywords *key, const char *s );
void Expand_percent( char **var );
void Expand_vars( void );
char *Set_DYN( char **v, const char *s );
void Clear_config( void );
char *Find_default_var_value( void *v );
void Get_config( int required, char *path );
void Reset_config( void );
void close_on_exec( int minfd );
void Setup_env_for_process( struct line_list *env, struct job *job );
void Getprintcap_pathlist( int required,
	struct line_list *raw, struct line_list *filters,
	char *path );
void Filterprintcap( struct line_list *raw, struct line_list *filters,
	const char *str );
int In_group( char *group, char *user );
int Check_for_rg_group( char *user );
void Init_tempfile( void );
int Make_temp_fd( char **temppath );
void Clear_tempfile_list(void);
void Unlink_tempfiles(void);
void Remove_tempfiles(void);
void Split_cmd_line( struct line_list *l, char *line );
int Make_passthrough( char *line, char *flags, struct line_list *passfd,
	struct job *job, struct line_list *env_init );
char *Clean_name( char *s );
int Is_meta( int c );
char *Find_meta( char *s );
void Clean_meta( char *t );
void Dump_parms( char *title, struct keywords *k );
struct sockaddr *Fix_auth( int sending, struct sockaddr *src_sin  );
void Fix_dollars( struct line_list *l, struct job *job );
char *Make_pathname( const char *dir,  const char *file );
int Get_keyval( char *s, struct keywords *controlwords );
char *Get_keystr( int c, struct keywords *controlwords );
char *Escape( char *str, int ws, int level );
void Unescape( char *str );
char *Find_str_in_str( char *str, const char *key, const char *sep );
int Find_key_in_list( struct line_list *l, const char *key, const char *sep, int *m );
char *Fix_str( char *str );
int Shutdown_or_close( int fd );

#endif
