/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1997, Patrick Powell, San Diego, CA
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: Setup_filter.h
 * PURPOSE: include file for setup_filter
 * setup_filter.h,v 3.5 1997/12/31 19:30:10 papowell Exp
 **************************************************************************/

int Make_filter( int key,
  struct control_file *cf,
  struct filter *filter, char *line, int noextra,
  int read_write, int print_fd,
  struct printcap_entry *printcap_entry, struct data_file *data_file, int acct_port,
  int stderr_to_logger, int read_from_file );
int Setup_filter( int fmt, struct control_file *cf,
	char *filtername, struct filter *filter, int noextra,
	struct data_file *data_file );

int Make_passthrough( struct filter *filter, char *line,
	int *fd, int fd_count, int stderr_to_logger,
	struct control_file *cf,
	struct printcap_entry *printcap_entry );

void setup_clean_fds( int minfd );
void setup_close_on_exec( int minfd );
char *find_executable( char *execname, struct filter *filter );
char *Expand_command( struct control_file *cfp,
  char *bp, char *ep, char *s, int fmt, struct data_file *data_file );
void Flush_filter( struct filter *filter );
void Kill_filter( struct filter *filter, int signal );
int Close_filter( struct control_file *cfp,
	struct filter *filter, int timeout, const char *pkey );
char *Filter_read( char *name, struct malloc_list *list, char *filter );

char * Do_dollar( struct control_file *cf, char *s, char *e,
	int type, int fmt, int space, int notag, struct data_file *df,
	int noquotes, char *name );
