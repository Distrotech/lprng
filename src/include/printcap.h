/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: printcap.h
 * PURPOSE:
 * printcap.h,v 3.3 1997/12/16 15:06:46 papowell Exp
 **************************************************************************/

#ifndef _PRINTCAP_H
#define _PRINTCAP_H
/*
 * we extract the printcap file and other information, and put
 * this in a file_entry data structure.
 */
EXTERN struct file_entry Config_info;
EXTERN struct file_entry Raw_printcap_files; /* raw printcap files */

EXTERN struct malloc_list Expanded_printcap_entries;	/* expanded entries */
	/* list is an array of struct printcap */

/* list of %X keys to be expanded in printcap or other files */
EXTERN struct keywords Keyletter[];

/* Printcap referenced variables */
extern struct keywords Pc_var_list[];

EXTERN struct malloc_list All_list;	 /* all printers list */


/* Release all printcap information */
void Free_printcap_information( void );
void Free_file_entry( struct file_entry *entry );

/* get first printer listed in printcap file */
char *Get_first_printer();

/* get printcap for the named printer */
char *Find_printcap_entry( char *name, struct printcap_entry **pc );

/* expand a %X in the printcap value to the current setting */
void Expand_value( struct keywords *vars, struct file_entry *raw );

/* find the printer variable values */
char *Get_printer_vars( char *name, struct printcap_entry **pc_entry );

/* find the printer variable values, and update from spool directory
 * as well.  This is used only by the LPD server
 */
char *Full_printer_vars( char *name, struct printcap_entry **pc_entry );

/* find the filter for the key */
char *Find_filter( int key, struct printcap_entry *printcap_entry );

/* read a printcap file */
char *Readprintcap( char *file, int fd, struct stat *statb,
		struct file_entry *raw );
/* parse the buffer */
int Parse_pc_buffer( char *buffer, char *pathname,
	struct file_entry *raw, int break_on_lines );

/* make a copy of the printcap variables */
char *Linearize_pc_list( struct printcap_entry *printcap_entry, char *parm_name );
/* check to see if printcap variables sorted correctly */
void Check_pc_table( void );

/* Read the printcapfiles */
void Get_all_printcap_entries( void );

/* get the option value for the key */
char  *Get_pc_option_value( char *str, struct printcap_entry *pc );

/* dump the printcap information */
void dump_printcap_entry ( char *title,  struct printcap_entry *entry );


/*
 * Default printcap variable values.  If there is not value here,
 * default is 0 or empty line
 */

void Clear_var_list( struct keywords *vars );
void Set_var_list( char *name, struct keywords *vars, char **values,
	struct file_entry *file_entry );
int Expand_percent( char *s, char *next, char *end );

#endif
